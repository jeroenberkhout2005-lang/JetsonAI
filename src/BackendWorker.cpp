#include "BackendWorker.h"

#include <QMutexLocker>
#include <QDebug>

BackendWorker::BackendWorker(QObject* parent)
    : QObject(parent)
{}

void BackendWorker::initialise() {
    QMutexLocker lock(&m_mutex);
    m_initialised = true;
}

void BackendWorker::startSession() {
    bool shouldEmitStarted = false;

    {
        QMutexLocker lock(&m_mutex);

        if (!m_initialised) {
            qWarning() << "[BackendWorker] startSession called before initialise";
        }

        if (!m_sessionActive) {
            m_sessionActive = true;
            m_latestPendingFrame = PendingFrame{};
            shouldEmitStarted = true;
        }
    }

    if (shouldEmitStarted) {
        Q_EMIT backendStarted();
    }
}

void BackendWorker::stopSession() {
    bool shouldEmitStopped = false;

    {
        QMutexLocker lock(&m_mutex);
        if (m_sessionActive) {
            m_sessionActive = false;
            m_latestPendingFrame = PendingFrame{};
            shouldEmitStopped = true;
        }
    }

    if (shouldEmitStopped) {
        Q_EMIT backendStopped();
    }
}

void BackendWorker::resetSession() {
    {
        QMutexLocker lock(&m_mutex);
        m_latestPendingFrame = PendingFrame{};
    }

    Q_EMIT backendReset();
}

void BackendWorker::submitRoiFrame(const QImage& roiFrame, int frameIndex) {
    if (roiFrame.isNull() || frameIndex < 0) {
        return;
    }

    QSize frameSize;
    bool accepted = false;

    {
        QMutexLocker lock(&m_mutex);
        if (!m_sessionActive) {
            return;
        }

        m_latestPendingFrame.image = roiFrame.copy();
        m_latestPendingFrame.frameIndex = frameIndex;
        m_latestPendingFrame.receivedAtUtc = QDateTime::currentDateTimeUtc();

        frameSize = roiFrame.size();
        accepted = true;
    }

    if (accepted) {
        Q_EMIT pendingFrameUpdated(frameIndex, frameSize);
    }
}

bool BackendWorker::isSessionActive() const {
    QMutexLocker lock(&m_mutex);
    return m_sessionActive;
}

bool BackendWorker::hasPendingFrame() const {
    QMutexLocker lock(&m_mutex);
    return !m_latestPendingFrame.image.isNull();
}

cv::Mat BackendWorker::qImageToCvMat(const QImage& image) {
    if (image.isNull()) {
        return cv::Mat();
    }

    QImage converted;
    switch (image.format()) {
    case QImage::Format_Grayscale8:
        converted = image;
        return cv::Mat(
            converted.height(),
            converted.width(),
            CV_8UC1,
            const_cast<uchar*>(converted.bits()),
            converted.bytesPerLine()
        ).clone();

    case QImage::Format_RGB888:
        converted = image;
        return cv::Mat(
            converted.height(),
            converted.width(),
            CV_8UC3,
            const_cast<uchar*>(converted.bits()),
            converted.bytesPerLine()
        ).clone();

    default:
        converted = image.convertToFormat(QImage::Format_RGB888);
        return cv::Mat(
            converted.height(),
            converted.width(),
            CV_8UC3,
            const_cast<uchar*>(converted.bits()),
            converted.bytesPerLine()
        ).clone();
    }
}
