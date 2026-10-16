using System.Buffers.Binary;
using System.Text;

namespace SerialTap.Core.Usb;

/// <summary>
/// Synthesises a USBPcap capture of a USB flash drive (BOT) session, for tests and demos:
/// INQUIRY, READ CAPACITY(10), TEST UNIT READY (fail) + REQUEST SENSE, READ(10), WRITE(10).
/// </summary>
public static class UsbSampleGenerator
{
    public const ushort Bus = 1, Device = 5;
    private const byte EpOut = 0x02, EpIn = 0x81;

    public static void Write(string path)
    {
        using var fs = File.Create(path);
        PcapReader.WriteClassic(fs, UsbPcapHeader.LinkType, Build());
    }

    public static List<(DateTimeOffset, byte[])> Build()
    {
        var recs = new List<(DateTimeOffset, byte[])>();
        var t = new DateTimeOffset(2026, 10, 16, 11, 0, 0, TimeSpan.Zero);
        ulong irp = 0xFFFF_A000_0000_1000;
        uint tag = 0x10;

        void Add(double ms, byte info, byte ep, ReadOnlySpan<byte> data, uint status = 0) =>
            recs.Add((t.AddTicks((long)(ms * TimeSpan.TicksPerMillisecond)),
                UsbPcapHeader.Build(irp++, status, info, Bus, Device, ep, UsbTransferType.Bulk, data)));

        double now = 0;
        void Command(byte[] cdb, bool dataIn, uint expected, byte[]? data, byte status, uint residue, double durMs)
        {
            var cbw = new byte[31];
            BinaryPrimitives.WriteUInt32LittleEndian(cbw, 0x43425355);
            BinaryPrimitives.WriteUInt32LittleEndian(cbw.AsSpan(4), tag);
            BinaryPrimitives.WriteUInt32LittleEndian(cbw.AsSpan(8), expected);
            cbw[12] = (byte)(dataIn ? 0x80 : 0x00);
            cbw[13] = 0;
            cbw[14] = (byte)cdb.Length;
            cdb.CopyTo(cbw, 15);

            Add(now, 0, EpOut, cbw);                    // CBW submit (OUT carries data)
            Add(now + 0.05, 1, EpOut, ReadOnlySpan<byte>.Empty); // CBW complete
            if (data is not null)
            {
                if (dataIn)
                {
                    Add(now + 0.1, 0, EpIn, ReadOnlySpan<byte>.Empty);
                    Add(now + durMs * 0.8, 1, EpIn, data);  // data IN on completion
                }
                else
                {
                    Add(now + 0.1, 0, EpOut, data);         // data OUT on submission
                    Add(now + durMs * 0.8, 1, EpOut, ReadOnlySpan<byte>.Empty);
                }
            }
            var csw = new byte[13];
            BinaryPrimitives.WriteUInt32LittleEndian(csw, 0x53425355);
            BinaryPrimitives.WriteUInt32LittleEndian(csw.AsSpan(4), tag);
            BinaryPrimitives.WriteUInt32LittleEndian(csw.AsSpan(8), residue);
            csw[12] = status;
            Add(now + durMs * 0.85, 0, EpIn, ReadOnlySpan<byte>.Empty);
            Add(now + durMs, 1, EpIn, csw);             // CSW on IN completion
            tag++;
            now += durMs + 2;
        }

        // INQUIRY
        var inq = new byte[36];
        inq[0] = 0x00; inq[1] = 0x80; inq[2] = 0x06; inq[3] = 0x02; inq[4] = 31;
        Encoding.ASCII.GetBytes("SanDisk ").CopyTo(inq, 8);
        Encoding.ASCII.GetBytes("Ultra USB 3.0   ").CopyTo(inq, 16);
        Encoding.ASCII.GetBytes("1.00").CopyTo(inq, 32);
        Command([0x12, 0, 0, 0, 36, 0], true, 36, inq, 0, 0, 1.2);

        // READ CAPACITY(10): last LBA 0x01DCDFFF (≈ 14.9 GiB @ 512)
        var cap = new byte[8];
        BinaryPrimitives.WriteUInt32BigEndian(cap, 0x01DC_DFFF);
        BinaryPrimitives.WriteUInt32BigEndian(cap.AsSpan(4), 512);
        Command([0x25, 0, 0, 0, 0, 0, 0, 0, 0, 0], true, 8, cap, 0, 0, 0.9);

        // TEST UNIT READY → Failed, then REQUEST SENSE: UNIT ATTENTION 28/00
        Command([0x00, 0, 0, 0, 0, 0], false, 0, null, 1, 0, 0.6);
        var sense = new byte[18];
        sense[0] = 0x70; sense[2] = 0x06; sense[7] = 10; sense[12] = 0x28; sense[13] = 0x00;
        Command([0x03, 0, 0, 0, 18, 0], true, 18, sense, 0, 0, 0.7);

        // READ(10) LBA 2048, 8 blocks (4096 bytes)
        var rd = new byte[4096];
        for (int i = 0; i < rd.Length; i++) rd[i] = (byte)i;
        Command([0x28, 0, 0, 0, 0x08, 0x00, 0, 0, 8, 0], true, 4096, rd, 0, 0, 3.4);

        // WRITE(10) LBA 100, 1 block
        var wr = new byte[512];
        Encoding.ASCII.GetBytes("SerialTap sample write").CopyTo(wr, 0);
        Command([0x2A, 0, 0, 0, 0, 100, 0, 0, 1, 0], false, 512, wr, 0, 0, 2.1);

        // SYNCHRONIZE CACHE(10)
        Command([0x35, 0, 0, 0, 0, 0, 0, 0, 0, 0], false, 0, null, 0, 0, 5.0);

        return recs;
    }
}
