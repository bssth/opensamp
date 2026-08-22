#pragma once

// Installs hooks on user32 input APIs so GTA's input layer can be sealed off
// while an ImGui modal (chat / server dialog) is up.
//
// Why API hooks instead of WndProc swallowing: GTA:SA reads mouse via periodic
// SetCursorPos(window_center) re-centering, and polls keyboard through
// GetAsyncKeyState — both bypass our window procedure entirely.

bool InstallInputGuards();
