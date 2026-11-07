#pragma once

#include <filesystem>

#include <QList>
#include <QPair>
#include <QString>

namespace Metadata {

using Fields = QList<QPair<QString, QString>>;

// Call once from the main thread before reading metadata from worker threads.
void initialize();

// Human-readable EXIF fields (label, value). Only fields present in the file are returned.
// Safe to call from a worker thread.
Fields readExif(const std::filesystem::path& path);

}
