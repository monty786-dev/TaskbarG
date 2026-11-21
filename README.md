# WinShell (C port, v7 feature set)

Taskbar, Start panel, title bars and Wine colours for Winlator / Wine. C port of `TaskbarH.py`.
Uses the same `config.json` (`%PROGRAMDATA%\WinShell\config.json`). Right-click the taskbar > **Settings**.

## Build (GitHub)
Push this folder, open **Actions > Build WinShell.exe > Run workflow**. Artifacts: `WinShell-exe`
(`WinShell-x64.exe`, `WinShell-x86.exe`) and **`build-log`** (`build.log`, `errors.log`, `warnings.log`).
The run page shows a build report with errors/warnings, and compile errors are annotated on the source lines.
`make -k` is used, so one run lists every error for both builds. Tag `v7.x.y` to attach the exes to a release.

Local: `sudo apt install gcc-mingw-w64-x86-64 gcc-mingw-w64-i686 && make`.
No MinGW? `tools/syntax-check/check.sh` type-checks all sources against stub headers.

## Your 13 requests

| # | Request | Where / status |
|---|---|---|
| 1 | Left / right (vertical) taskbar and reveal bar | Settings > Taskbar > Position; reveal strip follows the same edge |
| 2 | Right pane (links) that switches with pinned tiles | Start panel > "Right pane shows", switch button on the pane; **Right pane links**: on/off, order, icon, colour |
| 3 | Start panel styles that follow the original systems | Start panel style: Win 10 / 8 / 11 / 7 / XP / 98, Zorin, macOS, GNOME, Ubuntu, Mint, KDE, Xfce, Arch (4 layouts: tiles, two-column, icon grid, classic). Layout and metrics follow each OS; they are not pixel copies |
| 4 | Removed | custom cursors, all keyboard features, file manager, full-screen-app section |
| 5 | Cortana-style search in the taskbar | Search box: on/off, width, text; option to move the Start panel's search into it |
| 6 | Size customisation | Start panel 5 levels (+ full), tiles 8 levels, tile width/height %, start button size + width/height, desktop button width, search width, profile picture, power column, app list width/row height, pane width |
| 7 | Animations (slide left / right / up / down) | Taskbar, Start panel, programs opening, programs closing; each with on/off, direction, speed slider |
| 8 | Nine-box Start icon, icon/colour/position/visibility for every element | Default Start icon is nine boxes. Layout editor: drag elements on the taskbar and on the Start panel; per element on/off, any .png/.ico/.jpg/.bmp icon, any colour. Positions are relative, so they adapt to bar size |
| 9 | Title bar fixes | Bar kept directly above its window (event hook + 40 ms check), window and bar move together, per-button hover colours, click ripple, optional removal of Wine's own title bar |
| 10 | Random shuffle | Personalization: shuffle theme (palette, title bar style, Wine colours); Tile colours: shuffle sizes and colours of tiles |
| 11 | Programs button | Power column: groups Start-menu folders and Program Files, expandable like Windows |
| 12 | Wine controls | 30 colour slots (window, controls/buttons, tickbox/radio/dropdown frames, menu, menu bar, tooltip, scroll bar, selection, workspace, title bars) and 20 OS presets |
| 13 | Reveal bar opacity down to 0, error log in build.yml | Done |

## Honest limits
- Not yet run on a real Windows/Wine machine: the code is type-checked, not runtime-tested. Report anything odd.
- Wine controls: Wine has no per-control hover colour. Hot-tracking and menu-highlight are exposed; message boxes use the window/button colours. Colours are written to `HKCU\Control Panel\Colors` and applied live with `SetSysColors`.
- Title bars: in the default mode the bar covers the native one (same height). "Disable Wine's own title bar" changes the window style, which a few programs dislike; it is off by default and fully reversible.
- Program close animation runs when closing from WinShell (taskbar menu, custom title bar). A program closing itself cannot be animated from outside.
- Start panel list rows are not draggable; the other Start elements are.
