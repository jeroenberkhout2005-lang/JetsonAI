#include "MainWindow.h"
#include "CameraWorker.h"
#include "VideoStageWidget.h"
#include "BackendWorker.h"

#include <QApplication>
#include <QFile>
#include <QTextStream>
#include <QStringList>
#include <QDebug>
#include <QSizePolicy>
#include <QFont>
#include <QFrame>
#include <QStatusBar>
#include <QStyle>
#include <QFileInfo>
#include <QDir>
#include <QCoreApplication>

static const QList<QPair<QString, QString>> STAT_DEFS = {
    { "Particle Diameter",  "μm"      },
    { "Fat Content",        "%"       },
    { "Roundness",          ""        },
    { "Particle Count",     "/ frame" },
    { "Confidence",         "%"       },
    { "Frame Rate",         "fps"     },
    { "Mean Droplet Size",  "μm"      },
};

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("Fat Particle Detector  —  Jetson Nano - GUI 3.0");
    setWindowIcon(QIcon("resources/app_icon.ico"));
    resize(1280, 720);
    applyTheme();
    buildUI();


    CSV_PATH = QCoreApplication::applicationDirPath() + "/results_output.csv";

    m_workerThread = new QThread(this);
    m_cameraWorker = new CameraWorker();
    m_cameraWorker->moveToThread(m_workerThread);

    m_cameraWorker->setDeviceSource(0);
    // m_cameraWorker->setStreamSource("PASTE URL HERE");

    connect(m_workerThread, &QThread::started,
            m_cameraWorker, &CameraWorker::run);

    connect(m_cameraWorker, &CameraWorker::frameReady,
            this, &MainWindow::onFrameReady,
            Qt::QueuedConnection);

    connect(m_cameraWorker, &CameraWorker::errorOccurred,
            this, [this](const QString& msg) {
                qCritical() << msg;
                m_videoStage->setErrorText(msg);
                statusBar()->showMessage(
                    "  ● CAMERA ERROR     ● PREVIEW FAILED     |     JETSON NANO  |  CUDA");
            });

    connect(m_workerThread, &QThread::finished,
            m_cameraWorker, &QObject::deleteLater);

    m_backendThread = new QThread(this);
    m_backendWorker = new BackendWorker();
    m_backendWorker->moveToThread(m_backendThread);

    connect(m_backendThread, &QThread::started,
            m_backendWorker, &BackendWorker::initialise);

    connect(m_backendThread, &QThread::finished,
            m_backendWorker, &QObject::deleteLater);

    connect(m_backendWorker, &BackendWorker::backendStarted,
            this, &MainWindow::onBackendStarted,
            Qt::QueuedConnection);

    connect(m_backendWorker, &BackendWorker::backendStopped,
            this, &MainWindow::onBackendStopped,
            Qt::QueuedConnection);

    connect(m_backendWorker, &BackendWorker::backendReset,
            this, &MainWindow::onBackendReset,
            Qt::QueuedConnection);

    connect(m_backendWorker, &BackendWorker::backendFailed,
            this, &MainWindow::onBackendFailed,
            Qt::QueuedConnection);

    m_csvTimer = new QTimer(this);
    m_csvTimer->setInterval(100);
    connect(m_csvTimer, &QTimer::timeout, this, &MainWindow::onCsvTick);

    connect(m_videoStage, &VideoStageWidget::roiDragStarted,
            this, &MainWindow::onRoiDragStarted);

    connect(m_videoStage, &VideoStageWidget::roiDragFinished,
            this, &MainWindow::onRoiDragFinished);

    resetStatsToPlaceholder();
    clearOverlayState();

    m_workerThread->start();
    m_backendThread->start();

    statusBar()->showMessage(
        "  ● CAMERA PREVIEW LIVE     ● MODEL READY     |     JETSON NANO  |  CUDA");
}

MainWindow::~MainWindow() {
    if (m_backendWorker) {
        m_backendWorker->stopSession();
    }
    if (m_cameraWorker) {
        m_cameraWorker->stopWorker();
    }
    if (m_backendThread) {
        m_backendThread->quit();
        m_backendThread->wait(3000);
    }
    if (m_workerThread) {
        m_workerThread->quit();
        m_workerThread->wait(3000);
    }
}

void MainWindow::buildUI() {
    m_central = new QWidget(this);
    setCentralWidget(m_central);

    auto* root = new QHBoxLayout(m_central);
    root->setContentsMargins(0, 0, 0, 0);
    root->setSpacing(0);

    auto* leftPane = new QWidget();
    leftPane->setObjectName("leftPane");
    auto* leftLayout = new QVBoxLayout(leftPane);
    leftLayout->setContentsMargins(12, 12, 12, 12);

    auto* camHeader = new QHBoxLayout();
    auto* camTitle = new QLabel("CAM 0  /  LIVE");
    camTitle->setObjectName("camTitle");
    auto* recLabel = new QLabel("● REC");
    recLabel->setObjectName("recLabel");
    camHeader->addWidget(camTitle);
    camHeader->addStretch();
    camHeader->addWidget(recLabel);
    leftLayout->addLayout(camHeader);

    QFrame* videoFrame = new QFrame();
    videoFrame->setObjectName("videoFrame");
    videoFrame->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);

    QVBoxLayout* videoLayout = new QVBoxLayout(videoFrame);
    videoLayout->setContentsMargins(8, 8, 8, 8);
    videoLayout->setSpacing(0);

    m_videoStage = new VideoStageWidget();
    m_videoStage->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    videoLayout->addWidget(m_videoStage);

    leftLayout->addWidget(videoFrame, 1);
    root->addWidget(leftPane, 65);

    auto* divider = new QFrame();
    divider->setFrameShape(QFrame::VLine);
    divider->setObjectName("divider");
    root->addWidget(divider);

    auto* rightPane = new QWidget();
    rightPane->setObjectName("rightPane");
    auto* rightLayout = new QVBoxLayout(rightPane);
    rightLayout->setContentsMargins(14, 14, 14, 14);
    rightLayout->setSpacing(8);

    auto* statsTitle = new QLabel("MODEL OUTPUT");
    statsTitle->setObjectName("sectionTitle");
    rightLayout->addWidget(statsTitle);

    auto* csvBadge = new QLabel("⬤  READING: results_output.csv");
    csvBadge->setObjectName("csvBadge");
    rightLayout->addWidget(csvBadge);

    auto* statsWidget = new QWidget();
    m_statsGrid = new QGridLayout(statsWidget);
    m_statsGrid->setSpacing(5);
    m_statsGrid->setContentsMargins(0, 4, 0, 4);

    for (int i = 0; i < STAT_DEFS.size(); ++i) {
        const auto& def = STAT_DEFS[i];
        const QString& key = def.first;
        const QString& unit = def.second;

        auto* row = new QWidget();
        row->setObjectName("statRow");
        auto* rl = new QHBoxLayout(row);
        rl->setContentsMargins(10, 6, 10, 6);

        auto* nameLabel = new QLabel(key);
        nameLabel->setObjectName("statName");

        auto* valueLabel = new QLabel("—" + (unit.isEmpty() ? "" : " " + unit));
        valueLabel->setObjectName("statValue");
        valueLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);

        rl->addWidget(nameLabel);
        rl->addStretch();
        rl->addWidget(valueLabel);

        m_statsGrid->addWidget(row, i, 0);
        m_statValues[key] = valueLabel;
    }

    rightLayout->addWidget(statsWidget);
    rightLayout->addStretch();

    m_startBtn = new QPushButton("▶   START");
    m_startBtn->setObjectName("startBtn");
    m_startBtn->setMinimumHeight(72);
    m_startBtn->setCursor(Qt::PointingHandCursor);
    connect(m_startBtn, &QPushButton::clicked, this, &MainWindow::onToggleStart);
    rightLayout->addWidget(m_startBtn);

    root->addWidget(rightPane, 35);

    statusBar()->showMessage(
        "  ● CAMERA IDLE     ● MODEL READY     |     JETSON NANO  |  CUDA");
    statusBar()->setObjectName("mainStatusBar");
}

void MainWindow::applyTheme() {
    qApp->setStyle("Fusion");

    setStyleSheet(R"(
        QMainWindow, QWidget {
            background-color: #1e1e2e;
            color: #cdd6f4;
            font-family: Consolas, "Courier New", monospace;
            font-size: 12px;
        }

        /* ── Left pane ── */
        QWidget#leftPane {
            background-color: #12121c;
        }
        QLabel#camTitle {
            color: #4a9eff;
            font-size: 10px;
            letter-spacing: 1px;
        }
        QLabel#recLabel {
            color: #ff4444;
            font-size: 10px;
        }
        QLabel#cameraLabel {
            background-color: #0a0a14;
            border: 1px solid #2a2a3e;
            border-radius: 4px;
            color: #3a5a7a;
            font-size: 13px;
        }

        /* ── Divider ── */
        QFrame#divider {
            color: #3a3a52;
            max-width: 1px;
        }

        /* ── Right pane ── */
        QWidget#rightPane {
            background-color: #1a1a2e;
        }
        QLabel#sectionTitle {
            color: #5a7a9a;
            font-size: 16px;
            font-weight: 700;
            letter-spacing: 2px;
            border-bottom: 1px solid #2a2a42;
            padding-bottom: 8px;
        }
        QLabel#csvBadge {
            color: #2a8a4a;
            font-size: 13px;
            font-weight: 600;
            background-color: #111820;
            border: 1px solid #1e3028;
            border-radius: 6px;
            padding: 10px 12px;
        }

        /* ── Stat rows ── */
        QWidget#statRow {
            background-color: #12121e;
            border: 1px solid #2a2a3e;
            border-radius: 4px;
            min-height: 56px;
        }
        QWidget#statRow:hover {
            border-color: #3a3a5e;
        }
        QLabel#statName {
            color: #6a8aaa;
            font-size: 20px;
            font-weight: 600;
            padding-left: 10px;
        }
        QLabel#statValue {
            color: #7ad4ff;
            font-size: 28px;
            font-weight: bold;
            padding-right: 12px;
        }

        /* ── Start / Stop button ── */
        QPushButton#startBtn {
            background-color: #1a7a2a;
            border: 2px solid #2aaa3a;
            border-radius: 6px;
            color: #88ff99;
            font-size: 18px;
            font-weight: bold;
            letter-spacing: 3px;
        }
        QPushButton#startBtn:hover {
            background-color: #1e8c30;
            border-color: #33bb44;
        }
        QPushButton#startBtn:pressed {
            background-color: #155520;
        }
        QPushButton#startBtn[running="true"] {
            background-color: #7a1a1a;
            border-color: #aa2a2a;
            color: #ff8888;
        }
        QPushButton#startBtn[running="true"]:hover {
            background-color: #8c1e1e;
        }

        /* ── Status bar ── */
        QStatusBar#mainStatusBar {
            background-color: #0f0f1c;
            color: #7FC8FF;
            font-size: 14px;
            font-weight: 700;
            border-top: 1px solid #2a2a3e;
            letter-spacing: 1px;
        }
    )");
}

void MainWindow::onToggleStart() {
    if (!m_processingActive) {
        // Start each run from a clean CSV session so old particle rows are not
        // mixed into the new ROI / processing session.
        archiveCurrentCsvIfExists();
        setProcessingState(true, true, true);
        statusBar()->showMessage(
            "  ● CAMERA PREVIEW LIVE     ● INFERENCE RUNNING     |     JETSON NANO     |     CUDA");
    } else {
        archiveCurrentCsvIfExists();
        setProcessingState(false, true, false);
        clearOverlayState();
        statusBar()->showMessage(
            "  ● CAMERA PREVIEW LIVE     ● PROCESSING STOPPED     |     JETSON NANO     |     CUDA");
    }
}

void MainWindow::onFrameReady(const QImage& frame, int frameIndex) {
    m_latestFullFrame = frame;
    m_latestProcessingFrameIndex = frameIndex;

    m_videoStage->setErrorText("");
    m_videoStage->setFullFrame(frame);

    if (!m_frameClock.isValid()) {
        m_frameClock.start();
        m_lastFrameTimestampMs = m_frameClock.elapsed();
    } else {
        const qint64 nowMs = m_frameClock.elapsed();
        const qint64 deltaMs = nowMs - m_lastFrameTimestampMs;
        if (deltaMs > 0) {
            const double instantFps = 1000.0 / static_cast<double>(deltaMs);
            if (m_smoothedFps <= 0.0) {
                m_smoothedFps = instantFps;
            } else {
                m_smoothedFps = (0.85 * m_smoothedFps) + (0.15 * instantFps);
            }
        }
        m_lastFrameTimestampMs = nowMs;
    }

    if (m_processingActive) {
        submitLatestRoiFrameToBackend();
        updateOverlayForCurrentFrame();
        updateStatsForCurrentFrame();
    } else {
        clearOverlayState();
    }
}

void MainWindow::onCsvTick() {
    loadCsvValues();
}

void MainWindow::loadCsvValues() {
    m_particlesByFrame.clear();

    QFile file(CSV_PATH);
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (m_processingActive) {
            updateOverlayForCurrentFrame();
            updateStatsForCurrentFrame();
        }
        return;
    }

    QTextStream in(&file);
    if (in.atEnd()) {
        if (m_processingActive) {
            updateOverlayForCurrentFrame();
            updateStatsForCurrentFrame();
        }
        return;
    }

    const QString headerLine = in.readLine().trimmed();
    const QStringList headers = headerLine.split(',', Qt::KeepEmptyParts);

    int frameCol = -1;
    int particleIdCol = -1;
    int xCol = -1;
    int yCol = -1;
    int confidenceCol = -1;

    for (int i = 0; i < headers.size(); ++i) {
        const QString col = headers[i].trimmed();
        if (col.compare("frame", Qt::CaseInsensitive) == 0) frameCol = i;
        else if (col.compare("particle_id", Qt::CaseInsensitive) == 0) particleIdCol = i;
        else if (col.compare("x_px", Qt::CaseInsensitive) == 0) xCol = i;
        else if (col.compare("y_px", Qt::CaseInsensitive) == 0) yCol = i;
        else if (col.compare("confidence", Qt::CaseInsensitive) == 0) confidenceCol = i;
    }

    const bool requiredColumnsPresent =
        frameCol >= 0 && particleIdCol >= 0 && xCol >= 0 && yCol >= 0 && confidenceCol >= 0;

    if (!requiredColumnsPresent) {
        qWarning() << "[CSV] Missing required particle columns in" << CSV_PATH
                   << "Expected: frame, particle_id, x_px, y_px, confidence";
        if (m_processingActive) {
            updateOverlayForCurrentFrame();
            updateStatsForCurrentFrame();
        }
        return;
    }

    while (!in.atEnd()) {
        const QString line = in.readLine().trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const QStringList values = line.split(',', Qt::KeepEmptyParts);
        const int maxIndex = qMax(frameCol,
                           qMax(particleIdCol,
                           qMax(xCol,
                           qMax(yCol, confidenceCol))));
        if (values.size() <= maxIndex) {
            continue;
        }

        bool okFrame = false;
        bool okId = false;
        bool okX = false;
        bool okY = false;
        bool okConf = false;

        ParticleOverlay overlay;
        overlay.frameIndex = values[frameCol].trimmed().toInt(&okFrame);
        overlay.particleId = values[particleIdCol].trimmed().toInt(&okId);
        overlay.xPx = values[xCol].trimmed().toFloat(&okX);
        overlay.yPx = values[yCol].trimmed().toFloat(&okY);
        overlay.confidence = values[confidenceCol].trimmed().toFloat(&okConf);

        if (!(okFrame && okId && okX && okY && okConf)) {
            continue;
        }

        m_particlesByFrame[overlay.frameIndex].append(overlay);
    }

    if (m_processingActive) {
        updateOverlayForCurrentFrame();
        updateStatsForCurrentFrame();
    }
}

void MainWindow::updateStatLabel(const QString& key, const QString& value) {
    if (m_statValues.contains(key))
        m_statValues[key]->setText(value);
}

QString MainWindow::unitForStatKey(const QString& key) const {
    for (const auto& def : STAT_DEFS) {
        if (def.first == key) {
            return def.second;
        }
    }
    return "";
}

void MainWindow::resetStatsToPlaceholder() {
    for (auto it = m_statValues.begin(); it != m_statValues.end(); ++it) {
        const QString unit = unitForStatKey(it.key());
        it.value()->setText(unit.isEmpty() ? "-" : "- " + unit);
    }
}

void MainWindow::archiveCurrentCsvIfExists() {
    QFileInfo csvInfo(CSV_PATH);
    if (!csvInfo.exists() || !csvInfo.isFile()) {
        return;
    }

    QDir archiveDir(csvInfo.absolutePath() + "/archive");
    if (!archiveDir.exists()) {
        archiveDir.mkpath(".");
    }

    int nextIndex = 1;
    while (true) {
        QString candidate = archiveDir.filePath(
            QString("results_output_run_%1.csv").arg(nextIndex, 3, 10, QChar('0'))
        );
        if (!QFileInfo::exists(candidate)) {
            m_archivedRunCounter = nextIndex;
            QFile::rename(CSV_PATH, candidate);
            break;
        }
        ++nextIndex;
    }
}

void MainWindow::setProcessingState(bool enabled, bool updateButton, bool resetStats) {
    m_processingActive = enabled;
    m_cameraWorker->setProcessingEnabled(enabled);

    if (m_backendWorker) {
        if (enabled) {
            m_backendWorker->startSession();
        } else {
            m_backendWorker->stopSession();
        }
    }

    if (enabled) {
        m_latestProcessingFrameIndex = -1;
        m_particlesByFrame.clear();
        m_currentFrameParticles.clear();
        m_showNoParticlesText = false;
        m_csvTimer->start();
    } else {
        m_csvTimer->stop();
        clearOverlayState();
    }

    if (resetStats) {
        resetStatsToPlaceholder();
    }

    m_videoStage->setProcessingActive(enabled);

    if (updateButton) {
        if (enabled) {
            m_startBtn->setText("■   STOP");
            m_startBtn->setProperty("running", true);
        } else {
            m_startBtn->setText("▶   START");
            m_startBtn->setProperty("running", false);
        }

        m_startBtn->style()->unpolish(m_startBtn);
        m_startBtn->style()->polish(m_startBtn);
    }
}

void MainWindow::clearOverlayState() {
    m_currentFrameParticles.clear();
    m_showNoParticlesText = false;
    m_videoStage->setParticleOverlays(m_currentFrameParticles);
    m_videoStage->setShowNoParticlesText(false);
}

void MainWindow::updateOverlayForCurrentFrame() {
    if (!m_processingActive || m_latestProcessingFrameIndex < 0) {
        clearOverlayState();
        return;
    }

    m_currentFrameParticles = m_particlesByFrame.value(m_latestProcessingFrameIndex);
    m_showNoParticlesText = m_currentFrameParticles.isEmpty();

    m_videoStage->setParticleOverlays(m_currentFrameParticles);
    m_videoStage->setShowNoParticlesText(m_showNoParticlesText);
}

void MainWindow::updateStatsForCurrentFrame() {
    if (!m_processingActive) {
        return;
    }

    updateStatLabel("Particle Count", QString::number(m_currentFrameParticles.size()) + " / frame");

    if (!m_currentFrameParticles.isEmpty()) {
        double confidenceSum = 0.0;
        for (const auto& particle : m_currentFrameParticles) {
            confidenceSum += particle.confidence;
        }
        const double meanConfidencePercent = (confidenceSum / m_currentFrameParticles.size()) * 100.0;
        updateStatLabel("Confidence", QString::number(meanConfidencePercent, 'f', 1) + " %");
    } else {
        updateStatLabel("Confidence", "- %");
    }

    if (m_smoothedFps > 0.0) {
        updateStatLabel("Frame Rate", QString::number(m_smoothedFps, 'f', 1) + " fps");
    } else {
        updateStatLabel("Frame Rate", "- fps");
    }
}

void MainWindow::onRoiDragStarted() {
    clearOverlayState();

    if (!m_processingActive) {
        m_resumeAfterRoiDrag = false;
        return;
    }

    m_resumeAfterRoiDrag = true;

    archiveCurrentCsvIfExists();
    setProcessingState(false, false, true);

    statusBar()->showMessage(
        "  ● CAMERA PREVIEW LIVE     ● ROI MOVING — PROCESSING PAUSED     |     JETSON NANO     |     CUDA");
}

void MainWindow::onRoiDragFinished() {
    if (!m_resumeAfterRoiDrag) {
        return;
    }

    m_resumeAfterRoiDrag = false;

    setProcessingState(true, false, true);

    statusBar()->showMessage(
        "  ● CAMERA PREVIEW LIVE     ● INFERENCE RUNNING     |     JETSON NANO     |     CUDA");
}

void MainWindow::submitLatestRoiFrameToBackend() {
    if (!m_processingActive || !m_backendWorker || m_latestProcessingFrameIndex < 0) {
        return;
    }

    const QImage roiFrame = m_videoStage->currentRoiFrame();
    if (roiFrame.isNull()) {
        return;
    }

    m_backendWorker->submitRoiFrame(roiFrame, m_latestProcessingFrameIndex);
}

void MainWindow::onBackendStarted() {
    qDebug() << "[BackendWorker] Session started";
}

void MainWindow::onBackendStopped() {
    qDebug() << "[BackendWorker] Session stopped";
}

void MainWindow::onBackendReset() {
    qDebug() << "[BackendWorker] Session reset";
}

void MainWindow::onBackendFailed(const QString& message) {
    qCritical() << "[BackendWorker]" << message;
    statusBar()->showMessage(
        "  ● CAMERA PREVIEW LIVE     ● MODEL FAILED / CRASHED     |     JETSON NANO     |     CUDA");
}
