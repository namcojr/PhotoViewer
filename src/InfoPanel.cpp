#include "InfoPanel.h"

#include <QButtonGroup>
#include <QFormLayout>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QScrollArea>
#include <QToolButton>
#include <QVBoxLayout>

namespace {
constexpr int kPanelWidth = 300;
constexpr int kMaxUnbrokenRun = 20;

// Lets long file names and paths wrap inside the fixed-width panel.
QString breakable(const QString& text) {
    static const QChar zeroWidthSpace(0x200B);
    QString out;
    out.reserve(text.size() + text.size() / 8);
    int run = 0;
    for (const QChar ch : text) {
        out.append(ch);
        if (ch.isSpace()) {
            run = 0;
            continue;
        }
        ++run;
        if (ch == u'/' || ch == u'_' || ch == u'-' || ch == u'.' || run >= kMaxUnbrokenRun) {
            out.append(zeroWidthSpace);
            run = 0;
        }
    }
    return out;
}
}

InfoPanel::InfoPanel(QWidget* parent)
    : QWidget(parent),
      fileForm_(new QFormLayout),
      exifForm_(new QFormLayout),
      sideGroup_(new QButtonGroup(this)) {
    setFixedWidth(kPanelWidth);
    setFocusPolicy(Qt::NoFocus);
    setAutoFillBackground(true);
    setBackgroundRole(QPalette::Window);

    auto* content = new QWidget;
    auto* contentLayout = new QVBoxLayout(content);
    contentLayout->setContentsMargins(14, 12, 14, 12);
    contentLayout->setSpacing(6);

    for (auto* form : {fileForm_, exifForm_}) {
        form->setLabelAlignment(Qt::AlignLeft | Qt::AlignTop);
        form->setFormAlignment(Qt::AlignLeft | Qt::AlignTop);
        form->setRowWrapPolicy(QFormLayout::DontWrapRows);
        form->setFieldGrowthPolicy(QFormLayout::AllNonFixedFieldsGrow);
        form->setHorizontalSpacing(10);
        form->setVerticalSpacing(4);
    }

    contentLayout->addWidget(makeHeading(QStringLiteral("File"), content));
    contentLayout->addLayout(fileForm_);
    contentLayout->addSpacing(10);
    contentLayout->addWidget(makeHeading(QStringLiteral("Camera"), content));
    contentLayout->addLayout(exifForm_);
    contentLayout->addStretch(1);

    auto* scroll = new QScrollArea(this);
    scroll->setWidget(content);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
    scroll->setFocusPolicy(Qt::NoFocus);

    auto* separator = new QFrame(this);
    separator->setFrameShape(QFrame::HLine);
    separator->setFrameShadow(QFrame::Sunken);

    auto* sideLabel = new QLabel(QStringLiteral("Panel position"), this);
    auto* leftButton = new QToolButton(this);
    auto* rightButton = new QToolButton(this);
    leftButton->setText(QStringLiteral("Left"));
    rightButton->setText(QStringLiteral("Right"));
    for (auto* button : {leftButton, rightButton}) {
        button->setCheckable(true);
        button->setFocusPolicy(Qt::NoFocus);
        button->setToolButtonStyle(Qt::ToolButtonTextOnly);
        button->setMinimumWidth(56);
    }
    sideGroup_->setExclusive(true);
    sideGroup_->addButton(leftButton, static_cast<int>(Side::Left));
    sideGroup_->addButton(rightButton, static_cast<int>(Side::Right));
    rightButton->setChecked(true);

    connect(sideGroup_, &QButtonGroup::idClicked, this, [this](const int id) {
        emit sideChangeRequested(static_cast<Side>(id));
    });

    auto* sideRow = new QHBoxLayout;
    sideRow->setContentsMargins(14, 8, 14, 10);
    sideRow->addWidget(sideLabel);
    sideRow->addStretch(1);
    sideRow->addWidget(leftButton);
    sideRow->addWidget(rightButton);

    auto* layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(scroll, 1);
    layout->addWidget(separator);
    layout->addLayout(sideRow);

    clear();
}

void InfoPanel::setFileInfo(const Metadata::Fields& fields) {
    fillForm(fileForm_, fields, QStringLiteral("No image"));
}

void InfoPanel::setExifInfo(const Metadata::Fields& fields) {
    fillForm(exifForm_, fields, QStringLiteral("No EXIF metadata"));
}

void InfoPanel::clear() {
    setFileInfo({});
    setExifInfo({});
}

void InfoPanel::setSide(const Side side) {
    side_ = side;
    if (auto* button = sideGroup_->button(static_cast<int>(side))) {
        button->setChecked(true);
    }
}

InfoPanel::Side InfoPanel::side() const {
    return side_;
}

void InfoPanel::fillForm(QFormLayout* form, const Metadata::Fields& fields, const QString& emptyText) {
    while (form->rowCount() > 0) {
        form->removeRow(0);
    }

    auto* parent = form->parentWidget();

    if (fields.isEmpty()) {
        auto* empty = new QLabel(emptyText, parent);
        empty->setEnabled(false);
        form->addRow(empty);
        return;
    }

    // Same label column width in every section so values line up.
    const int labelWidth = parent->fontMetrics().horizontalAdvance(QStringLiteral("White balance")) + 4;

    for (const auto& [label, value] : fields) {
        auto* key = new QLabel(label, parent);
        key->setForegroundRole(QPalette::PlaceholderText);
        key->setMinimumWidth(labelWidth);

        auto* val = new QLabel(breakable(value), parent);
        val->setWordWrap(true);
        val->setTextFormat(Qt::PlainText);
        val->setTextInteractionFlags(Qt::TextSelectableByMouse);
        val->setFocusPolicy(Qt::NoFocus);
        val->setMinimumWidth(1);

        form->addRow(key, val);
    }
}

QLabel* InfoPanel::makeHeading(const QString& text, QWidget* parent) {
    auto* heading = new QLabel(text, parent);
    QFont font = heading->font();
    font.setBold(true);
    font.setPointSizeF(font.pointSizeF() * 1.1);
    heading->setFont(font);
    return heading;
}
