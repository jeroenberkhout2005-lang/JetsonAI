#pragma once

#include <QMainWindow>
#include <QLabel>
#include <QPushButton>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QTimer>
#include <QThread>
#include <QMap>
#include <QString>
#include <QVector>
#include <QElapsedTimer>

#include "VideoViewWidget.h"

class CameraWorker;
class VideoStageWidget;
class BackendWorker;

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    explicit MainWindow(QWidget* parent = nullptr);
    ~MainWindow();

private Q_SLOTS:
    void onToggleStart();
    void onFrameReady(const QImage& frame, int frameIndex);
    void onCsvTick();
    void onRoiDragStarted();
    void onRoiDragFinished();
    void onBackendStarted();
    void onBackendStopped();
    void onBackendReset();
    void onBackendFailed(const QString& message);

private:
    void buildUI();
    void applyTheme();
    void loadCsvValues();
    void updateStatLabel(const QString& key, const QString& value);
    void resetStatsToPlaceholder();
    void archiveCurrentCsvIfExists();
    void setProcessingState(bool enabled, bool updateButton = true, bool resetStats = false);
    void clearOverlayState();
    void updateOverlayForCurrentFrame();
    void updateStatsForCurrentFrame();
    void submitLatestRoiFrameToBackend();
    QString unitForStatKey(const QString& key) const;

    QWidget* m_central;
    VideoStageWidget* m_videoStage;
    QPushButton* m_startBtn;
    QGridLayout* m_statsGrid;

    QMap<QString, QLabel*> m_statValues;
    QImage m_latestFullFrame;

    QThread* m_workerThread = nullptr;
    CameraWorker* m_cameraWorker = nullptr;

    QThread* m_backendThread = nullptr;
    BackendWorker* m_backendWorker = nullptr;

    QTimer* m_csvTimer = nullptr;

    bool m_processingActive = false;
    bool m_resumeAfterRoiDrag = false;
    int m_archivedRunCounter = 0;

    QString CSV_PATH = "results_output.csv";

    int m_latestProcessingFrameIndex = -1;
    QMap<int, QVector<ParticleOverlay>> m_particlesByFrame;
    QVector<ParticleOverlay> m_currentFrameParticles;
    bool m_showNoParticlesText = false;

    QElapsedTimer m_frameClock;
    qint64 m_lastFrameTimestampMs = -1;
    double m_smoothedFps = 0.0;
};
