# Photo Viewer

Minimal keyboard-first Linux photo viewer implemented with Qt 6 Widgets and C++20.

## Build

Requirements:
- CMake 3.22+
- Qt6 (`Widgets`, `Concurrent`)
- Exiv2

```bash
cmake -S . -B build
cmake --build build -j
```

## Install

Use one of these prefixes:
- `/usr/local` for local/manual installs
- `/usr` for distro-style system installs

```bash
# local install
sudo cmake --install build --prefix /usr/local

# distro-style install
sudo cmake --install build --prefix /usr
```

This installs:
- `photo-viewer` to `<prefix>/bin/photo-viewer`
- `photo-viewer.desktop` to `<prefix>/share/applications/photo-viewer.desktop` (with GIF MIME support)

## Uninstall

The project provides an `uninstall` target that removes files listed in CMake's install manifest:

```bash
cmake -S . -B build
sudo cmake --build build --target uninstall
```

If you installed with a custom prefix, run uninstall from the same build directory used for that install.

## Run

```bash
photo-viewer /path/to/image.jpg
```

## Controls

- `Right Arrow`: next image (wraps)
- `Left Arrow`: previous image (wraps)
- `Page Up`: rotate right (90°)
- `Page Down`: rotate left (90°)
- `I`: show EXIF popup
- `Ctrl+C`: copy current full-resolution image to clipboard
- `q` / `Esc`: quit (and close EXIF dialog)

## Thunar integration

After install, run:

```bash
# use /usr/local/share/applications when installed with --prefix /usr/local
# use /usr/share/applications when installed with --prefix /usr
update-desktop-database /usr/local/share/applications
xdg-mime default photo-viewer.desktop image/jpeg
xdg-mime default photo-viewer.desktop image/png
xdg-mime default photo-viewer.desktop image/webp
xdg-mime default photo-viewer.desktop image/bmp
xdg-mime default photo-viewer.desktop image/tiff
xdg-mime default photo-viewer.desktop image/gif
```
