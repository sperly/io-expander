"""Load symbols from KiCad .kicad_sym libraries, flatten `extends`, report pins."""
import copy
import os

from sexp import Sym, find, find1, parse

STOCK = os.environ.get("KICAD_SYMBOL_DIR", "/usr/share/kicad/symbols")
_cache = {}


def _load_lib(path):
    if path not in _cache:
        with open(path, encoding="utf-8") as f:
            _cache[path] = parse(f.read())
    return _cache[path]


def _top_symbols(lib):
    return {s[1]: s for s in find(lib, "symbol")}


def _rename_units(sym, old, new):
    for sub in find(sym, "symbol"):
        if sub[1].startswith(old + "_"):
            sub[1] = new + sub[1][len(old):]


def get_symbol(lib_nick, name, path=None):
    """Return a flattened deep copy named 'lib_nick:name' suitable for lib_symbols."""
    path = path or os.path.join(STOCK, f"{lib_nick}.kicad_sym")
    syms = _top_symbols(_load_lib(path))
    s = copy.deepcopy(syms[name])
    ext = find1(s, "extends")
    if ext:
        parent = get_symbol(lib_nick, ext[1], path)
        pname = parent[1].split(":", 1)[1]
        merged = [x for x in parent if not (isinstance(x, list) and x and x[0] in ("property", "symbol"))]
        merged[1] = name
        # properties of the child override the parent
        props = {p[1]: p for p in find(parent, "property")}
        for p in find(s, "property"):
            props[p[1]] = p
        for k in ("pin_names", "pin_numbers", "exclude_from_sim", "in_bom", "on_board", "power"):
            c = find1(s, k)
            if c:
                merged = [x for x in merged if not (isinstance(x, list) and x and x[0] == k)]
                merged.insert(2, c)
        merged += list(props.values())
        units = copy.deepcopy(find(parent, "symbol"))
        for u in units:
            u[1] = name + u[1][len(pname):]
        merged += units
        s = merged
    s[1] = f"{lib_nick}:{name}"
    return s


def pins(sym, unit=1):
    """List of dicts: number, name, x, y, angle, length, type (common unit 0 + `unit`, body style 1)."""
    out = []
    base = sym[1].split(":", 1)[-1]
    for sub in find(sym, "symbol"):
        suffix = sub[1][len(base):]  # _U_B
        try:
            _, unit_s, style = suffix.split("_")
        except ValueError:
            continue
        if unit_s not in ("0", str(unit)) or style not in ("0", "1"):
            continue
        for p in find(sub, "pin"):
            at = find1(p, "at")
            out.append(
                dict(
                    type=str(p[1]),
                    number=find1(p, "number")[1],
                    name=find1(p, "name")[1],
                    x=float(at[1]),
                    y=float(at[2]),
                    angle=float(at[3]) if len(at) > 3 else 0.0,
                    length=float(find1(p, "length")[1]),
                )
            )
    return out


def prop(sym, key):
    for p in find(sym, "property"):
        if p[1] == key:
            return p
    return None


if __name__ == "__main__":
    import sys

    s = get_symbol(sys.argv[1], sys.argv[2])
    for p in sorted(pins(s), key=lambda p: (p["x"], -p["y"])):
        print(f'{p["number"]:>4} {p["name"]:<24} ({p["x"]:7.2f},{p["y"]:7.2f}) a={p["angle"]:5.0f} {p["type"]}')
    fp = prop(s, "Footprint")
    print("Footprint:", fp[2] if fp else None)
