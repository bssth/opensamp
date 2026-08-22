# Third-party notices

OpenSAMP itself is licensed under the **GNU General Public License v3.0**
(see [LICENSE](LICENSE)). It bundles, links, or vendors the third-party
components listed below, each of which keeps its own upstream license.

The statements here are based on the license headers and files present in the
sources we actually ship. Where a component's licensing is unclear, that is
recorded explicitly under [Open questions](#open-questions) rather than glossed
over.

Everything third-party is **vendored** — checked into this repository as
ordinary files. There are no git submodules. `git clone` gives you a tree that
builds.

| Component | Where | Upstream | License |
| --- | --- | --- | --- |
| RakNet 3.x, SA-MP-compatible fork (`sampraknet`) | `native/vendor/sampraknet/` | [mishpro-programm/sampraknet](https://github.com/mishpro-programm/sampraknet) `e891acf` | Multi-licensed by Rakkarsoft — OpenSAMP elects the **GPL v2-or-later** option |
| Dear ImGui | `native/vendor/imgui/` | [ocornut/imgui](https://github.com/ocornut/imgui) `eaa32bb7` (v1.92.5+) | MIT |
| MinHook | `native/vendor/minhook.h`, `native/lib/MinHook.x86.lib` | [TsudaKageyu/minhook](https://github.com/TsudaKageyu/minhook) | BSD-2-Clause |

Per-component provenance, the exact upstream commits, and the list of local
modifications live next to the code, in
[`native/vendor/sampraknet/OPENSAMP-NOTICE.md`](native/vendor/sampraknet/OPENSAMP-NOTICE.md)
and [`native/vendor/imgui/UPSTREAM.md`](native/vendor/imgui/UPSTREAM.md).

---

## Why OpenSAMP is copyleft and cannot be relicensed permissively

This is the single most important consequence of the dependency set, so it is
stated up front.

`native/vendor/sampraknet` is a fork of **RakNet 3.x** — the pre-2014,
Rakkarsoft-era codebase, *not* the 2014 BSD-licensed RakNet 4 release that
Oculus put out. Every RakNet source file we compile carries this header:

> ```
> /// This file is part of RakNet Copyright 2003 Kevin Jenkins.
> ///
> /// Usage of RakNet is subject to the appropriate license agreement.
> /// Creative Commons Licensees are subject to the
> /// license found at
> /// http://creativecommons.org/licenses/by-nc/2.5/
> /// Single application licensees are subject to the license found at
> /// http://www.rakkarsoft.com/SingleApplicationLicense.html
> /// Custom license users are subject to the terms therein.
> /// GPL license users are subject to the GNU General Public
> /// License as published by the Free
> /// Software Foundation; either version 2 of the License, or (at your
> /// option) any later version.
> ```

That is a four-way choice, and only one of the four options is a free-software
licence:

- **Creative Commons BY-NC 2.5** — non-commercial only, not an open-source
  licence, and incompatible with distributing a general-purpose client.
- **Single-application / custom commercial licence** — must be bought from a
  vendor that no longer exists in that form.
- **GNU GPL, version 2 *or, at your option, any later version*.**

OpenSAMP takes the **GPL option**. Because that grant is "v2 or later", GPL-3.0
is an allowed choice, which is why [LICENSE](LICENSE) is GPL-3.0.

**Practical consequences for contributors:**

1. OpenSAMP **cannot** be relicensed to MIT, BSD, Apache-2.0, or any other
   permissive licence while it links this RakNet fork. Do not open a PR
   proposing that.
2. Any binary distribution of OpenSAMP must ship complete corresponding source,
   including the vendored RakNet sources.
3. If the project ever wants a permissive licence, RakNet must be replaced —
   e.g. by a clean-room implementation of the SA-MP transport on top of a
   BSD/MIT UDP stack. That is an epic, not a checkbox.

---

## Components

### 1. RakNet 3.x — SA-MP-compatible fork (`sampraknet`)

- **Path:** `native/vendor/sampraknet/` (vendored)
- **Upstream:** <https://github.com/mishpro-programm/sampraknet> (`v1.1-1-ge891acf`)
- **Local modifications:** `src/RakPeer.cpp` only — an SEH trampoline around
  RPC dispatch plus assert removal. Carries the modification notice GPLv3 §5(a)
  requires; details in `OPENSAMP-NOTICE.md` next to the code.
- **Original author:** Kevin Jenkins / Rakkarsoft LLC, Copyright 2003.
  One file (`src/SocketLayer.cpp`) is attributed to "Rakkarsoft" rather than to
  Kevin Jenkins personally; the licence block is identical.
- **License:** as quoted above — OpenSAMP relies on the GPL-v2-or-later option.

56 of the vendored source files carry the RakNet licence header. The remaining
files fall into two groups.

**a) Third-party code bundled inside RakNet upstream:**

| File(s) | Origin | Stated terms |
| --- | --- | --- |
| `rijndael.h`, `rijndael-boxes.h`, `rijndael.cpp` | `rijndael-alg-fst` v2.0 (Aug 1999), taken from the *aescrypt* project | Header points at a file named `LICENSE-EST` — **that file is not present in the fork** (see Open questions) |
| `RSACrypt.h`, `Types.h` | catid (`cat02e@fsu.edu`), 2004 | No explicit licence header; shipped as part of RakNet |
| `SHA1.h`, `SHA1.cpp` | Dominik Reichl | Declared "100% free public domain implementation" in the header |

**b) SA-MP compatibility code added by the fork:**

`SAMPAuth.{h,cpp}`, `SAMPNetEncr.{h,cpp}`, `SAMPRPC.{h,cpp}`, `SAMP_VER.h`.
These carry no licence header of their own. `SAMPNetEncr.cpp` is annotated
"Updated to 0.3.7 by P3ti".

These implement SA-MP's datagram obfuscation (`kyretardizeDatagram`, which
every packet passes through) and a table of 512 challenge/response key pairs
used during connect. Both are **derived from SA-MP**, and both are required to
talk to a server at all.

Vendoring puts them in OpenSAMP's own tree, which sits in tension with the
clean-room rule in [CONTRIBUTING.md](CONTRIBUTING.md). The project's position,
stated rather than left implicit: these are protocol constants needed for
interoperability — the same category as a codec's magic numbers or a format's
signature bytes — not reimplemented SA-MP logic. **No SA-MP client code exists
anywhere in this repository.** The reasoning, and the fallback if it ever has
to change, are set out in
[`native/vendor/sampraknet/OPENSAMP-NOTICE.md`](native/vendor/sampraknet/OPENSAMP-NOTICE.md).

### 2. Dear ImGui

- **Path:** `native/vendor/imgui/` (vendored)
- **Upstream:** <https://github.com/ocornut/imgui>, commit `eaa32bb7`
  (`v1.92.5-141`)
- **License:** MIT — full text in `native/vendor/imgui/LICENSE.txt`
  (Copyright (c) 2014-2026 Omar Cornut). MIT is GPL-compatible.
- **Local modifications:** none. The files are byte-identical to upstream, and
  `UPSTREAM.md` next to them explains how to keep it that way.
- Only the sixteen files a Win32 + D3D9 build actually needs are vendored.
  `examples/`, `docs/`, `misc/` and the other twenty backends are not, so
  neither GLFW nor SDL nor any other example dependency is shipped here.

### 3. MinHook

- **Path:** `native/vendor/minhook.h` (header),
  `native/lib/MinHook.x86.lib` (prebuilt static library)
- **Upstream:** <https://github.com/TsudaKageyu/minhook>
- **License:** BSD-2-Clause, Copyright (C) 2009-2017 Tsuda Kageyu.
  Full text, including the bundled Hacker Disassembler Engine notice
  (Copyright (c) 2008-2009, Vyacheslav Patkov), is in
  [`native/vendor/MinHook.LICENSE.txt`](native/vendor/MinHook.LICENSE.txt).
  BSD-2-Clause is GPL-compatible.
- Because we ship a **prebuilt** `.lib`, the HDE notice must travel with any
  binary distribution — that is why the licence file lives in the repo rather
  than only in the header comment.

### 4. Microsoft redistributables

`redist/` is **not** tracked in git. The Visual C++ runtime is a Microsoft
redistributable that end users obtain from Microsoft; see
[docs/build.md](docs/build.md). Earlier revisions of this repository also
carried a .NET desktop runtime installer and the `comhost`/`ijwhost`/`nethost`
hosting DLLs — leftovers from a .NET-based launcher. The solution today builds
two native C++ projects only, nothing references them, and they have been
removed.

### 5. GTA: San Andreas

OpenSAMP is a client modification. **No Rockstar Games code, data, or asset is
included in, or distributed by, this repository**, and none ever may be. Users
supply their own legally obtained copy of GTA: San Andreas (US 1.0). The memory
addresses in `native/addresses.h` are interface facts about a binary, derived
from public reverse-engineering work; their provenance is tracked in
[docs/offsets.md](docs/offsets.md).

A pre-commit guard that rejects game assets ships in
[`tools/git-hooks/pre-commit`](tools/git-hooks/pre-commit); see
[CONTRIBUTING.md](CONTRIBUTING.md) for how to enable it.

---

## Open questions

These are known gaps. They are tracked in [TODO.md](TODO.md) and should be
closed before any binary release.

1. **The SA-MP-derived files carry no licence headers.** `SAMPAuth`,
   `SAMPNetEncr` and `SAMPRPC` came from the fork with no stated terms, and
   the auth key table is derived from SA-MP. The project's position is set out
   above and in `OPENSAMP-NOTICE.md`; it is a considered position, not a legal
   opinion. If it has to change, the fallback is to move the key table out of
   the repository and load it from a user-supplied file at runtime.
2. **`LICENSE-EST` is referenced but missing.** `rijndael.h` points at a licence
   file that upstream never shipped. The `rijndael-alg-fst` code is published
   under permissive terms, but we should carry the actual text rather than a
   dangling reference.

Resolved since the first draft of this document: the `imgui` submodule pin that
did not exist upstream, and the uncommitted local patch to `RakPeer.cpp`. Both
went away when the submodules were replaced by vendored copies — the patch is
now an ordinary tracked change with a modification notice.
