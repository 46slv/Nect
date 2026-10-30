"""Build wholly synthetic Nect font fixtures. No third-party font bytes or designs.
Uses fontTools when already available; never downloads dependencies.
Outputs text hex so fixture provenance and content are reviewable in source control.
"""
from pathlib import Path
from tempfile import TemporaryDirectory
from fontTools.fontBuilder import FontBuilder
from fontTools.pens.ttGlyphPen import TTGlyphPen
from fontTools.ttLib import TTCollection
from fontTools.designspaceLib import DesignSpaceDocument, AxisDescriptor, SourceDescriptor
from fontTools.varLib import build

OUT = Path(__file__).parent

def font(width, family="Nect Synthetic Private Test"):
    fb = FontBuilder(1000, isTTF=True)
    fb.setupGlyphOrder([".notdef", "space", "A", "B"])
    fb.setupCharacterMap({32: "space", 65: "A", 66: "B"})
    glyphs = {}
    for name in [".notdef", "space", "A", "B"]:
        pen = TTGlyphPen(None)
        if name != "space":
            pen.moveTo((50, 0))
            pen.lineTo((width - 100, 0))
            pen.lineTo((width - 100, 700))
            pen.lineTo((50, 700))
            pen.closePath()
        glyphs[name] = pen.glyph()
    fb.setupGlyf(glyphs)
    fb.setupHorizontalMetrics({g: (width, 50 if g != "space" else 0) for g in glyphs})
    fb.setupHorizontalHeader(ascent=800, descent=-200)
    fb.setupNameTable({"familyName": family, "styleName": "Regular", "uniqueFontIdentifier": family + " Regular", "fullName": family + " Regular", "psName": family.replace(" ", "") + "-Regular", "version": "Version 1.000", "copyright": "Original synthetic test data for Nect; unrestricted use."})
    fb.setupOS2(sTypoAscender=800, sTypoDescender=-200, usWinAscent=800, usWinDescent=200, fsType=0)
    fb.setupPost()
    fb.setupMaxp()
    fb.font["head"].created = fb.font["head"].modified = 2082844800
    fb.font.recalcTimestamp = False
    return fb.font

def save_hex(name, data):
    (OUT / name).write_text("\n".join(data.hex()[i:i+128] for i in range(0, len(data)*2, 128)) + "\n")

with TemporaryDirectory(prefix="nect-synthetic-fonts-") as temp:
    temp = Path(temp)
    collection = TTCollection()
    collection.fonts = [font(600), font(900)]  # Intentionally identical names/attributes.
    collection.save(temp / "collection.ttc")
    save_hex("private-collection.hex", (temp / "collection.ttc").read_bytes())
    doc = DesignSpaceDocument()
    axis = AxisDescriptor(); axis.name="Weight"; axis.tag="wght"; axis.minimum=100; axis.default=100; axis.maximum=900
    doc.addAxis(axis)
    for i, width in enumerate([500, 900]):
        path = temp / f"master-{i}.ttf"
        font(width, "Nect Synthetic Variable Test").save(path)
        source = SourceDescriptor(); source.path=str(path); source.name=f"master-{i}"; source.location={"Weight": 100 + i*800}
        if i == 0: source.copyInfo=source.copyLib=source.copyFeatures=True
        doc.addSource(source)
    doc.write(temp / "fixture.designspace")
    variable, _, _ = build(temp / "fixture.designspace")
    variable["head"].created = variable["head"].modified = 2082844800
    variable.recalcTimestamp = False
    variable.save(temp / "variable.ttf")
    save_hex("private-variable.hex", (temp / "variable.ttf").read_bytes())
