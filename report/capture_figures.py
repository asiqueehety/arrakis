"""Capture real application frames for the laboratory report."""

from pathlib import Path
import subprocess

from PIL import Image


ROOT = Path(__file__).resolve().parent.parent
FIGURES = Path(__file__).resolve().parent / "figures"
EXECUTABLE = ROOT / "build" / "rescue" / "Release" / "Arrakis.exe"


def main():
    FIGURES.mkdir(exist_ok=True)
    for mode, name in [
        ("--smoke-test", "scene"),
        ("--smoke-breach", "breach"),
        ("--smoke-mouse", "controls"),
        ("--smoke-pursuit", "pursuit"),
    ]:
        capture = FIGURES / f"{name}.bmp"
        subprocess.run([str(EXECUTABLE), mode, str(capture)], cwd=ROOT, check=True)
        with Image.open(capture) as image:
            image.convert("RGB").save(capture.with_suffix(".png"), optimize=True)
        capture.unlink()


if __name__ == "__main__":
    main()
