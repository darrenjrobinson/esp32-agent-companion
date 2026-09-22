#!/usr/bin/env python3
"""Validate the committed Jarvis lab pack and expose it to the native host build.

The Jarvis slot ships empty: no artwork is committed for it. When the pack is
absent this emits an empty blob so the native build still links, and the
firmware reports the character unavailable because kJarvisDataSize is zero.
Fill the slot by following docs/character-art-notes.md, then run
tools/export_jarvis_lab.py to produce the pack and its generated header.
"""
import hashlib
import json
from pathlib import Path
import platform

ROOT = Path(__file__).resolve().parents[1]
TRACKS = (
    "right", "left", "up", "down", "up_right", "up_left", "down_right", "down_left",
    "surprise", "working", "complete", "attention", "attention_alternate",
)


def blob(data_path):
    """Return the assembly that publishes the pack under kJarvisDataBlob."""
    symbol = "kJarvisDataBlob"
    section = ".section .rodata"
    if platform.system() == "Darwin":
        symbol = "_" + symbol
        section = ".section __TEXT,__const"
    body = ".byte 0\n"
    if data_path is not None:
        escaped = str(data_path).replace("\\", "\\\\").replace('"', '\\"')
        body = f'.incbin "{escaped}"\n'
    return f"{section}\n.balign 4\n.globl {symbol}\n{symbol}:\n{body}"


def main():
    output = ROOT / "build/jarvis_host.S"
    output.parent.mkdir(parents=True, exist_ok=True)
    data_path = ROOT / "assets/jarvis-lab.bin"
    metadata_path = ROOT / "assets/jarvis-lab.json"
    if not data_path.exists() and not metadata_path.exists():
        output.write_text(blob(None))
        print("Jarvis slot is empty; see docs/character-art-notes.md to fill it.")
        return
    metadata = json.loads(metadata_path.read_text())
    data = data_path.read_bytes()
    if (metadata.get("formatVersion") != 1
            or metadata.get("encoding") != "zlib-rgb565-word-up-be"
            or metadata.get("width") != 412 or metadata.get("height") != 352
            or metadata.get("steps") != 24 or tuple(metadata.get("directions", [])) != TRACKS
            or metadata.get("frameBlocks") != 13 * 24 * 5
            or metadata.get("dataBytes") != len(data)
            or metadata.get("dataSha256") != hashlib.sha256(data).hexdigest()):
        raise ValueError("Jarvis lab pack metadata is stale or invalid.")
    checks = {
        "directionManifestSha256": ROOT / "web/generated-sprites-jarvis/animation.json",
        "expressionManifestSha256": ROOT / "web/generated-expressions-jarvis/animation.json",
        "packExporterSha256": ROOT / "tools/export_jarvis_lab.py",
        "generatedHeaderSha256": ROOT / "firmware/Copilot/generated/jarvis_assets.h",
    }
    for field, path in checks.items():
        if metadata.get(field) != hashlib.sha256(path.read_bytes()).hexdigest():
            raise ValueError(f"Jarvis lab pack is stale for {path}.")
    output.write_text(blob(data_path))
    print(f"Jarvis lab payload: {len(data):,} bytes, SHA256 {metadata['dataSha256']}")


if __name__ == "__main__":
    main()
