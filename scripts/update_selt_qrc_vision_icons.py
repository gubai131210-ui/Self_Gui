# -*- coding: utf-8 -*-
"""Insert vision/*.svg entries into resources/selt.qrc."""
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
QRC = ROOT / "resources" / "selt.qrc"
VISION = ROOT / "resources" / "icons" / "vision"

svgs = sorted(p.name for p in VISION.glob("*.svg"))
entries = "\n".join(f"        <file>icons/vision/{name}</file>" for name in svgs)

text = QRC.read_text(encoding="utf-8")
marker_start = "        <!-- VISION_ICONS_BEGIN -->"
marker_end = "        <!-- VISION_ICONS_END -->"
block = f"{marker_start}\n{entries}\n{marker_end}"

if marker_start in text and marker_end in text:
    before, rest = text.split(marker_start, 1)
    _, after = rest.split(marker_end, 1)
    text = before + block + after
else:
    # Insert before theme files
    needle = "        <file>theme/theme_light.qss</file>"
    if needle not in text:
        raise SystemExit("cannot find insertion point in selt.qrc")
    text = text.replace(needle, block + "\n" + needle, 1)

QRC.write_text(text, encoding="utf-8")
print(f"registered {len(svgs)} vision icons in {QRC}")
