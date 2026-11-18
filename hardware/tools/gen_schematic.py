#!/usr/bin/env python3
"""Generate the RS485_IO8 rev B schematic (root + 5 sheets) into the hardware folder.

    python3 tools/gen_schematic.py

The KiCad files are build products of this script: change the design here and
re-run it. Every part carries Footprint, LCSC, MPN, Manufacturer and Spec fields;
tools/make_bom.py turns them into the LCSC / JLCPCB BOM.
"""
import os
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
OUT = os.path.dirname(HERE)
sys.path.insert(0, HERE)

import customsyms  # noqa: E402
from schgen import Project, Sheet  # noqa: E402

LIB = os.path.join(OUT, "IO8.kicad_sym")
customsyms.build(LIB)


def g(n):
    """Grid units (2.54 mm) -> mm."""
    return round(n * 2.54, 2)


# ----------------------------------------------------------------- catalogue
# key: (value shown, footprint, LCSC, MPN, manufacturer, spec)
R0603 = "Resistor_SMD:R_0603_1608Metric"
C0603 = "Capacitor_SMD:C_0603_1608Metric"
C0805 = "Capacitor_SMD:C_0805_2012Metric"
C1206 = "Capacitor_SMD:C_1206_3216Metric"
SOT23 = "Package_TO_SOT_SMD:SOT-23"
CAT = {
    "10k": ("10k", R0603, "C25804", "0603WAF1002T5E", "UNI-ROYAL", "1% 0.1W 0603"),
    "100k": ("100k", R0603, "C25803", "0603WAF1003T5E", "UNI-ROYAL", "1% 0.1W 0603"),
    "22k": ("22k", R0603, "C31850", "0603WAF2202T5E", "UNI-ROYAL", "1% 0.1W 0603"),
    "47k": ("47k", R0603, "C25819", "0603WAF4702T5E", "UNI-ROYAL", "1% 0.1W 0603"),
    "4k7": ("4.7k", R0603, "C23162", "0603WAF4701T5E", "UNI-ROYAL", "1% 0.1W 0603"),
    "390R": ("390R", R0603, "C23151", "0603WAF3900T5E", "UNI-ROYAL", "1% 0.1W 0603"),
    "120R": ("120R", "Resistor_SMD:R_1206_3216Metric", "C17909", "1206W4F1200T5E", "UNI-ROYAL", "1% 0.25W 1206"),
    "3k3_1206": ("3.3k", "Resistor_SMD:R_1206_3216Metric", "C26032", "1206W4F3301T5E", "UNI-ROYAL", "1% 0.25W 1206"),
    "100n": ("100n", C0603, "C14663", "CC0603KRX7R9BB104", "YAGEO", "50V X7R 0603"),
    "1n": ("1n", C0603, "C1588", "CL10B102KB8NNNC", "Samsung", "50V X7R 0603"),
    "1u": ("1u", C0603, "C15849", "CL10A105KB8NNNC", "Samsung", "50V X5R 0603"),
    "4u7_0603": ("4.7u", C0603, "C19666", "CL10A475KO8NNNC", "Samsung", "16V X5R 0603"),
    "4u7_0805": ("4.7u", C0805, "C1779", "CL21A475KAQNNNE", "Samsung", "25V X5R 0805"),
    "10u_0805": ("10u", C0805, "C15850", "CL21A106KAYNNNE", "Samsung", "25V X5R 0805"),
    "22u_1206": ("22u 25V", C1206, "C12891", "CL31A226KAHNNNE", "Samsung", "25V X5R 1206"),
    "10u_1206": ("10u 50V", C1206, "C13585", "CL31A106KBHNNNE", "Samsung", "50V X5R 1206"),
    "BC847B": ("BC847B", SOT23, "C20069135", "BC847B", "CJ", "NPN 45V 100mA"),
    "BAT54S": ("BAT54S", SOT23, "C7420333", "BAT54S", "CJ", "dual Schottky, series"),
    "SM712": ("SM712", SOT23, "C7420375", "SM712", "Hongjiacheng", "RS-485 TVS -7/+12V"),
    "TVS15": ("15V TVS", "Diode_SMD:D_SOD-323", "C20615813", "H15VND3B", "Hongjiacheng", "bidirectional 15V"),
    "TB2": ("", "TerminalBlock:TerminalBlock_Xinya_XY308-2.54-2P_1x02_P2.54mm_Horizontal",
            "C557685", "XY308-2.54-2P", "Xinya", "screw terminal 2P 2.54mm"),
    "TB4": ("", "TerminalBlock:TerminalBlock_Xinya_XY308-2.54-4P_1x04_P2.54mm_Horizontal",
            "C915913", "XY308-2.54-4P", "Xinya", "screw terminal 4P 2.54mm"),
    "TB5": ("", "TerminalBlock:TerminalBlock_Xinya_XY308-2.54-5P_1x05_P2.54mm_Horizontal",
            "C915914", "XY308-2.54-5P", "Xinya", "screw terminal 5P 2.54mm"),
    "TLP290": ("TLP290-4", "Package_SO:SOP-16_4.55x10.3mm_P1.27mm", "C39031", "TLP290-4(GB-TP,E",
               "Toshiba", "quad AC-input optocoupler, CTR 100-400 %"),
    "TB6": ("", "TerminalBlock:TerminalBlock_Xinya_XY308-2.54-6P_1x06_P2.54mm_Horizontal",
            "C915915", "XY308-2.54-6P", "Xinya", "screw terminal 6P 2.54mm"),
    "HDR2": ("", "Connector_PinHeader_2.54mm:PinHeader_1x02_P2.54mm_Vertical", "C492401",
             "PZ254V-11-02P", "XFCN", "pin header 1x2 2.54mm"),
    "HDR3": ("", "Connector_PinHeader_2.54mm:PinHeader_1x03_P2.54mm_Vertical", "C49257",
             "2.54-1*3P", "BOOMELE", "pin header 1x3 2.54mm"),
    "DB9F": ("DB9 SERIAL", "Connector_Dsub:DSUB-9_Socket_Horizontal_P2.77x2.84mm_EdgePinOffset7.70mm_Housed_MountingHolesOffset9.12mm",
             "C141882", "D-DMR009PF-D002", "Ckmtw", "D-Sub 9 female, right angle"),
    "SWD": ("SWD", "Connector_PinHeader_1.27mm:PinHeader_2x05_P1.27mm_Vertical", "C41426986",
            "A127M01-0B10P51-H10A39", "HanElectricity", "pin header 2x5 1.27mm"),
    "AP2112": ("AP2112K-3.3", "Package_TO_SOT_SMD:SOT-23-5", "C51118", "AP2112K-3.3TRG1", "Diodes Inc",
               "LDO 3.3V 600mA"),
    "STM32": ("STM32G030F6P6", "Package_SO:TSSOP-20_4.4x6.5mm_P0.65mm", "C724040", "STM32G030F6P6", "ST",
              "Cortex-M0+ 32KB TSSOP-20"),
    "EEPROM": ("24AA02T-I/OT", "Package_TO_SOT_SMD:SOT-23-5", "C478090", "24AA02T-I/OT", "Microchip",
               "2 kbit I2C EEPROM"),
    "MAX3160": ("MAX3160EAP+T", "Package_SO:SSOP-20_5.3x7.2mm_P0.65mm", "C406604", "MAX3160EAP+T",
                "Analog Devices", "RS-232/RS-485 multiprotocol transceiver"),
    "DRV8912": ("DRV8912QPWPRQ1",
                "Package_SO:HTSSOP-24-1EP_4.4x7.8mm_P0.65mm_EP3.4x7.8mm_Mask2.4x4.68mm_ThermalVias",
                "C1855817", "DRV8912QPWPRQ1", "Texas Instruments", "12-ch SPI half-bridge driver"),
    "MCP23S08": ("MCP23S08T-E/SS", "Package_SO:SSOP-20_5.3x7.2mm_P0.65mm", "C145600", "MCP23S08T-E/SS",
                 "Microchip", "8-bit SPI I/O expander"),
}


class Refs:
    def __init__(self):
        self.n = {}

    def __call__(self, prefix):
        self.n[prefix] = self.n.get(prefix, 0) + 1
        return f"{prefix}{self.n[prefix]}"


ref = Refs()


def cat(key, value=None):
    v, fp, lcsc, mpn, mfr, spec = CAT[key]
    return (value if value is not None else v), dict(Footprint=fp, LCSC=lcsc, MPN=mpn, Manufacturer=mfr, Spec=spec)


# text placement (dx, dy, justify) relative to the symbol origin
TXT_V = {"Reference": (2.54, -1.27, "left"), "Value": (2.54, 1.27, "left")}
TXT_VL = {"Reference": (-2.54, -1.27, "right"), "Value": (-2.54, 1.27, "right")}
TXT_H = {"Reference": (0, -4.57, None), "Value": (0, -2.54, None)}


def R(sh, key, x, y, horiz=False, left_text=False, prefix="R", lib=("Device", "R")):
    value, f = cat(key)
    text = TXT_H if horiz else (TXT_VL if left_text else TXT_V)
    return sh.add(lib[0], lib[1], ref(prefix), value, x, y, rot=90 if horiz else 0, text=text, **f)


def C(sh, key, x, y, left_text=False):
    return R(sh, key, x, y, False, left_text, prefix="C", lib=("Device", "C"))


def part(sh, lib, name, key, prefix, x, y, value=None, path=None, **kw):
    v, f = cat(key, value)
    return sh.add(lib, name, ref(prefix), v, x, y, path=path, **kw, **f)


def cap_to_gnd(sh, key, x, ytop, left_text=False):
    """Vertical capacitor, top pin at (x, ytop), GND symbol on the bottom pin."""
    c = C(sh, key, x, ytop + 3.81, left_text=left_text)
    sh.power("GND", c[2])
    return c


def pullup(sh, key, x, ybot, rail="+3V3", left_text=False):
    """Vertical resistor, bottom pin at (x, ybot), supply symbol on the top pin."""
    r = R(sh, key, x, ybot - 3.81, left_text=left_text)
    sh.power(rail, r[1])
    return r


def pulldown(sh, key, x, ytop, left_text=False):
    r = R(sh, key, x, ytop + 3.81, left_text=left_text)
    sh.power("GND", r[2])
    return r


def jumper2(sh, label, x, y, rot=0, text=None):
    v, f = cat("HDR2", label)
    f["Description"] = "2-pin strap (2.54 mm header + shunt)"
    return sh.add("Jumper", "Jumper_2_Open", ref("JP"), v, x, y, rot=rot,
                  text=text or {"Reference": (0, -3.81, None), "Value": (0, 2.54, None)}, **f)


def ends(p):
    """Return the two pins of a 2-pin part sorted top->bottom / left->right."""
    return sorted([p[1], p[2]], key=lambda q: (q[1], q[0]))


root = Sheet("RS485_IO8.kicad_sch", "RS485_IO8 rev B - overview", 1)
proj = Project("RS485_IO8", root)

# ============================================================== 2  power
pw = Sheet("power.kicad_sch", "Power input and output supply selection", 2)

pw.frame(g(6), g(8), g(80), g(36), "LOGIC SUPPLY   5 V in  ->  3.3 V")
v, f = cat("TB2", "5V IN")
f["Description"] = "Logic supply: 1 = +5 V, 2 = GND"
J1 = pw.add("Connector", "Screw_Terminal_01x02", ref("J"), v, g(12), g(18), mirror="y",
            text={"Reference": (0, -3.81, None), "Value": (0, 6.35, None)}, **f)
U1 = part(pw, "Regulator_Linear", "AP2112K-3.3", "AP2112", "U", g(44), g(19),
          text={"Reference": (0, -6.35, None), "Value": (3.81, 6.35, "left")})
pw.wire(J1[1], U1[1])                                   # +5V rail y = g18
pw.power("+5V", (g(18), g(18)))
pw.power("PWR_FLAG", (g(22), g(18)))
pw.wire(J1[2], (g(16), g(19)), (g(16), g(23)), (g(20), g(23)))
pw.power("GND", (g(16), g(23)))
pw.power("PWR_FLAG", (g(20), g(23)))
cap_to_gnd(pw, "10u_0805", g(27), g(18))
cap_to_gnd(pw, "1u", g(33), g(18))
pw.wire(U1[3], (g(38), g(19)), (g(38), g(18)))          # EN tied to VIN
pw.power("GND", U1[2])
pw.nc(U1[4])
pw.wire(U1[5], (g(68), g(18)))                          # +3V3 rail
cap_to_gnd(pw, "4u7_0805", g(56), g(18))
cap_to_gnd(pw, "100n", g(62), g(18))
pw.power("+3V3", (g(68), g(18)))
pw.note((g(8), g(28)), "J1: regulated 5 V (4.75 - 5.25 V).  AP2112K 600 mA LDO, board logic < 50 mA.\n"
        "No reverse-polarity protection: check the wiring before applying power.")

pw.frame(g(6), g(40), g(80), g(76), "OUTPUT DRIVER SUPPLY  VIO   (4.5 - 14 V)")
v, f = cat("TB2", "VIO EXT IN")
f["Description"] = "External output supply: 1 = EXT_VIO, 2 = GND"
J2 = pw.add("Connector", "Screw_Terminal_01x02", ref("J"), v, g(12), g(50), mirror="y",
            text={"Reference": (0, -3.81, None), "Value": (0, 6.35, None)}, **f)
v, f = cat("HDR3", "VIO SELECT")
f["Description"] = "3-pin strap: 1-2 = external VIO, 2-3 = +5 V"
JP1 = pw.add("Jumper", "Jumper_3_Open", ref("JP"), v, g(34), g(50),
             text={"Reference": (0, -3.81, None), "Value": (0, -6.35, None)}, **f)
pw.wire(J2[1], JP1[1])
pw.label((g(20), g(50)), "EXT_VIO", 0, kind="local")
pw.wire(J2[2], (g(16), g(51)))
pw.power("GND", (g(16), g(51)))
pw.power("+5V", JP1[3])
pw.wire(JP1[2], (g(34), g(56)), (g(96), g(56)))         # VIO rail
pw.power("VIO", (g(42), g(56)), lib="IO8", path=LIB)
pw.power("PWR_FLAG", (g(48), g(56)))
pw.note((g(8), g(62)), "JP1 1-2 : VIO from J2 (external 4.5 - 14 V)\n"
        "JP1 2-3 : VIO = on-board +5 V\n"
        "Fit exactly one shunt, move it only with power removed.\n"
        "VIO supplies the DRV8912 VM pins and the VIO pins of the output terminals.")

pw.frame(g(84), g(40), g(140), g(76), "VIO MONITOR  (ADC)")
r1 = R(pw, "100k", g(96), g(56) + 3.81, left_text=True)
pw.wire(r1[2], (g(96), g(64)), (g(124), g(64)))         # VIO_ADC node
pulldown(pw, "22k", g(96), g(64), left_text=True)
cap_to_gnd(pw, "1n", g(104), g(64))
v, f = cat("BAT54S")
D1 = pw.add("Diode", "BAT54S", ref("D"), v, g(114), g(62),
            text={"Reference": (0, -5.08, None), "Value": (0, -2.54, None)}, **f)
pw.power("GND", D1[1], rot=270)
pw.power("+3V3", D1[2], rot=270)
pw.label((g(124), g(64)), "VIO_ADC", 0, shape="output")
pw.note((g(86), g(71)), "VIO_ADC = VIO x 22k / 122k   (14 V -> 2.52 V)\n"
        "D1 clamps the ADC pin to GND / +3V3.\n"
        "Use a long ADC sample time (source ~18 kOhm).")

# ============================================================== 3  MCU
mc = Sheet("mcu.kicad_sch", "Microcontroller, EEPROM, address straps and SWD", 3)
mc.frame(g(60), g(8), g(124), g(70), "MCU   STM32G030F6P6  (TSSOP-20)")
U2 = part(mc, "MCU_ST_STM32G0", "STM32G030F6Px", "STM32", "U", g(88), g(40),
          text={"Reference": (12.7, -21.59, "left"), "Value": (12.7, 24.13, "left")})
mc.wire(U2[4], (g(88), g(26)), (g(100), g(26)))         # VDD
mc.power("+3V3", (g(88), g(26)))
cap_to_gnd(mc, "100n", g(94), g(26))
cap_to_gnd(mc, "4u7_0603", g(100), g(26))
mc.power("GND", U2[5])
for pin, name, shape in [
    (7, "VIO_ADC", "input"), (8, "SPI_SCK", "output"), (9, "SPI_MOSI", "output"),
    (10, "IN_IRQ", "input"), (11, "DRV_nCS", "output"), (12, "IN_nCS", "output"),
    (13, "SPI_MISO", "input"), (14, "DRV_nSLEEP", "output"), (15, "SER_DE", "output"),
    (16, "UART_TX", "output"), (17, "UART_RX", "input"),
]:
    mc.stub(U2[pin], name, "right", g(5), shape=shape)
mc.stub(U2[18], "SWDIO", "right", g(5))
mc.stub(U2[19], "SWCLK", "right", g(5))
mc.stub(U2[6], "NRST", "left", g(5))
mc.stub(U2[3], "ADDR1", "left", g(5), kind="local")
mc.stub(U2[20], "I2C_SCL", "left", g(5), kind="local")
mc.stub(U2[1], "I2C_SDA", "left", g(5), kind="local")
mc.stub(U2[2], "ADDR0", "left", g(5), kind="local")
mc.table((g(62), g(56)), [
    ("Pin", "Function", "Pin", "Function"),
    ("PA0", "VIO monitor (ADC_IN0)", "PA7", "driver nSLEEP"),
    ("PA1/PA2/PA6", "SPI1 SCK/MOSI/MISO", "PB0", "transceiver DE"),
    ("PA3", "expander IRQ (EXTI3)", "PA9/PA10", "USART1 TX/RX (remap PA11/12)"),
    ("PA4", "driver nCS", "PB6/PB7", "I2C1 SCL/SDA"),
    ("PA5", "expander nCS", "PC14/PC15", "address straps"),
], widths=[g(7), g(10), g(6), g(10)])

mc.frame(g(6), g(8), g(56), g(30), "RESET")
mc.wire((g(22), g(20)), (g(32), g(20)))
mc.label((g(32), g(20)), "NRST", 0)
pullup(mc, "10k", g(22), g(20), left_text=True)
cap_to_gnd(mc, "100n", g(27), g(20))
mc.note((g(8), g(26)), "NRST also resets the input expander.")

mc.frame(g(6), g(32), g(56), g(56), "CONFIGURATION EEPROM   (I2C address 0xA0)")
U3 = part(mc, "Memory_EEPROM", "24AA02-OT", "EEPROM", "U", g(18), g(48),
          text={"Reference": (-3.81, 8.89, "right"), "Value": (-3.81, 11.43, "right")})
mc.wire(U3[4], (g(18), g(42)), (g(13), g(42)))
mc.power("+3V3", (g(18), g(42)))
cap_to_gnd(mc, "100n", g(13), g(42), left_text=True)
mc.power("GND", U3[2])
mc.wire(U3[5], (g(12), g(48)))
mc.power("GND", (g(12), g(48)))
mc.wire(U3[3], (g(24), g(47)), (g(24), g(40)), (g(44), g(40)))   # SDA
mc.label((g(44), g(40)), "I2C_SDA", 0, kind="local")
pullup(mc, "4k7", g(32), g(40))
mc.wire(U3[1], (g(44), g(48)))                                    # SCL
mc.label((g(44), g(48)), "I2C_SCL", 0, kind="local")
pullup(mc, "4k7", g(38), g(48))

mc.frame(g(6), g(58), g(56), g(84), "ADDRESS STRAPS")
straps = []
for nm, y in (("ADDR0", g(68)), ("ADDR1", g(78))):
    mc.label((g(14), y), nm, 180, kind="local")
    mc.wire((g(14), y), (g(28), y))
    pullup(mc, "10k", g(20), y, left_text=True)
    jp = jumper2(mc, nm, g(30), y)
    mc.wire(jp[2], (g(34), y))
    mc.power("GND", (g(34), y))
    straps.append(jp)
mc.note((g(38), g(64)), "fitted = 0\nopen   = 1")

mc.frame(g(60), g(74), g(124), g(104), "SWD DEBUG   (ARM 10-pin 1.27 mm)")
v, f = cat("SWD")
J3 = mc.add("Connector", "Conn_ARM_JTAG_SWD_10", ref("J"), v, g(80), g(89),
            text={"Reference": (-10.16, -16.51, "right"), "Value": (-10.16, 17.78, "right")}, **f)
mc.power("+3V3", J3[1])
mc.power("GND", J3[3])
mc.wire(J3[9], J3[3])
mc.stub(J3[10], "NRST", "right", g(5))
mc.stub(J3[4], "SWCLK", "right", g(5))
mc.stub(J3[2], "SWDIO", "right", g(5))
for p in (6, 7, 8):
    mc.nc(J3[p])
mc.note((g(96), g(94)), "VTref only senses the target voltage;\npower the board through J1.")

# ============================================================== 4  serial
se = Sheet("serial.kicad_sch", "Serial interface - RS-485 / RS-232 (strap selectable)", 4)
se.frame(g(6), g(8), g(90), g(80), "TRANSCEIVER   MAX3160   (RS-232 / RS-485 multiprotocol, shared pins)")
U4 = part(se, "IO8", "MAX3160EAP", "MAX3160", "U", g(60), g(46), path=LIB,
          text={"Reference": (-17.78, -19.05, "left"), "Value": (-17.78, 26.67, "left")})
xl, xr = U4[16][0], U4[6][0]
# supply and charge pump
se.wire(U4[2], (U4[2][0], g(33)), (g(50), g(33)))
se.power("+3V3", (U4[2][0], g(33)))
cap_to_gnd(se, "100n", g(50), g(33), left_text=True)
se.power("GND", U4[4])
se.wire(U4[20], (U4[20][0], g(35)), (g(72), g(35)))
cap_to_gnd(se, "100n", g(72), g(35))
c_fly1 = C(se, "100n", xr + g(2), (U4[1][1] + U4[3][1]) / 2)
se.wire(U4[1], c_fly1[1])
se.wire(U4[3], c_fly1[2])
c_fly2 = C(se, "100n", xr + g(6), (U4[19][1] + U4[18][1]) / 2)
se.wire(U4[19], c_fly2[1])
se.wire(U4[18], c_fly2[2])
se.wire(U4[17], (U4[17][0], g(60)), (g(68), g(60)))
cap_to_gnd(se, "100n", g(68), g(60))
# logic side
se.stub(U4[16], "UART_TX", "left", g(6), shape="input")
se.stub(U4[8], "UART_RX", "left", g(6), shape="output")
se.nc(U4[7])
yh, ys, yf = U4[12][1], U4[9][1], U4[10][1]
se.wire(U4[12], (xl - g(2), yh), (xl - g(2), ys), U4[9])          # HDPLX + SHDN high
se.power("+3V3", (xl - g(2), yh))
se.wire(U4[10], (xl - g(4), yf))                                   # FAST low
se.power("GND", (xl - g(4), yf), rot=270)
ym = U4[11][1]
jm = jumper2(se, "MODE", g(38), ym, text={"Reference": (0, -3.81, None), "Value": (0, 2.54, None)})
se.wire(U4[11], jm[2])
se.power("GND", jm[1], rot=270)
pullup(se, "10k", g(44), ym, left_text=True)
se.label((g(46), ym), "MODE", 0, kind="local")
yd = U4[15][1]
se.wire(U4[15], (g(28), yd))
se.label((g(28), yd), "SER_DE", 180, shape="input")
pulldown(se, "10k", g(44), yd, left_text=True)
se.table((g(8), g(62)), [
    ("MODE strap", "Interface", "J4 pins used"),
    ("open", "RS-485 half duplex", "1 = A, 2 = B, 4 = GND"),
    ("fitted", "RS-232", "2 = TXD, 3 = RXD, 4 = GND"),
], widths=[g(9), g(12), g(14)])
se.note((g(8), g(70)), "HDPLX = 1 (half duplex)   FAST = 0 (slew limited, <= 250 kbit/s)   SHDN = 1\n"
        "DE has a pull-down: the RS-485 driver stays off until firmware enables it.\n"
        "The RS-485 receiver is always on: firmware discards its own echo.\n"
        "In RS-232 mode DE drives T2OUT on J4 pin 1 (unused - leave open).")
# line side
yA, yB, yR = g(30), g(38), g(66)
se.wire(U4[6], (g(78), U4[6][1]), (g(78), yA), (g(129), yA))
se.wire(U4[5], (g(80), U4[5][1]), (g(80), yB), (g(127), yB))
se.wire(U4[13], (g(76), U4[13][1]), (g(76), yR), (g(131), yR))
se.nc(U4[14])
se.label((g(84), yA), "LINE_A", 0, kind="local")
se.label((g(84), yB), "LINE_B_TXD", 0, kind="local")
se.label((g(84), yR), "LINE_RXD", 0, kind="local")

se.frame(g(92), g(8), g(150), g(80), "PROTECTION, TERMINATION, BIAS, TERMINAL")
v, f = cat("SM712")
D2 = se.add("Diode", "SM712_SOT23", ref("D"), v, g(98), yA + 8.89, rot=90,
            text={"Reference": (-2.54, -1.27, "right"), "Value": (-2.54, 1.27, "right")}, **f)
top_, bot_ = ends(D2)
se.wire(top_, (g(98), yA))
se.wire(bot_, (g(98), yB))
se.power("GND", D2[3])
rt = R(se, "120R", g(106), yA + 3.81)
jt = jumper2(se, "TERM", g(106), yA + g(1.5) + g(2.5), rot=90,
             text={"Reference": (3.81, -1.27, "left"), "Value": (3.81, 1.27, "left")})
a, b = ends(jt)
se.wire(rt[2], a)
se.wire(b, (g(106), yB))
jba = jumper2(se, "BIAS A", g(114), yA - g(2), rot=90,
              text={"Reference": (3.81, -1.27, "left"), "Value": (3.81, 1.27, "left")})
a, b = ends(jba)
se.wire(b, (g(114), yA))
pullup(se, "390R", g(114), a[1] - g(1))
se.wire(a, (g(114), a[1] - g(1)))
jbb = jumper2(se, "BIAS B", g(120), yB + g(2), rot=90,
              text={"Reference": (3.81, -1.27, "left"), "Value": (3.81, 1.27, "left")})
a, b = ends(jbb)
se.wire(a, (g(120), yB))
se.wire(b, (g(120), b[1] + g(1)))
pulldown(se, "390R", g(120), b[1] + g(1))
v, f = cat("TVS15")
D3 = se.add("Device", "D_TVS", ref("D"), v, g(98), yR + 3.81, rot=90,
            text={"Reference": (2.54, -1.27, "left"), "Value": (2.54, 1.27, "left")}, **f)
top_, bot_ = ends(D3)
se.wire(top_, (g(98), yR))
se.power("GND", bot_)
v, f = cat("TB4", "SERIAL")
f["Description"] = "1 = A, 2 = B / TXD, 3 = RXD, 4 = GND"
J4 = se.add("Connector", "Screw_Terminal_01x04", ref("J"), v, g(140), g(34),
            text={"Reference": (0, -5.08, None), "Value": (0, 7.62, None)}, **f)
se.wire((g(129), yA), (g(129), J4[1][1]), J4[1])
se.wire((g(127), yB), (g(127), J4[2][1]), J4[2])
se.wire((g(131), yR), (g(131), J4[3][1]), J4[3])
se.power("GND", J4[4])
# DB9 female (DCE pinout: straight cable to a PC / USB-RS232 adapter)
v, f = cat("DB9F")
f["Description"] = "1 = A, 2 = B / TXD, 3 = RXD, 5 = GND, 6 = +5 V (JP8), shell = GND"
J10 = se.add("Connector", "DE9_Socket_MountingHoles", "J10", v, g(136), g(46), mirror="y",
             text={"Reference": (-3.81, -16.51, "right"), "Value": (-3.81, -13.97, "right")}, **f)
se.stub(J10[1], "LINE_A", "right", g(2), kind="local")
se.stub(J10[2], "LINE_B_TXD", "right", g(2), kind="local")
se.stub(J10[3], "LINE_RXD", "right", g(2), kind="local")
se.wire(J10[5], (J10[5][0] + g(2), J10[5][1]))
se.power("GND", (J10[5][0] + g(2), J10[5][1]))
se.power("GND", J10["SH"])
for p_ in (4, 7, 8, 9):
    se.nc(J10[p_])
se.stub(J10[6], "DB9_PIN6", "right", g(2), kind="local")
jp8 = se.add("Jumper", "SolderJumper_2_Bridged", "JP8", "5V ON PIN 6", g(140), g(62),
             text={"Reference": (0, -2.54, None), "Value": (0, 2.54, None)},
             Footprint="Jumper:SolderJumper-2_P1.3mm_Bridged_RoundedPad1.0x1.5mm",
             Description="Cut to remove +5 V from DB9 pin 6", _nobom=True)
se.wire((jp8[1][0] - g(2), jp8[1][1]), jp8[1])
se.label((jp8[1][0] - g(2), jp8[1][1]), "DB9_PIN6", 180, kind="local")
se.wire(jp8[2], (jp8[2][0] + g(2), jp8[2][1]))
se.power("+5V", (jp8[2][0] + g(2), jp8[2][1]))
se.table((g(94), g(48)), [
    ("J4 pin", "DB9 pin", "RS-485", "RS-232"),
    ("1", "1", "A (non-inverting)", "- (leave open)"),
    ("2", "2", "B (inverting)", "TXD (output)"),
    ("3", "3", "-", "RXD (input)"),
    ("4", "5, shell", "GND", "GND"),
    ("-", "6", "+5 V (JP8, cut to remove)", "+5 V"),
], widths=[g(5), g(5), g(11), g(7)])
se.note((g(94), g(64)), "RS-485: fit TERM only at the two physical bus ends;\n"
        "        fit BIAS A + BIAS B on one node only (normally the master).\n"
        "RS-232: REMOVE TERM, BIAS A and BIAS B (they would load TXD).\n"
        "D2 SM712: -7 / +12 V TVS for the bus pins.\n"
        "D3: 15 V bidirectional TVS for the RS-232 receiver input.")

# ============================================================== 5  driver
dr = Sheet("driver.kicad_sch", "Output driver and output terminals", 5)
dr.frame(g(6), g(8), g(100), g(86), "HALF-BRIDGE DRIVER   DRV8912-Q1   (OUT1..OUT8 used)")
U5 = part(dr, "IO8", "DRV8912-Q1", "DRV8912", "U", g(60), g(46), path=LIB,
          text={"Reference": (-15.24, -21.59, "left"), "Value": (-15.24, 26.67, "left")})
dr.wire(U5[6], (U5[6][0], g(32)), (g(44), g(32)))        # VDD
dr.power("+3V3", (g(44), g(32)))
cap_to_gnd(dr, "100n", g(48), g(32))
cap_to_gnd(dr, "1u", g(52), g(32))
dr.wire(U5[16], (U5[16][0], g(30)), (g(82), g(30)))      # VM
dr.wire(U5[21], (U5[21][0], g(30)))
dr.power("VIO", (g(66), g(30)), lib="IO8", path=LIB)
cap_to_gnd(dr, "100n", g(70), g(30))
cap_to_gnd(dr, "22u_1206", g(75), g(30))
cap_to_gnd(dr, "10u_1206", g(82), g(30))
for n in (1, 13, 24, 25):
    dr.wire(U5[n], (U5[n][0], g(57)))
dr.wire((U5[1][0], g(57)), (U5[25][0], g(57)))
dr.power("GND", (g(60), g(57)))
dr.stub(U5[20], "SPI_SCK", "left", g(4), shape="input")
dr.stub(U5[5], "SPI_MOSI", "left", g(4), shape="input")
dr.stub(U5[7], "SPI_MISO", "left", g(4), shape="output")
y = U5[19][1]
dr.wire(U5[19], (g(26), y))
dr.label((g(26), y), "DRV_nCS", 180, shape="input")
pullup(dr, "10k", g(34), y, left_text=True)
y = U5[8][1]
dr.wire(U5[8], (g(26), y))
dr.label((g(26), y), "DRV_nSLEEP", 180, shape="input")
pulldown(dr, "100k", g(34), y, left_text=True)
y = U5[12][1]
dr.wire(U5[12], (g(46), y), (g(46), g(64)), (g(16), g(64)))
dr.label((g(38), g(64)), "DRV_nFAULT", 0, kind="local")
pullup(dr, "10k", g(30), g(64), left_text=True)
v, f = cat("HDR2", "FAULT TEST")
f["Description"] = "Test point: 1 = nFAULT, 2 = GND"
J5 = dr.add("Connector_Generic", "Conn_01x02", ref("J"), v, g(13), g(64), mirror="y",
            text={"Reference": (0, -2.54, None), "Value": (0, 5.08, None)}, **f)
dr.wire(J5[1], (g(16), g(64)))
dr.power("GND", J5[2])
dr.note((g(8), g(72)), "SPI mode 1 (CPOL 0, CPHA 1), 16-bit frames, <= 5 MHz.\n"
        "nSLEEP has a pull-down: outputs stay off until firmware wakes the driver.\n"
        "nFAULT is also reported over SPI; J5 is a test point.\n"
        "VM bulk 22 uF + 10 uF keeps >= 10 uF effective at 14 V (DC bias).\n"
        "OUT9..OUT12 are not used: keep them disabled in firmware.")
for ch, pin in {1: 2, 2: 23, 3: 14, 4: 11, 5: 3, 6: 10, 7: 4, 8: 22}.items():
    dr.stub(U5[pin], f"OUT{ch}", "right", g(6), kind="local")
for pin in (9, 15, 17, 18):
    dr.nc(U5[pin])

dr.frame(g(104), g(8), g(150), g(86), "OUTPUT TERMINALS   (2.54 mm screw)")
io_terms = []
for yy, chans in ((g(24), (1, 2, 3, 4)), (g(50), (5, 6, 7, 8))):
    v, f = cat("TB6", f"OUT{chans[0]}-OUT{chans[-1]}")
    # pin 1 = GND ... pin 6 = first output: on the top PCB edge (rotated 180 deg) this
    # reads OUTa, OUTa+1, OUTa+2, OUTa+3, VIO, GND from left to right
    f["Description"] = f"1 = GND, 2 = VIO, 3..6 = OUT{chans[-1]}..OUT{chans[0]}"
    J = dr.add("Connector", "Screw_Terminal_01x06", ref("J"), v, g(136), yy,
               text={"Reference": (0, -7.62, None), "Value": (0, 10.16, None)}, **f)
    io_terms.append(J)
    for i, ch in enumerate(chans):
        dr.stub(J[6 - i], f"OUT{ch}", "left", g(8), kind="local")
    dr.wire(J[2], (J[2][0] - g(4), J[2][1]))
    dr.power("VIO", (J[2][0] - g(4), J[2][1]), rot=90, lib="IO8", path=LIB)
    dr.wire(J[1], (J[1][0] - g(4), J[1][1]))
    dr.power("GND", (J[1][0] - g(4), J[1][1]), rot=270)
dr.note((g(106), g(66)), "OUTx: off, low-side switch, high-side\n"
        "switch or push-pull, optionally PWM.\n"
        "Low-side / high-side loads are returned to\n"
        "the VIO / GND pins of the same terminal.\n"
        "Do not drive OUTx externally above VIO\n"
        "or below GND (body diodes).")

# ============================================================== 6  inputs
ins = Sheet("inputs.kicad_sch", "Isolated inputs and SPI input expander", 6)
ins.frame(g(6), g(8), g(104), g(80), "ISOLATED INPUTS   TLP290-4 bidirectional optocouplers, 5 - 24 V, sink or source")
in_terms = []
v_opto, f_opto = cat("TLP290")
for grp in range(2):
    gx, gy = g(12 + 46 * grp), g(21)
    first = 4 * grp + 1
    v, f = cat("TB5", f"IN{first}-IN{first + 3} COM{grp + 1}")
    f["Description"] = f"1-4 = IN{first}..IN{first + 3}, 5 = COM{grp + 1} (isolated)"
    J = ins.add("Connector", "Screw_Terminal_01x05", ref("J"), v, gx, gy, mirror="y",
                text={"Reference": (0, -7.62, None), "Value": (0, 8.89, None)}, **f)
    in_terms.append(J)
    uref = ref("U")
    xpin = J[1][0]
    ox = xpin + g(24)
    com_x = ox - g(4)
    bottom = 0.0
    for k in range(4):
        ch = first + k
        ya = J[1][1] + k * g(7)                       # anode row of this channel
        # terminal pin k -> staircase -> series resistor
        rs = R(ins, "3k3_1206", ox - g(9), ya, horiz=True)
        lp, rp = sorted([rs[1], rs[2]])
        if k == 0:
            ins.wire(J[1], lp)
        else:
            xs = xpin + g(2 * (4 - k))
            ins.wire(J[k + 1], (xs, J[k + 1][1]), (xs, ya), lp)
        o = ins.add("Isolator", "TLP290-4", uref, v_opto, ox, ya + g(1), unit=k + 1,
                    text={"Reference": (0, -6.35, None), "Value": (0, 6.35, None)}, _hideval=True, **f_opto)
        an, ca = o[2 * k + 1], o[2 * k + 2]           # LED pins
        col, emi = o[16 - 2 * k], o[15 - 2 * k]       # transistor pins
        ins.wire(rp, an)
        # LED shunt: raises the threshold so a few volts of leakage read as off
        sh = R(ins, "4k7", ox - g(5), ya + 3.81, left_text=True)
        ins.wire(sh[2], (com_x, sh[2][1]))
        ins.wire(ca, (com_x, ca[1]))
        bottom = sh[2][1]
        # output side
        ins.power("GND", emi)
        pullup(ins, "22k", ox + g(5), col[1])
        ins.wire(col, (ox + g(9), col[1]))
        ins.label((ox + g(9), col[1]), f"IN{ch}", 0, kind="local")
    # COM bus: terminal pin 5 -> below the cells -> up the cathode column
    yb = bottom + g(2)
    ins.wire(J[5], (xpin + g(1), J[5][1]), (xpin + g(1), yb), (com_x, yb), (com_x, J[1][1] + g(2)))
    ins.label((xpin + g(4), yb), f"COM{grp + 1}", 0, kind="local")
ins.table((g(6), g(52)), [
    ("Wiring", "COM to", "Input is active when"),
    ("source (PNP sensor, switch to +V)", "GND of the field supply", "IN is connected to +V"),
    ("sink (NPN sensor, switch to GND)", "+V of the field supply", "IN is connected to GND"),
], widths=[g(20), g(14), g(14)])
ins.note((g(6), g(60)), "Active input -> LED current -> phototransistor on -> GPx low -> reads 1 (IPOL = 1).\n"
         "Input voltage 5 - 24 V (max 30 V): on above about 4 V, off below about 2 V.\n"
         "Input current about 0.9 mA at 5 V, 6.6 mA at 24 V (3.3k / 0.25 W series resistor).\n"
         "IN1-4 / COM1 and IN5-8 / COM2 are two isolated groups, each wired sink or source.\n"
         "Isolation 2.5 kV (TLP290-4); keep the field side clear of logic copper on the PCB.")

ins.frame(g(108), g(8), g(150), g(80), "SPI INPUT EXPANDER   MCP23S08")
U6 = part(ins, "IO8", "MCP23S08-SSOP", "MCP23S08", "U", g(134), g(42), path=LIB,
          text={"Reference": (-12.7, -19.05, "left"), "Value": (-12.7, 29.21, "left")})
ins.wire(U6[20], (U6[20][0], g(30)), (g(144), g(30)))
ins.power("+3V3", (U6[20][0], g(30)))
cap_to_gnd(ins, "100n", g(139), g(30))
cap_to_gnd(ins, "1u", g(144), g(30))
ins.power("GND", U6[9])
ins.stub(U6[1], "SPI_SCK", "left", g(3), shape="input")
ins.stub(U6[2], "SPI_MOSI", "left", g(3), shape="input")
ins.stub(U6[3], "SPI_MISO", "left", g(3), shape="output")
ins.stub(U6[6], "NRST", "left", g(3))
y = U6[7][1]
ins.wire(U6[7], (g(112), y))
ins.label((g(112), y), "IN_nCS", 180, shape="input")
pullup(ins, "10k", g(118), y, left_text=True)
y = U6[8][1]
ins.wire(U6[8], (g(112), y))
ins.label((g(112), y), "IN_IRQ", 180, shape="output")
pullup(ins, "10k", g(118), y, left_text=True)
ins.power("GND", U6[4], rot=270)
ins.power("GND", U6[5], rot=270)
for i in range(8):
    ins.stub(U6[12 + i], f"IN{i + 1}", "right", g(3), kind="local")
ins.nc(U6[10])
ins.nc(U6[11])
ins.note((g(110), g(70)), "SPI mode 0, hardware address A1 = A0 = 0.\n"
         "GP0..GP7 inputs, IPOL = 0xFF (active = 1).\n"
         "INT: open-drain, active low.")

# ============================================================== 1  root
sheets = [
    (pw, (g(10), g(16)), "J1 5 V in, AP2112K 3.3 V LDO,\nJ2 + JP1 output supply VIO,\nVIO monitor divider"),
    (mc, (g(44), g(16)), "STM32G030F6P6, 24AA02 EEPROM,\naddress straps, SWD header"),
    (se, (g(78), g(16)), "MAX3160 RS-485 / RS-232,\nMODE strap, TVS, termination,\nbias, J4 terminal, J10 DB9"),
    (dr, (g(44), g(32)), "DRV8912-Q1 half-bridge driver,\n8 outputs, terminals J6 / J7"),
    (ins, (g(78), g(32)), "8 isolated opto inputs (TLP290-4),\nterminals J8 / J9, MCP23S08"),
]
for sh, at, desc in sheets:
    proj.add_sheet(sh, at, (g(26), g(9)))
    root.note((at[0] + 1.27, at[1] + 1.27), desc)
root.note((g(8), g(6)), "RS485_IO8 rev B  -  8 isolated inputs, 8 outputs, RS-485 / RS-232 host link",
          size=3.0, bold=True)
root.note((g(10), g(44)), "Signal flow\n"
          "  host  <->  J4  <->  MAX3160  <->  USART1 of the STM32G030\n"
          "  STM32G030 SPI1  ->  DRV8912 (outputs)   and   MCP23S08 (inputs)  ->  STM32G030\n"
          "  OUTx terminal  <-  DRV8912 OUTx,   INx terminal  ->  TLP290-4  ->  MCP23S08 GPx", size=1.27)
root.table((g(10), g(52)), [
    ("Connector", "Name", "Pin assignment"),
    (J1.ref, "5V IN", "1 = +5 V, 2 = GND   (regulated 4.75 - 5.25 V)"),
    (J2.ref, "VIO EXT IN", "1 = EXT_VIO, 2 = GND   (4.5 - 14 V, used with JP1 1-2)"),
    (J4.ref, "SERIAL", "1 = A, 2 = B / TXD, 3 = RXD, 4 = GND"),
    (J10.ref, "DB9 SERIAL", "1 = A, 2 = B / TXD, 3 = RXD, 5 = GND, 6 = +5 V (JP8)"),
    (io_terms[0].ref, "OUT1-OUT4", "1 = GND, 2 = VIO, 3 = OUT4, 4 = OUT3, 5 = OUT2, 6 = OUT1"),
    (io_terms[1].ref, "OUT5-OUT8", "1 = GND, 2 = VIO, 3 = OUT8, 4 = OUT7, 5 = OUT6, 6 = OUT5"),
    (in_terms[0].ref, "IN1-IN4", "1..4 = IN1..IN4, 5 = COM1   (isolated, 5 - 24 V, sink or source)"),
    (in_terms[1].ref, "IN5-IN8", "1..4 = IN5..IN8, 5 = COM2   (isolated, 5 - 24 V, sink or source)"),
    (J3.ref, "SWD", "ARM Cortex 10-pin 1.27 mm"),
    (J5.ref, "FAULT TEST", "1 = DRV8912 nFAULT, 2 = GND"),
], widths=[g(7), g(9), g(30)], size=1.5)
root.table((g(10), g(68)), [
    ("Strap", "Name", "Setting"),
    (JP1.ref, "VIO SELECT", "1-2 = external VIO from J2,  2-3 = on-board +5 V"),
    (straps[0].ref, "ADDR0", "fitted = 0, open = 1"),
    (straps[1].ref, "ADDR1", "fitted = 0, open = 1"),
    (jm.ref, "MODE", "open = RS-485,  fitted = RS-232"),
    (jt.ref, "TERM", "120R across A/B - RS-485 bus ends only"),
    (jba.ref + " / " + jbb.ref, "BIAS A / B", "390R fail-safe bias - one RS-485 node per bus"),
], widths=[g(7), g(9), g(30)], size=1.5)
for i in range(4):
    root.add("Mechanical", "MountingHole", f"H{i + 1}", "M3", g(100 + 6 * i), g(52),
             text={"Reference": (0, -3.81, None), "Value": (0, 3.81, None)},
             Footprint="MountingHole:MountingHole_3.2mm_M3", _nobom=True)
root.note((g(98), g(46)), "Mounting holes (M3, not plated)", size=1.27)
root.note((g(10), g(84)), "All parts carry LCSC part numbers (field LCSC).  BOM: BOM.csv.  "
          "Generated by tools/gen_schematic.py.", size=1.27)

proj.write(OUT)
print("schematic written to", OUT)
