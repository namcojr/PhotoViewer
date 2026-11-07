#pragma once

#include <filesystem>
#include <vector>

#include <QFutureWatcher>
#include <QImage>
#include <QMainWindow>
#include <QMovie>
#include <QSettings>
#include <QTimer>

#include "ImageModel.h"
#include "InfoPanel.h"
#include "Metadata.h"

class QHBoxLayout;
class QKeySequence;
class QLabel;
class ImageViewerWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const std::filesystem::path& startFile, QWidget* parent = nullptr);

protected:
    void closeEvent(QCloseEvent* event) override;

private:
    struct LoadedImage {
        QImage image;
        QByteArray format;
        QSize dimensions;
        QString error;
        Metadata::Fields exif;
        bool animated = false;
    };

    // A file moved to the trash by Del, kept so Ctrl+Z can put it back.
    struct TrashedFile {
        QString originalPath;
        QString trashPath;
    };

    static LoadedImage loadImageFromDisk(const std::filesystem::path& path);

    void setupActions();
    template <typename Slot>
    void addShortcut(const QKeySequence& key, Slot slot);

    // Asks for the image at `index`. Only the most recent request is shown: requests made
    // while a load is running replace each other and start once that load finishes.
    // `direction` (+1 / -1) is where to look next if the image cannot be decoded.
    void requestImage(int index, int direction);
    void startPendingLoad();
    void onImageLoaded();
    void showLoadedImage(const LoadedImage& loaded);
    // The image the user is heading to: the newest request, else the one shown.
    [[nodiscard]] int targetIndex() const;

    void showNextImage();
    void showPreviousImage();
    void showFirstImage();
    void showLastImage();
    void toggleFullScreen();
    void escape();
    void moveCurrentImageToTrash();
    void undoMoveToTrash();
    void copyCurrentImageToClipboard();
    void saveCurrentImageAs();
    void toggleInfoPanel();
    void setInfoPanelSide(InfoPanel::Side side);

    void updateOverlay();
    void updateInfoPanel();
    void showTransientHint(const QString& message);
    void restoreHint();

    static QString formatOverlayText(const ImageModel::ImageEntry& entry);
    Metadata::Fields fileFields(const ImageModel::ImageEntry& entry) const;

    ImageModel model_;
    ImageViewerWidget* viewer_;
    QLabel* overlayLabel_;
    QLabel* hintLabel_;
    InfoPanel* infoPanel_;
    QHBoxLayout* contentLayout_;

    QSettings settings_;
    QTimer hintTimer_;
    QMovie movie_;

    Metadata::Fields currentExif_;
    std::vector<TrashedFile> trashed_;

    QFutureWatcher<LoadedImage> loadWatcher_;
    bool loading_ = false;
    int loadingIndex_ = -1;
    int loadingDirection_ = 1;
    int pendingIndex_ = -1;
    int pendingDirection_ = 1;
    int loadAttemptsRemaining_ = 0;
};
