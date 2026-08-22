#!/usr/bin/env python3
"""Minimal minidump parser: prints exception code, address, and thread context.

We don't have cdb/windbg on the box, so we crack open the file format directly
to pull the exception record and the crashing thread's EIP + a slice of stack.
Enough to correlate the crash site with an RPC trail from OpenSamp.log.
"""

import struct
import sys
from pathlib import Path

STREAM_TYPES = {
    3:  "ThreadList",
    4:  "ModuleList",
    5:  "MemoryList",
    6:  "Exception",
    7:  "SystemInfo",
    9:  "Memory64List",
    15: "MemoryInfoList",
    16: "ThreadInfoList",
}

EXCEPTION_CODES = {
    0xC0000005: "ACCESS_VIOLATION",
    0xC0000094: "INT_DIVIDE_BY_ZERO",
    0xC00000FD: "STACK_OVERFLOW",
    0xC000001D: "ILLEGAL_INSTRUCTION",
    0xC0000025: "NONCONTINUABLE_EXCEPTION",
    0xC0000096: "PRIV_INSTRUCTION",
    0x80000003: "BREAKPOINT",
    0x80000004: "SINGLE_STEP",
    0xC0000409: "STACK_BUFFER_OVERRUN",
    0xE06D7363: "CPP_EXCEPTION",
}


def parse(path: Path):
    data = path.read_bytes()
    sig, ver, nstreams, stream_rva, chksum, ts, flags = struct.unpack_from(
        "<IIIIIIQ", data, 0
    )
    assert sig == 0x504D444D, f"Not a minidump: sig={sig:#x}"

    print(f"=== {path.name} ({len(data):,} bytes) ===")
    print(f"streams: {nstreams}  timestamp: {ts:#x}")

    # Stream directory
    streams = {}
    for i in range(nstreams):
        stype, dsize, drva = struct.unpack_from("<III", data, stream_rva + i * 12)
        streams[stype] = (dsize, drva)

    # Exception stream
    if 6 in streams:
        dsize, drva = streams[6]
        thread_id, pad = struct.unpack_from("<II", data, drva)
        rec_off = drva + 8
        (
            ex_code, ex_flags, ex_record, ex_addr, nparams, _align,
        ) = struct.unpack_from("<IIQQII", data, rec_off)

        # MINIDUMP_LOCATION_DESCRIPTOR after the fixed MINIDUMP_EXCEPTION
        # (EXCEPTION size = 8+8+8+4+4+15*8 = 152; but field packing is
        # actually 8*4 + 8*2 + 15*8 per Win SDK; easier: use fixed layout)
        # Actually the MINIDUMP_EXCEPTION_RECORD is:
        #   ExceptionCode:DWORD, Flags:DWORD, ExceptionRecord:DWORD64,
        #   ExceptionAddress:DWORD64, NumberParameters:DWORD,
        #   __unusedAlignment:DWORD, ExceptionInformation[15]:DWORD64
        # That's 4+4+8+8+4+4+15*8 = 152 bytes.
        # Then ThreadContext:MINIDUMP_LOCATION_DESCRIPTOR (8 bytes: size, rva)
        ctx_size, ctx_rva = struct.unpack_from(
            "<II", data, rec_off + 152
        )

        ex_name = EXCEPTION_CODES.get(ex_code, "?")
        print(f"\n-- Exception --")
        print(f"  thread    : {thread_id}")
        print(f"  code      : {ex_code:#010x}  ({ex_name})")
        print(f"  address   : {ex_addr:#010x}")
        print(f"  flags     : {ex_flags}")
        if nparams:
            # ExceptionInformation[] sits right after the 32-byte fixed header.
            params = struct.unpack_from(
                f"<{nparams}Q", data, rec_off + 32
            )
            print(f"  params    : {[hex(p) for p in params]}")
            if ex_code == 0xC0000005 and nparams >= 2:
                kind = {0: "read", 1: "write", 8: "execute"}.get(params[0], "?")
                print(f"    -> {kind} fault at {params[1]:#010x}")

        # Parse x86 CONTEXT for EIP/ESP etc.
        # x86 CONTEXT layout (CONTEXT_FULL): ContextFlags @0, then Dr0..Dr7,
        # FloatSave, SegGs..SegSs, Edi,Esi,Ebx,Edx,Ecx,Eax,Ebp,Eip,SegCs,EFlags,
        # Esp,SegSs,ExtendedRegisters. For x86 it's 716 bytes.
        if ctx_rva and ctx_size >= 0xCC:
            ctx = data[ctx_rva : ctx_rva + ctx_size]
            # ContextFlags
            ctx_flags = struct.unpack_from("<I", ctx, 0)[0]
            print(f"  ctx_flags : {ctx_flags:#010x}")
            # integer regs start at offset 156 (0x9C) for x86 CONTEXT
            # Edi, Esi, Ebx, Edx, Ecx, Eax, Ebp, Eip, SegCs, EFlags, Esp, SegSs
            try:
                edi, esi, ebx, edx, ecx, eax, ebp, eip, segcs, eflags, esp = (
                    struct.unpack_from("<IIIIIIIIIII", ctx, 0x9C)
                )
                print(f"\n-- x86 registers --")
                print(f"  EAX={eax:08X}  EBX={ebx:08X}  ECX={ecx:08X}  EDX={edx:08X}")
                print(f"  ESI={esi:08X}  EDI={edi:08X}  EBP={ebp:08X}  ESP={esp:08X}")
                print(f"  EIP={eip:08X}  EFL={eflags:08X}")
            except struct.error:
                print("  (x86 context decode failed)")

    # ModuleList — find which module the crash address falls into
    if 4 in streams and 6 in streams:
        dsize, drva = streams[4]
        nmod = struct.unpack_from("<I", data, drva)[0]
        mods = []
        for i in range(nmod):
            off = drva + 4 + i * 108  # MINIDUMP_MODULE is 108 bytes
            base, size, chksum, ts, name_rva = struct.unpack_from(
                "<QIIIII", data, off
            )[:5]
            # Read MINIDUMP_STRING: length:DWORD then UTF-16LE
            slen = struct.unpack_from("<I", data, name_rva)[0]
            name = data[name_rva + 4 : name_rva + 4 + slen].decode("utf-16-le", "replace")
            mods.append((base, size, name))
        mods.sort()

        # Find module for exception address
        ds_, dr_ = streams[6]
        rec_off = dr_ + 8
        _, _, _, ex_addr, *_ = struct.unpack_from("<IIQQII", data, rec_off)

        for base, size, name in mods:
            if base <= ex_addr < base + size:
                print(f"\n-- Crash module --")
                print(f"  {Path(name).name}  [base={base:#010x} size={size:#x}]")
                print(f"  offset in module: {ex_addr - base:#x}")
                break
        else:
            print("\n(crash address not in any known module)")


if __name__ == "__main__":
    paths = sys.argv[1:]
    if not paths:
        crash_dir = Path("D:/dev/opensamp/bin/crash")
        paths = sorted(crash_dir.glob("*.dmp"))[-1:]
    for p in paths:
        parse(Path(p))
