"""Build the project symbol library IO8.kicad_sym (parts not in the stock KiCad libs).

Pin numbers are taken from the manufacturer datasheets:
  MAX3160EAP  - Maxim/ADI MAX3160-MAX3162 rev 3, 'Pin Description' (SSOP-20)
  DRV8912-Q1  - TI SLVSEC9C, 'Pin Functions - DRV8912-Q1' (HTSSOP-24 PWP)
  MCP23S08    - Microchip DS21919E, Table 1-1 (SSOP-20 column)
"""
import copy

import symlib
from sexp import Sym, dump, find

FONT = [Sym("font"), [Sym("size"), 1.27, 1.27]]


def _prop(k, v, x, y, hide=False):
    eff = [Sym("effects"), FONT]
    if hide:
        eff.append([Sym("hide"), Sym("yes")])
    return [Sym("property"), k, v, [Sym("at"), x, y, 0], eff]


def ic(name, ref, value, footprint, datasheet, desc, box, pins, keywords=""):
    """box = (x1, y1, x2, y2) lib coords; pins = [(num, name, type, x, y, angle)]."""
    x1, y1, x2, y2 = box
    body = [
        Sym("symbol"),
        f"{name}_0_1",
        [Sym("rectangle"), [Sym("start"), x1, y1], [Sym("end"), x2, y2],
         [Sym("stroke"), [Sym("width"), 0.254], [Sym("type"), Sym("default")]],
         [Sym("fill"), [Sym("type"), Sym("background")]]],
    ]
    pinunit = [Sym("symbol"), f"{name}_1_1"]
    for num, pname, ptype, x, y, ang in pins:
        pinunit.append(
            [Sym("pin"), Sym(ptype), Sym("line"), [Sym("at"), x, y, ang], [Sym("length"), 2.54],
             [Sym("name"), pname, [Sym("effects"), FONT]],
             [Sym("number"), str(num), [Sym("effects"), FONT]]]
        )
    return [
        Sym("symbol"), name,
        [Sym("pin_names"), [Sym("offset"), 1.016]],
        [Sym("exclude_from_sim"), Sym("no")],
        [Sym("in_bom"), Sym("yes")],
        [Sym("on_board"), Sym("yes")],
        _prop("Reference", ref, x1, y1 + 1.27),
        _prop("Value", value, x1, y2 - 1.27),
        _prop("Footprint", footprint, 0, 0, True),
        _prop("Datasheet", datasheet, 0, 0, True),
        _prop("Description", desc, 0, 0, True),
        _prop("ki_keywords", keywords, 0, 0, True),
        body,
        pinunit,
    ]


def left(n, y, num, name, t="input"):
    return (num, name, t, n, y, 0)


def right(n, y, num, name, t="output"):
    return (num, name, t, n, y, 180)


def top(x, n, num, name, t="power_in"):
    return (num, name, t, x, n, 270)


def bottom(x, n, num, name, t="power_in"):
    return (num, name, t, x, n, 90)


def max3160():
    L, R, T, B = -17.78, 17.78, 17.78, -25.4
    pins = [
        left(L - 2.54, 12.7, 16, "DI/T1IN"),
        left(L - 2.54, 10.16, 8, "RO/R2OUT", "tri_state"),
        left(L - 2.54, 7.62, 7, "R1OUT", "tri_state"),
        left(L - 2.54, 2.54, 12, "HDPLX"),
        left(L - 2.54, 0.0, 9, "~{SHDN}"),
        left(L - 2.54, -2.54, 10, "FAST"),
        left(L - 2.54, -7.62, 11, "RS485/~{RS232}"),
        left(L - 2.54, -12.7, 15, "DE485/T2IN"),
        right(R + 2.54, 12.7, 6, "Y(A)/T2OUT", "bidirectional"),
        right(R + 2.54, 10.16, 5, "Z(B)/T1OUT", "bidirectional"),
        right(R + 2.54, 5.08, 13, "A/R2IN", "input"),
        right(R + 2.54, 2.54, 14, "B/R1IN", "input"),
        right(R + 2.54, -5.08, 1, "C1+", "passive"),
        right(R + 2.54, -12.7, 3, "C1-", "passive"),
        right(R + 2.54, -15.24, 19, "C2+", "passive"),
        right(R + 2.54, -22.86, 18, "C2-", "passive"),
        top(-5.08, T + 2.54, 2, "VCC"),
        top(5.08, T + 2.54, 20, "V+", "passive"),
        bottom(-5.08, B - 2.54, 4, "GND"),
        bottom(5.08, B - 2.54, 17, "V-", "passive"),
    ]
    return ic("MAX3160EAP", "U", "MAX3160EAP+T", "Package_SO:SSOP-20_5.3x7.2mm_P0.65mm",
              "https://www.analog.com/media/en/technical-documentation/data-sheets/MAX3160-MAX3162.pdf",
              "RS-232 / RS-485 multiprotocol transceiver, shared pins, 3.0-5.5 V, SSOP-20",
              (L, T, R, B), pins, "RS232 RS485 transceiver multiprotocol")


def drv8912():
    L, R, T, B = -15.24, 15.24, 20.32, -20.32
    pins = [
        top(-7.62, T + 2.54, 6, "VDD"),
        top(2.54, T + 2.54, 16, "VM"),
        top(7.62, T + 2.54, 21, "VM"),
        bottom(-7.62, B - 2.54, 1, "GND"),
        bottom(-2.54, B - 2.54, 13, "GND"),
        bottom(2.54, B - 2.54, 24, "GND"),
        bottom(7.62, B - 2.54, 25, "EP"),
        left(L - 2.54, 12.7, 20, "SCLK"),
        left(L - 2.54, 10.16, 5, "SDI"),
        left(L - 2.54, 7.62, 7, "SDO", "tri_state"),
        left(L - 2.54, 2.54, 19, "~{SCS}"),
        left(L - 2.54, -2.54, 8, "~{SLEEP}"),
        left(L - 2.54, -7.62, 12, "~{FAULT}", "open_collector"),
    ]
    outs = [(1, 2), (2, 23), (3, 14), (4, 11), (5, 3), (6, 10), (7, 4), (8, 22),
            (9, 9), (10, 15), (11, 17), (12, 18)]
    y = 15.24
    for i, (o, n) in enumerate(outs):
        pins.append(right(R + 2.54, y - 2.54 * i, n, f"OUT{o}", "tri_state"))
    return ic("DRV8912-Q1", "U", "DRV8912QPWPRQ1",
              "Package_SO:HTSSOP-24-1EP_4.4x7.8mm_P0.65mm_EP3.4x7.8mm_Mask2.4x4.68mm",
              "https://www.ti.com/lit/ds/symlink/drv8912-q1.pdf",
              "12-channel SPI half-bridge driver, 4.5-32 V, HTSSOP-24 PowerPAD",
              (L, T, R, B), pins, "half-bridge driver SPI")


def mcp23s08():
    L, R, T, B = -12.7, 12.7, 17.78, -25.4
    pins = [
        top(0, T + 2.54, 20, "VDD"),
        bottom(0, B - 2.54, 9, "VSS"),
        left(L - 2.54, 15.24, 1, "SCK"),
        left(L - 2.54, 12.7, 2, "SI"),
        left(L - 2.54, 10.16, 3, "SO", "tri_state"),
        left(L - 2.54, 5.08, 6, "~{RESET}"),
        left(L - 2.54, -2.54, 7, "~{CS}"),
        left(L - 2.54, -15.24, 8, "INT", "output"),
        left(L - 2.54, -20.32, 4, "A1"),
        left(L - 2.54, -22.86, 5, "A0"),
        right(R + 2.54, -20.32, 11, "NC", "no_connect"),
        right(R + 2.54, -22.86, 10, "NC", "no_connect"),
    ]
    for i in range(8):
        pins.append(right(R + 2.54, 15.24 - 2.54 * i, 12 + i, f"GP{i}", "bidirectional"))
    return ic("MCP23S08-SSOP", "U", "MCP23S08T-E/SS", "Package_SO:SSOP-20_5.3x7.2mm_P0.65mm",
              "https://ww1.microchip.com/downloads/en/DeviceDoc/21919e.pdf",
              "8-bit SPI I/O expander, SSOP-20", (L, T, R, B), pins, "IO expander SPI")


def power_sym(name):
    s = copy.deepcopy(symlib.get_symbol("power", "+5V"))
    s[1] = name
    for p in find(s, "property"):
        if p[1] == "Value":
            p[2] = name
        if p[1] == "Description":
            p[2] = f'Power symbol creates a global label with name "{name}"'
    for sub in find(s, "symbol"):
        sub[1] = name + sub[1][len("+5V"):]
        for pin in find(sub, "pin"):
            for x in pin:
                if isinstance(x, list) and x and x[0] == "name":
                    x[1] = name
    return s


def build(path):
    lib = [Sym("kicad_symbol_lib"), [Sym("version"), 20241209], [Sym("generator"), "kicad_symbol_editor"],
           [Sym("generator_version"), "9.0"]]
    lib += [max3160(), drv8912(), mcp23s08(), power_sym("VIO")]
    with open(path, "w", encoding="utf-8") as f:
        f.write(dump(lib) + "\n")


if __name__ == "__main__":
    import sys

    build(sys.argv[1])
