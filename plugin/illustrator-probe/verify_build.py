"""Inspect local artifacts without loading the plugin or operating Illustrator."""
import hashlib
import json
from pathlib import Path
import struct

root = Path(__file__).resolve().parent
output = root / "build"
blob = (output / "plugin.pipl").read_bytes()
cursor = 0

def u32():
    global cursor
    value = struct.unpack_from(">I", blob, cursor)[0]
    cursor += 4
    return value

assert u32() == 1, "one PiPL expected"
assert u32() == 0, "unexpected PiPL version"
properties = {}
for _ in range(u32()):
    vendor = blob[cursor:cursor + 4]
    key = blob[cursor + 4:cursor + 8]
    cursor += 8
    prop_id, length = u32(), u32()
    assert cursor + length <= len(blob), "truncated PiPL property"
    properties[key] = blob[cursor:cursor + length].rstrip(b"\0")
    cursor += length
assert cursor == len(blob), "unexpected trailing PiPL bytes"
assert properties[b"pinm"] == b"StrokeMasterProposedProbe", "wrong script-visible plugin name"
assert properties[b"wx86"] == b"PluginMain", "wrong entry point"
binary = (output / "StrokeMasterProposedProbe.aip").read_bytes()
pe = struct.unpack_from("<I", binary, 0x3C)[0]
assert binary[pe:pe + 4] == b"PE\0\0"
assert struct.unpack_from("<H", binary, pe + 4)[0] == 0x8664, "not x64"
assert struct.unpack_from("<H", binary, pe + 22)[0] & 0x2000, "not a DLL"
assert blob in binary, "generated PiPL not embedded verbatim"
record = {
    "status": "PROPOSED: compile-only, not loaded",
    "plugin": properties[b"pinm"].decode("ascii"),
    "entryPoint": properties[b"wx86"].decode("ascii"),
    "architecture": "x64",
    "sha256": hashlib.sha256(binary).hexdigest(),
    "piplEmbedded": True,
}
(output / "artifact.json").write_text(json.dumps(record, indent=2) + "\n", encoding="utf-8")
print(json.dumps(record))
