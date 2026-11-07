#pragma once

#include <filesystem>

#include <QByteArray>
#include <QList>
#include <QPair>
#include <QSize>
#include <QString>

namespace Metadata {

using Fields = QList<QPair<QString, QString>>;

// Call once from the main thread before reading metadata from worker threads.
void initialize();

// Human-readable EXIF fields (label, value). Only fields present in the file are returned.
// Safe to call from a worker thread.
Fields readExif(const std::filesystem::path& path);

// Copies EXIF, IPTC and XMP from `source` into `encoded` (a freshly encoded image file in
// memory) and returns the result. The pixels are assumed to be upright already, so the
// orientation is reset to normal, the stale EXIF thumbnail is dropped and the pixel
// dimensions are updated to `pixelSize`. Returns `encoded` unchanged when the source has no
// metadata or the target format cannot hold it (e.g. BMP).
QByteArray withMetadataFrom(const std::filesystem::path& source, const QByteArray& encoded, QSize pixelSize);

}
