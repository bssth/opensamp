# Building OpenSAMP

Windows only, x86 only, GTA: San Andreas **US 1.0** only. Those are hard
constraints, not defaults — see [TODO.md](../TODO.md) for why other game builds
are an explicit non-goal.

## Prerequisites

| Requirement | Notes |
| --- | --- |
| Windows 10 or 11 | |
| Visual Studio 2022, or VS 2022 Build Tools | Workload: *Desktop development with C++*, toolset **v143** |
| DirectX SDK / Windows SDK with `d3d9.h` | Ships with the C++ workload |
| Git | Nothing to fetch beyond the clone — all dependencies are vendored |
| A legally obtained GTA: San Andreas, **US 1.0** | Not supplied, not downloadable from here |

The Visual C++ runtime is *not* vendored in this repository. Release builds
link `/MD`, so end users need the
[Microsoft Visual C++ Redistributable (x86)](https://aka.ms/vs/17/release/vc_redist.x86.exe).
Developers already have it via Visual Studio.

## Clone

```bash
git clone <repo-url>
```

That is the whole dependency step. Dear ImGui, sampraknet and MinHook are
vendored under `native/vendor/`; there are no submodules to initialise. See
[THIRD-PARTY-NOTICES.md](../THIRD-PARTY-NOTICES.md) for what each one is and
which licence it arrives under.

Then enable the commit guard once — it stops game assets and oversized files
from entering history:

```bash
git config core.hooksPath tools/git-hooks
```

## Lay out the game directory

`bin/` is both the build output directory and the game directory. Both projects
write there:

| Project | Output |
| --- | --- |
| `native` | `bin/Client.Native.dll` |
| `launcher` | `bin/opensamp.exe` |

So `bin/` must contain your GTA SA US 1.0 install — `gta_sa.exe`, `models/`,
`data/`, `audio/`, `anim/`, `scripts/`. The launcher resolves everything
relative to its own location: it starts `gta_sa.exe` from its own directory and
injects `Client.Native.dll` from there too. There is no configurable path and
no registry lookup.

`bin/` is `.gitignore`d. It must stay that way — it contains Rockstar's
proprietary assets.

## Build

### From the IDE

Open `opensamp.sln`, select **Debug|Win32** or **Release|Win32**, build the
solution. `x64` is not a supported configuration.

### From the command line

Building the solution is the easy path, because it sets `$(SolutionDir)` for
you:

```bash
msbuild opensamp.sln /p:Configuration=Debug /p:Platform=Win32 /m
```

If you build a single project directly, you **must** pass `SolutionDir`
yourself, with a trailing separator:

```bash
msbuild native/native.vcxproj /p:Configuration=Debug /p:Platform=Win32 /p:SolutionDir=C:\src\opensamp\ /m
```

Omit it and `$(SolutionDir)` falls back to the project directory, so the DLL
lands in `native/bin/` instead of the game folder — the build succeeds and the
launcher then injects a stale DLL, which is a confusing way to lose an hour.

With Build Tools rather than full Visual Studio, `msbuild` is not on `PATH`:

```
"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\MSBuild\Current\Bin\MSBuild.exe"
```

A clean build takes well under a minute and must produce **0 errors and 0
warnings**.

## Run

Launch `bin/opensamp.exe`. It starts `gta_sa.exe`, waits for the process to
initialise, and injects `Client.Native.dll` via `LoadLibraryW` in a remote
thread. Do not start `gta_sa.exe` yourself — nothing will be injected.

In-game:

- `//connect <ip> <port> [nick]` — connect to a server.
- `/<command>` — sent to the server as an RPC.
- `//<command>` — reserved for local client commands.

## When it crashes

The client installs a crash dumper that writes a minidump to `crash/`. Two
helper scripts in the repository root turn one into something readable:

```bash
python parse_dmp.py crash/<file>.dmp
python parse_stack.py
```

`OpenSamp.log` and `GameReady.log` are currently written next to
`gta_sa.exe`, i.e. into `bin/`. Moving them to `%LOCALAPPDATA%` is on the
roadmap.

## If the DLL fails to load

The launcher reports `LoadLibraryW returned NULL` with a remote
`GetLastError`. In practice it is almost always one of:

- the DLL was built for x64 rather than Win32;
- the Visual C++ runtime is missing on that machine;
- antivirus quarantined the DLL or blocked the remote thread — code that
  injects into another process looks exactly like malware to a heuristic
  scanner, and this one is not signed.
