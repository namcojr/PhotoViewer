# Photo Viewer

Minimal keyboard-first Linux photo viewer implemented with Qt 6 Widgets and C++20.
Works on KDE Plasma 6 (Wayland and X11) as well as other desktops and compositors.

## Build

Requirements:
- CMake 3.22+
- Qt6 (`Widgets`, `Concurrent`)
- Exiv2

```bash
cmake -S . -B build
cmake --build build -j
```

## Image formats

Decoding is done by Qt's image plugins, so the viewer opens every format the
installed plugins support, with no rebuild needed. Out of the box Qt only reads
JPEG, PNG, GIF, BMP, ICO and PBM/PGM/PPM/XBM/XPM. Install the plugin packages
for everything else:

| Package (Arch/Manjaro) | Adds |
| --- | --- |
| `qt6-imageformats` | WebP, TIFF, TGA, WBMP, ICNS, MNG |
| `kimageformats` | HEIC/HEIF, AVIF, JPEG XL, camera RAW (CR2/CR3/NEF/ARW/DNG/RAF/ORF/RW2…), EXR, HDR, PSD, XCF, Krita, OpenRaster, QOI, DDS, PCX, SGI, … |
| `qt6-svg` | SVG / SVGZ |

```bash
sudo pacman -S qt6-imageformats kimageformats qt6-svg
```

On KDE Plasma 6 these are normally already installed as part of the desktop.
On Debian/Ubuntu/Fedora the packages are `qt6-image-formats-plugins` and
`kf6-kimageformats` (names vary by distro).

To see what your system can open:

```bash
photo-viewer --formats
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
- `photo-viewer.desktop` to `<prefix>/share/applications/photo-viewer.desktop` (registers the image MIME types listed above)

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

All images in the same folder are listed in natural order (`IMG_2` before `IMG_10`).

## Controls

A hint bar at the bottom of the window shows the main keys.

- `Arrow Right` / `Arrow Left`: next / previous image (wraps; holding the key skips ahead)
- `Home` / `End`: first / last image in the folder
- `Page Down` / `Page Up`: rotate right / left (90°)
- `+` / `-`: zoom in / out
- `0`: fit to window
- `1`: actual size (100 %)
- `F` / `F11`: toggle fullscreen
- `I`: show / hide the info panel
- `S`: save as JPEG, PNG or BMP (includes the current rotation)
- `Ctrl+C`: copy current full-resolution image (with rotation) to clipboard
- `Del`: move current image to trash (no confirmation)
- `Ctrl+Z`: restore the last image moved to trash (repeat to restore earlier ones)
- `Esc`: leave fullscreen, or quit
- `q`: quit
- Mouse wheel: zoom at cursor; drag to pan when zoomed in
- Double-click: switch between fit to window and actual size at the clicked point

Animated GIF and WebP files play automatically.

## Saving

`S` saves a copy of the image as shown, including the rotation. JPEG and PNG copies
keep the original's EXIF, IPTC and XMP metadata (camera, date taken, GPS, …), with the
orientation reset because the pixels are already upright. BMP cannot store metadata.
Files are written atomically, so overwriting the original is safe even if saving fails.

## Info panel

Press `I` to toggle a vertical panel next to the image with file details
(name, folder, position in folder, size, dimensions, format, modification time)
and EXIF data (camera, lens, ISO, aperture, shutter, focal length, date taken, GPS, …).

The **Panel position** buttons at the bottom of the panel place it left or right
of the image. The side, the panel's visibility, the window size and the last format
used for saving are remembered in `~/.config/photo-viewer/photo-viewer.conf`.

## File manager integration

After install, refresh the desktop database:

```bash
# use /usr/local/share/applications when installed with --prefix /usr/local
# use /usr/share/applications when installed with --prefix /usr
update-desktop-database /usr/local/share/applications
```

### KDE Plasma 6 (Dolphin)

```bash
kbuildsycoca6
```

Then either right-click an image in Dolphin → **Open With** → **Photo Viewer** and
tick "Remember application association", or set it in
**System Settings → Default Applications → File Associations**.
The `xdg-mime` commands below also work on Plasma.

### Any desktop (Thunar, Nautilus, Dolphin, …)

```bash
for mime in image/jpeg image/png image/webp image/bmp image/tiff image/gif \
            image/heic image/heif image/avif image/jxl; do
    xdg-mime default photo-viewer.desktop "$mime"
done
```
