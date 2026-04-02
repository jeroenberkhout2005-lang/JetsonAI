#pragma once

#include <QWidget>
#include <QImage>
#include <QRect>
#include <QVector>

#include "VideoViewWidget.h"

class VideoViewWidget;

class VideoStageWidget : public QWidget {
    Q_OBJECT

public:
    enum class MainDisplayMode {
        FullFeed,
        RoiFeed
    };

    explicit VideoStageWidget(QWidget* parent = nullptr);

    void setFullFrame(const QImage& frame);
    void setProcessingActive(bool active);
    void setErrorText(const QString& text);
    void setParticleOverlays(const QVector<ParticleOverlay>& overlays);
    void setShowNoParticlesText(bool show);

    QRect roiRect() const;
    QImage currentRoiFrame() const;

Q_SIGNALS:
    void roiRectChanged(const QRect& roiRect);
    void roiDragStarted();
    void roiDragFinished();

protected:
    void resizeEvent(QResizeEvent* event) override;

private:
    void updateViews();
    void ensureDefaultRoi();
    void swapViews();

    VideoViewWidget* m_largeView = nullptr;
    VideoViewWidget* m_miniView = nullptr;

    QImage m_fullFrame;
    QRect m_roiRect;
    bool m_processingActive = false;
    QString m_errorText;
    QVector<ParticleOverlay> m_particleOverlays;
    bool m_showNoParticlesText = false;

    MainDisplayMode m_mainMode = MainDisplayMode::FullFeed;
};
