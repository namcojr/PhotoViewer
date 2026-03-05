#include "ImageModel.h"

#include <algorithm>
#include <cctype>

#include <QImageReader>

namespace {
std::string toLower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char ch) {
        return static_cast<char>(std::tolower(ch));
    });
    return value;
}
}

bool ImageModel::loadFromFile(const std::filesystem::path& filePath) {
    entries_.clear();
    currentIndex_ = -1;

    std::error_code ec;
    const auto canonicalPath = std::filesystem::weakly_canonical(filePath, ec);
    const auto targetPath = ec ? filePath : canonicalPath;

    const auto parent = targetPath.parent_path();
    if (parent.empty() || !std::filesystem::exists(parent, ec)) {
        return false;
    }

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

    std::sort(entries_.begin(), entries_.end(), [](const ImageEntry& lhs, const ImageEntry& rhs) {
        return toLower(lhs.path.filename().string()) < toLower(rhs.path.filename().string());
    });

    return setCurrentByPath(targetPath);
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
    std::error_code ec;
    const auto canonicalPath = std::filesystem::weakly_canonical(filePath, ec);
    const auto targetPath = ec ? filePath : canonicalPath;

    for (std::size_t i = 0; i < entries_.size(); ++i) {
        std::error_code entryEc;
        const auto canonicalEntry = std::filesystem::weakly_canonical(entries_[i].path, entryEc);
        const auto entryPath = entryEc ? entries_[i].path : canonicalEntry;

        if (entryPath == targetPath) {
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

std::optional<ImageModel::ImageEntry> ImageModel::makeEntry(const std::filesystem::path& path) {
    QImageReader reader(QString::fromStdString(path.string()));
    reader.setDecideFormatFromContent(true);
    if (!reader.canRead()) {
        return std::nullopt;
    }

    ImageEntry entry;
    entry.path = path;

    std::error_code ec;
    entry.fileSizeBytes = std::filesystem::file_size(path, ec);
    if (ec) {
        entry.fileSizeBytes = 0;
    }

    entry.format = reader.format();
    entry.dimensions = reader.size();

    return entry;
}
