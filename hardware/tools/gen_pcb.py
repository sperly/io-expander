#!/usr/bin/env python3
"""Create RS485_IO8.kicad_pcb: placed footprints, nets, outline - no routing.

    python3 tools/gen_pcb.py            (needs KiCad's pcbnew Python module)

Footprints are linked to the schematic symbols (same UUID paths), so later
"Update PCB from Schematic" in KiCad keeps the placement. The script refuses to
overwrite an existing board unless --force is given, because hand routing
would be lost.

Layout (top view, 82 x 68 mm, 4 layers, M3 mounting holes in the four corners):
    left edge   : DB9 (J10) above USB-C (J11), together centred on the edge;
                  CH340C U10 beside the USB-C
    top edge    : VIO input J2, then output terminals J6 / J7, driver below them;
                  reads GND VIO | OUT1..OUT4 VIO GND | OUT5..OUT8 VIO GND left to right;
                  VIO select JP1 next to J2
    bottom edge : power J1 and serial terminal J4 (left), isolated input terminals J8 / J9
                  each input channel is one column: terminal pin -> series R -> shunt -> opto
    stack-up    : F.Cu signal | In1.Cu GND plane | In2.Cu +3V3 plane | B.Cu signal
"""
import os
import re
import subprocess
import sys
import tempfile

import pcbnew

HW = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCH = os.path.join(HW, "RS485_IO8.kicad_sch")
PCB = os.path.join(HW, "RS485_IO8.kicad_pcb")
FPLIB = "/usr/share/kicad/footprints"

X0, Y0, W, H = 100.0, 100.0, 82.0, 68.0
XR, YB = X0 + W, Y0 + H

DB9_EDGE = 10.54   # footprint origin -> board edge (mounting holes 9.12 mm from the edge)
TB_FRONT = 4.4     # terminal block pin row -> board edge (body front 3.4 mm, silk stays on the board)
TOP_ROW, BOT_ROW = Y0 + TB_FRONT, YB - TB_FRONT

OPTO_Y = 142.5                         # opto centre = isolation barrier (field side below)
FIELD_X = 142.0                        # left end of the isolated field
BARRIER_X, BARRIER_Y = FIELD_X, OPTO_Y

# terminals: pin-1 x position on the top / bottom edge
TOP_TERMINALS = {"J7": 172.0, "J6": 154.0, "J2": 136.5}  # rotation 180: wire entry up, pin 1 on the right
BOT_TERMINALS = {"J1": 110.0, "J4": 117.2, "J8": 144.0, "J9": 161.0}   # rotation 0: wire entry down

# ref: (x, y, rotation_deg)   rotation is counter-clockwise, as in KiCad
PLACE = {
    # ---- mechanical
    "H1": (X0 + 4.0, Y0 + 4.0, 0), "H2": (XR - 4.0, Y0 + 4.0, 0),
    "H3": (X0 + 4.0, YB - 4.0, 0), "H4": (XR - 4.0, YB - 4.0, 0),
    # ---- left edge: DB9 above USB-C, centred on the edge (DB9 origin = pin 1, near its lower end)
    "J10": (X0 + DB9_EDGE, 133.97, 270),
    "JP8": (114.9, 133.4, 0),
    # ---- USB-C serial: connector flush with the edge, opening to the left
    "J11": (X0 + 3.7, 150.18, 270),
    "U9": (112.0, 152.0, 0), "C22": (112.0, 155.2, 0), "D4": (116.5, 151.2, 0),
    "R42": (116.6, 154.0, 0), "R43": (116.6, 156.0, 0),
    "U10": (130.6, 153.55, 90), "C23": (123.8, 152.0, 90),
    # wired-AND on UART_RX, next to the MAX3160 RO pin
    "D5": (122.5, 127.0, 0), "R44": (126.5, 127.0, 0),
    # ---- transceiver
    "U4": (124.0, 118.0, 0),
    "C10": (121.3, 111.5, 0),
    "C11": (131.5, 113.0, 90), "C12": (131.5, 117.0, 90), "C13": (131.5, 121.0, 90), "C14": (131.5, 125.0, 90),
    "R8": (116.0, 114.0, 0), "R9": (116.0, 117.0, 0),
    "D2": (116.0, 121.0, 0), "D3": (116.0, 126.0, 0),
    "R10": (116.0, 130.6, 0), "R11": (122.0, 133.0, 0), "R12": (127.0, 133.0, 0),
    "JP5": (117.0, 137.0, 0), "JP6": (122.0, 137.0, 0), "JP7": (127.0, 137.0, 0), "JP4": (132.0, 137.0, 0),
    # ---- power
    "C1": (114.0, 148.0, 90), "C2": (117.5, 148.0, 90),
    "U1": (122.0, 148.0, 0),
    "C3": (126.5, 148.0, 90), "C4": (129.5, 148.0, 90),
    "JP1": (124.8, 108.6, 90),                              # VIO select, next to J2 on the top edge
    # VIO monitor divider + clamp, next to the ADC input (U2 pin 7)
    "R1": (130.0, 128.7, 90), "R2": (131.8, 128.7, 90), "C5": (133.6, 128.7, 90), "D1": (132.0, 132.0, 0),
    # ---- MCU
    "U2": (145.0, 128.0, 0),
    "C6": (143.0, 121.0, 0), "C7": (147.0, 121.0, 0),
    "R3": (137.0, 125.0, 0), "C8": (137.0, 128.0, 0),
    "C9": (136.0, 136.0, 90), "U3": (140.0, 136.0, 0), "R4": (137.0, 139.5, 0), "R5": (137.0, 141.3, 0),
    "J3": (137.0, 113.0, 0),
    "JP2": (151.0, 115.0, 90), "JP3": (151.0, 119.0, 90), "R6": (157.0, 115.0, 0), "R7": (157.0, 119.0, 0),
    # ---- output driver (below the output terminals)
    "U5": (168.0, 122.0, 0),
    "C15": (162.0, 114.2, 0), "C16": (161.0, 117.0, 0), "C17": (168.0, 114.2, 0),
    "C18": (174.0, 114.2, 0), "C19": (179.0, 116.0, 90),
    "R13": (161.5, 120.0, 0), "R14": (161.5, 123.0, 0), "R15": (161.5, 126.0, 0),
    "J5": (176.0, 121.5, 0),
    # ---- inputs: logic side
    "U8": (155.0, 128.5, 0),
    "C20": (152.0, 123.5, 0), "C21": (156.0, 123.5, 0),
    "R40": (162.5, 130.5, 90), "R41": (164.5, 130.5, 90),
}
for ref, x in TOP_TERMINALS.items():
    PLACE[ref] = (x, TOP_ROW, 180)
for ref, x in BOT_TERMINALS.items():
    PLACE[ref] = (x, BOT_ROW, 0)

DENSE = {f"R{n}" for n in range(16, 40)} | {"C22", "C23", "R42", "R43", "D4", "J11", "R1", "R2", "C5", "C10", "JP8"}

# input columns: channel k sits above terminal pin k (pitch 2.54 = opto channel pitch)
#   y: pull-up 22k (logic) | opto | shunt 4.7k | series 3.3k (field) | terminal
for opto, term, refs in (("U6", "J8", (16, 19, 22, 25)), ("U7", "J9", (28, 31, 34, 37))):
    x1 = BOT_TERMINALS[term]
    PLACE[opto] = (x1 + 3.81, OPTO_Y, 90)   # LED pins (1..8) on the bottom row, towards the field
    for k, r in enumerate(refs):
        x = x1 + 2.54 * k
        PLACE[f"R{r}"] = (x, 153.5, 90)      # series resistor
        PLACE[f"R{r + 1}"] = (x, 149.3, 90)  # LED shunt
        PLACE[f"R{r + 2}"] = (x, 135.0, 90)  # collector pull-up


def netlist():
    with tempfile.TemporaryDirectory() as d:
        out = os.path.join(d, "n.net")
        subprocess.run(["kicad-cli", "sch", "export", "netlist", "--format", "kicadsexpr", "-o", out, SCH],
                       check=True, capture_output=True)
        t = open(out).read()
    comps = {}
    for c in re.split(r"\(comp\s*\n", t[t.index("(components"):t.index("(libparts")])[1:]:
        ref = re.search(r'\(ref "([^"]+)"\)', c).group(1)
        comps[ref] = dict(
            value=re.search(r'\(value "([^"]*)"\)', c).group(1),
            footprint=re.search(r'\(footprint "([^"]*)"\)', c).group(1),
            sheet=re.search(r'\(sheetpath\s*\(names "[^"]*"\)\s*\(tstamps "([^"]*)"\)', c).group(1),
            datasheet=(re.search(r'\(datasheet "([^"]*)"\)', c) or [None, ""])[1],
            fields=dict(re.findall(r'\(field\s*\(name "([^"]+)"\)\s*"([^"]*)"\)', c)),
            uuid=re.findall(r'\(tstamps "([^"]+)"', c)[-1],
        )
    nets = {}
    for blk in re.split(r"\(net\s*\n", t[t.index("(nets"):])[1:]:
        name = re.search(r'\(name "([^"]*)"\)', blk).group(1)
        nets[name] = re.findall(r'\(ref "([^"]+)"\)\s*\(pin "([^"]+)"\)', blk)
    return comps, nets


def mm(x, y):
    return pcbnew.VECTOR2I(pcbnew.FromMM(x), pcbnew.FromMM(y))


def silk(board, text, x, y, size=1.0, angle=0, layer=pcbnew.F_SilkS, justify=None):
    t = pcbnew.PCB_TEXT(board)
    t.SetText(text)
    t.SetPosition(mm(x, y))
    t.SetLayer(layer)
    t.SetTextSize(mm(size, size))
    t.SetTextThickness(pcbnew.FromMM(size * 0.15))
    t.SetTextAngleDegrees(angle)
    if justify == "left":
        t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_LEFT)
    elif justify == "right":
        t.SetHorizJustify(pcbnew.GR_TEXT_H_ALIGN_RIGHT)
    board.Add(t)


def line(board, x1, y1, x2, y2, layer, width=0.15):
    s = pcbnew.PCB_SHAPE(board)
    s.SetShape(pcbnew.SHAPE_T_SEGMENT)
    s.SetStart(mm(x1, y1))
    s.SetEnd(mm(x2, y2))
    s.SetLayer(layer)
    s.SetWidth(pcbnew.FromMM(width))
    board.Add(s)


def main():
    if os.path.exists(PCB) and "--force" not in sys.argv:
        sys.exit(f"{PCB} exists - use --force to overwrite (placement and routing would be lost)")
    comps, nets = netlist()
    missing = sorted(set(comps) - set(PLACE))
    if missing:
        sys.exit(f"no placement for: {missing}")

    board = pcbnew.CreateEmptyBoard()
    netinfo = {}
    for name in nets:
        n = pcbnew.NETINFO_ITEM(board, name)
        board.Add(n)
        netinfo[name] = n
    pad_net = {(r, p): name for name, nodes in nets.items() for r, p in nodes}

    fps = {}
    for ref, c in sorted(comps.items()):
        lib, name = c["footprint"].split(":", 1)
        fp = pcbnew.FootprintLoad(os.path.join(FPLIB, lib + ".pretty"), name)
        if fp is None:
            sys.exit(f"{ref}: footprint {c['footprint']} not found")
        fp.SetFPID(pcbnew.LIB_ID(lib, name))
        fp.SetReference(ref)
        fp.SetValue(c["value"])
        fp.SetPath(pcbnew.KIID_PATH(c["sheet"] + c["uuid"]))
        for k, v in c["fields"].items():
            if k not in ("Footprint", "Datasheet") and v:
                fp.SetField(k, v)
                fp.GetField(k).SetVisible(False)
        if c["datasheet"] and c["datasheet"] != "~":
            fp.SetField("Datasheet", c["datasheet"])
            fp.GetField("Datasheet").SetVisible(False)
        if ref in DENSE or ref.startswith("H"):
            fp.Reference().SetVisible(False)  # still on the Fab layer
        x, y, rot = PLACE[ref]
        fp.SetPosition(mm(x, y))
        fp.SetOrientationDegrees(rot)
        for pad in fp.Pads():
            num = pad.GetNumber()
            if not num and ref == "U5":  # thermal vias of the PowerPAD follow the exposed pad
                num = "25"
            net = pad_net.get((ref, num))
            if net:
                pad.SetNet(netinfo[net])
        board.Add(fp)
        fps[ref] = fp

    # board outline
    for (x1, y1, x2, y2) in ((X0, Y0, XR, Y0), (XR, Y0, XR, YB), (XR, YB, X0, YB), (X0, YB, X0, Y0)):
        line(board, x1, y1, x2, y2, pcbnew.Edge_Cuts, 0.1)

    # connector pin labels on the silkscreen
    def pin_labels(ref, names, side):
        for pad in fps[ref].Pads():
            if not (pad.GetNumber().isdigit() and int(pad.GetNumber()) <= len(names)):
                continue
            p = pad.GetPosition()
            x, y = pcbnew.ToMM(p.x), pcbnew.ToMM(p.y)
            name = names[int(pad.GetNumber()) - 1]
            if side == "left":     # terminal on the left edge, label to its right
                silk(board, name, X0 + 8.2, y, 0.9, justify="left")
            elif side == "top":    # vertical label below the terminal, reading upwards
                silk(board, name, x, TOP_ROW + 4.2, 0.8, angle=90, justify="right")
            else:                  # vertical label above the terminal
                silk(board, name, x, BOT_ROW - 4.1, 0.8, angle=90, justify="left")

    pin_labels("J6", ["GND", "VIO", "OUT4", "OUT3", "OUT2", "OUT1"], "top")
    pin_labels("J7", ["GND", "VIO", "OUT8", "OUT7", "OUT6", "OUT5"], "top")
    pin_labels("J8", ["IN1", "IN2", "IN3", "IN4", "COM1"], "bottom")
    pin_labels("J9", ["IN5", "IN6", "IN7", "IN8", "COM2"], "bottom")
    pin_labels("J4", ["A", "B/TXD", "RXD", "GND"], "bottom")
    pin_labels("J1", ["+5V", "GND"], "bottom")
    pin_labels("J2", ["VIO", "GND"], "top")
    # terminal and J11 references are hidden: the pin labels and the USB-C shape identify them
    fps["R41"].Reference().SetPosition(mm(166.4, 130.5))  # R40 / R41 sit side by side
    for ref in ("U6", "U7", "U8", "U10"):  # reference inside the body (clear of the barrier line / caps)
        fps[ref].Reference().SetPosition(mm(*PLACE[ref][:2]))
        fps[ref].Reference().SetTextAngleDegrees(0)
    fps["U9"].Reference().SetPosition(mm(109.3, 152.0))
    fps["U9"].Reference().SetTextAngleDegrees(90)
    fps["D5"].Reference().SetPosition(mm(122.5, 124.6))
    fps["D5"].Reference().SetTextAngleDegrees(0)
    fps["R44"].Reference().SetPosition(mm(126.5, 125.3))
    fps["D1"].Reference().SetPosition(mm(129.6, 132.0))
    fps["R5"].Reference().SetPosition(mm(137.0, 142.8))
    fps["JP5"].Reference().SetPosition(mm(117.9, 134.62))  # clear of JP8
    for ref in list(TOP_TERMINALS) + list(BOT_TERMINALS):  # no room on the silk: pin labels identify them
        fps[ref].Reference().SetVisible(False)
    silk(board, "RS-485 / RS-232", 101.0, 110.9, 0.8, justify="left")
    silk(board, "MODE: open=485 fit=232", 117, 141.8, 0.8, justify="left")
    silk(board, "VIO 1-2 EXT 2-3 +5V", 116.8, 105.6, 0.8, justify="left")
    silk(board, "RS485_IO8 rev B", 138.5, 160.8, 0.9, angle=90)

    # isolation barrier: field side (J8/J9, series resistors, shunts, opto LEDs) below the line
    bx, by = BARRIER_X, BARRIER_Y
    line(board, bx, by, XR, by, pcbnew.Dwgs_User, 0.2)
    line(board, bx, by, bx, YB, pcbnew.Dwgs_User, 0.2)
    u6x, u7x = PLACE["U6"][0], PLACE["U7"][0]
    for (x1, x2) in ((bx, bx + 0.3), (u6x + 5.49, u7x - 5.51), (u7x + 5.49, XR - 0.5)):  # silk only between the optos
        line(board, x1, by, x2, by, pcbnew.F_SilkS, 0.2)
    line(board, bx, by, bx, 156.5, pcbnew.F_SilkS, 0.2)
    silk(board, "ISOLATED", 178.5, 145.3, 0.8)

    # ---- 4-layer stack-up: F.Cu signal | In1.Cu GND | In2.Cu power | B.Cu signal
    board.SetCopperLayerCount(4)
    ds = board.GetDesignSettings()
    ds.SetCopperLayerCount(4)
    # stack-up: KiCad applies its default 4-layer 1.6 mm stack-up (pcbnew Python cannot set it)
    board.SetLayerName(pcbnew.In1_Cu, "GND")
    board.SetLayerName(pcbnew.In2_Cu, "PWR")
    board.SetLayerType(pcbnew.In1_Cu, pcbnew.LT_POWER)
    board.SetLayerType(pcbnew.In2_Cu, pcbnew.LT_POWER)

    def zone(layer, net, pts, priority=0):
        z = pcbnew.ZONE(board)
        z.SetLayer(layer)
        z.SetNet(netinfo[net])
        z.SetAssignedPriority(priority)
        z.SetLocalClearance(pcbnew.FromMM(0.3))
        z.SetMinThickness(pcbnew.FromMM(0.25))
        ol = z.Outline()
        ol.NewOutline()
        for x, y in pts:
            ol.Append(pcbnew.FromMM(x), pcbnew.FromMM(y))
        board.Add(z)
        return z

    board_pts = [(X0, Y0), (XR, Y0), (XR, YB), (X0, YB)]
    zone(pcbnew.In1_Cu, "GND", board_pts)
    zone(pcbnew.In2_Cu, "+3V3", board_pts)
    # isolated field: no pours on any copper layer (keeps planes out of the 2.5 kV barrier)
    ka = pcbnew.ZONE(board)
    ka.SetIsRuleArea(True)
    ka.SetZoneName("ISOLATED_FIELD")
    ka.SetLayerSet(pcbnew.LSET.AllCuMask())
    ka.SetDoNotAllowZoneFills(True)
    ka.SetDoNotAllowTracks(False)
    ka.SetDoNotAllowVias(False)
    ka.SetDoNotAllowPads(False)
    ka.SetDoNotAllowFootprints(False)
    ol = ka.Outline()
    ol.NewOutline()
    for x, y in ((bx - 0.5, by - 0.5), (XR, by - 0.5), (XR, YB), (bx - 0.5, YB)):
        ol.Append(pcbnew.FromMM(x), pcbnew.FromMM(y))
    board.Add(ka)
    pcbnew.ZONE_FILLER(board).Fill(board.Zones())

    board.BuildConnectivity()
    pcbnew.SaveBoard(PCB, board)
    print(f"wrote {PCB}: {len(fps)} footprints, {len(nets)} nets")


if __name__ == "__main__":
    main()
