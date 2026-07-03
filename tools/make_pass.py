#!/usr/bin/env python3
"""
make_pass.py - generate a valid PetPal "internet StreetPass" packet + seed URL.

PetPal exchanges 49-byte PetPalPacket structs through teampetpal.com/api/pass.
Use this to seed the pool with a pet (e.g. an official/event pet, or a test
friend) so a single console can verify the receive->friend pipeline.

IMPORTANT: PetPal's CRC-32 (source/util/Crc32.cpp) is NOT zlib-compatible even
though it looks standard - do NOT use zlib.crc32 here. This script replicates the
app's exact algorithm (verified against real hardware). Keep the layout in sync
with include/netpass/PetPalPacket.h and the enums in include/core/Types.h.

Usage:
    python tools/make_pass.py --id 0xB0B0CAFE12345678 --name Pixel \
        --species 0 --level 7 --c1 1 --c2 2
Species indices follow enum Species (Fox=0, Cat=1, Bunny=2, Dragon=3, Slime=4,
Robot=5, Axolotl=6). Prints the ready-to-open seed URL.
"""
import argparse
import struct

_TABLE = []
for _i in range(256):
    _c = _i
    for _ in range(8):
        _c = (0xEDB88820 ^ (_c >> 1)) if (_c & 1) else (_c >> 1)
    _TABLE.append(_c & 0xFFFFFFFF)


def app_crc32(data: bytes) -> int:
    """Exact port of source/util/Crc32.cpp (seed=0). NOT zlib-equivalent."""
    c = 0xFFFFFFFF
    for b in data:
        c = (_TABLE[(c ^ b) & 0xFF] ^ (c >> 8)) & 0xFFFFFFFF
    return c ^ 0xFFFFFFFF


def build_packet(pet_id, name, species, level, c1, c2, c3=0, style=0,
                 favorite=0, gift=0xFFFF, gift_qty=0) -> bytes:
    body = struct.pack("<IHH", 0x50455450, 1, 0)          # magic, version, reserved0
    body += struct.pack("<Q", pet_id)                      # petId
    nm = name.encode("utf-8")[:12]
    body += nm + b"\x00" * (13 - len(nm))                  # name[13]
    body += struct.pack("<BBH", species, 0, level)         # species, stage, level
    body += struct.pack("<BBBB", c1, c2, c3, style)        # colors + style
    body += struct.pack("<HH", favorite, 0)                # favoriteItem, reserved1
    body += struct.pack("<HH", gift, gift_qty)             # giftItem, giftQty
    assert len(body) == 45, len(body)
    return body + struct.pack("<I", app_crc32(body))       # + crc32


def main():
    ap = argparse.ArgumentParser(description="Generate a PetPal pass seed URL.")
    ap.add_argument("--id", required=True, help="pet id, e.g. 0xB0B0CAFE12345678")
    ap.add_argument("--name", required=True)
    ap.add_argument("--species", type=int, default=0)
    ap.add_argument("--level", type=int, default=1)
    ap.add_argument("--c1", type=int, default=0, help="primary color index")
    ap.add_argument("--c2", type=int, default=1, help="secondary color index")
    ap.add_argument("--gift", type=int, default=0xFFFF, help="gift item id (0xFFFF = none)")
    ap.add_argument("--gift-qty", type=int, default=0)
    ap.add_argument("--host", default="https://teampetpal.com")
    args = ap.parse_args()

    pet_id = int(args.id, 0) & 0xFFFFFFFFFFFFFFFF
    pkt = build_packet(pet_id, args.name, args.species, args.level, args.c1,
                        args.c2, gift=args.gift, gift_qty=args.gift_qty)
    print(f"{args.host}/api/pass?id={pet_id:016x}&pkt={pkt.hex()}")


if __name__ == "__main__":
    main()
