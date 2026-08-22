# OpenSAMP

A clean-room, open-source reimplementation of the **SA-MP**
(San Andreas Multiplayer) client for GTA: San Andreas (US 1.0).
Modern C++ with an ImGui overlay over D3D9.

OpenSAMP targets [**open.mp**](https://www.open.mp/) — the open-source,
backward-compatible SAMP server. Because open.mp documents the wire
protocol, the client is written from scratch and shares no code with the
original SA-MP client.

> **Status:** early alpha / work in progress.
> Connecting works, chat and dialogs work, and on-foot synchronization runs at
> 20 Hz. Vehicle synchronization is partial: the driver path is implemented but
> untested in game, and passenger / unoccupied / trailer sync are missing.
> No public builds are released at this time.

---

## What's in the box

- **`launcher/`** — small launcher that starts `gta_sa.exe` and injects
  the native DLL into it.
- **`native/`** — the DLL that lives inside the game process:
  - memory hooks and patches for GTA SA US 1.0 (via MinHook);
  - ImGui overlay on top of D3D9: chat, dialogs, debug windows;
  - networking layer on top of `sampraknet` (a RakNet fork used for
    SAMP wire-protocol compatibility);
  - connection state machine;
  - built-in crash dumper (minidump into the `crash/` folder).

## Requirements

- Windows 10/11, Visual Studio 2022 (toolset v143, x86).
- No submodules and nothing to fetch: [`imgui`](https://github.com/ocornut/imgui),
  [`sampraknet`](https://github.com/mishpro-programm/sampraknet) and
  [`MinHook`](https://github.com/TsudaKageyu/minhook) are vendored under
  `native/vendor/`, with their provenance and licences recorded there.
- Target game: **GTA: San Andreas, US 1.0** — other versions are not
  supported. You supply your own copy; nothing from Rockstar is distributed
  here.

## Building and running

Full instructions are in **[docs/build.md](docs/build.md)**. In short:

```bash
git clone <repo-url>
git config core.hooksPath tools/git-hooks
```

Put your GTA SA US 1.0 install in `bin/` — that is both the game directory and
the build output directory — then build `opensamp.sln` as **Win32**. The
launcher lands at `bin/opensamp.exe`; run it, and it starts the game and
injects `bin/Client.Native.dll`.

Connect in-game with `//connect <ip> <port> [nick]`.

## License

OpenSAMP is licensed under the **GNU General Public License v3.0**.
See [LICENSE](LICENSE) for the full text. Because OpenSAMP is a clean-room
reimplementation (not a SAMP fork), it is not bound by SAMP's license.

The copyleft is not merely a preference. The vendored RakNet fork
(`vendor/sampraknet`) is available to us only under its GPL-v2-or-later option,
so OpenSAMP cannot be relicensed permissively while it depends on that library.
Vendored third-party code keeps its own upstream license; all of it is
inventoried in [THIRD-PARTY-NOTICES.md](THIRD-PARTY-NOTICES.md), including the
gaps that still need closing.

## Roadmap and open work

See [TODO.md](TODO.md). It is a working document and it is candid about what is
broken.

## Contributing

See [CONTRIBUTING.md](CONTRIBUTING.md) and
[CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md).

Two rules matter more than the rest: **no SA-MP client code may ever be pasted
into this project** — the clean-room provenance is the whole legal basis for it
existing — and **no GTA game asset may ever be committed**. A pre-commit hook
enforces the second one.
