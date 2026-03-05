#include <filesystem>
#include <iostream>

#include <QApplication>

#include "MainWindow.h"

int main(int argc, char* argv[]) {
    QApplication::setApplicationName(QStringLiteral("photo-viewer"));
    QApplication app(argc, argv);

    if (argc < 2) {
        std::cerr << "Usage: photo-viewer <image-file>\n";
        return 1;
    }

    const std::filesystem::path startFile = argv[1];
    if (!std::filesystem::exists(startFile) || !std::filesystem::is_regular_file(startFile)) {
        std::cerr << "Provided path is not a readable file.\n";
        return 1;
    }

    MainWindow window(startFile);
    window.show();

    return app.exec();
}
