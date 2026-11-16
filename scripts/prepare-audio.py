"""Generate required PCM sidecars from a user's XU4 dataset."""
import argparse
from pathlib import Path
import subprocess

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument("dataset", type=Path)
parser.add_argument("--midi-renderer", type=Path, required=True)
parser.add_argument("--ogg-decoder", type=Path, required=True)
parser.add_argument("--soundfont", type=Path, required=True)
args = parser.parse_args()

for source in sorted(args.dataset.rglob("*")):
    if not source.is_file():
        continue
    destination = source.with_name(source.name + ".pcm")
    if source.name.lower() in {"gate_open.ogg", "title_fade.ogg"}:
        data = subprocess.check_output([str(args.ogg_decoder.resolve()), str(source)])
        destination.write_bytes(data)
    elif source.suffix.lower() == ".mid":
        subprocess.run([str(args.midi_renderer.resolve()), str(args.soundfont),
                        str(source), str(destination)], check=True)
