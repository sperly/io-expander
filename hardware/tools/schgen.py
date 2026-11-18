"""Tiny schematic-construction layer that writes KiCad 9/10 .kicad_sch files.

Coordinates are millimetres in sheet space (Y grows downward), as in KiCad.
"""
import math
import uuid as _uuid

import symlib
from sexp import Sym, dump, find, find1

PROJECT = "RS485_IO8"
FONT = [Sym("font"), [Sym("size"), 1.27, 1.27]]


_NS = _uuid.UUID("6f1f4f3e-9a52-4c8e-9a39-1d0e10a8b7c2")
_rng = __import__("random").Random(0x10E8)


def uid():
    """Deterministic pseudo-random UUIDs: regenerating gives identical files."""
    return str(_uuid.UUID(int=_rng.getrandbits(128), version=4))


def stable_uid(key):
    """UUID derived from a name: symbols keep their identity (PCB link) across edits."""
    return str(_uuid.uuid5(_NS, key))


def snap(v):
    return round(round(v / 0.01) * 0.01, 2)


def P(x, y):
    return (snap(x), snap(y))


class Part:
    def __init__(self, sheet, sym, ref, value, x, y, rot, mirror, fields, text, unit=1):
        self.sheet, self.sym, self.ref, self.value = sheet, sym, ref, value
        self.x, self.y, self.rot, self.mirror = snap(x), snap(y), rot, mirror
        self.fields = fields
        self.text = text  # dict of property -> (dx, dy, justify) overrides
        self.uuid = stable_uid(f"{sheet.filename}:{ref}:{unit}") if not ref.startswith("#") else uid()
        self.unit = unit
        self._pins = {}
        for p in symlib.pins(sym, unit):
            self._pins.setdefault(p["number"], []).append(p)

    def xf(self, px, py):
        """Library (Y-up) coordinates -> sheet coordinates."""
        if self.mirror == "y":
            px = -px
        elif self.mirror == "x":
            py = -py
        a = math.radians(self.rot)
        rx = px * math.cos(a) - py * math.sin(a)
        ry = px * math.sin(a) + py * math.cos(a)
        return P(self.x + rx, self.y - ry)

    def pin(self, num):
        p = self._pins[str(num)][0]
        return self.xf(p["x"], p["y"])

    def pin_numbers(self):
        return list(self._pins.keys())

    def pin_type(self, num):
        return self._pins[str(num)][0]["type"]

    __getitem__ = pin


class Sheet:
    def __init__(self, filename, title, page, paper="A3"):
        self.filename, self.title, self.page, self.paper = filename, title, page, paper
        self.uuid = stable_uid("sheet:" + filename)
        self.parts, self.wires, self.labels, self.nocons = [], [], [], []
        self.texts, self.rects, self.sheets = [], [], []
        self.libsyms = {}
        self.path = None  # instance path, set by Project

    # ---------------------------------------------------------------- placing
    def add(self, lib, name, ref, value, x, y, rot=0, mirror=None, path=None, text=None, unit=1, **fields):
        key = f"{lib}:{name}"
        if key not in self.libsyms:
            self.libsyms[key] = symlib.get_symbol(lib, name, path)
        part = Part(self, self.libsyms[key], ref, value, x, y, rot, mirror, fields, text or {}, unit)
        self.parts.append(part)
        return part

    def wire(self, *pts):
        pts = [P(*p) for p in pts]
        for a, b in zip(pts, pts[1:]):
            if a != b:
                assert a[0] == b[0] or a[1] == b[1], f"non-orthogonal wire {a}->{b}"
                self.wires.append((a, b))

    def route(self, a, b, first="h"):
        """L-shaped connection between two points."""
        a, b = P(*a), P(*b)
        if a[0] == b[0] or a[1] == b[1]:
            self.wire(a, b)
        elif first == "h":
            self.wire(a, (b[0], a[1]), b)
        else:
            self.wire(a, (a[0], b[1]), b)

    def label(self, at, name, angle=0, kind="global", shape="bidirectional"):
        self.labels.append(dict(at=P(*at), name=name, angle=angle, kind=kind, shape=shape))

    def stub(self, at, name, direction="right", length=5.08, kind="global", shape="bidirectional"):
        """Wire stub from a pin end and a label at its far end."""
        dx, dy, ang = {"right": (1, 0, 0), "left": (-1, 0, 180), "up": (0, -1, 90), "down": (0, 1, 270)}[direction]
        end = P(at[0] + dx * length, at[1] + dy * length)
        self.wire(at, end)
        self.label(end, name, ang, kind, shape)
        return end

    def power(self, name, at, rot=0, lib="power", path=None):
        key = f"{lib}:{name}"
        if key not in self.libsyms:
            self.libsyms[key] = symlib.get_symbol(lib, name, path)
        part = Part(self, self.libsyms[key], "#PWR?", name, at[0], at[1], rot, None, {}, {})
        part.is_power = True
        self.parts.append(part)
        return part

    def nc(self, at):
        self.nocons.append(P(*at))

    def note(self, at, text, size=1.27, bold=False):
        self.texts.append(dict(at=P(*at), text=text, size=size, bold=bold))

    def table(self, at, rows, widths, size=1.27, line=None):
        """Column-aligned text: rows is a list of tuples, widths in mm per column."""
        line = line or size * 1.6
        x0, y0 = at
        for r, row in enumerate(rows):
            x = x0
            for c, cell in enumerate(row):
                if cell:
                    self.note((x, y0 + r * line), cell, size=size, bold=(r == 0))
                x += widths[c]

    def frame(self, x1, y1, x2, y2, title=None):
        self.rects.append((P(x1, y1), P(x2, y2)))
        if title:
            self.note((x1 + 1.27, y1 + 1.27), title, size=2.0, bold=True)

    # ------------------------------------------------------------ connectivity
    def _points(self):
        """All electrically relevant points (pin ends, wire ends, labels)."""
        pts = []
        for part in self.parts:
            for n in part.pin_numbers():
                pts.append(part.pin(n))
        for a, b in self.wires:
            pts += [a, b]
        pts += [l["at"] for l in self.labels]
        return pts

    def junctions(self):
        ends = {}
        for a, b in self.wires:
            for p in (a, b):
                ends[p] = ends.get(p, 0) + 1
        pinpts = {}
        for part in self.parts:
            for n in part.pin_numbers():
                pp = part.pin(n)
                pinpts[pp] = pinpts.get(pp, 0) + 1
        out = set()
        for p, n in ends.items():
            if n + pinpts.get(p, 0) >= 3:
                out.add(p)
        # T-junctions: a wire endpoint (or pin) on the interior of another wire
        cand = set(ends) | set(pinpts)
        for a, b in self.wires:
            for p in cand:
                if p in (a, b):
                    continue
                if a[0] == b[0] == p[0] and min(a[1], b[1]) < p[1] < max(a[1], b[1]):
                    out.add(p)
                elif a[1] == b[1] == p[1] and min(a[0], b[0]) < p[0] < max(a[0], b[0]):
                    out.add(p)
        return sorted(out)

    def split_wires(self):
        """Split wires at T-junction points so every connection is at an endpoint."""
        js = set(self.junctions())
        new = []
        for a, b in self.wires:
            inner = []
            for p in js:
                if a[0] == b[0] == p[0] and min(a[1], b[1]) < p[1] < max(a[1], b[1]):
                    inner.append(p)
                elif a[1] == b[1] == p[1] and min(a[0], b[0]) < p[0] < max(a[0], b[0]):
                    inner.append(p)
            seq = sorted([a, b] + inner, key=lambda q: (q[0] - a[0]) ** 2 + (q[1] - a[1]) ** 2)
            new += list(zip(seq, seq[1:]))
        self.wires = new
        return sorted(js)

    # ------------------------------------------------------------------ output
    def _prop(self, key, val, at, hide=False, justify=None, angle=0):
        eff = [Sym("effects"), FONT]
        if justify:
            eff.append([Sym("justify")] + [Sym(j) for j in justify.split()])
        if hide:
            eff.append([Sym("hide"), Sym("yes")])
        return [Sym("property"), key, val, [Sym("at"), at[0], at[1], angle], eff]

    def _text_pos(self, part, key):
        if key in part.text:
            dx, dy, just = part.text[key]
            return P(part.x + dx, part.y + dy), just
        lp = symlib.prop(part.sym, key)
        at = find1(lp, "at")
        x, y = part.xf(float(at[1]), float(at[2]))
        just = None
        eff = find1(lp, "effects")
        j = find1(eff, "justify") if eff else None
        if j:
            just = " ".join(str(v) for v in j[1:])
        return (x, y), just

    def _symbol_sexpr(self, part, project, path):
        is_power = getattr(part, "is_power", False)
        e = [
            Sym("symbol"),
            [Sym("lib_id"), part.sym[1]],
            [Sym("at"), part.x, part.y, part.rot],
        ]
        if part.mirror:
            e.append([Sym("mirror"), Sym(part.mirror)])
        e += [
            [Sym("unit"), part.unit],
            [Sym("exclude_from_sim"), Sym("no")],
            [Sym("in_bom"), Sym("no" if is_power or part.fields.get("_nobom") else "yes")],
            [Sym("on_board"), Sym("yes")],
            [Sym("dnp"), Sym("no")],
            [Sym("uuid"), part.uuid],
        ]
        # field angle: KiCad renders field text relative to the symbol rotation,
        # so a 90/270 degree symbol needs a 90 degree field to read horizontally.
        fang = 90 if part.rot in (90, 270) else 0

        def _j(just):
            # for 90/270 degree symbols KiCad mirrors left/right justification
            if just and fang:
                return " ".join({"left": "right", "right": "left"}.get(w, w) for w in just.split())
            return just
        at, just = self._text_pos(part, "Reference")
        e.append(self._prop("Reference", part.ref, at, hide=is_power or part.fields.get("_hideref", False),
                            justify=_j(just), angle=fang))
        at, just = self._text_pos(part, "Value")
        e.append(self._prop("Value", part.value, at, hide=part.fields.get("_hideval", False), justify=_j(just),
                            angle=fang))
        fp = part.fields.get("Footprint")
        if fp is None:
            lp = symlib.prop(part.sym, "Footprint")
            fp = lp[2] if lp else ""
        e.append(self._prop("Footprint", fp, (part.x, part.y), hide=True))
        ds = part.fields.get("Datasheet")
        if ds is None:
            lp = symlib.prop(part.sym, "Datasheet")
            ds = lp[2] if lp else ""
        e.append(self._prop("Datasheet", ds, (part.x, part.y), hide=True))
        for k, v in part.fields.items():
            if k.startswith("_") or k in ("Footprint", "Datasheet"):
                continue
            e.append(self._prop(k, v, (part.x, part.y), hide=True))
        for n in part.pin_numbers():
            e.append([Sym("pin"), n, [Sym("uuid"), uid()]])
        e.append(
            [Sym("instances"), [Sym("project"), project, [Sym("path"), path, [Sym("reference"), part.ref], [Sym("unit"), part.unit]]]]
        )
        return e

    def render(self, project, root_uuid):
        path = "/" + root_uuid if self.path is None else self.path
        js = self.split_wires()
        e = [
            Sym("kicad_sch"),
            [Sym("version"), 20250114],
            [Sym("generator"), "eeschema"],
            [Sym("generator_version"), "9.0"],
            [Sym("uuid"), self.uuid],
            [Sym("paper"), self.paper],
            [
                Sym("title_block"),
                [Sym("title"), self.title],
                [Sym("date"), "2026-11-17"],
                [Sym("rev"), "B"],
                [Sym("company"), "Serial configurable 8-channel logic I/O"],
                [Sym("comment"), 1, "RS-485 / RS-232 (strap selectable) - LCSC sourced parts"],
                [Sym("comment"), 2, "Generated by tools/gen_schematic.py - edit the script, not this file"],
            ],
            [Sym("lib_symbols")] + list(self.libsyms.values()),
        ]
        for p in js:
            e.append([Sym("junction"), [Sym("at"), p[0], p[1]], [Sym("diameter"), 0], [Sym("color"), 0, 0, 0, 0], [Sym("uuid"), uid()]])
        for p in self.nocons:
            e.append([Sym("no_connect"), [Sym("at"), p[0], p[1]], [Sym("uuid"), uid()]])
        for a, b in self.wires:
            e.append(
                [
                    Sym("wire"),
                    [Sym("pts"), [Sym("xy"), a[0], a[1]], [Sym("xy"), b[0], b[1]]],
                    [Sym("stroke"), [Sym("width"), 0], [Sym("type"), Sym("default")]],
                    [Sym("uuid"), uid()],
                ]
            )
        for (x1, y1), (x2, y2) in self.rects:
            e.append(
                [
                    Sym("rectangle"),
                    [Sym("start"), x1, y1],
                    [Sym("end"), x2, y2],
                    [Sym("stroke"), [Sym("width"), 0.254], [Sym("type"), Sym("dash")], [Sym("color"), 132, 132, 132, 1]],
                    [Sym("fill"), [Sym("type"), Sym("none")]],
                    [Sym("uuid"), uid()],
                ]
            )
        for t in self.texts:
            font = [Sym("font"), [Sym("size"), t["size"], t["size"]]]
            if t["bold"]:
                font.append([Sym("bold"), Sym("yes")])
            e.append(
                [
                    Sym("text"),
                    t["text"],
                    [Sym("exclude_from_sim"), Sym("no")],
                    [Sym("at"), t["at"][0], t["at"][1], 0],
                    [Sym("effects"), font, [Sym("justify"), Sym("left"), Sym("top")]],
                    [Sym("uuid"), uid()],
                ]
            )
        for l in self.labels:
            ang = l["angle"]
            just = {0: "left", 90: "left", 180: "right", 270: "right"}[ang]
            if l["kind"] == "global":
                e.append(
                    [
                        Sym("global_label"),
                        l["name"],
                        [Sym("shape"), Sym(l["shape"])],
                        [Sym("at"), l["at"][0], l["at"][1], ang],
                        [Sym("fields_autoplaced"), Sym("yes")],
                        [Sym("effects"), FONT, [Sym("justify"), Sym(just)]],
                        [Sym("uuid"), uid()],
                        [
                            Sym("property"),
                            "Intersheetrefs",
                            "${INTERSHEET_REFS}",
                            [Sym("at"), l["at"][0], l["at"][1], 0],
                            [Sym("effects"), FONT, [Sym("hide"), Sym("yes")]],
                        ],
                    ]
                )
            else:
                e.append(
                    [
                        Sym("label"),
                        l["name"],
                        [Sym("at"), l["at"][0], l["at"][1], ang],
                        [Sym("effects"), FONT, [Sym("justify"), Sym(just), Sym("bottom")]],
                        [Sym("uuid"), uid()],
                    ]
                )
        for part in self.parts:
            e.append(self._symbol_sexpr(part, project, path))
        for sh in self.sheets:
            e.append(sh)
        return e

    def write(self, directory, project, root_uuid, extra=None):
        e = self.render(project, root_uuid)
        if extra:
            e += extra
        with open(f"{directory}/{self.filename}", "w", encoding="utf-8") as f:
            f.write(dump(e) + "\n")


class Project:
    def __init__(self, name, root):
        self.name, self.root = name, root
        self.subs = []

    def add_sheet(self, sheet, at, size):
        sheet.path = f"/{self.root.uuid}/{(su := stable_uid('instance:' + sheet.filename))}"
        self.subs.append((sheet, su))
        x, y = at
        w, h = size
        self.root.sheets.append(
            [
                Sym("sheet"),
                [Sym("at"), x, y],
                [Sym("size"), w, h],
                [Sym("exclude_from_sim"), Sym("no")],
                [Sym("in_bom"), Sym("yes")],
                [Sym("on_board"), Sym("yes")],
                [Sym("dnp"), Sym("no")],
                [Sym("fields_autoplaced"), Sym("yes")],
                [Sym("stroke"), [Sym("width"), 0.1524], [Sym("type"), Sym("solid")]],
                [Sym("fill"), [Sym("color"), 0, 0, 0, 0.0]],
                [Sym("uuid"), su],
                [Sym("property"), "Sheetname", sheet.title, [Sym("at"), x, snap(y - 0.71), 0],
                 [Sym("effects"), FONT, [Sym("justify"), Sym("left"), Sym("bottom")]]],
                [Sym("property"), "Sheetfile", sheet.filename, [Sym("at"), x, snap(y + h + 0.59), 0],
                 [Sym("effects"), FONT, [Sym("justify"), Sym("left"), Sym("top")], [Sym("hide"), Sym("yes")]]],
                [Sym("instances"), [Sym("project"), self.name, [Sym("path"), f"/{self.root.uuid}", [Sym("page"), str(sheet.page)]]]],
            ]
        )

    def annotate_power(self):
        n = 1
        for sh in [self.root] + [s for s, _ in self.subs]:
            for p in sh.parts:
                if getattr(p, "is_power", False):
                    p.ref = f"#PWR{n:03d}"
                    n += 1

    def write(self, directory):
        self.annotate_power()
        extra = [[Sym("sheet_instances"), [Sym("path"), "/", [Sym("page"), "1"]]]]
        self.root.write(directory, self.name, self.root.uuid, extra)
        for sh, _ in self.subs:
            sh.write(directory, self.name, self.root.uuid)
