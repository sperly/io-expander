"""Minimal S-expression reader/writer for KiCad files (no external deps)."""
import re

_TOKEN = re.compile(r'\s*(?:(\()|(\))|("(?:[^"\\]|\\.)*")|([^\s()"]+))')


class Sym(str):
    """Bare (unquoted) atom."""


def parse(text):
    stack, cur = [], []
    pos = 0
    n = len(text)
    while pos < n:
        m = _TOKEN.match(text, pos)
        if not m:
            if text[pos:].strip() == "":
                break
            raise ValueError(f"parse error at {pos}: {text[pos:pos+40]!r}")
        pos = m.end()
        lp, rp, qs, atom = m.groups()
        if lp:
            stack.append(cur)
            cur = []
        elif rp:
            done = cur
            cur = stack.pop()
            cur.append(done)
        elif qs is not None:
            cur.append(qs[1:-1].replace('\\"', '"').replace("\\\\", "\\"))
        elif atom is not None:
            cur.append(Sym(atom))
    return cur[0] if len(cur) == 1 else cur


def _fmt_atom(a):
    if isinstance(a, Sym):
        return str(a)
    if isinstance(a, bool):
        return "yes" if a else "no"
    if isinstance(a, int):
        return str(a)
    if isinstance(a, float):
        s = f"{a:.4f}".rstrip("0").rstrip(".")
        return "0" if s in ("-0", "") else s
    s = str(a).replace("\\", "\\\\").replace('"', '\\"').replace("\n", "\\n")
    return f'"{s}"'


def dump(e, indent=0):
    if not isinstance(e, list):
        return _fmt_atom(e)
    if not e:
        return "()"
    simple = all(not isinstance(x, list) for x in e)
    if simple:
        return "(" + " ".join(_fmt_atom(x) for x in e) + ")"
    pad = "\t" * (indent + 1)
    parts = []
    head = []
    for x in e:
        if not isinstance(x, list) and not parts:
            head.append(_fmt_atom(x))
        else:
            parts.append(x)
    out = "(" + " ".join(head)
    for p in parts:
        out += "\n" + pad + dump(p, indent + 1)
    out += "\n" + "\t" * indent + ")"
    return out


def find(e, key):
    """Return children lists whose head is key."""
    return [x for x in e if isinstance(x, list) and x and x[0] == key]


def find1(e, key):
    r = find(e, key)
    return r[0] if r else None
