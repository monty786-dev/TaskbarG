# WinShell (C port)

Taskbar + Start menu for Winlator / Wine, ported from the Python `TaskbarG.py` (WinShell v7).
Uses the **same `config.json`** (`%PROGRAMDATA%\WinShell\config.json`), so settings carry over.

## Get the .exe from GitHub
1. Create a repo and push this folder (keep the `.github/workflows/build.yml` path).
2. Open the **Actions** tab, run **Build WinShell.exe** (it also runs on every push).
3. Download the `WinShell-exe` artifact: `WinShell-x64.exe` and `WinShell-x86.exe`.
4. To publish a release: `git tag v7.0.0 && git push --tags` – the exes are attached automatically.

## Build locally
```
sudo apt install gcc-mingw-w64-x86-64 gcc-mingw-w64-i686
make            # dist/WinShell-x64.exe and dist/WinShell-x86.exe
```
On Windows with MSYS2 MinGW: `make native CC=gcc`.

## Run
`WinShell.exe` – starts the taskbar. `--restart` (used by the power menu), `--open <path>`.
Only one instance runs (mutex `WinShell.Taskbar.Mutex`).

## What is ported
| Area | Status |
|---|---|
| config.json load/save, all default keys, migrations, live reload on file change | done |
| Themes (12 built-in + `custom_themes`), accent, bar colour, adaptive text colours, scale, fonts | done |
| Glass look (gradient + gloss + hairlines), layered opacity, rounded bar, detach | done |
| Taskbar: position top/bottom, align left/centre/right, pinned apps, running windows grouped by exe | done |
| Shell-hook + timer window tracking, flashing windows, active underline, icon accent colour | done |
| Click to launch/minimise/restore, window list for multi-window apps, middle-click launch | done |
| Drag to reorder pinned apps (saved to config) | done |
| Right-click menus (entry menu, bar menu), pin/unpin, always on top, end task, open file location | done |
| Clock (format, India/system/custom timezone, offset), show-desktop button | done |
| Tray icons grabbed from the Wine tray, click / right-click / double-click forwarded | done |
| Hide Wine taskbar, reserve work area (restored on exit) | done |
| Full-screen / maximised handling (`show`/`resize`/`hide`), auto-hide, edge reveal + fade | done |
| Start menu: app list (Start Menu .lnk scan + built-ins + `start_apps`), search, Run, recent / most used, pinned tiles (medium/wide/large), rail, power menu, add app | done |
| Autostart (`autostart` -> HKCU Run key), single instance | done |

## Not ported yet (config keys are kept, but no UI/behaviour)
Settings window, setup wizard, built-in file manager (Explorer is launched instead),
desktop icons / wallpaper, window title-bar skin overlay + skin themes, custom cursors,
touch keyboard (button launches `osk.exe`), hover/window previews (the button shows a window list),
reveal strip, floating reveal icon, tile drag-and-drop and shuffle, wallpaper-blur glass,
themed owner-drawn menus (standard Windows popup menus are used), account photo.
`Settings` in the Start rail opens `config.json` in Notepad.

## Layout
```
src/json.c      JSON parser/writer (output identical to Python json.dump indent=2)
src/config.c    defaults + load/save
src/util.c      colours, clock, .lnk parser, paths
src/theme.c     themes, px(), bar/work rects
src/gdi.c       fonts, alpha, gradients, glass cache
src/icons.c     icon extraction + accent colour
src/tray.c      Wine tray host
src/taskbar.c   the taskbar window
src/startmenu.c the Start menu
src/app.c       run/popup/reload/shutdown glue
src/main.c      entry point
```
