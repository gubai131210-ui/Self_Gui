# -*- coding: utf-8 -*-
"""Verify every VisionNodeIds type has a matching SVG and qrc entry."""
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[1]
IDS_H = ROOT / "vision" / "registry" / "visionnodeids.h"
VISION = ROOT / "resources" / "icons" / "vision"
QRC = ROOT / "resources" / "selt.qrc"

ids_text = IDS_H.read_text(encoding="utf-8")
type_ids = [
    m for m in re.findall(r'QStringLiteral\("([^"]+)"\)', ids_text)
    if m.startswith("vision.") and m != "vision."
]
# drop the prefix check literal "vision."
type_ids = [t for t in type_ids if len(t) > len("vision.")]

qrc = QRC.read_text(encoding="utf-8")
missing_svg = []
missing_qrc = []
for tid in sorted(set(type_ids)):
    stem = tid.replace(".", "_")
    svg = VISION / f"{stem}.svg"
    if not svg.exists():
        missing_svg.append(stem)
    entry = f"icons/vision/{stem}.svg"
    if entry not in qrc:
        missing_qrc.append(stem)

cats = [
    "cat_input", "cat_preprocess", "cat_region", "cat_locate", "cat_measure",
    "cat_decision", "cat_logic", "cat_output", "cat_template", "cat_plugin",
]
missing_cat = [c for c in cats if not (VISION / f"{c}.svg").exists()]

ok = not missing_svg and not missing_qrc and not missing_cat
print(f"operators checked: {len(set(type_ids))}")
print(f"missing svg: {missing_svg or 'none'}")
print(f"missing qrc: {missing_qrc or 'none'}")
print(f"missing categories: {missing_cat or 'none'}")
sys.exit(0 if ok else 1)
