# Contributing to OpenSAMP

Thanks for looking. OpenSAMP is a clean-room reimplementation of the SA-MP
client for **GTA: San Andreas US 1.0**, targeting the [open.mp](https://www.open.mp/)
wire protocol. It is early alpha: connect works, synchronisation is partial.

Read [TODO.md](TODO.md) first — it is the real roadmap, and it says honestly
what is broken.

---

## The three rules that are not negotiable

### 1. Clean room. No SA-MP client code, ever.

OpenSAMP's entire legal position rests on being written from scratch against a
documented protocol. One pasted function from a leaked or decompiled SA-MP
client destroys that.

**Allowed sources:**

- open.mp's documented wire protocol and its open-source server.
- **[MTA:SA](https://github.com/multitheftauto/mtasa-blue) — the preferred
  reference, and you may copy from it.** MTA is GPL-3.0, the same licence as
  OpenSAMP, so this is not a grey area: port the code, keep the copyright, and
  cite the file in a comment. The clean-room rule above exists specifically
  against the SA-MP *client*; it does not apply to MTA. When you are unsure how
  something engine-side should be done, look at MTA and follow it rather than
  inventing an approach — the goal is a working client compatible with SA-MP,
  not from-scratch purity for its own sake. In practice MTA usually *calls* an
  engine function at a controlled moment where guesswork reaches for a blunt
  byte patch; the call is almost always the better answer.
- Other public reverse-engineering references for GTA SA
  ([plugin-sdk](https://github.com/DK22Pac/plugin-sdk), GTAModding wiki,
  SilentPatch — with attribution, and respecting their licences).
- Your own reverse engineering, observation of traffic, and black-box testing.

**Not allowed:** copying code out of the SA-MP client, leaked SA-MP sources, or
any decompiler output of `samp.dll` / `gta_sa.exe`. Describing observed
*behaviour* is fine. Transcribing someone's *implementation* is not.

If you learned something from reading a third-party implementation, say so in
the PR. It is far cheaper to discuss it before the code lands than after.

### 2. Never commit game data.

`bin/` holds a real GTA: San Andreas install. Committing `gta_sa.exe`,
`*.img`, `*.dff`, `*.txd`, `*.ide`, `*.ipl`, or `*.col` would publish
Rockstar's proprietary assets. `.gitignore` covers this, and a hook enforces it:

```bash
git config core.hooksPath tools/git-hooks
```

Run that once after cloning. It is the only setup step the repo asks of you.
The hook also rejects any staged file over 5 MB — build junk and installers do
not belong in history.

### 3. OpenSAMP is GPL-3.0, and that is load-bearing.

By contributing you agree your work ships under GPL-3.0. This is not a
preference: the vendored RakNet fork is available to us only under its
GPL-v2-or-later option. See [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md)
for the full reasoning. PRs proposing a permissive relicence will be closed.

---

## Building

See [docs/build.md](docs/build.md). Short version:

```bash
git clone <repo-url>
```

then open `opensamp.sln` in Visual Studio 2022 (toolset v143) and build
**Win32 / x86**. Everything else — where the game goes, how to attach a
debugger — is in `docs/build.md`.

A clean build is **0 errors, 0 warnings**. Do not add warnings; if you must,
explain why in the PR.

---

## Touching vendored code

`native/vendor/` holds third-party source checked in as ordinary files — Dear
ImGui, sampraknet (RakNet 3.x) and MinHook. Editing it is allowed, but it is
not free:

- **Prefer a wrapper.** A change in `native/gui/` costs nothing at update time;
  a change inside `vendor/imgui/` has to be re-applied by hand forever. The
  imgui copy is byte-identical to upstream today, which is why updating it is
  a file copy.
- **If you must edit `vendor/sampraknet/`, add a modification notice** at the
  top of the file with the date and what changed, and add a row to the table in
  `vendor/sampraknet/OPENSAMP-NOTICE.md`. GPLv2 §2(a) and GPLv3 §5(a) require
  this; it is not a stylistic preference. `src/RakPeer.cpp` shows the format.
- **Never remove or reword an upstream copyright header.** The Rakkarsoft
  banners on the RakNet files are load-bearing — they are the grant we rely on.

---

## Working on the offset map

`native/addresses.h` is the single source of truth for GTA SA US 1.0
addresses. Historically the codebase had ~422 magic numbers scattered across 23
files; consolidating them is ongoing work.

When you touch an address:

- **Put it in `addresses.h`**, named, not inline at the call site.
- **Record provenance** in [docs/offsets.md](docs/offsets.md) with the
  attribution tag and a confidence level:
  - `[SAMP]` — inferred from SA-MP's observable behaviour
  - `[MTA]` — cross-checked against MTA:SA's open source
  - `[SP]` — from SilentPatch
  - `[OWN]` — our own reverse engineering
- **Say how you verified it.** "Matches plugin-sdk" and "I watched it in a
  debugger" are both fine; "looks right" is not.

The ~200 opaque byte patches in `native/patches.cpp` are deliberately not yet
enumerated. Enumerating them is welcome, one coherent group at a time — not a
single 200-entry PR.

---

## Code style

The codebase is honestly inconsistent right now: `kick_game_start`,
`gta::sa::GetLocalPlayerPed`, `TickGameReady`, and `g_szNickName` all coexist.
Hungarian notation is inherited from SA-MP-era conventions. Picking one style
and enforcing it via `.clang-format` is on the roadmap.

Until that lands, the rule is simple: **match the file you are editing.** Do not
reformat a file you are otherwise not changing — it destroys `git blame` for
everyone else.

Beyond that:

- Comments and identifiers in **English**. The codebase has stray Russian
  comments; they are being translated, not added to.
- No `using namespace std;` in new code.
- Source files are CP-1251 for historical reasons. Do not put non-ASCII
  characters in string literals — use `->` rather than `→`. See
  `native/util/encoding.h` for the CP-1251 ↔ UTF-8 conversion used on the wire.
- Prefer adding to `native/addresses.h` over another magic constant.

---

## Pull requests

- **One concern per PR.** A sync fix and a refactor of the same file are two
  PRs.
- **Say what you tested.** "Compiles" and "tested in-game against an open.mp
  server" are very different claims, and the difference matters enormously in
  this project — most of the risky code touches live game memory or wire
  formats, where a clean compile proves almost nothing. Be explicit about which
  one you did.
- **Include the failure mode you saw**, if you are fixing a crash: the address,
  the minidump, the repro steps. `parse_dmp.py` and `parse_stack.py` in the
  repo root help turn a `crash/*.dmp` into something readable.
- Keep the first line of a commit message under ~72 characters, imperative
  mood, and explain *why* in the body if it is not obvious.

## Reporting bugs

Include:

- Your `gta_sa.exe` build (must be US 1.0 — other builds are explicitly
  unsupported and reports against them will be closed).
- The server you connected to, and whether it is open.mp or classic SA-MP.
- `OpenSamp.log`, and the `crash/*.dmp` if the game died.
- Whether it reproduces with a clean `bin/` (no other mods loaded).

## Code of conduct

Participation is governed by [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).
