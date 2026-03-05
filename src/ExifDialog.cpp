#include "ExifDialog.h"

#include <QFontDatabase>
#include <QKeyEvent>
#include <QPlainTextEdit>
#include <QVBoxLayout>

ExifDialog::ExifDialog(QWidget* parent)
    : QDialog(parent),
      textView_(new QPlainTextEdit(this)) {
    setWindowTitle(QStringLiteral("EXIF Metadata"));
    setModal(true);
    resize(520, 420);

    textView_->setReadOnly(true);
    textView_->setFont(QFontDatabase::systemFont(QFontDatabase::FixedFont));

    auto* layout = new QVBoxLayout(this);
    layout->addWidget(textView_);
}

void ExifDialog::setExifText(const QString& exifText) {
    textView_->setPlainText(exifText);
}

void ExifDialog::keyPressEvent(QKeyEvent* event) {
    if (event->key() == Qt::Key_Escape || event->key() == Qt::Key_Q) {
        reject();
        return;
    }

    QDialog::keyPressEvent(event);
}
