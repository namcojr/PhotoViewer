#include "MainWindow.h"

#include <QAction>
#include <QApplication>
#include <QBuffer>
#include <QClipboard>
#include <QCloseEvent>
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
#include <QSaveFile>
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
const QString kSettingsWindowGeometry = QStringLiteral("Window/geometry");

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
        {"+ / − / 0", "Zoom"},
        {"F", "Fullscreen"},
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
    if (!restoreGeometry(settings_.value(kSettingsWindowGeometry).toByteArray())) {
        if (const QScreen* screen = QGuiApplication::primaryScreen()) {
            resize(screen->availableGeometry().size() * 0.8);
        } else {
            resize(1280, 800);
        }
    }

    setupActions();
    viewer_->setFocus();

    connect(&loadWatcher_, &QFutureWatcher<LoadedImage>::finished, this, &MainWindow::onImageLoaded);
    connect(&movie_, &QMovie::frameChanged, this, [this]() { viewer_->setFrame(movie_.currentImage()); });

    if (!model_.loadFromFile(startFile) || model_.empty()) {
        QMetaObject::invokeMethod(this, [this]() {
            QMessageBox::critical(this, QStringLiteral("Photo Viewer"),
                                  QStringLiteral("No valid images found in folder."));
            qApp->exit(1);
        }, Qt::QueuedConnection);
        return;
    }

    requestImage(model_.currentIndex(), 1);
}

void MainWindow::closeEvent(QCloseEvent* event) {
    // Keep the last windowed size rather than remembering fullscreen.
    if (!isFullScreen()) {
        settings_.setValue(kSettingsWindowGeometry, saveGeometry());
    }
    QMainWindow::closeEvent(event);
}

void MainWindow::setupActions() {
    addShortcut(QKeySequence(Qt::Key_Right), &MainWindow::showNextImage);
    addShortcut(QKeySequence(Qt::Key_Left), &MainWindow::showPreviousImage);
    addShortcut(QKeySequence(Qt::Key_Home), &MainWindow::showFirstImage);
    addShortcut(QKeySequence(Qt::Key_End), &MainWindow::showLastImage);
    addShortcut(QKeySequence(Qt::Key_PageDown), [this] { viewer_->rotateClockwise(); });
    addShortcut(QKeySequence(Qt::Key_PageUp), [this] { viewer_->rotateCounterClockwise(); });
    addShortcut(QKeySequence(Qt::Key_Plus), [this] { viewer_->zoomIn(); });
    addShortcut(QKeySequence(Qt::Key_Equal), [this] { viewer_->zoomIn(); });
    addShortcut(QKeySequence(Qt::Key_Minus), [this] { viewer_->zoomOut(); });
    addShortcut(QKeySequence(Qt::Key_0), [this] { viewer_->zoomToFit(); });
    addShortcut(QKeySequence(Qt::Key_1), [this] { viewer_->zoomToActualSize(); });
    addShortcut(QKeySequence(Qt::Key_F), &MainWindow::toggleFullScreen);
    addShortcut(QKeySequence(Qt::Key_F11), &MainWindow::toggleFullScreen);
    addShortcut(QKeySequence(Qt::Key_Delete), &MainWindow::moveCurrentImageToTrash);
    addShortcut(QKeySequence::Undo, &MainWindow::undoMoveToTrash);
    addShortcut(QKeySequence(Qt::Key_I), &MainWindow::toggleInfoPanel);
    addShortcut(QKeySequence(Qt::Key_S), &MainWindow::saveCurrentImageAs);
    addShortcut(QKeySequence::Copy, &MainWindow::copyCurrentImageToClipboard);
    addShortcut(QKeySequence(Qt::Key_Q), [this] { close(); });
    addShortcut(QKeySequence(Qt::Key_Escape), &MainWindow::escape);
}

template <typename Slot>
void MainWindow::addShortcut(const QKeySequence& key, Slot slot) {
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
    result.animated = reader.supportsAnimation() && reader.imageCount() > 1;
    result.exif = Metadata::readExif(path);
    return result;
}

void MainWindow::requestImage(const int index, const int direction) {
    if (index < 0 || index >= model_.size()) {
        return;
    }

    pendingIndex_ = index;
    pendingDirection_ = direction;
    loadAttemptsRemaining_ = model_.size();
    if (!loading_) {
        startPendingLoad();
    }
}

void MainWindow::startPendingLoad() {
    const auto* entry = model_.at(pendingIndex_);
    if (entry == nullptr) {
        pendingIndex_ = -1;
        return;
    }

    loading_ = true;
    loadingIndex_ = pendingIndex_;
    loadingDirection_ = pendingDirection_;
    pendingIndex_ = -1;

    const auto path = entry->path;
    loadWatcher_.setFuture(QtConcurrent::run([path]() {
        return MainWindow::loadImageFromDisk(path);
    }));
}

void MainWindow::onImageLoaded() {
    loading_ = false;

    if (pendingIndex_ >= 0) {
        // The user has already moved on: drop this image and load the newest request.
        startPendingLoad();
        return;
    }

    const LoadedImage loaded = loadWatcher_.result();
    if (loaded.image.isNull()) {
        if (--loadAttemptsRemaining_ <= 0) {
            QMessageBox::critical(this, QStringLiteral("Photo Viewer"),
                                  QStringLiteral("Failed to load any image in this folder."));
            qApp->exit(1);
            return;
        }

        // Unreadable file: keep going in the same direction.
        const int count = model_.size();
        pendingIndex_ = ((loadingIndex_ + loadingDirection_) % count + count) % count;
        pendingDirection_ = loadingDirection_;
        startPendingLoad();
        return;
    }

    model_.setCurrentIndex(loadingIndex_);
    showLoadedImage(loaded);
}

void MainWindow::showLoadedImage(const LoadedImage& loaded) {
    movie_.stop();
    currentExif_ = loaded.exif;
    viewer_->setImage(loaded.image);

    if (auto* entry = model_.currentMutable()) {
        entry->format = loaded.format;
        entry->dimensions = loaded.dimensions;
        const QString path = QString::fromStdString(entry->path.string());
        setWindowTitle(QStringLiteral("%1 — Photo Viewer").arg(QFileInfo(path).fileName()));

        if (loaded.animated) {
            movie_.setFileName(path);
            movie_.setFormat(loaded.format);
            movie_.start();
        }
    }

    updateOverlay();
    updateInfoPanel();
}

int MainWindow::targetIndex() const {
    if (pendingIndex_ >= 0) {
        return pendingIndex_;
    }
    return loading_ ? loadingIndex_ : model_.currentIndex();
}

void MainWindow::showNextImage() {
    if (!model_.empty()) {
        requestImage((targetIndex() + 1) % model_.size(), 1);
    }
}

void MainWindow::showPreviousImage() {
    if (!model_.empty()) {
        requestImage((targetIndex() - 1 + model_.size()) % model_.size(), -1);
    }
}

void MainWindow::showFirstImage() {
    requestImage(0, 1);
}

void MainWindow::showLastImage() {
    requestImage(model_.size() - 1, -1);
}

void MainWindow::toggleFullScreen() {
    // XOR keeps the maximized flag, so leaving fullscreen returns to a maximized window.
    setWindowState(windowState() ^ Qt::WindowFullScreen);
}

void MainWindow::escape() {
    if (isFullScreen()) {
        toggleFullScreen();
    } else {
        close();
    }
}

void MainWindow::moveCurrentImageToTrash() {
    const auto* entry = model_.current();
    if (loading_ || entry == nullptr) {
        return;
    }

    const QString path = QString::fromStdString(entry->path.string());
    const QString name = QFileInfo(path).fileName().toHtmlEscaped();
    QString trashPath;
    if (!QFile::moveToTrash(path, &trashPath)) {
        showTransientHint(QStringLiteral("Could not move %1 to trash").arg(name));
        return;
    }
    trashed_.push_back({path, trashPath});

    model_.removeCurrent();
    if (model_.empty()) {
        close();
        return;
    }

    requestImage(model_.currentIndex(), 1);
    showTransientHint(QStringLiteral("Moved %1 to trash — Ctrl+Z to undo").arg(name));
}

void MainWindow::undoMoveToTrash() {
    if (loading_ || trashed_.empty()) {
        return;
    }

    const TrashedFile file = trashed_.back();
    trashed_.pop_back();

    const QFileInfo original(file.originalPath);
    const QString name = original.fileName().toHtmlEscaped();
    if (original.exists() || !QFile::rename(file.trashPath, file.originalPath)) {
        showTransientHint(QStringLiteral("Could not restore %1").arg(name));
        return;
    }

    // Remove the trash's record of the file: <trash>/info/<name>.trashinfo (freedesktop spec).
    const QFileInfo inTrash(file.trashPath);
    QDir trashDir = inTrash.dir();
    if (trashDir.cdUp()) {
        QFile::remove(trashDir.filePath(QStringLiteral("info/%1.trashinfo").arg(inTrash.fileName())));
    }

    const auto restored = std::filesystem::path(file.originalPath.toStdString());
    if (model_.insertFile(restored)) {
        requestImage(model_.indexOf(restored), 1);
    }
    showTransientHint(QStringLiteral("Restored %1").arg(name));
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

    QByteArray encoded;
    QBuffer buffer(&encoded);
    buffer.open(QIODevice::WriteOnly);
    QImageWriter writer(&buffer, format->writerFormat);
    if (isJpeg) {
        writer.setQuality(kJpegQuality);
        writer.setOptimizedWrite(true);
    }
    if (!writer.write(image)) {
        QMessageBox::warning(this, QStringLiteral("Save Image As"),
                             QStringLiteral("Could not save \"%1\":\n%2").arg(targetPath, writer.errorString()));
        return;
    }

    // Read from the source before touching the target, which may be the same file.
    encoded = Metadata::withMetadataFrom(entry->path, encoded, image.size());

    // QSaveFile writes to a temporary file and renames it, so a failed save never
    // leaves a half-written image behind (important when overwriting the original).
    QSaveFile file(targetPath);
    if (!file.open(QIODevice::WriteOnly) || file.write(encoded) != encoded.size() || !file.commit()) {
        QMessageBox::warning(this, QStringLiteral("Save Image As"),
                             QStringLiteral("Could not save \"%1\":\n%2").arg(targetPath, file.errorString()));
        return;
    }

    settings_.setValue(kSettingsSaveFormat, QString::fromLatin1(format->suffix));

    const auto savedPath = std::filesystem::path(targetPath.toStdString());
    std::error_code ec;
    if (std::filesystem::equivalent(savedPath, entry->path, ec)) {
        requestImage(model_.currentIndex(), 1);
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
