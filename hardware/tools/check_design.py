#!/usr/bin/env python3
"""Independent connectivity check of the generated schematic.

Exports the netlist with kicad-cli, then checks every connection listed below
(written from the datasheets, not from the layout code). Also runs ERC and checks
that every footprint exists in the installed KiCad libraries.

    python3 tools/check_design.py            # writes VALIDATION.txt and ERC.json
"""
import json
import os
import re
import subprocess
import sys
import tempfile

HW = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCH = os.path.join(HW, "RS485_IO8.kicad_sch")
FPDIR = "/usr/share/kicad/footprints"

# net name (as KiCad reports the short name) -> nodes that must all be on it
EXPECT = {
    "+5V": ["J1.1", "U1.1", "U1.3", "JP1.3", "JP8.2", "D4.1"],
    "+3V3": ["U1.5", "U2.4", "U3.4", "U4.2", "U4.12", "U4.9", "U5.6", "U8.20", "J3.1",
             "U10.16", "U10.4", "R44.1"],
    "VIO": ["JP1.2", "U5.16", "U5.21", "J6.2", "J7.2", "R1.1"],
    "EXT_VIO": ["J2.1", "JP1.1"],
    "GND": ["J1.2", "J2.2", "U1.2", "U2.5", "U3.2", "U3.5", "U4.4", "U4.10", "U5.1", "U5.13", "U5.24",
            "U5.25", "U8.9", "U8.4", "U8.5", "J3.3", "J3.5", "J3.9", "J4.4", "J6.1", "J7.1", "J5.2",
            "D2.3", "D3.1", "D1.1", "JP2.2", "JP3.2", "JP4.1", "J10.5", "J10.SH",
            "J11.A1", "J11.A12", "J11.B1", "J11.B12", "J11.SH", "U9.2", "U10.1", "R42.2", "R43.2",
            # opto phototransistor emitters (TLP290-4: E = 15, 13, 11, 9)
            "U6.15", "U6.13", "U6.11", "U6.9", "U7.15", "U7.13", "U7.11", "U7.9"],
    "VIO_ADC": ["R1.2", "R2.1", "C5.1", "D1.3", "U2.7"],
    # MCU pin map (STM32G030F6P6 TSSOP-20, DS12991)
    "NRST": ["U2.6", "J3.10", "U8.6", "R3.2", "C8.1"],
    "SPI_SCK": ["U2.8", "U5.20", "U8.1"],
    "SPI_MOSI": ["U2.9", "U5.5", "U8.2"],
    "SPI_MISO": ["U2.13", "U5.7", "U8.3"],
    "IN_IRQ": ["U2.10", "U8.8"],
    "DRV_nCS": ["U2.11", "U5.19"],
    "IN_nCS": ["U2.12", "U8.7"],
    "DRV_nSLEEP": ["U2.14", "U5.8"],
    "SER_DE": ["U2.15", "U4.15"],
    "UART_TX": ["U2.16", "U4.16", "U10.3"],
    # UART_RX is a wired-AND: D5 (BAT54A, 3 = common anode) with pull-up R44;
    # cathode 2 <- MAX3160 RO, cathode 1 <- CH340C TXD
    "UART_RX": ["U2.17", "D5.3", "R44.2"],
    "MAX_RO": ["U4.8", "D5.2"],
    "USB_TXD": ["U10.2", "D5.1"],
    "SWDIO": ["U2.18", "J3.2"],
    "SWCLK": ["U2.19", "J3.4"],
    "I2C_SDA": ["U2.1", "U3.3"],
    "I2C_SCL": ["U2.20", "U3.1"],
    "ADDR0": ["U2.2", "JP2.1"],
    "ADDR1": ["U2.3", "JP3.1"],
    # MAX3160 (SSOP-20): 6 = Y(A)/T2OUT, 5 = Z(B)/T1OUT, 13 = A/R2IN, 11 = RS485/RS232
    "LINE_A": ["U4.6", "J4.1", "D2.2", "J10.1"],
    "LINE_B_TXD": ["U4.5", "J4.2", "D2.1", "J10.2"],
    "LINE_RXD": ["U4.13", "J4.3", "D3.2", "J10.3"],
    "MODE": ["U4.11", "JP4.2"],
    # USB-C (J11), USBLC6-2SC6 (U9: I/O1 = 1, 6; I/O2 = 3, 4; VBUS = 5), CH340C (U10: UD+ = 5, UD- = 6)
    "VBUS": ["J11.A4", "J11.A9", "J11.B4", "J11.B9", "U9.5", "D4.2", "C22.1"],
    "CC1": ["J11.A5", "R42.1"],
    "CC2": ["J11.B5", "R43.1"],
    "USB_DM": ["J11.A7", "J11.B7", "U9.1", "U9.6", "U10.6"],
    "USB_DP": ["J11.A6", "J11.B6", "U9.3", "U9.4", "U10.5"],
    # DRV8912-Q1 PWP: OUT1..8 = pins 2, 23, 14, 11, 3, 10, 4, 22
    # output terminals: pin 1 = GND, 2 = VIO, 3..6 = OUT4..OUT1 (OUT8..OUT5)
    "OUT1": ["U5.2", "J6.6"], "OUT2": ["U5.23", "J6.5"], "OUT3": ["U5.14", "J6.4"], "OUT4": ["U5.11", "J6.3"],
    "OUT5": ["U5.3", "J7.6"], "OUT6": ["U5.10", "J7.5"], "OUT7": ["U5.4", "J7.4"], "OUT8": ["U5.22", "J7.3"],
    "DRV_nFAULT": ["U5.12", "J5.1"],
    # MCP23S08 SSOP GP0..GP7 = pins 12..19 <- TLP290-4 collectors (C = 16, 14, 12, 10)
    **{f"IN{i + 1}": [f"U8.{12 + i}", f"U{6 + i // 4}.{16 - 2 * (i % 4)}"] for i in range(8)},
    # input groups: cathodes (2, 4, 6, 8) on the COM terminal pin
    "COM1": ["J8.5", "U6.2", "U6.4", "U6.6", "U6.8"],
    "COM2": ["J9.5", "U7.2", "U7.4", "U7.6", "U7.8"],
}
# terminal pin -> exactly one series resistor -> opto anode (A = 1, 3, 5, 7)
VIA_RESISTOR = [(f"J{8 + i // 4}.{i % 4 + 1}", f"U{6 + i // 4}.{2 * (i % 4) + 1}") for i in range(8)]
# DB9 pin 6 reaches +5V only through the solder jumper
SAME_NET = [("J10.6", "JP8.1")]
# field-side nets must not touch logic nets
ISOLATED_NETS = ["COM1", "COM2"]
# pins of the same part that must NOT share a net
MUST_DIFFER = [("U4.6", "U4.5"), ("U4.5", "U4.13"), ("U4.11", "U4.4"), ("U4.15", "U4.4"),
               ("J11.A5", "J11.B5"), ("U4.8", "U2.17"), ("U10.2", "U2.17"), ("U4.8", "U10.2")]
NO_CONNECT_OK = {"J3.6", "J3.7", "J3.8", "U1.4", "U4.7", "U4.14", "U5.9", "U5.15", "U5.17", "U5.18",
                 "U8.10", "U8.11", "J10.4", "J10.7", "J10.8", "J10.9", "J11.A8", "J11.B8",
                 *(f"U10.{n}" for n in range(7, 16))}


def netlist():
    with tempfile.TemporaryDirectory() as d:
        out = os.path.join(d, "n.net")
        subprocess.run(["kicad-cli", "sch", "export", "netlist", "--format", "kicadsexpr", "-o", out, SCH],
                       check=True, capture_output=True)
        t = open(out).read()
    comps = dict(re.findall(r'\(comp\s*\(ref "([^"]+)"\)\s*\(value "([^"]*)"\)', t))
    fps = dict(re.findall(r'\(comp\s*\(ref "([^"]+)"\).*?\(footprint "([^"]*)"\)', t, re.S))
    nets = {}
    for blk in re.split(r"\(net\s*\n", t[t.index("(nets"):])[1:]:
        full = re.search(r'\(name "([^"]*)"\)', blk).group(1)
        # sheet path prefix off; unconnected-(...) names can contain '/' inside a pin name
        name = full if full.startswith("unconnected-") else full.rsplit("/", 1)[-1]
        nodes = {f"{r}.{p}" for r, p in re.findall(r'\(ref "([^"]+)"\)\s*\(pin "([^"]+)"\)', blk)}
        nets[name] = nodes
    return comps, fps, nets


def main():
    lines, fails = [], 0
    comps, fps, nets = netlist()
    node_net = {n: name for name, nodes in nets.items() for n in nodes}

    def check(ok, msg):
        nonlocal fails
        lines.append(("PASS " if ok else "FAIL ") + msg)
        fails += 0 if ok else 1

    for name, nodes in EXPECT.items():
        for n in nodes:
            check(node_net.get(n) == name, f"{n} on {name} (found {node_net.get(n)})")
    for term, anode in VIA_RESISTOR:
        nt, na = node_net.get(term), node_net.get(anode)
        rs = [r for r in {n.split(".")[0] for n in nets.get(nt, ())} if r.startswith("R")
              and {f"{r}.1", f"{r}.2"} <= nets[nt] | nets.get(na, set())]
        check(len(rs) == 1 and len(nets[nt]) == 2, f"{term} -> series resistor {rs} -> {anode}")
    field = set()
    for name in ISOLATED_NETS:
        field |= {n.split(".")[0] for n in nets.get(name, ())}
    for name, nodes in nets.items():
        if name in ("GND", "+3V3", "+5V", "VIO"):
            refs = {n.split(".")[0] for n in nodes}
            bad = {r for r in refs & field if not r.startswith("U")}  # optos legitimately span the barrier
            check(not bad and not (set(nets.get("COM1", ())) | set(nets.get("COM2", ()))) & nodes,
                  f"{name} has no field-side connection")
    for a, b in SAME_NET:
        check(node_net.get(a) is not None and node_net.get(a) == node_net.get(b), f"{a} and {b} on one net")
    check(node_net.get("J10.6") != node_net.get("JP8.2"), "J10.6 separated from +5V by JP8")
    for a, b in MUST_DIFFER:
        check(node_net.get(a) != node_net.get(b), f"{a} and {b} on different nets")
    for name, nodes in nets.items():
        if name.startswith("unconnected-"):
            for n in nodes:
                check(n in NO_CONNECT_OK, f"{n} intentionally unconnected")
        elif len(nodes) < 2:
            check(False, f"net {name} has a single node {nodes}")
    for ref, fp in sorted(fps.items()):
        lib, name = fp.split(":", 1)
        check(os.path.exists(os.path.join(FPDIR, lib + ".pretty", name + ".kicad_mod")), f"{ref} footprint {fp}")

    erc = os.path.join(HW, "ERC.json")
    subprocess.run(["kicad-cli", "sch", "erc", "--format", "json", "--severity-all", "-o", erc, SCH],
                   check=True, capture_output=True)
    nviol = sum(len(s["violations"]) for s in json.load(open(erc))["sheets"])
    check(nviol == 0, f"KiCad ERC: {nviol} violations (errors + warnings)")

    ver = subprocess.run(["kicad-cli", "--version"], capture_output=True, text=True).stdout.strip()
    head = [f"RS485_IO8 rev B schematic validation (kicad-cli {ver})",
            f"{len(lines) - fails} checks passed, {fails} failed.", ""]
    with open(os.path.join(HW, "VALIDATION.txt"), "w") as f:
        f.write("\n".join(head + lines) + "\n\nNot hardware tested.\n")
    print("\n".join(head))
    for l in lines:
        if l.startswith("FAIL"):
            print(l)
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
