"""Check the compiled report and render page previews for visual inspection."""

from pathlib import Path
import re

import pymupdf
from PIL import Image, ImageDraw


REPORT = Path(__file__).resolve().parent


def main():
    output = REPORT / "build"
    output.mkdir(exist_ok=True)
    document = pymupdf.open(REPORT / "Arrakis_Report.pdf")
    text = "\n".join(page.get_text() for page in document)
    required = [
        "Asique Ehetasamul Haque", "2107096", "Group: B2",
        "Computer Graphics and Image Processing Laboratory", "Description",
        "Curved", "Motion", "Pipeline", "Audio", "shadow",
        "Ten-Slide", "Two-Minute", "Karpov", "Thank You",
        "210", "150", "60", "120", "36",
    ]
    missing = [term for term in required if term not in text]
    if missing:
        raise RuntimeError(f"Missing expected report content: {missing}")
    log = (output / "Arrakis_Report.log").read_text(encoding="utf-8", errors="replace")
    issues = re.findall(r"^.*(?:Overfull|undefined|Missing character|LaTeX Warning|ignored error|Infinite glue shrinkage).*$", log, flags=re.M)
    outside = []
    for index, page in enumerate(document):
        for block in page.get_text("blocks"):
            if block[0] < -1 or block[1] < -1 or block[2] > page.rect.width + 1 or block[3] > page.rect.height + 1:
                outside.append(index + 1)
    thumbnails = []
    for index, page in enumerate(document):
        pixmap = page.get_pixmap(matrix=pymupdf.Matrix(0.48, 0.48), alpha=False)
        thumb = Image.frombytes("RGB", (pixmap.width, pixmap.height), pixmap.samples)
        panel = Image.new("RGB", (300, 435), "#dddddd")
        panel.paste(thumb, ((300 - thumb.width) // 2, 18))
        ImageDraw.Draw(panel).text((8, 3), f"PDF page {index + 1}", fill="black")
        thumbnails.append(panel)
    for start in range(0, len(thumbnails), 12):
        group = thumbnails[start:start + 12]
        sheet = Image.new("RGB", (1200, 435 * ((len(group) + 3) // 4)), "white")
        for index, thumb in enumerate(group):
            sheet.paste(thumb, ((index % 4) * 300, (index // 4) * 435))
        sheet.save(output / f"preview-{start // 12 + 1}.png")
    document[0].get_pixmap(matrix=pymupdf.Matrix(1.2, 1.2), alpha=False).save(output / "cover.png")
    print(f"Pages: {len(document)}; words: {len(text.split())}; screenshots: {sum(len(p.get_images()) for p in document)}")
    print(f"Required-content checks: PASS; out-of-page text: {outside or 'none'}")
    print("Layout/reference warnings:", issues or "none")
    if outside or issues:
        raise RuntimeError("Inspect and resolve the reported document layout issues.")


if __name__ == "__main__":
    main()
