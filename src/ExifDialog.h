#pragma once

#include <QDialog>

class QPlainTextEdit;

class ExifDialog : public QDialog {
    Q_OBJECT

public:
    explicit ExifDialog(QWidget* parent = nullptr);

    void setExifText(const QString& exifText);

protected:
    void keyPressEvent(QKeyEvent* event) override;

private:
    QPlainTextEdit* textView_;
};
