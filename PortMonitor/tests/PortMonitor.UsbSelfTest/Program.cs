using System.Buffers.Binary;
using PortMonitor.Core.Usb;

// Usage: dotnet run --project tests/PortMonitor.UsbSelfTest [-- <output sample path>]
var samplePath = args.Length > 0 ? args[0] : Path.Combine(Path.GetTempPath(), "usb-msc-sample.pcap");
int failures = 0;
void Check(bool ok, string what)
{
    Console.WriteLine($"{(ok ? "PASS" : "FAIL")}  {what}");
    if (!ok) failures++;
}

UsbSampleGenerator.Write(samplePath);
Console.WriteLine($"sample → {samplePath} ({new FileInfo(samplePath).Length} bytes)");

List<ScsiCommand> Decode(string path, out int packets)
{
    var svc = new UsbCaptureService();
    var dec = new MassStorageDecoder();
    var rows = new List<ScsiCommand>();
    svc.PacketCaptured += p => { if (dec.Push(p) is { } c) rows.Add(c); };
    packets = svc.LoadFile(path);
    return rows;
}

var cmds = Decode(samplePath, out int pk);
Console.WriteLine($"packets={pk} commands={cmds.Count}");
foreach (var c in cmds)
    Console.WriteLine($"  #{c.Index} tag=0x{c.Tag:X} {c.OpcodeName,-22} LBA={c.Info.Lba?.ToString() ?? "-",-6} blk={c.Info.Blocks?.ToString() ?? "-",-3} {c.StatusLabel,-7} res={c.Residue} {c.DurationMs:0.###}ms | {c.Summary}");

Check(pk == 38, "packet count = 38 (7×CBW+CSW legs + 5 data phases)");
Check(cmds.Count == 7, "7 SCSI commands decoded");
Check(cmds.All(c => c.Device == 5 && c.Bus == 1), "device 1.5");
Check(cmds[0].OpcodeName == "INQUIRY" && cmds[0].Response.Contains("SanDisk") && cmds[0].Response.Contains("Ultra USB 3.0") && cmds[0].Response.Contains("1.00"), "INQUIRY vendor/product/rev");
Check(cmds[1].OpcodeName == "READ CAPACITY(10)" && cmds[1].Response.Contains("块大小=512") && cmds[1].Response.Contains("14.9"), "READ CAPACITY → 512 B blocks, ~14.9 GB");
Check(cmds[2].OpcodeName == "TEST UNIT READY" && cmds[2].Status == CswStatus.Failed, "TEST UNIT READY failed");
Check(cmds[3].OpcodeName == "REQUEST SENSE" && cmds[3].Response.Contains("UNIT ATTENTION") && cmds[3].Response.Contains("28/00"), "REQUEST SENSE → UNIT ATTENTION 28/00");
Check(cmds[4].OpcodeName == "READ(10)" && cmds[4].Info.Lba == 2048 && cmds[4].Info.Blocks == 8 && cmds[4].DataBytes == 4096 && cmds[4].Status == CswStatus.Passed, "READ(10) LBA 2048 ×8, 4096 B, Passed");
Check(Math.Abs((cmds[4].DurationMs ?? 0) - 3.4) < 0.01, "READ(10) latency 3.4 ms");
Check(cmds[5].OpcodeName == "WRITE(10)" && cmds[5].Info.Lba == 100 && cmds[5].Info.Blocks == 1 && cmds[5].DataBytes == 512 && !cmds[5].DataIn, "WRITE(10) LBA 100 ×1, 512 B OUT");
Check(cmds[6].OpcodeName == "SYNCHRONIZE CACHE(10)", "SYNCHRONIZE CACHE(10)");
Check(cmds.Select(c => c.Tag).SequenceEqual(Enumerable.Range(0x10, 7).Select(i => (uint)i)), "tags 0x10..0x16 correlated");

// CDB decode spot checks
Check(ScsiDecoder.DecodeCdb([0x88, 0, 0, 0, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0x20, 0, 0]) is { Name: "READ(16)", Lba: 0x1_0000_0000, Blocks: 0x20 }, "READ(16) LBA/len");
Check(ScsiDecoder.DecodeCdb([0xAA, 0, 0, 0, 1, 0, 0, 0, 0, 4, 0, 0]) is { Name: "WRITE(12)", Lba: 256, Blocks: 4 }, "WRITE(12) LBA/len");
Check(ScsiDecoder.DecodeCdb([0x9E, 0x10, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 32, 0, 0]).Name == "READ CAPACITY(16)", "READ CAPACITY(16)");
Check(ScsiDecoder.DecodeCdb([0xC7, 1, 2]).Name == "OP 0xC7", "unknown opcode → hex");

// pcapng round-trip: wrap the same records in SHB/IDB/EPB
var ngPath = Path.ChangeExtension(samplePath, ".pcapng");
using (var fs = File.Create(ngPath))
{
    void Block(uint type, byte[] body)
    {
        int pad = (4 - body.Length % 4) % 4;
        uint total = (uint)(12 + body.Length + pad);
        Span<byte> h = stackalloc byte[8];
        BinaryPrimitives.WriteUInt32LittleEndian(h, type);
        BinaryPrimitives.WriteUInt32LittleEndian(h[4..], total);
        fs.Write(h); fs.Write(body); fs.Write(new byte[pad]);
        fs.Write(h[4..]);
    }
    var shb = new byte[16];
    BinaryPrimitives.WriteUInt32LittleEndian(shb, 0x1A2B3C4D);
    BinaryPrimitives.WriteUInt16LittleEndian(shb.AsSpan(4), 1);
    BinaryPrimitives.WriteInt64LittleEndian(shb.AsSpan(8), -1);
    Block(0x0A0D0D0A, shb);
    var idb = new byte[8];
    BinaryPrimitives.WriteUInt16LittleEndian(idb, UsbPcapHeader.LinkType);
    Block(1, idb);
    foreach (var (ts, data) in UsbSampleGenerator.Build())
    {
        ulong us = (ulong)((ts - DateTimeOffset.UnixEpoch).Ticks / 10);
        var epb = new byte[20 + data.Length];
        BinaryPrimitives.WriteUInt32LittleEndian(epb.AsSpan(4), (uint)(us >> 32));
        BinaryPrimitives.WriteUInt32LittleEndian(epb.AsSpan(8), (uint)us);
        BinaryPrimitives.WriteUInt32LittleEndian(epb.AsSpan(12), (uint)data.Length);
        BinaryPrimitives.WriteUInt32LittleEndian(epb.AsSpan(16), (uint)data.Length);
        data.CopyTo(epb, 20);
        Block(6, epb);
    }
}
var ng = Decode(ngPath, out int ngPk);
Check(ngPk == 38 && ng.Count == 7 && ng[4].Info.Lba == 2048 && Math.Abs((ng[4].DurationMs ?? 0) - 3.4) < 0.01, "pcapng: same 7 commands + timing");
File.Delete(ngPath);

// CSV export
var csv = Path.ChangeExtension(samplePath, ".scsi.csv");
ScsiCsvExporter.Write(csv, cmds);
var lines = File.ReadAllLines(csv);
Check(lines.Length == 8 && lines[5].Contains("READ(10)") && lines[5].Contains("2048"), "SCSI CSV export");
File.Delete(csv);

Console.WriteLine(failures == 0 ? "ALL TESTS PASSED" : $"{failures} FAILURE(S)");
return failures == 0 ? 0 : 1;
