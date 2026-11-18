#!/usr/bin/env python3
"""Export BOM.csv (full) and BOM_JLCPCB.csv (JLCPCB assembly format) via kicad-cli.

If the kicad-mcp JLCPCB/LCSC SQLite snapshot exists, the stock and unit price
columns are filled from it (snapshot date is printed).
"""
import csv
import io
import json
import os
import sqlite3
import subprocess

HW = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
SCH = os.path.join(HW, "RS485_IO8.kicad_sch")
DB = os.path.expanduser("~/.local/share/kicad-mcp/jlcpcb_parts.db")

out = subprocess.run(
    ["kicad-cli", "sch", "export", "bom", "-o", "/dev/stdout",
     "--fields", "Reference,Value,Spec,Footprint,LCSC,MPN,Manufacturer,${QUANTITY}",
     "--labels", "Designator,Value,Spec,Footprint,LCSC,MPN,Manufacturer,Qty",
     "--group-by", "LCSC", "--ref-range-delimiter", "", SCH],
    check=True, capture_output=True, text=True).stdout
rows = [r for r in csv.DictReader(io.StringIO(out)) if r.get("Footprint")]  # drop kicad-cli status lines

db = sqlite3.connect(DB) if os.path.exists(DB) else None
for r in rows:
    r["Stock"], r["Unit price USD"], r["Library"] = "", "", ""
    if db:
        q = db.execute("select stock, price_json, library_type from components where lcsc=?", (r["LCSC"],)).fetchone()
        if q:
            r["Stock"] = q[0]
            try:
                r["Unit price USD"] = json.loads(q[1])[0].get("price", "")
            except Exception:
                pass
            r["Library"] = q[2]

cols = ["Designator", "Qty", "Value", "Spec", "Footprint", "LCSC", "MPN", "Manufacturer", "Library", "Stock",
        "Unit price USD"]
with open(os.path.join(HW, "BOM.csv"), "w", newline="") as f:
    w = csv.DictWriter(f, fieldnames=cols, extrasaction="ignore")
    w.writeheader()
    w.writerows(rows)
with open(os.path.join(HW, "BOM_JLCPCB.csv"), "w", newline="") as f:
    w = csv.writer(f)
    w.writerow(["Comment", "Designator", "Footprint", "LCSC Part #"])
    for r in rows:
        w.writerow([r["MPN"], r["Designator"], r["Footprint"].split(":")[-1], r["LCSC"]])

total = sum(int(r["Qty"]) for r in rows)
print(f"{len(rows)} line items, {total} parts")
for r in rows:
    flag = "" if r["Stock"] == "" or int(r["Stock"]) >= 10 * int(r["Qty"]) else "   <-- low stock"
    print(f'{r["LCSC"]:>10} x{r["Qty"]:>3} {r["MPN"]:<26} stock {r["Stock"]:>9} {r["Library"]:<9}{flag}')
