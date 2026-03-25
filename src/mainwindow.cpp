#include "mainwindow.h"
#include "usb_card.h"
#include "log_panel.h"
#include "settings_dialog.h"
#include "usb_monitor.h"
#include "copy_engine.h"
#include "ftp_uploader.h"
#include "disk_tool.h"
#include "file_record.h"
#include "config.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QLabel>
#include <QPushButton>
#include <QStatusBar>
#include <QScrollArea>
#include <QDebug>

MainWindow::MainWindow(QWidget* parent)
    : QMainWindow(parent)
{
    setWindowTitle("🚀 U盘自动转储工具");
    setMinimumSize(900, 600);
    setStyleSheet("QMainWindow { background: #1e1e1e; } QLabel { color: #e0e0e0; } QStatusBar { background: #252525; color: #b0b0b0; }");

    // Central widget
    QWidget* central = new QWidget();
    setCentralWidget(central);
    QVBoxLayout* vl = new QVBoxLayout(central);

    // ── Header ────────────────────────────────────────────
    QFrame* header = new QFrame();
    header->setStyleSheet("background: #252525; border-bottom: 1px solid #3c3c3c; padding: 8px;");
    QHBoxLayout* hl = new QHBoxLayout(header);

    QLabel* title = new QLabel("🚀 U盘自动转储工具");
    title->setStyleSheet("font-size: 16px; font-weight: bold; color: #ffffff;");
    hl->addWidget(title);
    hl->addStretch();

    QPushButton* settingsBtn = new QPushButton("⚙ 设置");
    settingsBtn->setStyleSheet("background: #424242; color: white; border: none; border-radius: 4px; padding: 6px 12px;");
    connect(settingsBtn, &QPushButton::clicked, this, &MainWindow::onSettingsClicked);
    hl->addWidget(settingsBtn);

    m_ftpStatusLabel = new QLabel("FTP: ○未连接");
    m_ftpStatusLabel->setStyleSheet("color: #9e9e9e; font-size: 12px; padding: 0 8px;");
    hl->addWidget(m_ftpStatusLabel);
    vl->addWidget(header);

    // ── USB Cards Area ───────────────────────────────────
    QScrollArea* scroll = new QScrollArea();
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setStyleSheet("background: transparent;");

    QWidget* cardsWidget = new QWidget();
    QGridLayout* cardsLayout = new QGridLayout(cardsWidget);
    cardsLayout->setSpacing(12);

    m_cards.reserve(8);
    for (int i = 0; i < 8; ++i) {
        USBCard* card = new USBCard();
        m_cards.append(card);
        cardsLayout->addWidget(card, i / 4, i % 4);
        connect(card, &USBCard::formatClicked, this, &MainWindow::onFormatClicked);
        connect(card, &USBCard::ejectClicked, this, &MainWindow::onEjectClicked);
        connect(card, &USBCard::cancelClicked, this, &MainWindow::onCancelClicked);
    }
    scroll->setWidget(cardsWidget);
    vl->addWidget(scroll, 1);

    // ── Log Panel ────────────────────────────────────────
    m_logPanel = new LogPanel();
    vl->addWidget(m_logPanel);

    // ── Status Bar ────────────────────────────────────────
    QStatusBar* sb = new QStatusBar();
    setStatusBar(sb);

    // ── Services ────────────────────────────────────────────
    m_usbMonitor = new USBMonitor(this);
    m_copyEngine = new CopyEngine(this);
    m_ftpUploader = new FTPUploader(this);

    connect(m_usbMonitor, &USBMonitor::deviceInserted, this, &MainWindow::onDeviceInserted);
    connect(m_usbMonitor, &USBMonitor::deviceRemoved, this, &MainWindow::onDeviceRemoved);
    connect(m_copyEngine, &CopyEngine::copyProgress, this, &MainWindow::onCopyProgress);
    connect(m_copyEngine, &CopyEngine::copyDone, this, &MainWindow::onCopyDone);
    connect(m_ftpUploader, &FTPUploader::uploadDone, this, &MainWindow::onUploadDone);
    connect(m_ftpUploader, &FTPUploader::connectedChanged, this, [this](bool connected) {
        m_ftpStatusLabel->setText(connected ? "FTP: ●已连接" : "FTP: ○未连接");
        m_ftpStatusLabel->setStyleSheet(connected
            ? "color: #4CAF50; font-size: 12px; padding: 0 8px;"
            : "color: #9e9e9e; font-size: 12px; padding: 0 8px;");
    });
    connect(m_ftpUploader, &FTPUploader::logMessage, m_logPanel, &LogPanel::appendInfo);

    // Start services
    m_ftpUploader->start();

    // Initial device scan
    for (const USBDevice& dev : m_usbMonitor->currentDevices()) {
        QTimer::singleShot(500, this, [this, dev]() {
            onDeviceInserted(dev);
        });
    }

    // Status bar timer
    QTimer* statusTimer = new QTimer(this);
    connect(statusTimer, &QTimer::timeout, this, &MainWindow::updateStatusBar);
    statusTimer->start(2000);

    m_logPanel->appendInfo("服务已启动，等待 USB 设备...");
}

MainWindow::~MainWindow() {
    m_ftpUploader->stop();
}

void MainWindow::onDeviceInserted(const USBDevice& dev) {
    QString drive = dev.driveLetter;
    m_logPanel->appendInfo(QString("%1 插入，容量 %2GB，已用 %3%")
        .arg(drive)
        .arg(dev.totalSize / 1024.0 / 1024 / 1024, 0, 'f', 1)
        .arg(int(dev.usedPercent())));
    allocateCard(drive, dev);
    onSettingsClicked(); // force settings check on first insertion
}

void MainWindow::onDeviceRemoved(const QString& drive) {
    m_logPanel->appendInfo(drive + " 已移除");
    releaseCard(drive);
}

void MainWindow::allocateCard(const QString& drive, const USBDevice& dev) {
    for (USBCard* card : m_cards) {
        if (card->property("drive").toString().isEmpty()) {
            m_driveToCard[drive] = card;
            card->setProperty("drive", drive);
            card->setDrive(drive, dev.label, dev.totalSize, dev.usedSize());
            card->setStatus("copying");
            m_copyEngine->startCopy(drive);
            return;
        }
    }
}

void MainWindow::releaseCard(const QString& drive) {
    USBCard* card = m_driveToCard.take(drive);
    if (card) {
        card->clear();
        card->setProperty("drive", QString());
    }
}

void MainWindow::onCopyProgress(const QString& drive, int done, int total, const CopyTask& task) {
    USBCard* card = m_driveToCard.value(drive, nullptr);
    if (card) {
        card->updateProgress(done, total, task.progress);
        if (task.status == "copied") {
            m_logPanel->appendInfo("复制完成: " + task.srcPath);
        }
    }
}

void MainWindow::onCopyDone(const QString& drive, const QList<CopyTask>& tasks) {
    USBCard* card = m_driveToCard.value(drive, nullptr);
    if (!card) return;

    int copied = 0, errors = 0;
    for (const CopyTask& t : tasks) {
        if (t.status == "copied") copied++;
        else if (t.status == "error") errors++;
    }
    m_logPanel->appendInfo(QString("%1 复制完成: %2个成功，%3个失败").arg(drive).arg(copied).arg(errors));
    card->setStatus("done");
    updateStatusBar();
}

void MainWindow::onUploadDone(const UploadTask& task) {
    if (task.status == "uploaded") {
        m_logPanel->appendInfo("上传成功: " + task.localPath);
    } else if (task.status == "error") {
        m_logPanel->appendError("上传失败: " + task.localPath + " - " + task.errorMsg);
    }
    updateStatusBar();
}

void MainWindow::onFormatClicked(const QString& drive) {
    USBCard* card = m_driveToCard.value(drive, nullptr);
    if (!card) return;

    int r = QMessageBox::question(this, "确认格式化",
        QString("确定要格式化 %1 吗？所有数据将被清除！").arg(drive),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    if (r != QMessageBox::Yes) return;

    card->setStatus("formatting");
    m_logPanel->appendWarning("开始格式化 " + drive + " ...");

    bool ok = DiskTool::formatDrive(drive);
    if (ok) {
        m_logPanel->appendInfo("格式化成功: " + drive);
    } else {
        m_logPanel->appendError("格式化失败: " + drive);
    }
    if (card) card->setStatus("done");
}

void MainWindow::onEjectClicked(const QString& drive) {
    m_logPanel->appendInfo("弹出 " + drive + " ...");
    bool ok = DiskTool::ejectDrive(drive);
    m_logPanel->appendInfo(ok ? ("已弹出: " + drive) : ("弹出失败: " + drive));
}

void MainWindow::onCancelClicked(const QString& drive) {
    m_copyEngine->cancelCopy(drive);
    m_logPanel->appendWarning("已取消复制: " + drive);
    USBCard* card = m_driveToCard.value(drive, nullptr);
    if (card) card->setStatus("idle");
}

void MainWindow::onSettingsClicked() {
    // Check if password is set
    Config& cfg = Config::instance();
    cfg.load();
    if (!cfg.isPasswordSet()) {
        QMessageBox::warning(this, "请先设置密码",
            "请先设置加密密码，再配置 FTP。\n\n"
            "打开「设置 ⚙」→「加密密码」输入密码后保存。");
    }
    SettingsDialog dlg(this);
    dlg.exec();
}

void MainWindow::updateStatusBar() {
    auto pending = FileRecordDB::instance().pendingCountAndSize();
    auto uploaded = FileRecordDB::instance().uploadedCountAndSize();
    int queue = m_ftpUploader ? m_ftpUploader->queueSize() : 0;

    QString fmt = [](qint64 b) {
        if (b >= 1024*1024*1024) return QString::number(b/1024.0/1024/1024,'f',1) + "GB";
        if (b >= 1024*1024) return QString::number(b/1024.0/1024,'f',1) + "MB";
        if (b >= 1024) return QString::number(b/1024.0,'f',1) + "KB";
        return QString::number(b) + "B";
    };

    statusBar()->showMessage(
        QString("  待上传: %1个 (%2)  |  已上传: %3个 (%4)  |  队列: %5")
            .arg(pending.first).arg(fmt(pending.second))
            .arg(uploaded.first).arg(fmt(uploaded.second))
            .arg(queue)
    );
}
