# sampraknet — vendored copy, and the licence election that permits it

Upstream: <https://github.com/mishpro-programm/sampraknet>
Commit:   `e891acf76dc4b95ad4fe95b353df2879394a445c` (`v1.1-1-ge891acf`)

This directory used to be a git submodule. It is now vendored, for three
reasons: upstream is a 2003 codebase that will never publish another release
we want; OpenSAMP has to patch it, and a patch nobody can see is a patch
nobody can review; and the GPL obligation to ship complete corresponding
source is satisfied by construction once the source is simply *here*.

## Which licence, and why it is not a free choice

`sampraknet` is a fork of **RakNet 3.x** by Kevin Jenkins / Rakkarsoft, 2003 —
*not* the RakNet 4 that Oculus released under BSD in 2014. Fifty-six of the
files here carry this header:

```
/// This file is part of RakNet Copyright 2003 Kevin Jenkins.
///
/// Usage of RakNet is subject to the appropriate license agreement.
/// Creative Commons Licensees are subject to the
/// license found at
/// http://creativecommons.org/licenses/by-nc/2.5/
/// Single application licensees are subject to the license found at
/// http://www.rakkarsoft.com/SingleApplicationLicense.html
/// Custom license users are subject to the terms therein.
/// GPL license users are subject to the GNU General Public
/// License as published by the Free
/// Software Foundation; either version 2 of the License, or (at your
/// option) any later version.
```

Four options, and only one is a free-software licence. Creative Commons BY-NC
forbids commercial use. The single-application and custom licences came from a
vendor that no longer sells them. That leaves **the GNU GPL, version 2 or, at
your option, any later version** — and "or later" is what makes GPL-3.0
available.

**OpenSAMP hereby exercises the GPL option and distributes this code under
GPL-3.0**, the same licence as the rest of the project. See the repository
root [`LICENSE`](../../../LICENSE), and
[`THIRD-PARTY-NOTICES.md`](../../../THIRD-PARTY-NOTICES.md) for the full
dependency inventory.

Consequences worth stating plainly:

- Original copyright headers must stay **intact**. Do not "clean up" the
  Rakkarsoft banners.
- Every file changed here needs a prominent modification notice and a date —
  GPLv2 §2(a) and GPLv3 §5(a) require it, and the list below has to stay
  accurate.
- OpenSAMP cannot be relicensed permissively while this code is linked in.

## Modifications by OpenSAMP

| File | Date | Change |
| --- | --- | --- |
| `src/RakPeer.cpp` | 2026-08-08 | SEH trampoline `invoke_rpc_handler_safe()` around both RPC dispatch call sites in `HandleRPCPacket()`; `UNDEFINED_RPC_INDEX` and missing-node `RakAssert`s downgraded to silent returns; removed a per-RPC trace that stalled the game at sync rates. |

Everything else is as it came from upstream.

The SEH wrapper is not a nicety. `HandleRPCPacket` constructs a `BitStream` on
the stack, so MSVC refuses `__try` in that frame, and without the trampoline a
single faulting handler takes the whole game process down instead of producing
one log line.

## Third-party code bundled inside RakNet

These arrived with RakNet upstream and are not ours:

| Files | Origin | Terms as stated |
| --- | --- | --- |
| `rijndael.{h,cpp}`, `rijndael-boxes.h` | `rijndael-alg-fst` v2.0 (1999), via the *aescrypt* project | Header points at a `LICENSE-EST` file that upstream never shipped |
| `RSACrypt.h`, `Types.h` | catid, 2004 | No explicit header; distributed as part of RakNet |
| `SHA1.{h,cpp}` | Dominik Reichl | Header declares public domain |

## The SA-MP interoperability files — read this part

`SAMPAuth.{h,cpp}`, `SAMPNetEncr.{h,cpp}`, `SAMPRPC.{h,cpp}` and `SAMP_VER.h`
were added by the fork, carry no licence headers, and exist for one purpose:
speaking SA-MP's wire protocol. `SAMPNetEncr.cpp` is annotated "Updated to
0.3.7 by P3ti".

Two of them deserve to be named explicitly rather than buried:

- **`SAMPNetEncr.cpp`** implements SA-MP's datagram obfuscation
  (`kyretardizeDatagram`). Every packet passes through it —
  `src/SocketLayer.cpp` calls it directly — so it is not optional for
  connecting to anything.
- **`SAMPAuth.cpp`** is a table of 512 challenge/response key pairs for SA-MP
  0.3.7. The server sends a challenge, the client answers with the matching
  key. `native/sampraknet_bridge.cpp` performs that exchange during connect.

These key pairs are **derived from SA-MP**, and vendoring them puts them in
OpenSAMP's own tree. That sits in tension with the clean-room rule in
[`CONTRIBUTING.md`](../../../CONTRIBUTING.md), so the position is stated rather
than left implicit: they are protocol constants required for interoperability,
in the same category as a codec's magic numbers or a file format's signature
bytes — not reimplemented SA-MP logic. **No SA-MP client code exists anywhere
in this repository**, including here.

That is a considered position, not a legal opinion, and it is the single most
questionable artefact in the dependency tree. If it ever has to change, the
fallback is to move the table out of the repository and load it from a
user-supplied file at runtime. Tracked in [`TODO.md`](../../../TODO.md).
