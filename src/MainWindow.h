#pragma once

#include <filesystem>

#include <QFutureWatcher>
#include <QImage>
#include <QMainWindow>

#include "ImageModel.h"

class QLabel;
class ImageViewerWidget;
class ExifDialog;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(const std::filesystem::path& startFile, QWidget* parent = nullptr);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    struct LoadedImage {
        QImage image;
        QByteArray format;
        QSize dimensions;
        QString error;
    };

    static LoadedImage loadImageFromDisk(const std::filesystem::path& path);

    void loadCurrentImageWithFallback(int preferredIndex, int directionIfFail);
    void beginLoadIndex(int index, int directionIfFail, int attemptsRemaining);

    void showNextImage();
    void showPreviousImage();
    void copyCurrentImageToClipboard() const;
    void showExifDialog();
    void updateOverlay();

    static QString formatOverlayText(const ImageModel::ImageEntry& entry);
    static QString readExifText(const std::filesystem::path& path);

    ImageModel model_;
    ImageViewerWidget* viewer_;
    QLabel* overlayLabel_;
    ExifDialog* exifDialog_;

    QImage currentImage_;
    QFutureWatcher<LoadedImage> loadWatcher_;
    bool loading_ = false;
    int loadDirectionIfFail_ = 1;
    int loadAttemptsRemaining_ = 0;
};
