#pragma once

#include <QObject>
#include <QImage>
#include <QString>
#include <atomic>

// OpenCV
#include <opencv2/opencv.hpp>

// LibTorch (PyTorch C++ frontend)
#include <torch/script.h>
#include <torch/torch.h>

// Detected fat particle result.
// Kept for future direct in-memory model integration, even though the
// GUI overlay path now comes from results_output.csv rather than drawing here.
struct Detection {
    cv::Point center;
    int       radius;
    float     confidence;
};

class CameraWorker : public QObject {
    Q_OBJECT

public:
    enum class SourceType {
        Device,
        Stream
    };

    explicit CameraWorker(QObject* parent = nullptr);
    ~CameraWorker();

    // Controls whether the worker loop itself should keep running.
    void stopWorker();

    // Controls whether model/backend processing is considered active.
    // When this flips from false -> true, the processing frame counter resets
    // so the GUI and CSV can both start again from frame 0.
    void setProcessingEnabled(bool enabled);

    void setDeviceSource(int index);
    void setStreamSource(const QString& url);

public Q_SLOTS:
    // Entry point — called when thread starts.
    void run();

Q_SIGNALS:
    // Raw full frame only. The GUI owns ROI cropping and overlay drawing.
    // frameIndex is only meaningful while processing is active.
    // When processing is off, frameIndex is emitted as -1.
    void frameReady(const QImage& frame, int frameIndex);
    void errorOccurred(const QString& msg);

private:
    bool openCameraSource();
    bool loadModel();
    std::vector<Detection> runInference(const cv::Mat& frame);
    QImage matToQImage(const cv::Mat& mat);

    std::atomic<bool> m_workerRunning{true};
    std::atomic<bool> m_processingEnabled{false};

    cv::VideoCapture m_cap;
    torch::jit::script::Module m_model;
    bool m_modelLoaded = false;
    bool m_modelLoadAttempted = false;

    SourceType m_sourceType = SourceType::Device;
    int m_cameraIndex = 0;
    QString m_streamUrl;

    // Counts only processing frames so it lines up with CSV frame numbers.
    int m_processingFrameIndex = 0;

    // === CONFIGURE THESE FOR YOUR SETUP ===
    const int    INPUT_WIDTH    = 640;
    const int    INPUT_HEIGHT   = 480;
    const float  CONF_THRESHOLD = 0.5f;
    const std::string MODEL_PATH = "fat_detector.pt";
    // ======================================
};
