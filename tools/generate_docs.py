"""Render project explanation chapters and complete numbered sources as a PDF."""

from __future__ import annotations

import argparse
import hashlib
import json
import re
import textwrap
from datetime import datetime, timezone
from pathlib import Path
from xml.sax.saxutils import escape

from reportlab.lib import colors
from reportlab.lib.enums import TA_CENTER
from reportlab.lib.pagesizes import A4
from reportlab.lib.styles import ParagraphStyle, getSampleStyleSheet
from reportlab.lib.units import mm
from reportlab.platypus import (
    BaseDocTemplate, Frame, PageBreak, PageTemplate, Paragraph,
    Preformatted, Spacer, Table, TableStyle,
)
from reportlab.platypus.tableofcontents import TableOfContents

ROOT = Path(__file__).resolve().parents[1]
CHAPTERS = (
    "01-project-and-build.md", "02-renderer.md", "03-scene-and-runtime.md",
    "04-game-and-tests.md", "05-hud-and-sandworm.md", "06-audio-and-vendor.md",
    "07-documentation-tools.md",
)
SOURCE_PATHS = (
    "src/arrakis.cpp", "src/arrakis_game.h", "src/arrakis_hud.h",
    "src/arrakis_worm.h", "src/arrakis_audio.h", "tests/arrakis_tests.h",
    "CMakeLists.txt", "Arrakis_001.vcxproj", "Arrakis_001.vcxproj.filters",
    ".gitignore", ".vscode/c_cpp_properties.json", ".vscode/launch.json",
    "vcpkg.json", "tools/generate_docs.py", "tools/requirements.txt",
    "third_party/miniaudio/miniaudio_impl.cpp", "third_party/glad/glad.c",
    "third_party/miniaudio/miniaudio.h",
)


def inline(text: str) -> str:
    """Escape literal text before adding the supported Markdown inline tags."""
    parts = re.split(r"(`[^`]+`)", text)
    rendered = []
    for part in parts:
        if part.startswith("`") and part.endswith("`"):
            rendered.append('<font name="Courier">' + escape(part[1:-1]) + '</font>')
        else:
            safe = escape(part)
            safe = re.sub(r"\*\*(.+?)\*\*", r"<b>\1</b>", safe)
            safe = re.sub(r"\[([^\]]+)\]\(([^)]+)\)", r"\1 (\2)", safe)
            rendered.append(safe)
    return "".join(rendered)


def source_rows(content: str, width: int = 100) -> list[str]:
    """Wrap rather than truncate source; continuations retain their line number."""
    rows = []
    for number, line in enumerate(content.splitlines(), 1):
        chunks = textwrap.wrap(
            line.expandtabs(4), width=width, replace_whitespace=False,
            drop_whitespace=False, break_long_words=True, break_on_hyphens=False,
        ) or [""]
        for index, chunk in enumerate(chunks):
            prefix = f"{number:6d} | " if index == 0 else "       > "
            rows.append(prefix + chunk)
    return rows


class ExplanationDocument(BaseDocTemplate):
    """Provide page furniture, PDF bookmarks, and a paginated table of contents."""

    def __init__(self, filename: str, styles: dict):
        super().__init__(
            filename, pagesize=A4, leftMargin=19 * mm, rightMargin=19 * mm,
            topMargin=20 * mm, bottomMargin=19 * mm,
            title="Arrakis: Complete Code and Functionality Explanation",
            author="Arrakis Project Documentation", pageCompression=1,
        )
        frame = Frame(self.leftMargin, self.bottomMargin, self.width, self.height, id="body")
        self.addPageTemplates(PageTemplate(id="default", frames=[frame], onPage=self.decorate))
        self.styles = styles
        self.heading_number = 0

    def beforeDocument(self):
        self.heading_number = 0

    def decorate(self, canvas, document):
        canvas.saveState()
        canvas.setStrokeColor(colors.HexColor("#c9b99e"))
        canvas.line(self.leftMargin, A4[1] - 14 * mm, A4[0] - self.rightMargin, A4[1] - 14 * mm)
        canvas.setFillColor(colors.HexColor("#665b4d"))
        canvas.setFont("Helvetica", 8)
        canvas.drawString(self.leftMargin, A4[1] - 11 * mm, "ARRAKIS / CODE AND FUNCTIONALITY REFERENCE")
        canvas.drawString(self.leftMargin, 11 * mm, "CSE 4102 | Application, tests, configuration, and library source")
        canvas.drawRightString(A4[0] - self.rightMargin, 11 * mm, str(document.page))
        canvas.restoreState()

    def afterFlowable(self, flowable):
        level = getattr(flowable, "toc_level", None)
        if level is None:
            return
        self.heading_number += 1
        key = f"section-{self.heading_number}"
        title = flowable.getPlainText()
        self.canv.bookmarkPage(key)
        self.canv.addOutlineEntry(title, key, level=level, closed=level > 0)
        self.notify("TOCEntry", (level, title, self.page, key))


def make_styles() -> dict:
    base = getSampleStyleSheet()
    styles = {"body": ParagraphStyle(
        "Body", parent=base["BodyText"], fontName="Helvetica", fontSize=9,
        leading=13.5, spaceAfter=6, splitLongWords=True,
    )}
    for level, size in ((1, 20), (2, 13), (3, 10.5), (4, 10)):
        styles[f"h{level}"] = ParagraphStyle(
            f"H{level}", parent=base["Heading1"], fontName="Helvetica-Bold",
            fontSize=size, leading=size * 1.25, spaceBefore=12, spaceAfter=8,
            textColor=colors.HexColor("#77532d"), keepWithNext=True,
        )
    styles["code"] = ParagraphStyle("Code", fontName="Courier", fontSize=6.5, leading=8)
    styles["source"] = ParagraphStyle("Source", fontName="Courier", fontSize=7.2, leading=9)
    styles["table"] = ParagraphStyle("TableText", parent=styles["body"], fontSize=7.8, leading=10)
    styles["title"] = ParagraphStyle(
        "CoverTitle", parent=styles["h1"], fontSize=30, leading=36, alignment=TA_CENTER,
    )
    styles["center"] = ParagraphStyle("Center", parent=styles["body"], alignment=TA_CENTER)
    return styles


def heading(text: str, level: int, styles: dict) -> Paragraph:
    paragraph = Paragraph(inline(text), styles[f"h{min(level, 4)}"])
    if level <= 2:
        paragraph.toc_level = level - 1
    return paragraph


def markdown_flowables(content: str, styles: dict) -> list:
    """Convert the chapter's Markdown subset without allowing raw HTML execution."""
    output = []
    lines = content.splitlines()
    paragraph = []
    index = 0

    def flush():
        if paragraph:
            output.append(Paragraph(inline(" ".join(paragraph)), styles["body"]))
            paragraph.clear()

    while index < len(lines):
        line = lines[index].strip()
        if line.startswith("```"):
            flush()
            index += 1
            code = []
            while index < len(lines) and not lines[index].strip().startswith("```"):
                code.extend(textwrap.wrap(
                    lines[index].expandtabs(4), width=109, replace_whitespace=False,
                    drop_whitespace=False, break_on_hyphens=False,
                ) or [""])
                index += 1
            for offset in range(0, len(code), 45):
                output.append(Preformatted("\n".join(code[offset:offset + 45]), styles["code"]))
            output.append(Spacer(1, 6))
        elif re.match(r"^#{1,6} ", line):
            flush()
            marker, title = line.split(" ", 1)
            output.append(heading(title, min(len(marker), 4), styles))
        elif line.startswith("|") and line.endswith("|"):
            flush()
            rows = []
            while index < len(lines) and lines[index].strip().startswith("|"):
                cells = [cell.strip() for cell in lines[index].strip().strip("|").split("|")]
                if not all(re.fullmatch(r":?-+:?", cell.replace(" ", "")) for cell in cells):
                    rows.append([Paragraph(inline(cell), styles["table"]) for cell in cells])
                index += 1
            index -= 1
            if rows:
                columns = max(len(row) for row in rows)
                for row in rows:
                    row.extend([""] * (columns - len(row)))
                table = Table(rows, colWidths=[(A4[0] - 38 * mm - 12) / columns] * columns, repeatRows=1)
                table.setStyle(TableStyle([
                    ("BACKGROUND", (0, 0), (-1, 0), colors.HexColor("#eee5d6")),
                    ("VALIGN", (0, 0), (-1, -1), "TOP"),
                    ("GRID", (0, 0), (-1, -1), 0.3, colors.HexColor("#d8cbbb")),
                    ("TOPPADDING", (0, 0), (-1, -1), 5),
                    ("BOTTOMPADDING", (0, 0), (-1, -1), 5),
                ]))
                output.extend([table, Spacer(1, 8)])
        elif re.match(r"^[-*+] ", line) or re.match(r"^\d+[.)] ", line):
            flush()
            output.append(Paragraph(inline(line), styles["body"]))
        elif not line or line in ("---", "***", "___"):
            flush()
        elif line.startswith("> "):
            flush()
            output.append(Paragraph(inline(line[2:]), styles["body"]))
        else:
            paragraph.append(line)
        index += 1
    flush()
    return output


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=ROOT / "docs/Arrakis_Code_Explanation.pdf")
    arguments = parser.parse_args()
    chapters = [ROOT / "docs/chapters" / name for name in CHAPTERS]
    sources = [ROOT / name for name in SOURCE_PATHS]
    missing = [str(path) for path in chapters + sources if not path.is_file()]
    if missing:
        parser.error("Required documentation inputs are missing: " + ", ".join(missing))
    if not arguments.output.parent.is_dir():
        parser.error("Output parent directory must already exist")

    styles = make_styles()
    story = [Spacer(1, 42 * mm)]
    story.append(Paragraph("ARRAKIS<br/>Harvester Down", styles["title"]))
    story.append(Spacer(1, 10 * mm))
    story.append(Paragraph("Complete Code and Functionality Explanation", styles["center"]))
    story.append(Paragraph("Plain-language walkthroughs, mathematics, tests, build details,<br/>and complete numbered source references", styles["center"]))
    story.append(Spacer(1, 12 * mm))
    story.append(Paragraph("CSE 4102: Computer Graphics and Image Processing Laboratory", styles["center"]))
    story.append(Paragraph("Documented source snapshot: " + datetime.now(timezone.utc).strftime("%Y-%m-%d UTC"), styles["center"]))
    story.append(PageBreak())
    story.append(Paragraph("Contents", styles["h1"]))
    toc = TableOfContents()
    toc.levelStyles = [
        ParagraphStyle("TOCChapter", fontName="Helvetica-Bold", fontSize=9, leading=13, spaceBefore=5),
        ParagraphStyle("TOCSection", fontName="Helvetica", fontSize=8, leading=11, leftIndent=12),
    ]
    story.append(toc)
    for chapter in chapters:
        story.append(PageBreak())
        story.extend(markdown_flowables(chapter.read_text(encoding="utf-8"), styles))

    manifest = {"generated_at": datetime.now(timezone.utc).isoformat(), "sources": [], "chapters": []}
    story.append(PageBreak())
    story.append(heading("Appendix A: Source Snapshot and Reading Rules", 1, styles))
    story.append(Paragraph(
        "The following appendices contain every line of the retained application, tests, build/editor "
        "configuration, documentation generator, and vendored C/C++ sources. The left number is the "
        "physical source line, not a PDF line. A &gt; continuation is the rest of that same source line; "
        "long lines are wrapped, never omitted. SHA-256 fingerprints identify this exact snapshot. "
        "Application semantics are explained in the preceding code-range chapters. The libraries are "
        "explained by functional families; their full source is included for reference, not presented "
        "as original game logic. Installed external GLFW, GLM and GLAD headers are dependencies, "
        "not files implemented in this repository.", styles["body"],
    ))
    for path in sources:
        data = path.read_bytes()
        content = data.decode("utf-8-sig")
        relative = path.relative_to(ROOT).as_posix()
        entry = {"path": relative, "lines": len(content.splitlines()), "sha256": hashlib.sha256(data).hexdigest()}
        manifest["sources"].append(entry)
        story.append(Paragraph(inline(f"{relative}: {entry['lines']} lines; SHA-256 `{entry['sha256']}`"), styles["body"]))

    for path, entry in zip(sources, manifest["sources"]):
        story.append(PageBreak())
        story.append(heading("Source Reference: " + entry["path"], 1, styles))
        rows = source_rows(path.read_text(encoding="utf-8-sig"))
        for offset in range(0, len(rows), 72):
            story.append(Preformatted("\n".join(rows[offset:offset + 72]), styles["source"]))

    for chapter in chapters:
        data = chapter.read_bytes()
        manifest["chapters"].append({
            "path": chapter.relative_to(ROOT).as_posix(), "sha256": hashlib.sha256(data).hexdigest(),
            "words": len(data.decode("utf-8").split()),
        })
    document = ExplanationDocument(str(arguments.output), styles)
    document.multiBuild(story, maxPasses=6)
    from pypdf import PdfReader
    manifest["pdf"] = {
        "path": str(arguments.output.relative_to(ROOT)) if arguments.output.is_relative_to(ROOT) else str(arguments.output),
        "pages": len(PdfReader(arguments.output).pages), "bytes": arguments.output.stat().st_size,
    }
    (ROOT / "docs/documentation_manifest.json").write_text(json.dumps(manifest, indent=2) + "\n", encoding="utf-8")
    print(f"Created {arguments.output}: {manifest['pdf']['pages']} pages, {manifest['pdf']['bytes']:,} bytes")
    print(f"Included {len(sources)} source/config files and {len(chapters)} explanation chapters")


if __name__ == "__main__":
    main()
