#!/usr/bin/env python3
"""One-shot converter: MidiController YAML configs -> data/*.json.

The C++ firmware reads JSON, not YAML (see docs/plan.md, "Decisions"). This
script does a faithful 1:1 YAML->JSON conversion and *preserves key order*.
Order matters: ``set_params`` emits MIDI in the order params appear, so the JSON
loader (nlohmann::ordered_json) must see the same order the YAML had.

PyYAML (>=5.1) and json.dump (sort_keys=False) both preserve insertion order on
modern Python, so a plain load->dump round-trips order correctly.

Usage:
    python3 tools/yaml2json.py [SRC_DIR] [DEST_DIR]

SRC_DIR defaults to the original Python project; DEST_DIR defaults to ./data.
"""
import json
import os
import sys

import yaml

DEFAULT_SRC = "/mnt/c/Users/A-A-Ron/git/MidiController"
DEFAULT_DEST = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "data")


def convert_file(src_path, dest_path):
    with open(src_path, "r") as f:
        data = yaml.full_load(f)
    os.makedirs(os.path.dirname(dest_path), exist_ok=True)
    with open(dest_path, "w") as f:
        json.dump(data, f, indent=2, ensure_ascii=False, sort_keys=False)
        f.write("\n")
    print("  {} -> {}".format(os.path.basename(src_path), os.path.relpath(dest_path, DEFAULT_DEST)))


def convert_dir(src_dir, dest_dir):
    if not os.path.isdir(src_dir):
        print("  (skip, missing) {}".format(src_dir))
        return
    for name in sorted(os.listdir(src_dir)):
        if name.endswith(".yaml"):
            stem = name[:-5]
            convert_file(os.path.join(src_dir, name), os.path.join(dest_dir, stem + ".json"))


def main():
    src = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_SRC
    dest = sys.argv[2] if len(sys.argv) > 2 else DEFAULT_DEST

    print("Converting YAML -> JSON")
    print("  src:  {}".format(src))
    print("  dest: {}".format(dest))

    # Top-level controller config
    print("controller config:")
    convert_file(os.path.join(src, "Main", "Conf", "midi_controller.yaml"),
                 os.path.join(dest, "midi_controller.json"))

    # Per-pedal command maps
    print("pedals:")
    convert_dir(os.path.join(src, "Main", "Conf", "MidiPedals"), os.path.join(dest, "pedals"))

    # Songs and sets
    print("songs:")
    convert_dir(os.path.join(src, "PartSongSet", "Songs"), os.path.join(dest, "songs"))
    print("sets:")
    convert_dir(os.path.join(src, "PartSongSet", "Sets"), os.path.join(dest, "sets"))

    print("Done.")


if __name__ == "__main__":
    main()
