#include "CameraWorker.h"

#include <QThread>
#include <QDebug>
#include <stdexcept>

CameraWorker::CameraWorker(QObject* parent)
    : QObject(parent)
{}

CameraWorker::~CameraWorker() {
    m_workerRunning = false;
    if (m_cap.isOpened())
        m_cap.release();
}

void CameraWorker::stopWorker() {
    m_workerRunning = false;
}

void CameraWorker::setProcessingEnabled(bool enabled) {
    const bool wasEnabled = m_processingEnabled.load();
    m_processingEnabled = enabled;

    // Each processing session starts from frame 0 so the GUI can match
    // results_output.csv rows by frame number cleanly.
    if (enabled && !wasEnabled) {
        m_processingFrameIndex = 0;
    }
}

void CameraWorker::setDeviceSource(int index) {
    m_sourceType = SourceType::Device;
    m_cameraIndex = index;
    m_streamUrl.clear();
}

void CameraWorker::setStreamSource(const QString& url) {
    m_sourceType = SourceType::Stream;
    m_streamUrl = url;
}

bool CameraWorker::openCameraSource() {
    if (m_cap.isOpened()) {
        m_cap.release();
    }

    bool opened = false;

    if (m_sourceType == SourceType::Device) {
        qDebug() << "[CameraWorker] Opening device camera index:" << m_cameraIndex;
        opened = m_cap.open(m_cameraIndex);
    } else {
        qDebug() << "[CameraWorker] Opening stream URL:" << m_streamUrl;
        opened = m_cap.open(m_streamUrl.toStdString());
    }

    if (!opened || !m_cap.isOpened()) {
        QString msg;
        if (m_sourceType == SourceType::Device) {
            msg = "Failed to open camera device index " + QString::number(m_cameraIndex);
        } else {
            msg = "Failed to open stream URL:\n" + m_streamUrl;
        }
        Q_EMIT errorOccurred(msg);
        return false;
    }

    m_cap.set(cv::CAP_PROP_FRAME_WIDTH, INPUT_WIDTH);
    m_cap.set(cv::CAP_PROP_FRAME_HEIGHT, INPUT_HEIGHT);

    return true;
}

// ─────────────────────────────────────────────
//  Thread entry point
// ─────────────────────────────────────────────
void CameraWorker::run() {
    if (!openCameraSource()) {
        return;
    }

    cv::Mat frame;

    while (m_workerRunning) {
        if (!m_cap.read(frame) || frame.empty()) {
            qWarning() << "[CameraWorker] Empty frame — skipping";
            QThread::msleep(30);
            continue;
        }

        const bool processingEnabled = m_processingEnabled.load();

        // Keep temporary model-loading behaviour available for later real model
        // integration, but do not draw or emit detections from here anymore.
        if (processingEnabled && !m_modelLoadAttempted) {
            m_modelLoaded = loadModel();
            m_modelLoadAttempted = true;

            if (!m_modelLoaded) {
                qWarning() << "[CameraWorker] Model not loaded — GUI preview / CSV mode only";
            }
        }

        const int frameIndex = processingEnabled ? m_processingFrameIndex++ : -1;
        Q_EMIT frameReady(matToQImage(frame), frameIndex);

        QThread::msleep(33);
    }

    m_cap.release();
}

// ─────────────────────────────────────────────
//  Model loading
// ─────────────────────────────────────────────
bool CameraWorker::loadModel() {
    try {
        m_model = torch::jit::load(MODEL_PATH);
        m_model.eval();

        if (torch::cuda::is_available()) {
            m_model.to(torch::kCUDA);
            qDebug() << "[CameraWorker] Model loaded on CUDA";
        } else {
            qDebug() << "[CameraWorker] Model loaded on CPU";
        }
        return true;
    } catch (const std::exception& e) {
        qWarning() << "[CameraWorker] Model load failed:" << e.what();
        return false;
    }
}

// ─────────────────────────────────────────────
//  Inference
//  ─ Kept for future direct model integration.
//    It is not currently used by the GUI overlay path.
// ─────────────────────────────────────────────
std::vector<Detection> CameraWorker::runInference(const cv::Mat& frame) {
    std::vector<Detection> results;

    try {
        cv::Mat rgb;
        cv::cvtColor(frame, rgb, cv::COLOR_BGR2RGB);

        cv::Mat resized;
        cv::resize(rgb, resized, cv::Size(INPUT_WIDTH, INPUT_HEIGHT));

        torch::Tensor tensor = torch::from_blob(
            resized.data,
            {1, INPUT_HEIGHT, INPUT_WIDTH, 3},
            torch::kByte
        ).permute({0, 3, 1, 2}).to(torch::kFloat32).div(255.0f);

        if (torch::cuda::is_available())
            tensor = tensor.to(torch::kCUDA);

        std::vector<torch::jit::IValue> inputs{tensor};
        auto output = m_model.forward(inputs).toTensor().cpu();

        auto accessor = output.accessor<float, 2>();
        for (int i = 0; i < accessor.size(0); ++i) {
            float conf = accessor[i][4];
            if (conf < CONF_THRESHOLD) continue;

            float cx = accessor[i][0] * frame.cols;
            float cy = accessor[i][1] * frame.rows;
            float w  = accessor[i][2] * frame.cols;
            float h  = accessor[i][3] * frame.rows;
            int   r  = static_cast<int>(std::max(w, h) / 2.0f);

            results.push_back({
                cv::Point(static_cast<int>(cx), static_cast<int>(cy)),
                r,
                conf
            });
        }

    } catch (const std::exception& e) {
        qWarning() << "[CameraWorker] Inference error:" << e.what();
    }

    return results;
}

// ─────────────────────────────────────────────
//  cv::Mat → QImage (RGB888, deep copy)
// ─────────────────────────────────────────────
QImage CameraWorker::matToQImage(const cv::Mat& mat) {
    cv::Mat rgb;
    cv::cvtColor(mat, rgb, cv::COLOR_BGR2RGB);
    return QImage(
        rgb.data,
        rgb.cols, rgb.rows,
        static_cast<int>(rgb.step),
        QImage::Format_RGB888
    ).copy();
}
