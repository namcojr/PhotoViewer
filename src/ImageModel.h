#pragma once

#include <filesystem>
#include <optional>
#include <vector>

#include <QByteArray>
#include <QSize>

class ImageModel {
public:
    struct ImageEntry {
        std::filesystem::path path;
        std::uintmax_t fileSizeBytes = 0;
        QSize dimensions;
        QByteArray format;
    };

    bool loadFromFile(const std::filesystem::path& filePath);

    [[nodiscard]] bool empty() const;
    [[nodiscard]] int size() const;
    [[nodiscard]] int currentIndex() const;

    [[nodiscard]] const ImageEntry* current() const;
    [[nodiscard]] const ImageEntry* at(int index) const;
    ImageEntry* currentMutable();

    [[nodiscard]] int wrappedNextIndex() const;
    [[nodiscard]] int wrappedPreviousIndex() const;

    bool setCurrentByPath(const std::filesystem::path& filePath);
    void setCurrentIndex(int index);
    bool removeCurrent();

private:
    static std::optional<ImageEntry> makeEntry(const std::filesystem::path& path);

    std::vector<ImageEntry> entries_;
    int currentIndex_ = -1;
};
