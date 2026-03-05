#include "ImageViewerWidget.h"

#include <algorithm>

#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>

ImageViewerWidget::ImageViewerWidget(QWidget* parent)
    : QWidget(parent) {
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

void ImageViewerWidget::setImage(const QImage& image) {
    image_ = image;
    offset_ = QPointF(0.0, 0.0);
    quarterTurns_ = 0;
    fitToWindow();
    update();
}

void ImageViewerWidget::rotateClockwise() {
    if (image_.isNull()) {
        return;
    }

    quarterTurns_ = (quarterTurns_ + 1) % 4;
    clampOffset();
    if (!isPannable()) {
        offset_ = QPointF(0.0, 0.0);
    }
    update();
}

void ImageViewerWidget::rotateCounterClockwise() {
    if (image_.isNull()) {
        return;
    }

    quarterTurns_ = (quarterTurns_ + 3) % 4;
    clampOffset();
    if (!isPannable()) {
        offset_ = QPointF(0.0, 0.0);
    }
    update();
}

bool ImageViewerWidget::hasImage() const {
    return !image_.isNull();
}

void ImageViewerWidget::paintEvent(QPaintEvent*) {
    QPainter painter(this);
    painter.fillRect(rect(), Qt::black);

    if (image_.isNull()) {
        return;
    }

    painter.setRenderHint(QPainter::SmoothPixmapTransform, true);

    const qreal rotationDegrees = static_cast<qreal>(quarterTurns_) * 90.0;
    const QSizeF sourceSize(static_cast<qreal>(image_.width()) * scale_,
                            static_cast<qreal>(image_.height()) * scale_);
    const QPointF center(width() / 2.0 + offset_.x(), height() / 2.0 + offset_.y());

    painter.translate(center);
    painter.rotate(rotationDegrees);

    const QRectF target(-sourceSize.width() / 2.0,
                        -sourceSize.height() / 2.0,
                        sourceSize.width(),
                        sourceSize.height());
    painter.drawImage(target, image_);
}

void ImageViewerWidget::resizeEvent(QResizeEvent*) {
    if (image_.isNull()) {
        return;
    }

    if (!isPannable()) {
        fitToWindow();
    }
    clampOffset();
}

void ImageViewerWidget::wheelEvent(QWheelEvent* event) {
    if (image_.isNull()) {
        event->ignore();
        return;
    }

    const int delta = event->angleDelta().y();
    if (delta == 0) {
        return;
    }

    const qreal factor = delta > 0 ? kWheelZoomStep : (1.0 / kWheelZoomStep);
    applyZoomAt(event->position(), factor);
    event->accept();
}

void ImageViewerWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && hasImage() && isPannable()) {
        dragging_ = true;
        lastMousePos_ = event->pos();
        setCursor(Qt::ClosedHandCursor);
        event->accept();
        return;
    }

    QWidget::mousePressEvent(event);
}

void ImageViewerWidget::mouseMoveEvent(QMouseEvent* event) {
    if (dragging_) {
        const QPoint delta = event->pos() - lastMousePos_;
        lastMousePos_ = event->pos();
        offset_ += QPointF(delta.x(), delta.y());
        clampOffset();
        update();
        event->accept();
        return;
    }

    QWidget::mouseMoveEvent(event);
}

void ImageViewerWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton && dragging_) {
        dragging_ = false;
        unsetCursor();
        event->accept();
        return;
    }

    QWidget::mouseReleaseEvent(event);
}

QSizeF ImageViewerWidget::scaledImageSize() const {
    if (image_.isNull()) {
        return QSizeF();
    }

    if (isQuarterTurnOdd()) {
        return QSizeF(image_.height() * scale_, image_.width() * scale_);
    }

    return QSizeF(image_.width() * scale_, image_.height() * scale_);
}

bool ImageViewerWidget::isQuarterTurnOdd() const {
    return (quarterTurns_ % 2) != 0;
}

bool ImageViewerWidget::isPannable() const {
    const QSizeF drawSize = scaledImageSize();
    return drawSize.width() > width() || drawSize.height() > height();
}

void ImageViewerWidget::fitToWindow() {
    if (image_.isNull() || width() <= 0 || height() <= 0) {
        scale_ = 1.0;
        return;
    }

    const qreal xRatio = static_cast<qreal>(width()) / static_cast<qreal>(image_.width());
    const qreal yRatio = static_cast<qreal>(height()) / static_cast<qreal>(image_.height());
    scale_ = std::clamp(std::min(xRatio, yRatio), kMinZoom, kMaxZoom);
    offset_ = QPointF(0.0, 0.0);
}

void ImageViewerWidget::clampOffset() {
    if (image_.isNull()) {
        offset_ = QPointF(0.0, 0.0);
        return;
    }

    const QSizeF drawSize = scaledImageSize();

    const qreal maxX = std::max(0.0, (drawSize.width() - width()) / 2.0);
    const qreal maxY = std::max(0.0, (drawSize.height() - height()) / 2.0);

    offset_.setX(std::clamp(offset_.x(), -maxX, maxX));
    offset_.setY(std::clamp(offset_.y(), -maxY, maxY));
}

void ImageViewerWidget::applyZoomAt(const QPointF& cursorPos, const qreal zoomFactor) {
    const qreal oldScale = scale_;
    scale_ = std::clamp(scale_ * zoomFactor, kMinZoom, kMaxZoom);

    if (qFuzzyCompare(oldScale, scale_)) {
        return;
    }

    const QPointF viewportCenter(width() / 2.0, height() / 2.0);
    const QPointF centered = cursorPos - viewportCenter - offset_;
    const qreal scaleRatio = scale_ / oldScale;
    offset_ -= centered * (scaleRatio - 1.0);

    clampOffset();
    update();
}
