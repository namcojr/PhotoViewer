#include "MainWindow.h"

#include <cmath>
#include <optional>

#include <exiv2/exiv2.hpp>

#include <QApplication>
#include <QClipboard>
#include <QFile>
#include <QFileInfo>
#include <QFuture>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImageReader>
#include <QKeyEvent>
#include <QLabel>
#include <QMessageBox>
#include <QMetaObject>
#include <QStatusBar>
#include <QStringList>
#include <QVBoxLayout>
#include <QtConcurrent>

#include "ExifDialog.h"
#include "ImageViewerWidget.h"

namespace {
QString rationalToString(const Exiv2::Value& value) {
    if (value.count() == 0) {
        return {};
    }

    try {
        const auto rat = value.toRational(0);
        if (rat.second == 0) {
            return {};
        }

        if (rat.second == 1) {
            return QString::number(rat.first);
        }

        return QStringLiteral("%1/%2").arg(rat.first).arg(rat.second);
    } catch (...) {
        return {};
    }
}

QString exifValue(const Exiv2::ExifData& data, const char* key) {
    const auto it = data.findKey(Exiv2::ExifKey(key));
    if (it == data.end()) {
        return {};
    }
    return QString::fromStdString(it->toString());
}
}

MainWindow::MainWindow(const std::filesystem::path& startFile, QWidget* parent)
    : QMainWindow(parent),
      viewer_(new ImageViewerWidget(this)),
      overlayLabel_(new QLabel(this)),
      exifDialog_(new ExifDialog(this)) {
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    overlayLabel_->setFixedHeight(36);
    overlayLabel_->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    overlayLabel_->setStyleSheet(QStringLiteral(
        "QLabel {"
        " background-color: rgba(0, 0, 0, 165);"
        " color: white;"
        " font-size: 15px;"
        " font-weight: 600;"
        "}"
    ));

    layout->addWidget(overlayLabel_);
    layout->addWidget(viewer_, 1);

    setCentralWidget(central);
    setWindowTitle(QStringLiteral("Photo Viewer"));
    resize(1920, 1200);
    setMinimumSize(960, 600);
    setMaximumSize(QWIDGETSIZE_MAX, QWIDGETSIZE_MAX);

    connect(&loadWatcher_, &QFutureWatcher<LoadedImage>::finished, this, [this]() {
        loading_ = false;
        const LoadedImage loaded = loadWatcher_.result();

        if (loaded.image.isNull()) {
            --loadAttemptsRemaining_;
            if (loadAttemptsRemaining_ <= 0) {
                QMessageBox::critical(this, QStringLiteral("Photo Viewer"),
                                      QStringLiteral("Failed to load any image in this folder."));
                qApp->exit(1);
                return;
            }

            int nextIndex = model_.currentIndex();
            if (loadDirectionIfFail_ >= 0) {
                nextIndex = model_.wrappedNextIndex();
            } else {
                nextIndex = model_.wrappedPreviousIndex();
            }
            beginLoadIndex(nextIndex, loadDirectionIfFail_, loadAttemptsRemaining_);
            return;
        }

        currentImage_ = loaded.image;
        viewer_->setImage(currentImage_);

        auto* currentEntry = model_.currentMutable();
        if (currentEntry != nullptr) {
            currentEntry->format = loaded.format;
            currentEntry->dimensions = loaded.dimensions;
        }

        updateOverlay();
    });

    if (!model_.loadFromFile(startFile) || model_.empty()) {
        QMetaObject::invokeMethod(this, [this]() {
            QMessageBox::critical(this, QStringLiteral("Photo Viewer"),
                                  QStringLiteral("No valid images found in folder."));
            qApp->exit(1);
        }, Qt::QueuedConnection);
        return;
    }

    loadCurrentImageWithFallback(model_.currentIndex(), 1);
}

void MainWindow::keyPressEvent(QKeyEvent* event) {
    if ((event->modifiers() & Qt::ControlModifier) && event->key() == Qt::Key_C) {
        copyCurrentImageToClipboard();
        event->accept();
        return;
    }

    switch (event->key()) {
    case Qt::Key_PageDown:
        showNextImage();
        event->accept();
        return;
    case Qt::Key_PageUp:
        showPreviousImage();
        event->accept();
        return;
    case Qt::Key_Home:
        showFirstImage();
        event->accept();
        return;
    case Qt::Key_End:
        showLastImage();
        event->accept();
        return;
    case Qt::Key_Delete:
        moveCurrentImageToTrash();
        event->accept();
        return;
    case Qt::Key_Right:
        viewer_->rotateClockwise();
        event->accept();
        return;
    case Qt::Key_Left:
        viewer_->rotateCounterClockwise();
        event->accept();
        return;
    case Qt::Key_I:
        showExifDialog();
        event->accept();
        return;
    case Qt::Key_Q:
    case Qt::Key_Escape:
        close();
        event->accept();
        return;
    default:
        break;
    }

    QMainWindow::keyPressEvent(event);
}

MainWindow::LoadedImage MainWindow::loadImageFromDisk(const std::filesystem::path& path) {
    LoadedImage result;
    const QString filePath = QString::fromStdString(path.string());

    QImageReader reader(filePath);
    reader.setDecideFormatFromContent(true);
    reader.setAutoTransform(true);
    if (!reader.canRead()) {
        result.error = QStringLiteral("Unreadable image");
        return result;
    }

    result.format = reader.format();
    result.dimensions = reader.size();
    result.image = reader.read();
    if (result.image.isNull()) {
        result.error = reader.errorString();
    }

    if (!result.dimensions.isValid() && !result.image.isNull()) {
        result.dimensions = result.image.size();
    }

    return result;
}

void MainWindow::loadCurrentImageWithFallback(const int preferredIndex, const int directionIfFail) {
    if (model_.empty()) {
        qApp->exit(1);
        return;
    }

    model_.setCurrentIndex(preferredIndex);
    loadDirectionIfFail_ = directionIfFail;
    loadAttemptsRemaining_ = model_.size();
    beginLoadIndex(model_.currentIndex(), directionIfFail, model_.size());
}

void MainWindow::beginLoadIndex(const int index, const int directionIfFail, const int attemptsRemaining) {
    if (loading_ || attemptsRemaining <= 0 || model_.empty()) {
        return;
    }

    model_.setCurrentIndex(index);
    const auto* entry = model_.current();
    if (entry == nullptr) {
        qApp->exit(1);
        return;
    }

    loading_ = true;
    const auto path = entry->path;
    QFuture<LoadedImage> future = QtConcurrent::run([path]() {
        return MainWindow::loadImageFromDisk(path);
    });

    loadWatcher_.setFuture(future);
}

void MainWindow::showNextImage() {
    if (model_.empty()) {
        return;
    }
    loadCurrentImageWithFallback(model_.wrappedNextIndex(), 1);
}

void MainWindow::showPreviousImage() {
    if (model_.empty()) {
        return;
    }
    loadCurrentImageWithFallback(model_.wrappedPreviousIndex(), -1);
}

void MainWindow::showFirstImage() {
    if (model_.empty()) {
        return;
    }
    loadCurrentImageWithFallback(0, 1);
}

void MainWindow::showLastImage() {
    if (model_.empty()) {
        return;
    }
    loadCurrentImageWithFallback(model_.size() - 1, -1);
}

void MainWindow::moveCurrentImageToTrash() {
    if (loading_ || model_.empty()) {
        return;
    }

    const auto* entry = model_.current();
    if (entry == nullptr) {
        return;
    }

    if (!QFile::moveToTrash(QString::fromStdString(entry->path.string()))) {
        return;
    }

    if (!model_.removeCurrent()) {
        return;
    }

    if (model_.empty()) {
        close();
        return;
    }

    loadCurrentImageWithFallback(model_.currentIndex(), 1);
}

void MainWindow::copyCurrentImageToClipboard() const {
    if (currentImage_.isNull()) {
        return;
    }
    QGuiApplication::clipboard()->setImage(currentImage_);
}

void MainWindow::showExifDialog() {
    const auto* entry = model_.current();
    if (entry == nullptr) {
        return;
    }

    exifDialog_->setExifText(readExifText(entry->path));
    exifDialog_->exec();
}

void MainWindow::updateOverlay() {
    const auto* entry = model_.current();
    if (entry == nullptr) {
        overlayLabel_->setText(QString());
        return;
    }

    overlayLabel_->setText(formatOverlayText(*entry));
}

QString MainWindow::formatOverlayText(const ImageModel::ImageEntry& entry) {
    const QFileInfo info(QString::fromStdString(entry.path.string()));
    const double sizeMb = static_cast<double>(entry.fileSizeBytes) / (1024.0 * 1024.0);
    const QString format = entry.format.isEmpty() ? QStringLiteral("Unknown")
                                                   : QString::fromLatin1(entry.format).toUpper();

    QString dimensions = QStringLiteral("?");
    if (entry.dimensions.isValid()) {
        dimensions = QStringLiteral("%1x%2").arg(entry.dimensions.width()).arg(entry.dimensions.height());
    }

    return QStringLiteral("%1 | %2 MB | %3 | %4")
        .arg(info.fileName())
        .arg(QString::number(sizeMb, 'f', 1))
        .arg(dimensions)
        .arg(format);
}

QString MainWindow::readExifText(const std::filesystem::path& path) {
    try {
        auto image = Exiv2::ImageFactory::open(path.string());
        if (!image) {
            return QStringLiteral("No EXIF metadata available.");
        }

        image->readMetadata();
        const auto& exif = image->exifData();
        if (exif.empty()) {
            return QStringLiteral("No EXIF metadata available.");
        }

        QStringList lines;

        const QString cameraModel = exifValue(exif, "Exif.Image.Model");
        const QString lensModel = exifValue(exif, "Exif.Photo.LensModel");
        const QString iso = exifValue(exif, "Exif.Photo.ISOSpeedRatings");
        const QString aperture = exifValue(exif, "Exif.Photo.FNumber");
        const QString shutter = exifValue(exif, "Exif.Photo.ExposureTime");
        const QString focalLength = exifValue(exif, "Exif.Photo.FocalLength");
        const QString dateTaken = exifValue(exif, "Exif.Photo.DateTimeOriginal");

        lines << QStringLiteral("Camera model: %1").arg(cameraModel.isEmpty() ? QStringLiteral("N/A") : cameraModel);
        lines << QStringLiteral("Lens: %1").arg(lensModel.isEmpty() ? QStringLiteral("N/A") : lensModel);
        lines << QStringLiteral("ISO: %1").arg(iso.isEmpty() ? QStringLiteral("N/A") : iso);
        lines << QStringLiteral("Aperture: %1").arg(aperture.isEmpty() ? QStringLiteral("N/A") : aperture);
        lines << QStringLiteral("Shutter speed: %1").arg(shutter.isEmpty() ? QStringLiteral("N/A") : shutter);
        lines << QStringLiteral("Focal length: %1").arg(focalLength.isEmpty() ? QStringLiteral("N/A") : focalLength);
        lines << QStringLiteral("Date taken: %1").arg(dateTaken.isEmpty() ? QStringLiteral("N/A") : dateTaken);

        return lines.join('\n');
    } catch (...) {
        return QStringLiteral("No EXIF metadata available.");
    }
}
