#pragma once

#include <QObject>
#include <QImage>
#include <QMutex>
#include <QSize>
#include <QString>
#include <QDateTime>

#include <opencv2/opencv.hpp>

class BackendWorker : public QObject {
    Q_OBJECT

public:
    explicit BackendWorker(QObject* parent = nullptr);
    ~BackendWorker() override = default;

    // Session lifecycle thread-safe skeleton.
    void startSession();
    void stopSession();
    void resetSession();

    // Latest-frame-only handoff point from the GUI.
    // If a newer ROI arrives before the backend consumes the previous one,
    // the older pending frame is overwritten.
    void submitRoiFrame(const QImage& roiFrame, int frameIndex);

    bool isSessionActive() const;
    bool hasPendingFrame() const;

    // Reverse conversion path needed for future backend/model work.
    static cv::Mat qImageToCvMat(const QImage& image);

public Q_SLOTS:
    void initialise();

Q_SIGNALS:
    void backendStarted();
    void backendStopped();
    void backendReset();
    void backendFailed(const QString& message);
    void pendingFrameUpdated(int frameIndex, const QSize& frameSize);

private:
    struct PendingFrame {
        QImage image;
        int frameIndex = -1;
        QDateTime receivedAtUtc;
    };

    mutable QMutex m_mutex;

    bool m_sessionActive = false;
    bool m_initialised = false;
    PendingFrame m_latestPendingFrame;
};
