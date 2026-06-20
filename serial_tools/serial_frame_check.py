#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
串口长度帧校验工具。

默认校验 send_dt_command() 风格协议：
    +DT=<len>,<payload>\r\n
串口是字节流，电脑一次 read() 读到半帧、粘包都正常；本脚本按长度字段重新组帧。
先安装：py -m pip install pyserial
运行时从列表选择串口与波特率；其他参数可在下面“配置区”修改，然后运行：py serial_tools/serial_frame_check.py
"""

from __future__ import annotations

import binascii
import csv
import os
import queue
import sys
import threading
import time
from datetime import datetime
from typing import Optional

# ============================== 配置区 ==============================
# 串口与波特率在运行时由用户从列表选择
PORT = ""
BAUD = 0

COMMON_BAUDS = (9600, 19200, 38400, 57600, 115200, 230400, 460800, 921600)
DEFAULT_BAUD = 230400
BYTESIZE = 8
PARITY = "N"
STOPBITS = 1
TIMEOUT_SEC = 0.02
READ_SIZE = 4096

# 默认协议：+DT=<len>,<payload>\r\n。其他类似命令只改 PREFIX / EXPECT_LEN。
PREFIX = b"+DT="
SEP = b","
TAIL = b"\r\n"
EXPECT_LEN = 320

# 20ms/320 bytes/230400bps 建议开启接收线程，避免 print/写文件阻塞串口读取。
USE_RX_THREAD = True
QUEUE_MAX_BLOCKS = 200

# 日志配置
PRINT_EVERY_N = 1
PAYLOAD_HEAD_BYTES = 8
BAD_CONTEXT_BYTES = 32
LOG_TO_FILE = True
LOG_DIR = "serial_logs"
EXPECT_INTERVAL_MS: Optional[float] = 20.0
INTERVAL_TOLERANCE_MS = 5.0

# 数据对比模式：none / first / file_loop / file_seq / seq
COMPARE_MODE = "none"
REF_FILE = "ref_payload.bin"
REF_LOOP = True
SEQ_OFFSET = 0
SEQ_SIZE = 4               # 1/2/4/8
SEQ_ENDIAN = "little"      # little/big
SEQ_STEP = 1
# ====================================================================

STOP = threading.Event()
START_MONO = time.monotonic()


def now() -> str:
    return datetime.now().strftime("%Y-%m-%d %H:%M:%S.%f")[:-3]


def rel_ms() -> float:
    return (time.monotonic() - START_MONO) * 1000.0


def hex_head(data: bytes, n: int = PAYLOAD_HEAD_BYTES) -> str:
    return data[:n].hex(" ") if n > 0 else ""


def first_diff(ref: bytes, cur: bytes):
    for i, (a, b) in enumerate(zip(ref, cur)):
        if a != b:
            return i, a, b
    if len(ref) != len(cur):
        i = min(len(ref), len(cur))
        return i, ref[i] if i < len(ref) else None, cur[i] if i < len(cur) else None
    return None, None, None


class LengthParser:
    def __init__(self):
        self.buf = bytearray()
        self.dropped = 0
        self.bad = 0

    def feed(self, data: bytes):
        self.buf.extend(data)

    def parse_one(self):
        """返回 (kind, info)。kind: frame / need_more / dropped / bad。"""
        if not self.buf:
            return "need_more", None

        pos = self.buf.find(PREFIX)
        if pos < 0:
            # 保留最后 len(PREFIX)-1 字节，防止半个帧头被丢弃。
            keep = max(len(PREFIX) - 1, 0)
            if len(self.buf) <= keep:
                return "need_more", None
            n = len(self.buf) - keep
            del self.buf[:n]
            self.dropped += n
            return "dropped", ("NO_PREFIX", n)

        if pos > 0:
            del self.buf[:pos]
            self.dropped += pos
            return "dropped", ("DROP_BEFORE_PREFIX", pos)

        sep_pos = self.buf.find(SEP, len(PREFIX))
        if sep_pos < 0:
            return "need_more", None

        len_bytes = bytes(self.buf[len(PREFIX):sep_pos])
        if not len_bytes or not len_bytes.isdigit():
            ctx = bytes(self.buf[:BAD_CONTEXT_BYTES])
            del self.buf[:1]
            self.bad += 1
            return "bad", ("BAD_LEN_FIELD", ctx)

        payload_len = int(len_bytes)
        total = sep_pos + len(SEP) + payload_len + len(TAIL)
        if len(self.buf) < total:
            return "need_more", None

        if bytes(self.buf[total - len(TAIL):total]) != TAIL:
            ctx = bytes(self.buf[:min(len(self.buf), total + BAD_CONTEXT_BYTES)])
            del self.buf[:1]
            self.bad += 1
            return "bad", ("BAD_TAIL", ctx)

        start = sep_pos + len(SEP)
        payload = bytes(self.buf[start:start + payload_len])
        del self.buf[:total]
        return "frame", (payload_len, payload, total)


class Comparator:
    def __init__(self):
        self.first: Optional[bytes] = None
        self.ref: Optional[bytes] = None
        self.ref_pos = 0
        self.last_seq: Optional[int] = None
        if COMPARE_MODE in ("file_loop", "file_seq"):
            with open(REF_FILE, "rb") as f:
                self.ref = f.read()
            if not self.ref:
                raise RuntimeError(f"参考文件为空: {REF_FILE}")
            if COMPARE_MODE == "file_loop" and len(self.ref) < EXPECT_LEN:
                raise RuntimeError(f"参考文件长度 {len(self.ref)} 小于 EXPECT_LEN {EXPECT_LEN}")

    def check(self, payload: bytes):
        if COMPARE_MODE == "none":
            return {"status": "NA"}
        if COMPARE_MODE == "first":
            if self.first is None:
                self.first = payload
                return {"status": "REF_SET"}
            return self._cmp_bytes(self.first, payload)
        if COMPARE_MODE == "file_loop":
            return self._cmp_bytes(self.ref[:EXPECT_LEN], payload)  # type: ignore[index]
        if COMPARE_MODE == "file_seq":
            ref = self._next_ref_chunk()
            if ref is None:
                return {"status": "REF_END"}
            return self._cmp_bytes(ref, payload)
        if COMPARE_MODE == "seq":
            return self._cmp_seq(payload)
        return {"status": "BAD_CFG", "detail": f"unknown COMPARE_MODE={COMPARE_MODE}"}

    def _next_ref_chunk(self) -> Optional[bytes]:
        assert self.ref is not None
        if self.ref_pos + EXPECT_LEN <= len(self.ref):
            out = self.ref[self.ref_pos:self.ref_pos + EXPECT_LEN]
            self.ref_pos += EXPECT_LEN
            return out
        if not REF_LOOP:
            return None
        remain = self.ref[self.ref_pos:]
        need = EXPECT_LEN - len(remain)
        if need <= len(self.ref):
            out = remain + self.ref[:need]
            self.ref_pos = need
            return out
        out = bytearray()
        while len(out) < EXPECT_LEN:
            out.extend(self.ref)
        self.ref_pos = EXPECT_LEN % len(self.ref)
        return bytes(out[:EXPECT_LEN])

    @staticmethod
    def _cmp_bytes(ref: bytes, cur: bytes):
        off, rb, cb = first_diff(ref, cur)
        if off is None:
            return {"status": "OK"}
        return {"status": "DIFF", "diff_offset": off, "ref_byte": rb, "cur_byte": cb}

    def _cmp_seq(self, payload: bytes):
        if SEQ_SIZE not in (1, 2, 4, 8) or SEQ_OFFSET + SEQ_SIZE > len(payload):
            return {"status": "BAD_CFG", "detail": "invalid seq config"}
        seq = int.from_bytes(payload[SEQ_OFFSET:SEQ_OFFSET + SEQ_SIZE], SEQ_ENDIAN)
        if self.last_seq is None:
            self.last_seq = seq
            return {"status": "REF_SET", "seq": seq}
        expect = self.last_seq + SEQ_STEP
        self.last_seq = seq
        if seq == expect:
            return {"status": "OK", "seq": seq, "expected_seq": expect}
        return {"status": "SEQ_JUMP", "seq": seq, "expected_seq": expect}


def _prompt_index(prompt: str, count: int, default: int) -> int:
    while True:
        raw = input(f"{prompt} [1-{count}, 默认 {default}]: ").strip()
        if not raw:
            return default
        try:
            idx = int(raw)
        except ValueError:
            print(f"请输入 1 到 {count} 之间的序号")
            continue
        if 1 <= idx <= count:
            return idx
        print(f"请输入 1 到 {count} 之间的序号")


def prompt_serial_settings() -> Optional[tuple[str, int]]:
    """列出可用串口与常用波特率，用户输入序号选择。无可用串口时返回 None。"""
    try:
        from serial.tools import list_ports  # type: ignore
    except ImportError:
        print("pyserial 未安装，请先执行: py -m pip install pyserial")
        raise

    ports = list(list_ports.comports())
    if not ports:
        print("未检测到可用串口，请检查连接后重试。")
        return None

    print("\n可用串口:")
    for i, p in enumerate(ports, 1):
        desc = p.description or ""
        hwid = p.hwid or ""
        extra = f"  {desc}" if desc else ""
        if hwid:
            extra += f"  ({hwid})"
        print(f"  {i}. {p.device}{extra}")

    port_idx = _prompt_index("选择串口序号", len(ports), 1)
    port = ports[port_idx - 1].device

    print("\n常用波特率:")
    default_baud_idx = next((i for i, b in enumerate(COMMON_BAUDS, 1) if b == DEFAULT_BAUD), 1)
    for i, baud in enumerate(COMMON_BAUDS, 1):
        mark = "  <-- 默认" if baud == DEFAULT_BAUD else ""
        print(f"  {i}. {baud}{mark}")

    baud_idx = _prompt_index("选择波特率序号", len(COMMON_BAUDS), default_baud_idx)
    baud = COMMON_BAUDS[baud_idx - 1]
    return port, baud


def open_serial():
    try:
        import serial  # type: ignore
    except ImportError:
        print("pyserial 未安装，请先执行: py -m pip install pyserial")
        raise
    return serial.Serial(PORT, BAUD, bytesize=BYTESIZE, parity=PARITY, stopbits=STOPBITS, timeout=TIMEOUT_SEC)


def rx_worker(rxq: "queue.Queue[bytes]"):
    try:
        ser = open_serial()
    except Exception as e:
        print(f"{now()} 打开串口失败: {PORT} {BAUD}, {e}")
        STOP.set()
        return
    print(f"open {PORT} {BAUD}, expect payload len {EXPECT_LEN}")
    print(f"protocol: prefix={PREFIX!r} sep={SEP!r} tail={TAIL!r}, compare={COMPARE_MODE}")
    try:
        while not STOP.is_set():
            data = ser.read(READ_SIZE)
            if not data:
                continue
            try:
                rxq.put(data, timeout=0.5)
            except queue.Full:
                print(f"{now()} RX_QUEUE_FULL drop_block len={len(data)}")
    finally:
        ser.close()


def log_open():
    if not LOG_TO_FILE:
        return None, None, None
    os.makedirs(LOG_DIR, exist_ok=True)
    stamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    txt = os.path.join(LOG_DIR, f"{stamp}_serial_frame_check.log")
    csv_path = os.path.join(LOG_DIR, f"{stamp}_serial_frame_check.csv")
    txt_fp = open(txt, "w", encoding="utf-8")
    csv_fp = open(csv_path, "w", encoding="utf-8", newline="")
    writer = csv.writer(csv_fp)
    writer.writerow(["time", "rel_ms", "frame", "status", "len", "dt_ms", "crc32", "dropped", "bad", "compare", "diff_offset", "ref_byte", "cur_byte", "seq", "expected_seq", "first_bytes"])
    print(f"text log: {txt}")
    print(f"csv  log: {csv_path}")
    return txt_fp, csv_fp, writer


def bhex(v):
    return "" if v is None else f"0x{v:02X}"


def handle_frame(frame_no, payload_len, payload, parser, cmp_ret, last_mono):
    now_mono = time.monotonic()
    dt_ms = None if last_mono is None else (now_mono - last_mono) * 1000.0
    status = "OK" if payload_len == EXPECT_LEN else "LEN_BAD"
    crc32 = f"{binascii.crc32(payload) & 0xFFFFFFFF:08X}"
    warn = ""
    if EXPECT_INTERVAL_MS is not None and dt_ms is not None:
        if abs(dt_ms - EXPECT_INTERVAL_MS) > INTERVAL_TOLERANCE_MS:
            warn = " interval=WARN"
    extra = ""
    if "diff_offset" in cmp_ret:
        extra += f" diff_offset={cmp_ret['diff_offset']} ref={bhex(cmp_ret.get('ref_byte'))} cur={bhex(cmp_ret.get('cur_byte'))}"
    if "seq" in cmp_ret:
        extra += f" seq={cmp_ret['seq']}"
    if cmp_ret.get("expected_seq") is not None and cmp_ret.get("seq") != cmp_ret.get("expected_seq"):
        extra += f" expect_seq={cmp_ret['expected_seq']}"
    line = (f"{now()} t={rel_ms():.1f}ms frame={frame_no} {status} len={payload_len}"
            f"{'' if dt_ms is None else f' dt={dt_ms:.1f}ms'}{warn} first{PAYLOAD_HEAD_BYTES}={hex_head(payload)} "
            f"crc32={crc32} dropped={parser.dropped} bad={parser.bad} cmp={cmp_ret['status']}{extra}")
    return line, dt_ms, status, crc32, now_mono


def parse_loop(rxq: "queue.Queue[bytes]"):
    parser = LengthParser()
    cmpor = Comparator()
    txt_fp, csv_fp, writer = log_open()
    frame_no = 0
    len_bad = 0
    last_mono = None
    try:
        while not STOP.is_set():
            try:
                data = rxq.get(timeout=0.2)
            except queue.Empty:
                continue
            parser.feed(data)
            while not STOP.is_set():
                kind, info = parser.parse_one()
                if kind == "need_more":
                    break
                if kind == "dropped":
                    reason, n = info
                    line = f"{now()} t={rel_ms():.1f}ms DROPPED bytes={n} reason={reason} total_dropped={parser.dropped}"
                    print(line)
                    if txt_fp: txt_fp.write(line + "\n"); txt_fp.flush()
                    continue
                if kind == "bad":
                    reason, ctx = info
                    line = f"{now()} t={rel_ms():.1f}ms BAD reason={reason} bad={parser.bad} context={ctx[:BAD_CONTEXT_BYTES].hex(' ')}"
                    print(line)
                    if txt_fp: txt_fp.write(line + "\n"); txt_fp.flush()
                    continue

                payload_len, payload, _total = info
                frame_no += 1
                cmp_ret = cmpor.check(payload)
                line, dt_ms, status, crc32, last_mono = handle_frame(frame_no, payload_len, payload, parser, cmp_ret, last_mono)
                if status != "OK":
                    len_bad += 1
                need_print = PRINT_EVERY_N <= 1 or frame_no % PRINT_EVERY_N == 0 or status != "OK" or cmp_ret["status"] not in ("NA", "OK", "REF_SET")
                if need_print:
                    print(line)
                if txt_fp:
                    txt_fp.write(line + "\n"); txt_fp.flush()
                if writer:
                    writer.writerow([now(), f"{rel_ms():.1f}", frame_no, status, payload_len, "" if dt_ms is None else f"{dt_ms:.3f}", crc32, parser.dropped, parser.bad, cmp_ret["status"], cmp_ret.get("diff_offset", ""), bhex(cmp_ret.get("ref_byte")), bhex(cmp_ret.get("cur_byte")), cmp_ret.get("seq", ""), cmp_ret.get("expected_seq", ""), hex_head(payload)])
                    csv_fp.flush()
                if len_bad and len_bad % 20 == 0:
                    print(f"{now()} WARN LEN_BAD count={len_bad}, check EXPECT_LEN={EXPECT_LEN}")
    finally:
        if txt_fp: txt_fp.close()
        if csv_fp: csv_fp.close()


def main() -> int:
    global PORT, BAUD
    settings = prompt_serial_settings()
    if settings is None:
        return 0
    PORT, BAUD = settings
    print(f"使用串口 {PORT}，波特率 {BAUD}")

    if not PREFIX or not SEP or not TAIL:
        print("配置错误：PREFIX/SEP/TAIL 不能为空")
        return 2
    if COMPARE_MODE not in ("none", "first", "file_loop", "file_seq", "seq"):
        print(f"配置错误：不支持 COMPARE_MODE={COMPARE_MODE}")
        return 2
    rxq: "queue.Queue[bytes]" = queue.Queue(maxsize=QUEUE_MAX_BLOCKS)
    try:
        if USE_RX_THREAD:
            threading.Thread(target=rx_worker, args=(rxq,), daemon=True).start()
            parse_loop(rxq)
        else:
            ser = open_serial()
            print(f"open {PORT} {BAUD}, expect payload len {EXPECT_LEN}")
            threading.Thread(target=parse_loop, args=(rxq,), daemon=True).start()
            while not STOP.is_set():
                data = ser.read(READ_SIZE)
                if data:
                    rxq.put(data)
    except KeyboardInterrupt:
        print("\n用户停止")
    except Exception as e:
        print(f"运行异常: {e}")
        return 1
    finally:
        STOP.set()
    return 0


if __name__ == "__main__":
    exit_code = main()
    if exit_code != 0:
        sys.exit(exit_code)
