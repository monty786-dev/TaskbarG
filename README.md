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

## Latest update (11 requests)
| # | Request | Where |
|---|---|---|
| 1 | Auto-hide with the reveal bar | Taskbar > Reveal bar: "Auto-hide the taskbar" + reveal time; hover/click the strip brings the bar back |
| 2 | Wine / desktop shortcuts <-> Start list and tiles | Full desktop > Shortcuts sync. New `.lnk/.exe/.url` in the user and Public Desktop folders go to the Start app list **and** tiles; Start apps/tiles appear on the WinShell desktop (checked every ~10 s) |
| 3 | Add an application to the tray | Right-click a program (taskbar / Start / desktop) > **Add to tray**, or Taskbar > Tray apps. Click = open, right-click = remove |
| 4 | Move anything freely, resize, auto toggles | Taskbar > Layout editor / Start panel > Start elements. Drag to move anywhere, drag the corner square to resize, right-click: **Auto position ON/OFF**, **Auto size ON/OFF**, Reset, **Auto-adapt ALL**. Covers every taskbar element, the running-app group (icon size / button width), the Start **app list window** and **pinned tiles window** |
| 5 | App list and tiles up / down | Start panel: "Pinned tiles / links side" = Above / Below the app list, split slider |
| 6 | Full desktop mode | Settings > Full desktop: wallpaper (fill/fit/stretch/center/tile), colour, movable + resizable shortcuts, app tiles, clock / calendar / system widgets, per-item and global move lock, grid snap |
| 7 | Game mode | Settings > Game mode (or taskbar menu, or `--game`): PlayStation 5, Steam Big Picture, Nintendo Switch, Xbox, retro list. Per game: icon/cover, background, description, colour, start arguments. Keyboard, mouse and XInput controller; returns to game mode when the game closes |
| 8 | Start panel size / position | Start panel sizes: width, height, move up/down/left/right, tiles window w/h, app list height, power column length + width, profile picture |
| 9 | Start button icon | Taskbar > Start button: style, size, width/height, colour, **your own image** |
| 10 | Transparency | Appearance > Transparency: icon and text visibility for the taskbar and the Start panel |
| 11 | Window preview button | Live thumbnails of every window; click = go to it; X = close; pin = always on top; **Close all windows**; right-click for more |
| - | App icon | Nine-box icon embedded in the exe (`src/winshell.ico`, `assets/winshell-icon.png`) |

Earlier features (vertical bars, OS themes, Start layouts, animations, title bars, Wine colours, shuffle) are unchanged; see Settings.

## Honest limits
- Not run on a real Wine/Windows machine yet; the code is type-checked only. Send the build log or any odd behaviour and I will fix it.
- Sync creates items inside WinShell's own desktop; it does not write real `.lnk` files.
- "Add to tray" means launcher icons in the tray area, not "minimise to tray".
- Wine's `PrintWindow` can return blank thumbnails for some programs; the preview then shows the program icon.
- Game mode themes imitate each console's layout and colours; they are not pixel copies. Controller needs Wine to expose XInput.
- Start app-list rows are not draggable; the app list and tiles windows themselves are.
