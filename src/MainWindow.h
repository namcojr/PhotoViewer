#pragma once

#include <filesystem>

#include <QFutureWatcher>
#include <QImage>
#include <QMainWindow>
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

private:
    struct LoadedImage {
        QImage image;
        QByteArray format;
        QSize dimensions;
        QString error;
        Metadata::Fields exif;
    };

    static LoadedImage loadImageFromDisk(const std::filesystem::path& path);

    void setupActions();
    void addShortcut(const QKeySequence& key, void (MainWindow::*slot)());

    void loadCurrentImageWithFallback(int preferredIndex, int directionIfFail);
    void beginLoadIndex(int index, int directionIfFail, int attemptsRemaining);
    void reloadCurrentImage();

    void showNextImage();
    void showPreviousImage();
    void showFirstImage();
    void showLastImage();
    void rotateClockwise();
    void rotateCounterClockwise();
    void moveCurrentImageToTrash();
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

    QImage currentImage_;
    Metadata::Fields currentExif_;
    QFutureWatcher<LoadedImage> loadWatcher_;
    bool loading_ = false;
    int loadDirectionIfFail_ = 1;
    int loadAttemptsRemaining_ = 0;
};
