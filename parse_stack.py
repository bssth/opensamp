#!/usr/bin/env python3
"""Walk the crashing thread's stack and print any DWORD that looks like a
return address into gta_sa.exe. Gives us a rough call chain without symbols."""

import struct
import sys
from pathlib import Path


def parse(path: Path):
    data = path.read_bytes()
    e_lfanew = struct.unpack_from("<I", data, 0x3C)[0]
    sig, ver, nstreams, stream_rva = struct.unpack_from("<IIII", data, 0)
    assert sig == 0x504D444D

    # Index streams
    streams = {}
    for i in range(nstreams):
        stype, dsize, drva = struct.unpack_from("<III", data, stream_rva + i * 12)
        streams.setdefault(stype, []).append((dsize, drva))

    # Exception stream: get thread id, context (esp, eip)
    dsize, drva = streams[6][0]
    thread_id = struct.unpack_from("<I", data, drva)[0]
    rec_off = drva + 8
    ex_code, ex_flags, _, ex_addr = struct.unpack_from("<IIQQ", data, rec_off)[:4]
    ctx_size, ctx_rva = struct.unpack_from("<II", data, rec_off + 152)
    ctx = data[ctx_rva : ctx_rva + ctx_size]
    edi, esi, ebx, edx, ecx, eax, ebp, eip, segcs, eflags, esp = struct.unpack_from(
        "<IIIIIIIIIII", ctx, 0x9C
    )

    # Module map
    mods = []
    for dsize2, drva2 in streams.get(4, []):
        nmod = struct.unpack_from("<I", data, drva2)[0]
        for i in range(nmod):
            off = drva2 + 4 + i * 108
            base, size, _, _, name_rva = struct.unpack_from("<QIIIII", data, off)[:5]
            slen = struct.unpack_from("<I", data, name_rva)[0]
            name = data[name_rva + 4 : name_rva + 4 + slen].decode("utf-16-le", "replace")
            mods.append((base, size, Path(name).name))

    def module_for(addr):
        for base, size, name in mods:
            if base <= addr < base + size:
                return name, addr - base
        return None, 0

    # Memory64List or MemoryList: find range containing ESP and walk up
    # Memory64List header: NumberOfMemoryRanges(Q), BaseRva(Q)
    mem_ranges = []
    if 9 in streams:
        for dsize2, drva2 in streams[9]:
            nmem, base_rva = struct.unpack_from("<QQ", data, drva2)
            off_in_file = base_rva
            for i in range(nmem):
                st_addr, rng_size = struct.unpack_from(
                    "<QQ", data, drva2 + 16 + i * 16
                )
                mem_ranges.append((st_addr, rng_size, off_in_file))
                off_in_file += rng_size
    if 5 in streams:  # MemoryList
        for dsize2, drva2 in streams[5]:
            nmem = struct.unpack_from("<I", data, drva2)[0]
            for i in range(nmem):
                st_addr, dsz, drv = struct.unpack_from("<QII", data, drva2 + 4 + i * 16)
                mem_ranges.append((st_addr, dsz, drv))

    def read_mem(addr, size):
        for start, rsize, fo in mem_ranges:
            if start <= addr and addr + size <= start + rsize:
                off = fo + (addr - start)
                return data[off : off + size]
        return None

    print(f"=== {path.name} ===")
    print(
        f"EIP={eip:08X}  ESP={esp:08X}  EBP={ebp:08X}  EAX={eax:08X}"
        f"  ECX={ecx:08X}  EDX={edx:08X}  EBX={ebx:08X}"
    )
    m, off = module_for(eip)
    print(f"crash in {m}+{off:#x}")
    print()

    # Walk stack — first frame's return is [esp]. Skim up to 512 DWORDs.
    stack = read_mem(esp, 512 * 4)
    if not stack:
        print("!! stack memory not in dump")
        return

    print("Candidate return addresses on stack:")
    seen = set()
    for i in range(len(stack) // 4):
        addr = struct.unpack_from("<I", stack, i * 4)[0]
        m, off = module_for(addr)
        if m and off and (addr, m) not in seen:
            # Only interesting ranges — .text in gta_sa or our dll
            seen.add((addr, m))
            print(f"  +{i*4:04x}  {addr:08X}  {m}+{off:#x}")


if __name__ == "__main__":
    paths = sys.argv[1:]
    if not paths:
        paths = sorted(Path("D:/dev/opensamp/bin/crash").glob("*.dmp"))[-2:-1]
    for p in paths:
        parse(Path(p))
