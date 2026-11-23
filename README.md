# WinShell (C port)

Taskbar, Start panel, full desktop, game mode, title bars and Wine colours for Winlator / Wine.
Uses `config.json` in `%PROGRAMDATA%\WinShell`. Right-click the taskbar > **Settings**.
Command line: `WinShell.exe [--game] [--restart] [--open <path>]`.

## Build on GitHub
1. Upload this whole folder to a repository (keep `.github/workflows/build.yml`).
2. **Actions > Build WinShell.exe > Run workflow**.
3. Download artifact **WinShell-exe** (`WinShell-x64.exe`, `WinShell-x86.exe`). Failed? Download **build-log** (`build.log`, `errors.log`, `warnings.log`); the run page also shows a report and marks the failing lines.
4. Tag `v7.1.0` to attach the exes to a release.

Local: `sudo apt install gcc-mingw-w64-x86-64 gcc-mingw-w64-i686 && make`. Without MinGW: `tools/syntax-check/check.sh` type-checks everything against stub headers.

## Latest update (15 requests)
| # | Request | Where |
|---|---|---|
| 1 | Fake apps (image icon only) and "WinShell Settings" as an app | Settings > Apps > Fake and built-in apps: add an image, name it, optionally point it at a program or `winshell:settings`. Add any fake or built-in app (Settings, Game mode, desktop switch, layout editor) to the Start list, tiles, taskbar, desktop or tray |
| 2 | Start panel parts behind the taskbar; elements that would not move | The taskbar now stays just below an open Start panel / preview instead of covering it. Moving is done in the new editor window (no window-edge limits) |
| 3 | Special editor window | Taskbar menu > Layout editor... (or Settings > Layout editor): live preview of the taskbar or Start panel, zoom/pan, drag to move, corner square to resize, switches for visible (add/delete), auto position, auto size, sliders for X/Y/width/height, icon image, colour, reset, add a program |
| 4 | Close Start when clicking elsewhere (on/off) | Start panel > "Close the Start panel when clicking anywhere else" |
| 5 | App list and tiles views | Start panel: App list view = list / 2 columns / grid 2-5 / tiles (8 sizes); Pinned items view = tiles / grid 2-5 / 2 columns |
| 6 | Open-app animation in Wine / Winlator | New windows are now found by polling (the shell hook is unreliable in Wine) and animated after their position settles |
| 7 | Auto-hide only with the reveal bar | The auto-hide switch lives in Reveal bar and only works while the reveal bar is on |
| 8 | Widget transparency and custom tiles | Widget / tile visibility 0-100 % (global slider, per-item menu), **image tiles**, app tiles |
| 9 | Better game mode | Intro fade and slide-in, smooth selection scaling, cross-fading backgrounds, text slide, pulsing selection, hint bar |
| 10 | Title bar | Bar stays directly above its window and is raised with it on focus change; real app icon (taken from the program's exe); title / icon / buttons alignment (left, middle, right); your own button icons; create and save title bar themes |
| 11 | Toggle button groups | Short choice lists are shown as segmented button groups in Settings |
| 12 | Backup and restore | Settings > Startup and user: one `.wsbk` file with settings, every image/icon the settings use, shortcuts, and the Wine / user Desktop shortcuts |
| 13 | Desktop / normal mode button | Taskbar element and Start panel button (hidden by default; enable in the layout editor or Settings) |
| 14 | OS theme in Personalization, random for all OS settings | Personalization: OS theme + Apply, **Random OS theme**, **Random everything** |
| 15 | Icon shapes | Fill colour, first-letter badges for apps without an icon, **pill style** (buttons, switches, labels become colour-filled text-sized shapes) with a random colour shuffle |

Earlier features are unchanged.

## Honest limits
- Not run on a real Wine/Windows machine; the code is type-checked only. Send the `build-log` artifact or any odd behaviour and I will fix it.
- Pill style covers Settings, the layout editor and the taskbar (clock, search box, window titles). The Start panel keeps normal shapes.
- Open-app animation moves the window after it appears; a window that repositions itself again can fight it. Closing animations only run when closing from WinShell.
- In the layout editor, vertical taskbars use "along / across the bar" for X / Y.
- Backups copy images, icons, .lnk and .url files; programs (.exe) are not copied.
