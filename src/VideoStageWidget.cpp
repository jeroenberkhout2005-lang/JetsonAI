#include "VideoStageWidget.h"
#include "VideoViewWidget.h"

#include <QResizeEvent>

VideoStageWidget::VideoStageWidget(QWidget* parent)
    : QWidget(parent)
{
    setMinimumSize(400, 300);

    m_largeView = new VideoViewWidget(this);
    m_miniView  = new VideoViewWidget(this);

    m_largeView->setPlaceholderText("Camera feed will appear here");
    m_miniView->setPlaceholderText("ROI preview");

    m_miniView->setStyleSheet(
        "background-color:#0A0D18;"
        "border:1px solid #2C3D73;"
        "border-radius:8px;"
    );

    connect(m_largeView, &VideoViewWidget::roiRectChanged,
            this, [this](const QRect& rect) {
                m_roiRect = rect;
                updateViews();
                Q_EMIT roiRectChanged(m_roiRect);
            });

    connect(m_largeView, &VideoViewWidget::roiDragStarted,
            this, &VideoStageWidget::roiDragStarted);

    connect(m_largeView, &VideoViewWidget::roiDragFinished,
            this, &VideoStageWidget::roiDragFinished);

    connect(m_miniView, &VideoViewWidget::clicked,
            this, &VideoStageWidget::swapViews);
}

void VideoStageWidget::setFullFrame(const QImage& frame) {
    m_fullFrame = frame;
    ensureDefaultRoi();
    updateViews();
}

void VideoStageWidget::setProcessingActive(bool active) {
    m_processingActive = active;
    updateViews();
}

void VideoStageWidget::setErrorText(const QString& text) {
    m_errorText = text;
    updateViews();
}

void VideoStageWidget::setParticleOverlays(const QVector<ParticleOverlay>& overlays) {
    m_particleOverlays = overlays;
    updateViews();
}

void VideoStageWidget::setShowNoParticlesText(bool show) {
    m_showNoParticlesText = show;
    updateViews();
}

QRect VideoStageWidget::roiRect() const {
    return m_roiRect;
}

QImage VideoStageWidget::currentRoiFrame() const {
    if (m_fullFrame.isNull() || !m_roiRect.isValid()) return QImage();
    return m_fullFrame.copy(m_roiRect);
}

void VideoStageWidget::ensureDefaultRoi() {
    if (m_fullFrame.isNull()) return;
    if (m_roiRect.isValid()) return;

    int roiW = static_cast<int>(m_fullFrame.width() * 0.20);
    int roiH = static_cast<int>(m_fullFrame.height() * 0.20);

    int x = (m_fullFrame.width()  - roiW) / 2;
    int y = (m_fullFrame.height() - roiH) / 2;

    m_roiRect = QRect(x, y, roiW, roiH);
}

void VideoStageWidget::swapViews() {
    m_mainMode = (m_mainMode == MainDisplayMode::FullFeed)
        ? MainDisplayMode::RoiFeed
        : MainDisplayMode::FullFeed;

    updateViews();
}

void VideoStageWidget::updateViews() {
    bool fullIsMain = (m_mainMode == MainDisplayMode::FullFeed);

    m_largeView->setImage(m_fullFrame);
    m_largeView->setRoiRect(m_roiRect);
    m_largeView->setProcessingLocked(m_processingActive);
    m_largeView->setParticleOverlays(m_particleOverlays);
    m_largeView->setShowNoParticlesText(m_showNoParticlesText);

    if (fullIsMain) {
        m_largeView->setViewMode(VideoViewWidget::ViewMode::FullFeed);
        m_largeView->setShowRoiOverlay(true);
        m_largeView->setRoiEditable(true);
        m_largeView->setPlaceholderText(m_errorText.isEmpty() ? "Camera feed will appear here"
                                                              : "CAMERA / STREAM ERROR\n\n" + m_errorText);
    } else {
        m_largeView->setViewMode(VideoViewWidget::ViewMode::RoiFeed);
        m_largeView->setShowRoiOverlay(false);
        m_largeView->setRoiEditable(false);
        m_largeView->setPlaceholderText(m_errorText.isEmpty() ? "ROI preview"
                                                              : "ROI unavailable");
    }

    m_miniView->setImage(m_fullFrame);
    m_miniView->setRoiRect(m_roiRect);
    m_miniView->setProcessingLocked(m_processingActive);
    m_miniView->setRoiEditable(false);
    m_miniView->setParticleOverlays(m_particleOverlays);
    m_miniView->setShowNoParticlesText(m_showNoParticlesText);

    if (fullIsMain) {
        m_miniView->setViewMode(VideoViewWidget::ViewMode::RoiFeed);
        m_miniView->setShowRoiOverlay(false);
        m_miniView->setPlaceholderText("ROI preview");
    } else {
        m_miniView->setViewMode(VideoViewWidget::ViewMode::FullFeed);
        m_miniView->setShowRoiOverlay(true);
        m_miniView->setPlaceholderText(m_errorText.isEmpty() ? "Full feed"
                                                             : "CAMERA / STREAM ERROR");
    }
}

void VideoStageWidget::resizeEvent(QResizeEvent* event) {
    QWidget::resizeEvent(event);

    int margin = 12;
    m_largeView->setGeometry(rect());

    int miniW = static_cast<int>(width() * 0.26);
    miniW = qMin(miniW, 320);
    int miniH = static_cast<int>(miniW * 3.0 / 4.0);
    if (miniH > height() / 2) {
        miniH = height() / 2;
        miniW = static_cast<int>(miniH * 4.0 / 3.0);
    }
    int miniX = margin;
    int miniY = height() - miniH - margin;

    m_miniView->setGeometry(margin, margin, miniW, miniH);
    m_miniView->raise();
}
