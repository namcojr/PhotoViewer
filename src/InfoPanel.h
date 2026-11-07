#pragma once

#include <QWidget>

#include "Metadata.h"

class QButtonGroup;
class QFormLayout;
class QLabel;

// Vertical side bar with file and EXIF information for the current image.
class InfoPanel : public QWidget {
    Q_OBJECT

public:
    enum class Side { Left, Right };

    explicit InfoPanel(QWidget* parent = nullptr);

    void setFileInfo(const Metadata::Fields& fields);
    void setExifInfo(const Metadata::Fields& fields);
    void clear();

    void setSide(Side side);
    [[nodiscard]] Side side() const;

signals:
    void sideChangeRequested(InfoPanel::Side side);

private:
    static void fillForm(QFormLayout* form, const Metadata::Fields& fields, const QString& emptyText);
    static QLabel* makeHeading(const QString& text, QWidget* parent);

    QFormLayout* fileForm_;
    QFormLayout* exifForm_;
    QButtonGroup* sideGroup_;
    Side side_ = Side::Right;
};
