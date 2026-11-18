#!/usr/bin/env python3
"""Small Modbus RTU command-line client for the RS485_IO8 board (needs pyserial).

Examples
  io8.py /dev/ttyUSB0 status
  io8.py /dev/ttyUSB0 mode 1 push-pull       # output OUT1
  io8.py /dev/ttyUSB0 set 1 on
  io8.py /dev/ttyUSB0 pwm 2 --gen 1 --duty 50 --freq 2k
  io8.py /dev/ttyUSB0 invert 0x80             # IN8 is a normally-closed contact
  io8.py /dev/ttyUSB0 debounce 20
  io8.py /dev/ttyUSB0 save
Options: --addr N (default 1), --baud 19200, --parity E|N|O
"""
import argparse
import struct
import sys
import time

try:
    import serial
except ImportError:
    sys.exit("pyserial is required: pip install pyserial")

MODES = {"off": 0, "low-side": 1, "high-side": 2, "push-pull": 3}
FREQS = {"80": 0, "100": 1, "200": 2, "2k": 3}
HR_MODE, HR_OUT, HR_PWM, HR_DUTY, HR_FREQ, HR_INVERT, HR_DEBOUNCE, HR_COMMAND = 0, 8, 16, 24, 28, 32, 33, 38
FLAGS = ["comm-timeout", "driver-fault", "config-loaded", "eeprom-error", "driver-missing"]


def crc16(data):
    crc = 0xFFFF
    for b in data:
        crc ^= b
        for _ in range(8):
            crc = (crc >> 1) ^ 0xA001 if crc & 1 else crc >> 1
    return crc


class Io8:
    def __init__(self, port, addr, baud, parity):
        self.s = serial.Serial(port, baud, parity=parity, stopbits=1, timeout=0.5)
        self.addr = addr

    def _xfer(self, pdu, resp_len):
        frame = bytes([self.addr]) + pdu
        frame += struct.pack("<H", crc16(frame))
        self.s.reset_input_buffer()
        self.s.write(frame)
        r = self.s.read(resp_len)
        if len(r) >= 5 and r[1] & 0x80:
            raise RuntimeError(f"Modbus exception {r[2]}")
        if len(r) != resp_len or crc16(r) != 0:
            raise RuntimeError("no / bad response")
        return r

    def read(self, fc, start, count):
        r = self._xfer(struct.pack(">BHH", fc, start, count), 5 + 2 * count)
        return list(struct.unpack(f">{count}H", r[3:3 + 2 * count]))

    def write(self, reg, value):
        self._xfer(struct.pack(">BHH", 6, reg, value), 8)


def main():
    ap = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("port")
    ap.add_argument("--addr", type=int, default=1)
    ap.add_argument("--baud", type=int, default=19200)
    ap.add_argument("--parity", default="E", choices="ENO")
    sub = ap.add_subparsers(dest="cmd", required=True)
    sub.add_parser("status")
    p = sub.add_parser("mode")
    p.add_argument("ch", type=int)
    p.add_argument("mode", choices=MODES)
    p = sub.add_parser("set")
    p.add_argument("ch", type=int)
    p.add_argument("value", choices=["on", "off"])
    p = sub.add_parser("pwm")
    p.add_argument("ch", type=int)
    p.add_argument("--gen", type=int, default=1, choices=[0, 1, 2, 3, 4])
    p.add_argument("--duty", type=float, default=50.0, help="percent")
    p.add_argument("--freq", default="2k", choices=FREQS)
    p = sub.add_parser("invert")
    p.add_argument("mask", type=lambda x: int(x, 0), help="bit n = IN(n+1)")
    p = sub.add_parser("debounce")
    p.add_argument("ms", type=int)
    sub.add_parser("save")
    sub.add_parser("clear-faults")
    a = ap.parse_args()

    io = Io8(a.port, a.addr, a.baud, a.parity)
    if a.cmd == "status":
        ir = io.read(4, 0, 19)
        hr = io.read(3, 0, 38)
        print(f"firmware {ir[0] >> 8}.{ir[0] & 0xFF}  address {ir[6]}  straps {ir[7]}  uptime {(ir[8] << 16) | ir[9]} s")
        print(f"VIO {ir[2] / 1000:.2f} V  driver IC_STAT 0x{ir[3]:02X}  OCP 0x{ir[4]:04X}")
        print("flags:", ", ".join(f for i, f in enumerate(FLAGS) if ir[5] & (1 << i)) or "-")
        names = {v: k for k, v in MODES.items()}
        for ch in range(8):
            pwm = f"PWM gen {hr[HR_PWM + ch]}" if hr[HR_PWM + ch] else ""
            print(f"OUT{ch + 1}: {names.get(hr[HR_MODE + ch], '?'):9s} value={hr[HR_OUT + ch]} {pwm}")
        print(f"inputs: invert 0x{hr[HR_INVERT]:02X}  debounce {hr[HR_DEBOUNCE]} ms")
        for ch in range(8):
            print(f"IN{ch + 1}: {'ACTIVE' if ir[1] >> ch & 1 else 'off   '}  opto={'on ' if ir[18] >> ch & 1 else 'off'}"
                  f"  activations={ir[10 + ch]}")
        for g in range(4):
            print(f"PWM gen {g + 1}: duty {hr[HR_DUTY + g] * 100 / 255:5.1f} %  freq code {hr[HR_FREQ + g]}")
    elif a.cmd == "mode":
        io.write(HR_MODE + a.ch - 1, MODES[a.mode])
    elif a.cmd == "set":
        io.write(HR_OUT + a.ch - 1, 1 if a.value == "on" else 0)
    elif a.cmd == "pwm":
        if a.gen:
            io.write(HR_DUTY + a.gen - 1, round(a.duty * 255 / 100))
            io.write(HR_FREQ + a.gen - 1, FREQS[a.freq])
        io.write(HR_PWM + a.ch - 1, a.gen)
    elif a.cmd == "invert":
        io.write(HR_INVERT, a.mask & 0xFF)
    elif a.cmd == "debounce":
        io.write(HR_DEBOUNCE, a.ms)
    elif a.cmd == "save":
        io.write(HR_COMMAND, 0xA55A)
        time.sleep(0.2)
    elif a.cmd == "clear-faults":
        io.write(HR_COMMAND, 0xC1F1)


if __name__ == "__main__":
    main()
