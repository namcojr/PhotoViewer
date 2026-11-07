#include "ImageModel.h"

#include <algorithm>

#include <QCollator>
#include <QFileInfo>
#include <QImageReader>
#include <QSet>

namespace {
std::filesystem::path normalized(const std::filesystem::path& path) {
    std::error_code ec;
    auto canonical = std::filesystem::weakly_canonical(path, ec);
    return ec ? path : canonical;
}

// File suffixes handled by the image plugins installed for this Qt (qt6-imageformats,
// kimageformats, qt6-svg, ...). Queried once at runtime, so new plugins work without a rebuild.
const QSet<QString>& knownSuffixes() {
    static const QSet<QString> suffixes = [] {
        QSet<QString> result;
        for (const QByteArray& format : QImageReader::supportedImageFormats()) {
            result.insert(QString::fromLatin1(format).toLower());
        }
        // Common aliases that some plugins do not list explicitly.
        if (result.contains(QStringLiteral("jpeg"))) {
            result.insert(QStringLiteral("jpg"));
            result.insert(QStringLiteral("jpe"));
            result.insert(QStringLiteral("jfif"));
        }
        if (result.contains(QStringLiteral("tiff"))) {
            result.insert(QStringLiteral("tif"));
        }
        if (result.contains(QStringLiteral("heif"))) {
            result.insert(QStringLiteral("heic"));
            result.insert(QStringLiteral("hif"));
        }
        return result;
    }();
    return suffixes;
}
}

bool ImageModel::loadFromFile(const std::filesystem::path& filePath) {
    entries_.clear();
    currentIndex_ = -1;

    std::error_code ec;
    const auto targetPath = normalized(filePath);

    const auto parent = targetPath.parent_path();
    if (parent.empty() || !std::filesystem::exists(parent, ec)) {
        return false;
    }
    directory_ = parent;

    for (const auto& item : std::filesystem::directory_iterator(parent, ec)) {
        if (ec) {
            break;
        }

        const auto name = item.path().filename().string();
        if (!name.empty() && name.front() == '.') {
            continue;
        }

        if (!item.is_regular_file(ec)) {
            continue;
        }

        auto maybeEntry = makeEntry(item.path());
        if (maybeEntry.has_value()) {
            entries_.push_back(std::move(*maybeEntry));
        }
    }

    if (entries_.empty()) {
        return false;
    }

    sortEntries();

    if (setCurrentByPath(targetPath)) {
        return true;
    }

    // The requested file was not picked up by the scan (e.g. hidden file or unknown suffix):
    // probe it directly so explicitly opened files always work.
    if (auto entry = makeEntry(targetPath)) {
        entries_.push_back(std::move(*entry));
        sortEntries();
        return setCurrentByPath(targetPath);
    }
    return false;
}

void ImageModel::sortEntries() {
    QCollator collator;
    collator.setNumericMode(true);
    collator.setCaseSensitivity(Qt::CaseInsensitive);
    std::sort(entries_.begin(), entries_.end(), [&collator](const ImageEntry& lhs, const ImageEntry& rhs) {
        return collator.compare(QString::fromStdString(lhs.path.filename().string()),
                                QString::fromStdString(rhs.path.filename().string())) < 0;
    });
}

bool ImageModel::insertFile(const std::filesystem::path& filePath) {
    const auto target = normalized(filePath);
    if (target.parent_path() != directory_) {
        return false;
    }

    std::filesystem::path currentPath;
    if (const auto* entry = current()) {
        currentPath = entry->path;
    }

    const bool alreadyListed = std::any_of(entries_.begin(), entries_.end(), [&target](const ImageEntry& entry) {
        return normalized(entry.path) == target;
    });

    if (!alreadyListed) {
        auto entry = makeEntry(target);
        if (!entry) {
            return false;
        }
        entries_.push_back(std::move(*entry));
        sortEntries();
    }

    if (!currentPath.empty()) {
        setCurrentByPath(currentPath);
    }
    return true;
}

const std::filesystem::path& ImageModel::directory() const {
    return directory_;
}

bool ImageModel::empty() const {
    return entries_.empty();
}

int ImageModel::size() const {
    return static_cast<int>(entries_.size());
}

int ImageModel::currentIndex() const {
    return currentIndex_;
}

const ImageModel::ImageEntry* ImageModel::current() const {
    return at(currentIndex_);
}

const ImageModel::ImageEntry* ImageModel::at(const int index) const {
    if (index < 0 || index >= size()) {
        return nullptr;
    }
    return &entries_[static_cast<std::size_t>(index)];
}

ImageModel::ImageEntry* ImageModel::currentMutable() {
    if (currentIndex_ < 0 || currentIndex_ >= size()) {
        return nullptr;
    }
    return &entries_[static_cast<std::size_t>(currentIndex_)];
}

int ImageModel::wrappedNextIndex() const {
    if (empty()) {
        return -1;
    }
    return (currentIndex_ + 1) % size();
}

int ImageModel::wrappedPreviousIndex() const {
    if (empty()) {
        return -1;
    }
    return (currentIndex_ - 1 + size()) % size();
}

bool ImageModel::setCurrentByPath(const std::filesystem::path& filePath) {
    const auto targetPath = normalized(filePath);

    for (std::size_t i = 0; i < entries_.size(); ++i) {
        if (normalized(entries_[i].path) == targetPath) {
            currentIndex_ = static_cast<int>(i);
            return true;
        }
    }
    return false;
}

void ImageModel::setCurrentIndex(const int index) {
    if (index < 0 || index >= size()) {
        return;
    }
    currentIndex_ = index;
}

bool ImageModel::removeCurrent() {
    if (currentIndex_ < 0 || currentIndex_ >= size()) {
        return false;
    }

    entries_.erase(entries_.begin() + currentIndex_);

    if (entries_.empty()) {
        currentIndex_ = -1;
        return true;
    }

    if (currentIndex_ >= size()) {
        currentIndex_ = size() - 1;
    }

    return true;
}

std::optional<ImageModel::ImageEntry> ImageModel::makeEntry(const std::filesystem::path& path) {
    const QString filePath = QString::fromStdString(path.string());
    const QString suffix = QFileInfo(filePath).suffix().toLower();

    ImageEntry entry;
    entry.path = path;

    if (!knownSuffixes().contains(suffix)) {
        // Unknown or missing extension: sniff the content. Formats are filled in on load
        // for files with a known suffix, which keeps scanning large folders fast.
        QImageReader reader(filePath);
        if (!reader.canRead()) {
            return std::nullopt;
        }
        entry.format = reader.format();
    }

    std::error_code ec;
    entry.fileSizeBytes = std::filesystem::file_size(path, ec);
    if (ec) {
        entry.fileSizeBytes = 0;
    }

    return entry;
}
