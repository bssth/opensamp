# MinHook — vendored copy

Upstream: <https://github.com/TsudaKageyu/minhook>
Commit:   `d94c64d32ea37bc4f5ee47d580709f70c6fb6080` (`master`, 2026-06-13)
Release:  post-`v1.3.3`; upstream has not tagged since 2017-01
License:  BSD-2-Clause — see [LICENSE.txt](LICENSE.txt), which also carries the
          Hacker Disassembler Engine notice that MinHook's own sources require

These files are **byte-identical to upstream**. Keep it that way — the same
rule as `vendor/imgui`: prefer a wrapper over a local edit, and if a change
genuinely has to live here, mark it `MODIFIED BY OPENSAMP` and list it below.

## Why source rather than the prebuilt library

An earlier revision of this repository linked `native/lib/MinHook.x86.lib`.
That was an **import** library, not a static one — its only archive member was
`MinHook.x86.dll/` — so `Client.Native.dll` carried a hard import on
`MinHook.x86.dll`, and that DLL lived only in `bin/`, which is not tracked and
is actively rejected by the commit hook and by CI. A fresh clone therefore
compiled and linked and produced a DLL that could not load. Nobody noticed,
because linking only needs the import library.

Building from source removes the runtime dependency entirely, which is worth
having on its own for a DLL injected into someone else's process, and it
settles a provenance question that could not otherwise be answered: the header
in the tree came from `master`, and what the prebuilt DLL was built from was
not recorded anywhere.

## Why `master` and not `v1.3.3`

`v1.3.3` is from January 2017 and upstream has not tagged since, but `master`
carries real fixes to the code this project depends on:

- `07c7c41` — `JMP_REL`, `JMP_REL_SHORT` and `JCC_REL` operands were stored
  unsigned, which produced a wrong trampoline whenever the offset was
  negative. Directly relevant here: this hooks a 32-bit binary at fixed
  addresses, where backward relative jumps are ordinary.
- `4a45552`, `96c2309`, `acc8f7d`, `df15207` — thread enumeration and freezing
  fixes, including the single-thread case and `OpenThread` failure handling.
- `974c5ef` — report allocation failure during thread freezing instead of
  continuing.

The header already in the tree was from `master` too, so this also aligns the
declarations with the implementation rather than mixing a `master` header with
an unknown binary.

## What is vendored

Upstream's whole `src/` and `include/`, plus the licence. That is everything
the library is — thirteen files, ~130 KB.

| | |
| --- | --- |
| Public header | `include/MinHook.h` |
| Core | `src/hook.c`, `src/buffer.{c,h}`, `src/trampoline.{c,h}` |
| Disassembler | `src/hde/hde32.{c,h}`, `src/hde/hde64.{c,h}`, `src/hde/table32.h`, `src/hde/table64.h`, `src/hde/pstdint.h` |

Both HDE variants are compiled; each is guarded on `_M_X64` internally and the
inactive one collapses to nothing, which is what upstream's own project does.
Deliberately not vendored: `build/`, `dll_resources/`, `test/`, `.github/`.

## Updating

1. Check out the upstream commit you want somewhere.
2. Copy `include/`, `src/` and `LICENSE.txt` over this directory. Nothing else.
3. Update the commit hash and date at the top of this file, and say here what
   changed if it is not a pure refresh.
4. Rebuild and confirm `dumpbin /dependents bin\Client.Native.dll` still lists
   no `MinHook` entry — a stray import means the prebuilt library came back.
