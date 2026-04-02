#pragma once

#include <QWidget>
#include <QImage>
#include <QRect>
#include <QString>
#include <QVector>

// ROI-relative particle overlay row parsed from results_output.csv.
struct ParticleOverlay {
    int frameIndex = -1;
    int particleId = -1;
    float xPx = 0.0f;
    float yPx = 0.0f;
    float confidence = 0.0f;
};

class VideoViewWidget : public QWidget {
    Q_OBJECT

public:
    enum class ViewMode {
        FullFeed,
        RoiFeed
    };

    explicit VideoViewWidget(QWidget* parent = nullptr);

    void setImage(const QImage& image);
    void setRoiRect(const QRect& roiRect);
    void setViewMode(ViewMode mode);
    void setShowRoiOverlay(bool show);
    void setRoiEditable(bool editable);
    void setProcessingLocked(bool locked);
    void setPlaceholderText(const QString& text);
    void setParticleOverlays(const QVector<ParticleOverlay>& overlays);
    void setShowNoParticlesText(bool show);

Q_SIGNALS:
    void roiRectChanged(const QRect& roiRect);
    void roiDragStarted();
    void roiDragFinished();
    void clicked();

protected:
    void paintEvent(QPaintEvent* event) override;
    void mousePressEvent(QMouseEvent* event) override;
    void mouseMoveEvent(QMouseEvent* event) override;
    void mouseReleaseEvent(QMouseEvent* event) override;

private:
    QRect imageRectToWidgetRect(const QRect& imageRect) const;
    QPoint widgetPointToImagePoint(const QPoint& widgetPoint) const;
    QRect clampedRoiRect(const QRect& roiRect) const;
    QRect currentSourceRect() const;
    QPointF roiPointToWidgetPoint(float xPx, float yPx) const;

    QImage m_image;
    QRect m_roiRect;
    ViewMode m_viewMode = ViewMode::FullFeed;

    bool m_showRoiOverlay = false;
    bool m_roiEditable = false;
    bool m_processingLocked = false;
    bool m_showNoParticlesText = false;

    QVector<ParticleOverlay> m_particleOverlays;

    bool m_dragging = false;
    bool m_dragMoved = false;
    bool m_dragSignalSent = false;
    QPoint m_dragOffsetImage;

    QRect m_lastDrawRect;
    QString m_placeholderText = "Camera feed will appear here";

    // Change this to tweak the GUI debug marker size later.
    static constexpr int OVERLAY_RADIUS_PX = 12;
};
