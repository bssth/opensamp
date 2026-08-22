# Dear ImGui — vendored copy

Upstream: <https://github.com/ocornut/imgui>
Commit:   `eaa32bb787574510baed7f73a1010ea7347ff202`
Describe: `v1.92.5-141-geaa32bb78` (`IMGUI_VERSION` reports `1.92.6 WIP`)
License:  MIT — see [LICENSE.txt](LICENSE.txt)

These files are **byte-identical to upstream**. Keep it that way: if a change
to Dear ImGui is ever needed, prefer a wrapper in `native/gui/`, and if the
change genuinely has to live here, add a `MODIFIED BY OPENSAMP` block at the
top of the file and list it in this document. Otherwise the next update turns
into an archaeology exercise.

> An earlier revision of this repository carried imgui as a git submodule
> pinned to a local commit that renamed `imgui.h` to `imgui_manager.h`. That
> commit existed on exactly one workstation, so `git submodule update --init`
> failed for everyone else. The rename has been reverted and the submodule
> replaced by this vendored copy.

## What is vendored

Only what the build compiles, plus the headers those files need:

| | |
| --- | --- |
| Core | `imgui.cpp`, `imgui_draw.cpp`, `imgui_tables.cpp`, `imgui_widgets.cpp`, `imgui_demo.cpp` |
| Headers | `imgui.h`, `imgui_internal.h`, `imconfig.h`, `imstb_rectpack.h`, `imstb_textedit.h`, `imstb_truetype.h` |
| Backends | `backends/imgui_impl_dx9.{cpp,h}`, `backends/imgui_impl_win32.{cpp,h}` |

Deliberately **not** vendored: `examples/`, `docs/`, `misc/`, the other twenty
backends, and the `.github` workflows. Upstream's full tree is 8.2 MB and 268
files; this subset is 3.8 MB and 16 files, and nothing else is reachable from a
Win32 + D3D9 build.

`imgui_demo.cpp` is kept on purpose — the demo window is a genuinely useful
debugging tool when the overlay misbehaves.

## Updating

1. Pick an upstream tag and check out that commit somewhere.
2. Copy the sixteen files above over this directory. Do not copy anything else.
3. Update the commit hash, describe string and version line at the top of this
   file.
4. Rebuild. Breaking changes usually surface in `native/gui/imgui_manager.cpp`
   and `native/d3d.cpp` first; upstream keeps a migration log at the top of
   `imgui.cpp`.
