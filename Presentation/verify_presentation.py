"""Validate deliverables and create contact sheets from the actual PDF export."""
from pathlib import Path
import hashlib
import json
import zipfile

from PIL import Image, ImageDraw
from pptx import Presentation
import pymupdf


HERE = Path(__file__).resolve().parent


def main():
    deck = Presentation(HERE / "Arrakis_Presentation.pptx")
    assert len(deck.slides) == 10, "Exactly ten slides required"
    assert abs(deck.slide_width / deck.slide_height - 16 / 9) < 0.001
    texts = ["\n".join(shape.text for shape in slide.shapes if shape.has_text_frame) for slide in deck.slides]
    notes = [slide.notes_slide.notes_text_frame.text for slide in deck.slides]
    assert "INTRODUCTION" in texts[0] and "Outline" in texts[1] and "THANK YOU" in texts[-1]
    expected = ["Asique Ehetasamul Haque", "2107096", "4th", "1st", "B2", "CSE - 4102", "Computer Graphics and Image Processing Laboratory", "Khulna University of Engineering and Technology"]
    assert all(value in texts[0] for value in expected), "Cover identity incomplete"
    assert all(len(note.split()) >= 100 for note in notes), "Speaker notes missing"
    for slide in deck.slides:
        for shape in slide.shapes:
            assert shape.left >= 0 and shape.top >= 0
            assert shape.left + shape.width <= deck.slide_width + 1000
            assert shape.top + shape.height <= deck.slide_height + 1000
    with zipfile.ZipFile(HERE / "Arrakis_Presentation.pptx") as package:
        assert not any(name.endswith((".mp4", ".mov", ".wmv", ".avi")) for name in package.namelist()), "Video must not be embedded"
    manifest = json.loads((HERE / "presentation_manifest.json").read_text(encoding="utf-8"))
    for relative, expected_hash in manifest["source_sha256"].items():
        assert hashlib.sha256((HERE.parent / relative).read_bytes()).hexdigest() == expected_hash, f"Source changed after generation: {relative}"
    document = pymupdf.open(HERE / "Arrakis_Presentation.pdf")
    assert len(document) == 10
    exported_text = "\n".join(page.get_text() for page in document)
    assert "2107096" in exported_text and "THANK YOU" in exported_text
    assert "150" in exported_text and "23 / 27" in exported_text
    previews = HERE / "preview"
    previews.mkdir(exist_ok=True)
    for index, page in enumerate(document):
        page.get_pixmap(matrix=pymupdf.Matrix(1600 / page.rect.width, 1600 / page.rect.width), alpha=False).save(previews / f"slide-{index + 1:02d}.png")
    for start in (0, 6):
        rows = 2 if start == 0 else 2
        sheet = Image.new("RGB", (1200, rows * 247), "#DDDDDD")
        for index in range(start, min(start + 6, len(document))):
            with Image.open(previews / f"slide-{index + 1:02d}.png") as image:
                image.thumbnail((390, 220))
                column, row = (index - start) % 3, (index - start) // 3
                sheet.paste(image, (column * 400 + 5, row * 247 + 22))
                ImageDraw.Draw(sheet).text((column * 400 + 8, row * 247 + 4), f"Slide {index + 1:02d}", fill="black")
        sheet.save(previews / f"contact-{start // 6 + 1}.png")
    overflow_path = previews / "text_overflow.json"
    overflow = json.loads(overflow_path.read_text(encoding="utf-8-sig")) if overflow_path.exists() else []
    if overflow:
        print(json.dumps(overflow, indent=2))
        raise RuntimeError("Correct the text overflows detected by PowerPoint.")
    print(f"PASS: 10 slides, 16:9, complete identity, 10 presenter notes, {len(manifest['feature_coverage'])} feature mappings, no embedded video.")
    print("PASS: source snapshot unchanged, exported ten-page PDF, slide bounds, and PowerPoint text-fit checks.")


if __name__ == "__main__":
    main()
