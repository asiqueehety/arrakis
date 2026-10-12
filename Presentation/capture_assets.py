"""Produce gameplay frames and clearly labelled inspection captures for the deck."""
from pathlib import Path
import argparse
import subprocess

from PIL import Image

HERE = Path(__file__).resolve().parent
ROOT = HERE.parent


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--capture-exe", type=Path, required=True)
    args = parser.parse_args()
    assets = HERE / "assets"
    assets.mkdir(exist_ok=True)
    executable = ROOT / "build/rescue/Release/Arrakis.exe"
    for mode, name in [("--smoke-test", "scene"), ("--smoke-breach", "breach"),
                       ("--smoke-pursuit", "pursuit"), ("--smoke-mouse", "controls")]:
        subprocess.run([str(executable), mode, str(assets / f"{name}.bmp")], cwd=ROOT, check=True)
    subprocess.run([str(args.capture_exe), str(assets)], cwd=ROOT, check=True)
    for capture in assets.glob("*.bmp"):
        with Image.open(capture) as image:
            image.convert("RGB").save(capture.with_suffix(".png"), optimize=True)
        capture.unlink()


if __name__ == "__main__":
    main()
