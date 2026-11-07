#pragma once

#include <QImage>
#include <QPoint>
#include <QPointF>
#include <QWidget>

class ImageViewerWidget : public QWidget {
    Q_OBJECT

public:
    explicit ImageViewerWidget(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    // Replaces the pixels (e.g. the next animation frame) and keeps zoom, pan and rotation.
    void setFrame(const QImage& frame);
    void rotateClockwise();
    void rotateCounterClockwise();
    void zoomIn();
    void zoomOut();
    void zoomToFit();
    void zoomToActualSize();
    [[nodiscard]] bool hasImage() const;

    // The current image with the on-screen rotation applied.
    [[nodiscard]] QImage transformedImage() const;

protected:
    void paintEvent(QPaintEvent* event) override;
    void resizeEvent(QResizeEvent* event) override;
    void wheelEvent(QWheelEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    [[nodiscard]] bool isQuarterTurnOdd() const;
    [[nodiscard]] QSizeF scaledImageSize() const;
    [[nodiscard]] bool isPannable() const;
    void rotateBy(int quarterTurns);
    void fitToWindow();
    void clampOffset();
    void applyZoomAt(const QPointF& cursorPos, qreal zoomFactor);
    // Scale at which one image pixel covers one physical screen pixel.
    [[nodiscard]] qreal actualSizeScale() const;

    QImage image_;
    qreal scale_ = 1.0;
    QPointF offset_;
    int quarterTurns_ = 0;
    bool fitMode_ = true;
    bool dragging_ = false;
    QPoint lastMousePos_;

    static constexpr qreal kMinZoom = 0.1;
    static constexpr qreal kMaxZoom = 10.0;
    static constexpr qreal kWheelZoomStep = 1.15;
};
