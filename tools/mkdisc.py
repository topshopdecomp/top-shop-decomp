"""Write a mkpsxiso project for the rebuilt game.

Takes the dumpsxiso project of the original disc (disc.xml), keeps every file
from disc/ except the boot EXE, which comes from the build. Output image names
are set to build/topshop.bin / .cue. Paths in the written XML are absolute.

With --voices DIR, the files of DIR that are on the disc (GM00.XA-GM06.XA, from
tools/extract_voices.py) replace the disc's and are written as XA sectors
(mkpsxiso's "mixed" type), each followed by dummy sectors up to the original
file's sector count, so that no file after it moves: the executable reads its
files at fixed positions.

Usage: python3 -I tools/mkdisc.py [--voices DIR] disc.xml build/SLUS_013.33 build/disc.xml
"""
import os
import sys
import xml.etree.ElementTree as ET

XA_SECTOR = 2336            # a "mixed" source file holds raw sectors: subheader, data, EDC

args = sys.argv[1:]
voices = None
if args[:1] == ["--voices"]:
    voices = os.path.abspath(args[1])
    args = args[2:]
src_xml, exe, out_xml = args[:3]
root_dir = os.path.abspath(os.path.dirname(src_xml))
out_dir = os.path.abspath(os.path.dirname(out_xml))

tree = ET.parse(src_xml)
proj = tree.getroot()
proj.set("image_name", os.path.join(out_dir, "topshop.bin"))
proj.set("cue_sheet", os.path.join(out_dir, "topshop.cue"))

for lic in proj.iter("license"):
    lic.set("file", os.path.join(root_dir, lic.get("file")))


def sector_count(path, file_type):
    size = os.path.getsize(path)
    if file_type in ("mixed", "xa", "str"):
        return size // XA_SECTOR
    return (size + 2047) // 2048


def replace_voices(parent, src_dir):
    """Put DIR's files in place of the disc's in parent (a directory_tree or a dir), recursively."""
    for i, el in reversed(list(enumerate(parent))):
        if el.tag == "dir":
            replace_voices(el, os.path.join(src_dir, el.get("source", el.get("name"))))
            continue
        if el.tag != "file":
            continue
        name = el.get("name")
        new = os.path.join(voices, name)
        if not os.path.isfile(new):
            continue
        old_sectors = sector_count(os.path.join(src_dir, el.get("source", name)), el.get("type", "data"))
        new_sectors = sector_count(new, "mixed")
        if os.path.getsize(new) % XA_SECTOR or new_sectors == 0:
            sys.exit("mkdisc: %s is not a whole number of %d-byte XA sectors" % (new, XA_SECTOR))
        if new_sectors > old_sectors:
            sys.exit("mkdisc: %s is %d sectors, more than the disc's %d: the files after it would move"
                     % (new, new_sectors, old_sectors))
        el.set("source", new)
        el.set("type", "mixed")
        if new_sectors < old_sectors:
            pad = ET.Element("dummy", {"sectors": str(old_sectors - new_sectors), "type": "0"})
            pad.tail = el.tail
            parent.insert(i + 1, pad)


for tree_el in proj.iter("directory_tree"):
    tree_el.set("source", os.path.join(root_dir, tree_el.get("source")))
    for f in tree_el.findall("file"):
        if f.get("name") == "SLUS_013.33":
            f.set("source", os.path.abspath(exe))
    if voices:
        replace_voices(tree_el, tree_el.get("source"))

tree.write(out_xml, encoding="utf-8", xml_declaration=True)
