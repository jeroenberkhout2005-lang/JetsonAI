#include "VideoViewWidget.h"

#include <QPainter>
#include <QMouseEvent>
#include <QPen>
#include <QBrush>
#include <QDebug>
#include <QFontMetrics>
#include <QtMath>

VideoViewWidget::VideoViewWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(160, 120);
    setMouseTracking(true);
}

void VideoViewWidget::setImage(const QImage& image) {
    m_image = image;
    update();
}

void VideoViewWidget::setRoiRect(const QRect& roiRect) {
    m_roiRect = roiRect;
    update();
}

void VideoViewWidget::setViewMode(ViewMode mode) {
    m_viewMode = mode;
    update();
}

void VideoViewWidget::setShowRoiOverlay(bool show) {
    m_showRoiOverlay = show;
    update();
}

void VideoViewWidget::setRoiEditable(bool editable) {
    m_roiEditable = editable;
    update();
}

void VideoViewWidget::setProcessingLocked(bool locked) {
    m_processingLocked = locked;
    update();
}

void VideoViewWidget::setPlaceholderText(const QString& text) {
    m_placeholderText = text;
    update();
}

void VideoViewWidget::setParticleOverlays(const QVector<ParticleOverlay>& overlays) {
    m_particleOverlays = overlays;
    update();
}

void VideoViewWidget::setShowNoParticlesText(bool show) {
    m_showNoParticlesText = show;
    update();
}

QRect VideoViewWidget::currentSourceRect() const {
    if (m_image.isNull()) return QRect();

    if (m_viewMode == ViewMode::FullFeed || !m_roiRect.isValid()) {
        return QRect(0, 0, m_image.width(), m_image.height());
    }

    QRect imageBounds(0, 0, m_image.width(), m_image.height());
    return m_roiRect.intersected(imageBounds);
}

QRect VideoViewWidget::imageRectToWidgetRect(const QRect& imageRect) const {
    if (m_image.isNull() || m_lastDrawRect.isEmpty()) return QRect();

    QRect source = currentSourceRect();
    if (!source.isValid() || source.width() <= 0 || source.height() <= 0) return QRect();

    double sx = static_cast<double>(m_lastDrawRect.width()) / source.width();
    double sy = static_cast<double>(m_lastDrawRect.height()) / source.height();

    int x = m_lastDrawRect.x() + static_cast<int>((imageRect.x() - source.x()) * sx);
    int y = m_lastDrawRect.y() + static_cast<int>((imageRect.y() - source.y()) * sy);
    int w = static_cast<int>(imageRect.width() * sx);
    int h = static_cast<int>(imageRect.height() * sy);

    return QRect(x, y, w, h);
}

QPoint VideoViewWidget::widgetPointToImagePoint(const QPoint& widgetPoint) const {
    if (m_image.isNull() || m_lastDrawRect.isEmpty()) return QPoint();

    QRect source = currentSourceRect();
    if (!source.isValid() || source.width() <= 0 || source.height() <= 0) return QPoint();

    double sx = static_cast<double>(source.width()) / m_lastDrawRect.width();
    double sy = static_cast<double>(source.height()) / m_lastDrawRect.height();

    int x = source.x() + static_cast<int>((widgetPoint.x() - m_lastDrawRect.x()) * sx);
    int y = source.y() + static_cast<int>((widgetPoint.y() - m_lastDrawRect.y()) * sy);

    return QPoint(x, y);
}

QRect VideoViewWidget::clampedRoiRect(const QRect& roiRect) const {
    if (m_image.isNull()) return roiRect;

    QRect result = roiRect;
    int maxX = m_image.width() - result.width();
    int maxY = m_image.height() - result.height();

    if (result.x() < 0) result.moveLeft(0);
    if (result.y() < 0) result.moveTop(0);
    if (result.x() > maxX) result.moveLeft(maxX);
    if (result.y() > maxY) result.moveTop(maxY);

    return result;
}

QPointF VideoViewWidget::roiPointToWidgetPoint(float xPx, float yPx) const {
    QRect source = currentSourceRect();
    if (!source.isValid() || source.width() <= 0 || source.height() <= 0 || m_lastDrawRect.isEmpty()) {
        return QPointF();
    }

    // ROI overlay coordinates are ROI-relative, so for the ROI feed they map from
    // local ROI image space directly into the displayed ROI widget rectangle.
    double sx = static_cast<double>(m_lastDrawRect.width()) / source.width();
    double sy = static_cast<double>(m_lastDrawRect.height()) / source.height();

    return QPointF(
        m_lastDrawRect.x() + (xPx * sx),
        m_lastDrawRect.y() + (yPx * sy)
    );
}

void VideoViewWidget::paintEvent(QPaintEvent*) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.fillRect(rect(), QColor("#070914"));

    if (m_image.isNull()) {
        p.setPen(QColor("#4C7DBA"));
        p.drawText(rect(), Qt::AlignCenter, m_placeholderText);
        return;
    }

    QRect sourceRect = currentSourceRect();
    if (!sourceRect.isValid()) {
        p.setPen(QColor("#4C7DBA"));
        p.drawText(rect(), Qt::AlignCenter, m_placeholderText);
        return;
    }

    QImage displayImage = m_image.copy(sourceRect);
    QSize scaledSize = displayImage.size();
    scaledSize.scale(size(), Qt::KeepAspectRatio);

    int x = (width()  - scaledSize.width()) / 2;
    int y = (height() - scaledSize.height()) / 2;
    m_lastDrawRect = QRect(x, y, scaledSize.width(), scaledSize.height());

    p.drawImage(m_lastDrawRect, displayImage);

    if (m_viewMode == ViewMode::FullFeed && m_showRoiOverlay && m_roiRect.isValid()) {
        QRect roiWidget = imageRectToWidgetRect(m_roiRect);

        QColor lineColor = m_processingLocked
            ? QColor(255, 80, 80, 160)
            : QColor(255, 80, 80, 220);

        int boxThickness = m_processingLocked ? 1 : 2;
        int guideThickness = 1;

        QPen guidePen(lineColor, guideThickness);
        guidePen.setCosmetic(true);
        p.setPen(guidePen);

        QPoint topMid(roiWidget.center().x(), roiWidget.top());
        QPoint bottomMid(roiWidget.center().x(), roiWidget.bottom());
        QPoint leftMid(roiWidget.left(), roiWidget.center().y());
        QPoint rightMid(roiWidget.right(), roiWidget.center().y());

        p.drawLine(topMid, QPoint(topMid.x(), m_lastDrawRect.top()));
        p.drawLine(bottomMid, QPoint(bottomMid.x(), m_lastDrawRect.bottom()));
        p.drawLine(leftMid, QPoint(m_lastDrawRect.left(), leftMid.y()));
        p.drawLine(rightMid, QPoint(m_lastDrawRect.right(), rightMid.y()));

        QPen boxPen(lineColor, boxThickness);
        boxPen.setCosmetic(true);
        p.setPen(boxPen);

        QColor fillColor = m_processingLocked
            ? QColor(255, 80, 80, 18)
            : QColor(255, 80, 80, 28);
        p.setBrush(fillColor);
        p.drawRect(roiWidget);
    }

    // Draw ROI-only particle overlays.
    if (m_viewMode == ViewMode::RoiFeed && m_processingLocked) {
        QRect source = currentSourceRect();
        const double sx = static_cast<double>(m_lastDrawRect.width()) / qMax(1, source.width());
        const double sy = static_cast<double>(m_lastDrawRect.height()) / qMax(1, source.height());
        const double radiusScale = qMin(sx, sy);
        const int drawRadius = qMax(4, static_cast<int>(OVERLAY_RADIUS_PX * radiusScale));

        QPen circlePen(QColor(255, 60, 60), 2);
        circlePen.setCosmetic(true);
        p.setPen(circlePen);
        p.setBrush(Qt::NoBrush);

        QFont labelFont = p.font();
        labelFont.setBold(true);
        labelFont.setPointSize(10);
        p.setFont(labelFont);

        for (const auto& overlay : m_particleOverlays) {
            QPointF centre = roiPointToWidgetPoint(overlay.xPx, overlay.yPx);
            p.drawEllipse(centre, drawRadius, drawRadius);

            QString idText = QString::number(overlay.particleId);
            QFontMetrics fm(p.font());
            QRect textRect = fm.boundingRect(idText);
            QRect centredTextRect(
                static_cast<int>(centre.x() - textRect.width() / 2.0),
                static_cast<int>(centre.y() - textRect.height() / 2.0),
                textRect.width(),
                textRect.height()
            );

            p.setPen(QColor(255, 90, 90));
            p.drawText(centredTextRect, Qt::AlignCenter, idText);
            p.setPen(circlePen);
        }

        if (m_showNoParticlesText) {
            QFont messageFont = p.font();
            messageFont.setBold(true);
            messageFont.setPointSize(14);
            p.setFont(messageFont);
            p.setPen(QColor(255, 70, 70));
            p.drawText(m_lastDrawRect, Qt::AlignCenter, "NO PARTICLES DETECTED");
        }
    }

    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor("#1E2A55"), 1));
    p.drawRect(rect().adjusted(0, 0, -1, -1));
}

void VideoViewWidget::mousePressEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;

    m_dragging = false;
    m_dragMoved = false;
    m_dragSignalSent = false;

    if (m_viewMode == ViewMode::FullFeed && m_roiEditable && !m_image.isNull() && m_roiRect.isValid()) {
        QPoint imgPt = widgetPointToImagePoint(event->pos());
        if (m_roiRect.contains(imgPt)) {
            m_dragging = true;
            m_dragOffsetImage = imgPt - m_roiRect.topLeft();
        }
    }
}

void VideoViewWidget::mouseMoveEvent(QMouseEvent* event) {
    if (!m_dragging || !m_roiEditable || m_image.isNull() || !m_roiRect.isValid())
        return;

    QPoint imgPt = widgetPointToImagePoint(event->pos());
    QPoint newTopLeft = imgPt - m_dragOffsetImage;

    QRect movedRect(newTopLeft, m_roiRect.size());
    movedRect = clampedRoiRect(movedRect);

    if (movedRect != m_roiRect) {
        if (!m_dragSignalSent) {
            Q_EMIT roiDragStarted();
            m_dragSignalSent = true;
        }

        m_roiRect = movedRect;
        m_dragMoved = true;
        Q_EMIT roiRectChanged(m_roiRect);
        update();
    }
}

void VideoViewWidget::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() != Qt::LeftButton) return;

    bool wasDragging = m_dragging;
    bool wasMoved = m_dragMoved;
    bool dragSignalSent = m_dragSignalSent;

    m_dragging = false;
    m_dragMoved = false;
    m_dragSignalSent = false;

    if (dragSignalSent && wasDragging && wasMoved) {
        Q_EMIT roiDragFinished();
        return;
    }

    Q_EMIT clicked();
}
