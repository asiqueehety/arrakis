# 7. The Documentation Generator, Line by Line

## Purpose and Boundaries

`tools/generate_docs.py` produces the PDF requested for this project. It renders authored Markdown explanation chapters, generates page numbers and navigation, and appends complete numbered source/configuration listings. It also writes a machine-readable snapshot manifest. It does not infer the meaning of the game through artificial intelligence, execute the game, prove the prose correct, or automatically update descriptions after gameplay changes. The written chapters must be maintained when behavior changes; source hashes make a stale snapshot detectable.

The generator deliberately uses Python plus ReportLab instead of requiring a browser, LaTeX distribution, Node.js, or an online conversion service. The PDF is created locally. `tools/requirements.txt` contains two constraint lines: `reportlab>=4.2,<5` allows compatible 4.x renderer versions, and `pypdf>=5,<7` allows supported PDF-inspection versions. The lower bounds request available functionality; the upper bounds avoid silently adopting a major-version API change. Pillow is brought in as a transitive ReportLab dependency and is not a game dependency.

## Lines 1-23: Module Setup and Imports

The module docstring describes the file's purpose and later supplies command-line help. `from __future__ import annotations` postpones annotation evaluation; types such as `list[str]` communicate expected values without affecting game execution. `argparse` defines command-line options and user-facing errors. `hashlib` computes SHA-256 fingerprints. `json` serializes the snapshot manifest. `re` matches the supported Markdown syntax. `textwrap` splits long text without truncating it. `datetime` and `timezone` produce a UTC generation date/time. `Path` joins and resolves paths independently of the current working directory. XML escaping protects Paragraph text from treating source characters such as `<` or `&` as formatting markup.

The ReportLab imports provide RGB/hex colors, centered alignment, the A4 page size, paragraph styles, millimeter conversion, a base multi-pass document, text frames, page templates, page breaks, formatted paragraphs, literal preformatted blocks, spacing, tables, table styling, and a table of contents. These classes divide responsibilities: Flowables are pieces of page content, a Frame defines where pieces can fit, and a PageTemplate defines recurring page furniture.

## Lines 25-39: Exact Input Lists

`ROOT` resolves this script's actual location, takes its parent `tools` directory, then its parent repository directory. This makes normal input lookup independent of the shell's current directory. `CHAPTERS` is an ordered tuple of all seven required chapter filenames. A tuple fixes the desired reading sequence rather than relying on operating-system directory enumeration. `SOURCE_PATHS` explicitly lists application, test, build, editor, dependency, generator, and vendor files. No generated build files or removed model packs are included. Miniaudio's implementation unit is listed before GLAD and the large miniaudio header, so the most oversized source reference appears at the end instead of interrupting explanations.

These lists are intentionally explicit. Adding a new source file requires adding it to the listing and writing its explanation; the script does not pretend it can determine which newly added folder is meaningful. Before rendering, every listed input is checked. A missing chapter or source causes a clear error rather than an incomplete PDF that silently omits the file.

## Lines 42-54: inline, Safe Inline Formatting

`inline(text)` receives ordinary text and returns the limited markup understood by ReportLab's Paragraph renderer. `re.split` separates backtick-delimited inline-code fragments while retaining the matched fragments. Each fragment is examined in order. A backtick pair is removed and its literal content escaped, then wrapped in a Courier font tag. This makes identifiers/paths visually distinct and ensures a code expression containing XML characters is not interpreted as markup.

Non-code fragments are escaped first. The bold substitution turns `**text**` into a `<b>` span. The Markdown-link substitution renders the readable label followed by the destination in parentheses; it does not fetch that destination or embed remote content. Appending results to a list and joining once avoids repeatedly growing a string for every fragment. The function deliberately does not implement the complete CommonMark standard, nested Markdown, or arbitrary raw HTML. Unrecognized markup remains text instead of enabling unexpected processing.

## Lines 57-68: source_rows, Preserve Every Source Line

`source_rows(content, width=100)` converts source text into printable lines. `splitlines()` obtains physical source lines; enumeration begins at one to match editor line numbers. Tabs expand to four spaces to give a consistent printed indentation. `textwrap.wrap` caps code fragments at 100 characters, preserves whitespace, permits splitting a very long uninterrupted identifier/string, and avoids additional hyphen-based breaks. An empty original line becomes one empty chunk rather than disappearing.

The first fragment receives a six-character right-aligned line number and ` | ` separator. Every continuation receives spaces and ` > ` instead. The `>` means this is still the same original source line, not a new line number. Each fragment is appended to the result. No character-limit truncation or first-N-lines selection is applied. Line endings and tabs are normalized for presentation, but the manifest hash is calculated from original file bytes rather than this display transformation.

## Lines 71-109: ExplanationDocument and PDF Navigation

`ExplanationDocument` inherits ReportLab's `BaseDocTemplate`. Its constructor passes the output filename, A4 size, 19 mm left/right margins, 20 mm top margin, 19 mm bottom margin, descriptive PDF title/author, and compression setting to the base class. A body Frame fills the allowed document width/height. A default PageTemplate uses that frame and calls `decorate` on each page. The style dictionary is retained on the object and the heading counter starts at zero.

`beforeDocument` resets the counter each time a rendering pass begins. The table of contents requires multiple passes: the first discovers heading page numbers, and later passes lay out the resulting TOC and adjust pagination. Resetting yields stable bookmark identifiers across passes rather than continually increasing them.

`decorate` saves the canvas's graphics state before drawing. It sets a pale sand-colored stroke and draws a horizontal line 14 mm below the top. It sets muted text color and an 8-point Helvetica font, adds the reference title above the line, adds course/reference text at the bottom, and right-aligns the current page number. The saved state is restored so these choices cannot accidentally change a paragraph, diagram, or source listing drawn afterward. Dimensions are points internally; multiplying by `mm` converts the intended physical dimensions.

`afterFlowable` runs after a Flowable has been placed. A Flowable without `toc_level` is ordinary text and returns immediately. A numbered heading obtains a unique `section-N` key and a plain-text title. `bookmarkPage` binds that key to the current page. `addOutlineEntry` inserts a clickable PDF sidebar entry at chapter or subsection level; subsections start collapsed. `notify("TOCEntry", ...)` tells the TOC its level, title, page number, and bookmark destination. The counter is navigation state, not a source-code line counter.

## Lines 112-131: make_styles, Typography

The renderer begins with ReportLab's standard sample stylesheet, then builds named styles rather than scattering font settings throughout parsing code. Body text is 9-point Helvetica with 13.5-point line spacing and six points after a paragraph; long words can split to avoid overflowing margins. A loop defines heading levels 1-4 with sizes 20, 13, 10.5, and 10. Each is bold, uses proportional line spacing, has space before/after, uses warm brown text, and remains with the next Flowable so headings do not sit alone at a page bottom.

Code samples use 6.5-point Courier with 8-point leading. Source appendices use 7.2-point Courier with 9-point leading, balancing very large retained libraries against readable monospaced references. Table cells inherit body behavior but use 7.8-point text and 10-point leading. The cover title uses 30-point size, 36-point leading, and centered alignment. A separate centered body style supports cover subtitles. The function returns the complete dictionary, making later style selection deterministic.

## Lines 134-138: heading, Document Hierarchy

`heading(text, level, styles)` creates a Paragraph from escaped/formatted text. Levels deeper than four use the fourth visual style. Only levels one and two receive `toc_level`: level one becomes TOC/outline level zero, and level two becomes level one. This includes navigable chapter/subsection entries while keeping hundreds of small line-range headings out of the contents. The returned object is still a normal Paragraph with one additional navigation attribute.

## Lines 141-206: markdown_flowables, Each Parsing Branch

The parser creates an output list, splits the chapter into lines, creates a pending prose buffer, and tracks a current index. Its nested `flush` function turns accumulated prose lines into a single escaped Paragraph separated by spaces, then clears the buffer. This preserves ordinary paragraphs spread across multiple editor lines. Flushing before structural blocks preserves their intended order.

The main loop strips whitespace only for syntax recognition; code blocks later use the original unstripped line. A line beginning with three backticks starts a literal code block. Pending prose is flushed, the opening marker skipped, and original code lines collected until a closing marker or end of file. They expand tabs and wrap at 109 characters with whitespace preserved. Chunks of at most 45 printed lines become Preformatted objects, reducing oversized unsplittable blocks. A small Spacer follows the code block. The language label after the opening marker is not used for syntax highlighting; this is a plain source-reference renderer.

A heading matched by one through six `#` characters followed by a space is split into marker and title. Its visual depth is capped at four and passed through `heading`. A Markdown table is recognized when a stripped line starts and ends with `|`. Consecutive pipe rows are collected; cells split on the separator and have surrounding whitespace removed. Separator rows consisting only of dashes and optional alignment colons are skipped, while real cells become formatted Paragraphs. This is a simple pipe-table subset: escaped literal pipes inside a cell are not a supported feature and should be written in prose/code blocks instead.

The table branch determines the largest cell count and pads shorter rows with empty cells so the table is rectangular. Available page width is divided evenly among columns. Header row repetition keeps column context on later pages. The header gets a pale sand background, all cells align at the top, a thin grid separates values, and top/bottom padding prevents cramped text. A Spacer separates the table from following content. The parser decreases its index once because the outer loop also advances it; without that correction the first non-table line would be skipped.

Lines beginning with `- `, `* `, or `+ `, and lines matching a numbered-list marker, flush pending prose and become their own paragraphs. The markers remain visible text; the parser does not implement nested list indentation. Empty lines and simple horizontal separators flush prose. A `> ` line becomes its own paragraph without the quotation marker; no special shaded quote box is generated. Everything else joins the pending buffer. After the loop, a final flush emits any trailing paragraph. The function returns the Flowables in original chapter order.

## Lines 209-226: Arguments and Input Validation

`main` creates an ArgumentParser using the module description. `--output` accepts a Path and defaults to `docs/Arrakis_Code_Explanation.pdf`. Chapter and source Paths are joined beneath the known repository root. A list comprehension identifies inputs that are not files. If any are missing, `parser.error` prints the complete missing-input list and exits before generating a misleading partial document. A nonexistent output parent is also rejected; the tool does not create arbitrary external directory trees. An explicitly supplied relative output path is interpreted relative to the invoking shell, while default inputs and output remain repository-root based.

## Lines 228-247: Cover, Contents, and Chapter Text

The generator creates styles and a Story list with an initial vertical spacer. The cover title uses explicit line breaks between Arrakis and Harvester Down. Subtitle paragraphs state the document's purpose, source-reference scope, and course. A UTC date records the documented snapshot's generation day. A page break starts Contents. The TOC has a bold 9-point chapter style and an 8-point indented subsection style. The TOC object joins the Story before any explanation chapters.

Each chapter then starts on a fresh page. Its UTF-8 text is parsed into Flowables and extended onto the Story. This makes chapter ordering explicit and does not require combining the editable Markdown files into another duplicate manual on disk. The Story is a sequence of rendering objects held in memory until the multi-pass build.

## Lines 249-267: Snapshot Manifest and Source-Reference Rules

The manifest begins with UTC timestamp, empty source list, and empty chapter list. An appendix heading and introductory paragraph explain the exact line-number and continuation convention. They also distinguish authored explanations from third-party source reference and exclude installed external headers from the claimed repository scope.

For each source/configuration file, the generator reads its bytes, decodes UTF-8 while tolerating an initial byte-order mark, derives a repository-relative forward-slash path, counts physical text lines, and computes SHA-256 on the unchanged bytes. A dictionary records the path, count, and digest; a corresponding human-readable paragraph is added to the appendix. Hashing bytes detects changes in content and line endings, unlike merely checking file length. The SHA-256 value is an identity fingerprint, not evidence that the implementation is secure or correct.

## Lines 269-286: Complete Source Appendices and PDF Verification

The source loop pairs each input Path with its manifest entry, begins a new page, and adds a navigable source-reference heading. `source_rows` formats every line of that file. Blocks of 72 printed rows become Preformatted Flowables; ReportLab can split them at page boundaries if necessary. The complete 95,000-plus-line miniaudio header is therefore retained, not cut down to the handful of calls the game makes.

Each explanation chapter is separately hashed and its whitespace-separated word count recorded. The custom document performs `multiBuild` with up to six layout passes so TOC page numbers stabilize. If layout or parsing fails, the exception remains visible instead of recording fabricated success. After successful PDF creation, pypdf reads the actual output and counts its pages. The manifest's PDF entry records the path, physical page count, and byte size. Outputs inside the repository use relative paths; an external requested destination is kept as an external path.

The manifest is serialized with two-space JSON indentation, UTF-8 encoding, and a terminal newline to `docs/documentation_manifest.json`. That generated JSON is not a source input to its own PDF, which would otherwise create a circular hash/size dependency. Two console messages report the actual PDF page/byte count and number of included source/configuration files and chapters. These counts confirm generation, not semantic completeness of the prose. The final `if __name__ == "__main__"` guard runs `main()` only when executing the script, allowing validation tools to import formatting helpers without regenerating the PDF.

## How to Update and Validate the Manual

First update the explanation chapter corresponding to a behavior change, including code-range references if line numbers move. Add new project files to `SOURCE_PATHS` and new chapters to `CHAPTERS`. Then install the constrained dependencies and run the generator. Its strict input checks prevent accidental omission of already-listed files. Compare the manifest digests against repository files to verify that the PDF source snapshot is current. Use the PDF contents/sidebar to navigate explanation sections and source files rather than scrolling through thousands of vendor-reference pages.

The generator's subset parser handles the chapter conventions used here, not arbitrary Markdown extensions, Mermaid diagrams, remote image downloads, HTML execution, or syntax-highlighted code. Standard built-in fonts prioritize portability; unusual non-ASCII symbols should be written as words or ASCII formulas if a glyph is unavailable. Keeping these limitations explicit avoids confusing presentation features with implemented game features.
