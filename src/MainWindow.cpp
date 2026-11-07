#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QFuture>
#include <QGuiApplication>
#include <QHBoxLayout>
#include <QImageReader>
#include <QImageWriter>
#include <QKeySequence>
#include <QLabel>
#include <QLocale>
#include <QMessageBox>
#include <QMetaObject>
#include <QPainter>
#include <QScreen>
#include <QVBoxLayout>
#include <QtConcurrent>

#include "ImageViewerWidget.h"

namespace {
constexpr int kTransientHintMs = 3000;
constexpr int kJpegQuality = 92;

const QString kSettingsPanelVisible = QStringLiteral("InfoPanel/visible");
const QString kSettingsPanelSide = QStringLiteral("InfoPanel/side");
const QString kSettingsSaveFormat = QStringLiteral("Save/format");

struct SaveFormat {
    const char* writerFormat;
    const char* suffix;
    const char* filter;
};

constexpr SaveFormat kSaveFormats[] = {
    {"jpeg", "jpg", "JPEG image (*.jpg *.jpeg)"},
    {"png", "png", "PNG image (*.png)"},
    {"bmp", "bmp", "BMP image (*.bmp)"},
};

const SaveFormat* saveFormatForSuffix(const QString& suffix) {
    const QString lower = suffix.toLower();
    if (lower == u"jpeg") {
        return &kSaveFormats[0];
    }
    for (const auto& format : kSaveFormats) {
        if (lower == QLatin1String(format.suffix)) {
            return &format;
        }
    }
    return nullptr;
}

const SaveFormat* saveFormatForFilter(const QString& filter) {
    for (const auto& format : kSaveFormats) {
        if (filter == QLatin1String(format.filter)) {
            return &format;
        }
    }
    return nullptr;
}

QString hintText() {
    struct Hint {
        const char* keys;
        const char* action;
    };
    static constexpr Hint hints[] = {
        {"← →", "Browse"},
        {"Home / End", "First / Last"},
        {"PgUp / PgDn", "Rotate"},
        {"I", "Info"},
        {"S", "Save as"},
        {"Ctrl+C", "Copy"},
        {"Del", "Trash"},
        {"Q", "Quit"},
    };

    QStringList parts;
    for (const auto& hint : hints) {
        parts << QStringLiteral("<span style=\"color:#ffffff; font-weight:600;\">%1</span>&nbsp;%2")
                     .arg(QString::fromUtf8(hint.keys).toHtmlEscaped(), QString::fromUtf8(hint.action).toHtmlEscaped());
    }
    return parts.join(QStringLiteral("&nbsp;&nbsp;&nbsp;&nbsp;"));
}

QString sideToString(const InfoPanel::Side side) {
    return side == InfoPanel::Side::Left ? QStringLiteral("left") : QStringLiteral("right");
}

InfoPanel::Side sideFromString(const QString& value) {
    return value.compare(u"left", Qt::CaseInsensitive) == 0 ? InfoPanel::Side::Left : InfoPanel::Side::Right;
}
}

MainWindow::MainWindow(const std::filesystem::path& startFile, QWidget* parent)
    : QMainWindow(parent),
      viewer_(new ImageViewerWidget(this)),
      overlayLabel_(new QLabel(this)),
      hintLabel_(new QLabel(this)),
      infoPanel_(new InfoPanel(this)),
      contentLayout_(new QHBoxLayout),
      // ~/.config/photo-viewer/photo-viewer.conf (honours $XDG_CONFIG_HOME)
      settings_(QSettings::NativeFormat, QSettings::UserScope,
                QStringLiteral("photo-viewer"), QStringLiteral("photo-viewer")) {
    auto* central = new QWidget(this);
    auto* layout = new QVBoxLayout(central);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);

    overlayLabel_->setFixedHeight(36);
    overlayLabel_->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    overlayLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    overlayLabel_->setStyleSheet(QStringLiteral(
        "QLabel {"
        " background-color: rgb(20, 20, 20);"
        " color: white;"
        " font-size: 15px;"
        " font-weight: 600;"
        "}"
    ));

    hintLabel_->setFixedHeight(26);
    hintLabel_->setAlignment(Qt::AlignVCenter | Qt::AlignHCenter);
    hintLabel_->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    hintLabel_->setTextFormat(Qt::RichText);
    hintLabel_->setStyleSheet(QStringLiteral(
        "QLabel {"
        " background-color: rgb(20, 20, 20);"
        " color: rgb(160, 160, 160);"
        " font-size: 12px;"
        "}"
    ));
    restoreHint();

    hintTimer_.setSingleShot(true);
    connect(&hintTimer_, &QTimer::timeout, this, &MainWindow::restoreHint);

    contentLayout_->setContentsMargins(0, 0, 0, 0);
    contentLayout_->setSpacing(0);
    contentLayout_->addWidget(viewer_, 1);

    layout->addWidget(overlayLabel_);
    layout->addLayout(contentLayout_, 1);
    layout->addWidget(hintLabel_);

    infoPanel_->setVisible(settings_.value(kSettingsPanelVisible, false).toBool());
    setInfoPanelSide(sideFromString(settings_.value(kSettingsPanelSide).toString()));
    connect(infoPanel_, &InfoPanel::sideChangeRequested, this, &MainWindow::setInfoPanelSide);

    setCentralWidget(central);
    setWindowTitle(QStringLiteral("Photo Viewer"));
    setMinimumSize(480, 320);
    if (const QScreen* screen = QGuiApplication::primaryScreen()) {
        resize(screen->availableGeometry().size() * 0.8);
    } else {
        resize(1280, 800);
    }

    setupActions();
    viewer_->setFocus();

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
        currentExif_ = loaded.exif;
        viewer_->setImage(currentImage_);

        auto* currentEntry = model_.currentMutable();
        if (currentEntry != nullptr) {
            currentEntry->format = loaded.format;
            currentEntry->dimensions = loaded.dimensions;
            setWindowTitle(QStringLiteral("%1 — Photo Viewer")
                               .arg(QString::fromStdString(currentEntry->path.filename().string())));
        }

        updateOverlay();
        updateInfoPanel();
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

void MainWindow::setupActions() {
    addShortcut(QKeySequence(Qt::Key_Right), &MainWindow::showNextImage);
    addShortcut(QKeySequence(Qt::Key_Left), &MainWindow::showPreviousImage);
    addShortcut(QKeySequence(Qt::Key_Home), &MainWindow::showFirstImage);
    addShortcut(QKeySequence(Qt::Key_End), &MainWindow::showLastImage);
    addShortcut(QKeySequence(Qt::Key_PageDown), &MainWindow::rotateClockwise);
    addShortcut(QKeySequence(Qt::Key_PageUp), &MainWindow::rotateCounterClockwise);
    addShortcut(QKeySequence(Qt::Key_Delete), &MainWindow::moveCurrentImageToTrash);
    addShortcut(QKeySequence(Qt::Key_I), &MainWindow::toggleInfoPanel);
    addShortcut(QKeySequence(Qt::Key_S), &MainWindow::saveCurrentImageAs);
    addShortcut(QKeySequence::Copy, &MainWindow::copyCurrentImageToClipboard);

    auto quit = [this]() { close(); };
    for (const auto key : {Qt::Key_Q, Qt::Key_Escape}) {
        auto* action = new QAction(this);
        action->setShortcut(QKeySequence(key));
        connect(action, &QAction::triggered, this, quit);
        addAction(action);
    }
}

void MainWindow::addShortcut(const QKeySequence& key, void (MainWindow::*slot)()) {
    auto* action = new QAction(this);
    action->setShortcut(key);
    action->setShortcutContext(Qt::WindowShortcut);
    connect(action, &QAction::triggered, this, slot);
    addAction(action);
}

MainWindow::LoadedImage MainWindow::loadImageFromDisk(const std::filesystem::path& path) {
    LoadedImage result;
    const QString filePath = QString::fromStdString(path.string());

    // Let Qt pick the plugin from the suffix first and fall back to content sniffing.
    // Some formats (TGA, many camera RAWs) cannot be detected from content alone.
    QImageReader reader(filePath);
    reader.setAutoTransform(true);
    if (!reader.canRead()) {
        result.error = QStringLiteral("Unreadable image");
        return result;
    }

    result.format = reader.format();
    result.image = reader.read();
    if (result.image.isNull()) {
        result.error = reader.errorString();
        return result;
    }

    // Size after EXIF orientation has been applied.
    result.dimensions = result.image.size();
    result.exif = Metadata::readExif(path);
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

void MainWindow::reloadCurrentImage() {
    if (!model_.empty()) {
        loadCurrentImageWithFallback(model_.currentIndex(), 1);
    }
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

void MainWindow::rotateClockwise() {
    viewer_->rotateClockwise();
}

void MainWindow::rotateCounterClockwise() {
    viewer_->rotateCounterClockwise();
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
        showTransientHint(QStringLiteral("Could not move %1 to trash")
                              .arg(QString::fromStdString(entry->path.filename().string()).toHtmlEscaped()));
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

void MainWindow::copyCurrentImageToClipboard() {
    if (!viewer_->hasImage()) {
        return;
    }
    QGuiApplication::clipboard()->setImage(viewer_->transformedImage());
    showTransientHint(QStringLiteral("Image copied to clipboard"));
}

void MainWindow::saveCurrentImageAs() {
    const auto* entry = model_.current();
    if (loading_ || entry == nullptr || !viewer_->hasImage()) {
        return;
    }

    const SaveFormat* initialFormat = saveFormatForSuffix(settings_.value(kSettingsSaveFormat).toString());
    if (initialFormat == nullptr) {
        initialFormat = &kSaveFormats[0];
    }

    const QFileInfo source(QString::fromStdString(entry->path.string()));
    const QString initialPath = source.dir().filePath(
        QStringLiteral("%1.%2").arg(source.completeBaseName(), QLatin1String(initialFormat->suffix)));

    QStringList filters;
    for (const auto& format : kSaveFormats) {
        filters << QString::fromLatin1(format.filter);
    }

    QFileDialog dialog(this, QStringLiteral("Save Image As"));
    dialog.setAcceptMode(QFileDialog::AcceptSave);
    dialog.setFileMode(QFileDialog::AnyFile);
    dialog.setNameFilters(filters);
    dialog.selectNameFilter(QString::fromLatin1(initialFormat->filter));
    dialog.setDefaultSuffix(QString::fromLatin1(initialFormat->suffix));
    dialog.setDirectory(source.dir());
    dialog.selectFile(initialPath);
    connect(&dialog, &QFileDialog::filterSelected, &dialog, [&dialog](const QString& filter) {
        if (const auto* format = saveFormatForFilter(filter)) {
            dialog.setDefaultSuffix(QString::fromLatin1(format->suffix));
        }
    });

    const bool accepted = dialog.exec() == QDialog::Accepted && !dialog.selectedFiles().isEmpty();
    activateWindow();
    viewer_->setFocus();
    if (!accepted) {
        return;
    }

    QString targetPath = dialog.selectedFiles().constFirst();
    const SaveFormat* format = saveFormatForSuffix(QFileInfo(targetPath).suffix());
    if (format == nullptr) {
        // Unsupported or missing extension typed by the user: use the selected filter's format.
        format = saveFormatForFilter(dialog.selectedNameFilter());
        if (format == nullptr) {
            format = initialFormat;
        }
        targetPath += QLatin1Char('.') + QLatin1String(format->suffix);
    }

    QImage image = viewer_->transformedImage();
    const bool isJpeg = qstrcmp(format->writerFormat, "jpeg") == 0;
    if (isJpeg && image.hasAlphaChannel()) {
        // JPEG has no alpha channel: flatten onto white instead of letting transparency turn black.
        QImage flattened(image.size(), QImage::Format_RGB32);
        flattened.fill(Qt::white);
        QPainter painter(&flattened);
        painter.drawImage(0, 0, image);
        painter.end();
        image = flattened;
    }

    QImageWriter writer(targetPath, format->writerFormat);
    if (isJpeg) {
        writer.setQuality(kJpegQuality);
        writer.setOptimizedWrite(true);
    }
    if (!writer.write(image)) {
        QMessageBox::warning(this, QStringLiteral("Save Image As"),
                             QStringLiteral("Could not save \"%1\":\n%2").arg(targetPath, writer.errorString()));
        return;
    }

    settings_.setValue(kSettingsSaveFormat, QString::fromLatin1(format->suffix));

    const auto savedPath = std::filesystem::path(targetPath.toStdString());
    std::error_code ec;
    if (std::filesystem::equivalent(savedPath, entry->path, ec)) {
        reloadCurrentImage();
    } else if (model_.insertFile(savedPath)) {
        updateOverlay();
        updateInfoPanel();
    }

    showTransientHint(QStringLiteral("Saved %1").arg(QFileInfo(targetPath).fileName().toHtmlEscaped()));
}

void MainWindow::toggleInfoPanel() {
    const bool show = infoPanel_->isHidden();
    infoPanel_->setVisible(show);
    settings_.setValue(kSettingsPanelVisible, show);
    if (show) {
        updateInfoPanel();
    }
    viewer_->setFocus();
}

void MainWindow::setInfoPanelSide(const InfoPanel::Side side) {
    infoPanel_->setSide(side);
    contentLayout_->removeWidget(infoPanel_);
    if (side == InfoPanel::Side::Left) {
        contentLayout_->insertWidget(0, infoPanel_);
    } else {
        contentLayout_->addWidget(infoPanel_);
    }
    settings_.setValue(kSettingsPanelSide, sideToString(side));
    viewer_->setFocus();
}

void MainWindow::updateOverlay() {
    const auto* entry = model_.current();
    if (entry == nullptr) {
        overlayLabel_->setText(QString());
        return;
    }

    overlayLabel_->setText(formatOverlayText(*entry));
}

void MainWindow::updateInfoPanel() {
    if (infoPanel_->isHidden()) {
        return;
    }

    const auto* entry = model_.current();
    if (entry == nullptr) {
        infoPanel_->clear();
        return;
    }

    infoPanel_->setFileInfo(fileFields(*entry));
    infoPanel_->setExifInfo(currentExif_);
}

void MainWindow::showTransientHint(const QString& message) {
    hintLabel_->setText(QStringLiteral("<span style=\"color:#ffffff;\">%1</span>").arg(message));
    hintTimer_.start(kTransientHintMs);
}

void MainWindow::restoreHint() {
    hintLabel_->setText(hintText());
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

Metadata::Fields MainWindow::fileFields(const ImageModel::ImageEntry& entry) const {
    const QFileInfo info(QString::fromStdString(entry.path.string()));
    const QLocale locale;

    Metadata::Fields fields;
    fields.append({QStringLiteral("Name"), info.fileName()});
    fields.append({QStringLiteral("Folder"), info.absolutePath()});
    fields.append({QStringLiteral("Position"),
                   QStringLiteral("%1 of %2").arg(model_.currentIndex() + 1).arg(model_.size())});
    fields.append({QStringLiteral("File size"),
                   locale.formattedDataSize(static_cast<qint64>(entry.fileSizeBytes))});

    if (entry.dimensions.isValid()) {
        const double megapixels =
            static_cast<double>(entry.dimensions.width()) * entry.dimensions.height() / 1'000'000.0;
        fields.append({QStringLiteral("Dimensions"),
                       QStringLiteral("%1 × %2 (%3 MP)")
                           .arg(entry.dimensions.width())
                           .arg(entry.dimensions.height())
                           .arg(QString::number(megapixels, 'f', 1))});
    }

    if (!entry.format.isEmpty()) {
        fields.append({QStringLiteral("Format"), QString::fromLatin1(entry.format).toUpper()});
    }

    const QDateTime modified = info.lastModified();
    if (modified.isValid()) {
        fields.append({QStringLiteral("Modified"), locale.toString(modified, QLocale::ShortFormat)});
    }

    return fields;
}
