# -*- coding: utf-8 -*-
"""Generate original Selt vision operator SVG icons (project-owned stroke style)."""
from pathlib import Path

OUT = Path(__file__).resolve().parents[1] / "resources" / "icons" / "vision"
OUT.mkdir(parents=True, exist_ok=True)

HDR = """<?xml version="1.0" encoding="UTF-8"?>
<svg xmlns="http://www.w3.org/2000/svg" width="24" height="24" viewBox="0 0 24 24" fill="none" stroke="#2c3e50" stroke-width="1.8" stroke-linecap="round" stroke-linejoin="round">"""
FTR = "</svg>\n"


def write(name: str, body: str) -> None:
    (OUT / f"{name}.svg").write_text(HDR + "\n" + body + "\n" + FTR, encoding="utf-8")


# Category glyphs (Chinese keys used by toolbox / fallback)
write("cat_input", '<rect x="4" y="6" width="16" height="12" rx="2"/><path d="M9 12h6M12 9v6"/>')
write(
    "cat_preprocess",
    '<circle cx="12" cy="12" r="3"/>'
    '<path d="M12 3v2M12 19v2M3 12h2M19 12h2M5.6 5.6l1.4 1.4M17 17l1.4 1.4M5.6 18.4l1.4-1.4M17 7l1.4-1.4"/>',
)
write("cat_region", '<path d="M5 8l7-3 7 3v8l-7 3-7-3z"/><path d="M12 5v14"/>')
write(
    "cat_locate",
    '<circle cx="12" cy="12" r="7"/><circle cx="12" cy="12" r="2"/>'
    '<path d="M12 2v3M12 19v3M2 12h3M19 12h3"/>',
)
write(
    "cat_measure",
    '<path d="M4 18V6h3v12H4z"/><path d="M4 9h3M4 12h3M4 15h3"/>'
    '<path d="M10 18h10M10 6h10M10 6v12"/>',
)
write("cat_decision", '<path d="M12 3l8 8-8 8-8-8z"/>')
write(
    "cat_logic",
    '<rect x="3" y="8" width="6" height="8" rx="1"/>'
    '<rect x="15" y="8" width="6" height="8" rx="1"/><path d="M9 12h6"/>',
)
write(
    "cat_output",
    '<rect x="4" y="5" width="16" height="12" rx="2"/>'
    '<path d="M8 17v2h8v-2"/><circle cx="12" cy="11" r="2.5"/>',
)
write(
    "cat_template",
    '<rect x="7" y="7" width="12" height="12" rx="1.5"/>'
    '<rect x="4" y="4" width="12" height="12" rx="1.5"/>',
)
write(
    "cat_plugin",
    '<path d="M9 3v4M15 3v4"/><rect x="5" y="7" width="14" height="11" rx="2"/>'
    '<path d="M9 18v3M15 18v3"/>',
)

# Input / template
write(
    "vision_imageLoader",
    '<rect x="3" y="5" width="18" height="14" rx="2"/>'
    '<circle cx="9" cy="11" r="2"/><path d="M3 16l5-4 4 3 3-2 6 4"/>',
)
write(
    "vision_templateSource",
    '<rect x="8" y="8" width="12" height="12" rx="1.5"/>'
    '<rect x="4" y="4" width="12" height="12" rx="1.5"/><path d="M8 12h4M10 10v4"/>',
)
write(
    "vision_templateMatch",
    '<rect x="3" y="4" width="11" height="11" rx="1.5"/>'
    '<rect x="12" y="11" width="9" height="9" rx="1.5"/><path d="M7 9h3M8.5 7.5v3"/>',
)
write(
    "vision_templateMatchMulti",
    '<rect x="3" y="3" width="8" height="8" rx="1"/>'
    '<rect x="13" y="3" width="8" height="8" rx="1"/>'
    '<rect x="8" y="13" width="8" height="8" rx="1"/>'
    '<circle cx="7" cy="7" r="1.2" fill="#2c3e50" stroke="none"/>'
    '<circle cx="17" cy="7" r="1.2" fill="#2c3e50" stroke="none"/>'
    '<circle cx="12" cy="17" r="1.2" fill="#2c3e50" stroke="none"/>',
)

# Preprocess
write(
    "vision_grayscale",
    '<rect x="4" y="5" width="7" height="14" rx="1"/>'
    '<rect x="13" y="5" width="7" height="14" rx="1"/>'
    '<path d="M7.5 8v8M17.5 8v8" opacity="0.35"/>',
)
write(
    "vision_threshold",
    '<rect x="4" y="5" width="16" height="14" rx="2"/>'
    '<path d="M4 14h16"/><path d="M12 5v14"/>',
)
write(
    "vision_otsuThreshold",
    '<rect x="4" y="5" width="16" height="14" rx="2"/>'
    '<path d="M4 13h16"/><path d="M8 9h2M14 9h2"/><path d="M12 5v8"/>',
)
write(
    "vision_adaptiveThreshold",
    '<rect x="3" y="5" width="18" height="14" rx="2"/>'
    '<path d="M3 10h6M9 14h6M15 11h6"/>',
)
write(
    "vision_blur",
    '<circle cx="9" cy="12" r="5"/><circle cx="15" cy="12" r="5" opacity="0.55"/>',
)
write(
    "vision_gaussianBlur",
    '<path d="M4 16c2-8 14-8 16 0"/><path d="M6 12c1.5-4 10.5-4 12 0"/>'
    '<circle cx="12" cy="8" r="1.5" fill="#2c3e50" stroke="none"/>',
)
write(
    "vision_medianBlur",
    '<path d="M5 17V7h3v10H5zM10.5 17V10h3v7h-3zM16 17V5h3v12h-3z"/>',
)
write(
    "vision_bilateralFilter",
    '<circle cx="9" cy="12" r="4"/><circle cx="15" cy="12" r="4"/><path d="M9 12h6"/>',
)
write("vision_canny", '<path d="M4 16l4-8 3 5 3-7 6 10"/>')
write("vision_sobel", '<path d="M5 18V6M5 12h4"/><path d="M12 18V6l7 6-7 6"/>')
write(
    "vision_morphology",
    '<rect x="4" y="4" width="7" height="7" rx="1"/>'
    '<rect x="13" y="4" width="7" height="7" rx="1"/>'
    '<rect x="4" y="13" width="7" height="7" rx="1"/>'
    '<circle cx="16.5" cy="16.5" r="3.2"/>',
)
write(
    "vision_resize",
    '<rect x="4" y="8" width="8" height="8" rx="1"/>'
    '<rect x="12" y="4" width="8" height="12" rx="1"/>'
    '<path d="M14 14l4 4M18 14v4h-4"/>',
)
write(
    "vision_colorConvert",
    '<circle cx="9" cy="10" r="4"/><circle cx="15" cy="10" r="4"/>'
    '<circle cx="12" cy="15" r="4"/>',
)
write(
    "vision_geometricTransform",
    '<rect x="5" y="7" width="10" height="10" rx="1"/>'
    '<path d="M9 5l8 3-3 8"/><path d="M15 8l3-1"/>',
)

# Region
write("vision_maskCombine", '<circle cx="10" cy="12" r="6"/><circle cx="14" cy="12" r="6"/>')
write("vision_findContours", '<path d="M5 16c2-8 5-10 7-6s4 8 7 4"/><path d="M5 16h14"/>')
write(
    "vision_connectedComponents",
    '<rect x="3" y="4" width="7" height="7" rx="1"/>'
    '<rect x="14" y="4" width="7" height="7" rx="1"/>'
    '<rect x="8" y="13" width="8" height="7" rx="1"/>',
)
write(
    "vision_regionFill",
    '<rect x="5" y="5" width="14" height="14" rx="2"/>'
    '<path d="M8 12h8M12 8v8" stroke-width="2.2"/>',
)
write(
    "vision_blobAnalyze",
    '<circle cx="8" cy="10" r="3"/><circle cx="16" cy="9" r="2.2"/>'
    '<circle cx="13" cy="16" r="3.5"/><path d="M5 20h14"/>',
)

# Locate
write(
    "vision_houghLines",
    '<path d="M4 18L20 6"/><path d="M5 8L15 20"/>'
    '<circle cx="11" cy="12" r="1.5" fill="#2c3e50" stroke="none"/>',
)
write(
    "vision_houghCircles",
    '<circle cx="12" cy="12" r="7"/><circle cx="12" cy="12" r="3"/>'
    '<path d="M12 2v2M12 20v2"/>',
)
write(
    "vision_blobLocate",
    '<circle cx="12" cy="12" r="4"/>'
    '<path d="M12 3v3M12 18v3M3 12h3M18 12h3"/>'
    '<circle cx="12" cy="12" r="1.2" fill="#2c3e50" stroke="none"/>',
)
write(
    "vision_featureMatch",
    '<path d="M6 8l2 2-2 2-2-2z"/><path d="M16 6l2 2-2 2-2-2z"/>'
    '<path d="M14 14l3 1-1 3-3-1z"/><path d="M8 10l6-2M9 12l5 3"/>',
)
write(
    "vision_subpixelRefine",
    '<rect x="5" y="5" width="14" height="14" rx="1"/>'
    '<path d="M5 12h14M12 5v14"/><circle cx="12" cy="12" r="2"/>',
)

# Measure
write(
    "vision_rectangleMeasure",
    '<rect x="5" y="6" width="14" height="12" rx="1"/>'
    '<path d="M5 6h14M5 18h14"/><path d="M8 9v6M16 9v6"/>',
)
write(
    "vision_circleMeasure",
    '<circle cx="12" cy="12" r="7"/>'
    '<path d="M12 5v14M5 12h14"/><path d="M12 12l5-3"/>',
)
write("vision_lineMeasure", '<path d="M5 17L19 7"/><path d="M5 17h4M19 7h-4"/>')
write(
    "vision_distanceMeasure",
    '<path d="M5 8v8M19 8v8"/><path d="M5 12h14"/>'
    '<path d="M8 10l-3 2 3 2M16 10l3 2-3 2"/>',
)
write(
    "vision_angleMeasure",
    '<path d="M5 18V6"/><path d="M5 18h14"/><path d="M5 18c6 0 10-6 10-12"/>',
)
write(
    "vision_parallelDistance",
    '<path d="M5 8h14M5 16h14"/><path d="M12 8v8"/><path d="M10 11l2-3 2 3"/>',
)
write(
    "vision_caliperMeasure",
    '<path d="M4 12h16"/><path d="M7 7v10M17 7v10"/>'
    '<path d="M7 9h2M7 15h2M15 9h2M15 15h2"/>',
)
write(
    "vision_fitCircle",
    '<circle cx="12" cy="12" r="6"/>'
    '<circle cx="8" cy="9" r="1" fill="#2c3e50" stroke="none"/>'
    '<circle cx="16" cy="10" r="1" fill="#2c3e50" stroke="none"/>'
    '<circle cx="10" cy="16" r="1" fill="#2c3e50" stroke="none"/>'
    '<circle cx="15" cy="15" r="1" fill="#2c3e50" stroke="none"/>',
)
write(
    "vision_fitLine",
    '<path d="M4 16L20 8"/>'
    '<circle cx="7" cy="14.5" r="1" fill="#2c3e50" stroke="none"/>'
    '<circle cx="12" cy="12" r="1" fill="#2c3e50" stroke="none"/>'
    '<circle cx="17" cy="9.5" r="1" fill="#2c3e50" stroke="none"/>',
)
write(
    "vision_measureStatistics",
    '<path d="M5 19V9h3v10H5zM10.5 19v-7h3v7h-3zM16 19V5h3v14h-3z"/>'
    '<path d="M4 19h16"/>',
)
write(
    "vision_rangeDecision",
    '<path d="M4 12h16"/><path d="M8 8v8M16 8v8"/>'
    '<circle cx="12" cy="12" r="2" fill="#2c3e50" stroke="none"/>',
)
write(
    "vision_toleranceDecision",
    '<path d="M12 4l8 14H4z"/><path d="M12 10v4"/>'
    '<circle cx="12" cy="16.5" r="1" fill="#2c3e50" stroke="none"/>',
)

# Logic / output
write("vision_boolAnd", '<path d="M6 7h5a5 5 0 010 10H6z"/><path d="M4 7v10"/>')
write(
    "vision_boolOr",
    '<path d="M5 7h4c4 0 7 2.5 7 5s-3 5-7 5H5"/><path d="M5 7v10"/>',
)
write(
    "vision_boolNot",
    '<circle cx="14" cy="12" r="5"/><path d="M5 7v10M5 12h4"/>'
    '<circle cx="19" cy="12" r="1.2" fill="#2c3e50" stroke="none"/>',
)
write(
    "vision_numericCompare",
    '<path d="M6 9h8M6 15h8"/><path d="M16 8l4 4-4 4"/>',
)
write(
    "vision_switch",
    '<path d="M7 5v14"/><path d="M7 8h8l3 4-3 4H7"/>'
    '<circle cx="7" cy="8" r="1.5" fill="#2c3e50" stroke="none"/>',
)
write(
    "vision_counter",
    '<rect x="4" y="6" width="16" height="12" rx="2"/>'
    '<path d="M8 12h2M12 10v4M16 12h1"/><path d="M9 9v6"/>',
)
write(
    "vision_aggregate",
    '<path d="M5 17l3-8 3 5 3-7 5 10"/><path d="M4 19h16"/>',
)
write(
    "vision_resultPreview",
    '<rect x="3" y="5" width="18" height="12" rx="2"/>'
    '<path d="M8 20h8"/><path d="M12 17v3"/><circle cx="12" cy="11" r="2.5"/>',
)
write(
    "vision_resultWriter",
    '<path d="M6 3h9l5 5v13H6z"/><path d="M15 3v5h5"/><path d="M9 13h8M9 17h6"/>',
)

print(f"generated {len(list(OUT.glob('*.svg')))} icons -> {OUT}")
