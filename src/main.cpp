#include <filesystem>
#include <iostream>

#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QIcon>
#include <QImageReader>
#include <QUrl>

#include "MainWindow.h"
#include "Metadata.h"

namespace {
// Qt refuses to decode images that need more than 256 MB by default, which rules out
// large panoramas and high-resolution camera files. Raise the ceiling generously.
constexpr int kImageAllocationLimitMb = 4096;

void printSupportedFormats() {
    std::cout << "Readable image formats (from installed Qt image plugins):\n ";
    for (const QByteArray& format : QImageReader::supportedImageFormats()) {
        std::cout << ' ' << format.constData();
    }
    std::cout << "\n\nMIME types:\n";
    for (const QByteArray& mime : QImageReader::supportedMimeTypes()) {
        std::cout << "  " << mime.constData() << '\n';
    }
}
}

int main(int argc, char* argv[]) {
    QApplication::setApplicationName(QStringLiteral("photo-viewer"));
    QApplication::setApplicationDisplayName(QStringLiteral("Photo Viewer"));
    QApplication::setApplicationVersion(QStringLiteral(PHOTO_VIEWER_VERSION));
    // Matches photo-viewer.desktop: Wayland compositors (KWin on Plasma 6) use it as the
    // app_id to pick the taskbar icon and group windows.
    QGuiApplication::setDesktopFileName(QStringLiteral("photo-viewer"));

    QApplication app(argc, argv);
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("image-x-generic")));

    QCommandLineParser parser;
    parser.setApplicationDescription(QStringLiteral("Minimal keyboard-first photo viewer."));
    parser.addHelpOption();
    parser.addVersionOption();
    const QCommandLineOption formatsOption(QStringLiteral("formats"),
                                           QStringLiteral("List the image formats this system can open."));
    parser.addOption(formatsOption);
    parser.addPositionalArgument(QStringLiteral("image"), QStringLiteral("Image file to open."));
    parser.process(app);

    if (parser.isSet(formatsOption)) {
        printSupportedFormats();
        return 0;
    }

    const QStringList args = parser.positionalArguments();
    if (args.isEmpty()) {
        std::cerr << "Usage: photo-viewer <image-file>\n";
        return 1;
    }

    // Accept both plain paths and file:// URLs (some launchers pass URLs).
    const QUrl url = QUrl::fromUserInput(args.constFirst(), QDir::currentPath(), QUrl::AssumeLocalFile);
    const QString localPath = url.isLocalFile() ? url.toLocalFile() : args.constFirst();

    const std::filesystem::path startFile = localPath.toStdString();
    std::error_code ec;
    if (!std::filesystem::is_regular_file(startFile, ec)) {
        std::cerr << "Provided path is not a readable file.\n";
        return 1;
    }

    QImageReader::setAllocationLimit(kImageAllocationLimitMb);
    Metadata::initialize();

    MainWindow window(startFile);
    window.show();

    return app.exec();
}
