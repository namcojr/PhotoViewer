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

    // Index of the entry for `filePath`, or -1 if it is not listed.
    [[nodiscard]] int indexOf(const std::filesystem::path& filePath) const;
    bool setCurrentByPath(const std::filesystem::path& filePath);
    void setCurrentIndex(int index);
    bool removeCurrent();

    // Adds a file that lives in the current folder (e.g. a freshly saved copy).
    // Keeps the current image selected. Returns false if it is not a readable image.
    bool insertFile(const std::filesystem::path& filePath);

    [[nodiscard]] const std::filesystem::path& directory() const;

private:
    static std::optional<ImageEntry> makeEntry(const std::filesystem::path& path);
    void sortEntries();

    std::filesystem::path directory_;

    std::vector<ImageEntry> entries_;
    int currentIndex_ = -1;
};
