#include "usb_card.h"
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QStyle>

USBCard::USBCard(QWidget* parent)
    : QFrame(parent)
{
    setFrameStyle(QFrame::StyledPanel | QFrame::Raised);
    setStyleSheet(R"(
        QFrame {
            background: #2d2d2d;
            border: 1px solid #4a4a4a;
            border-radius: 8px;
            padding: 8px;
            min-width: 160px;
        }
    )");

    QVBoxLayout* vl = new QVBoxLayout(this);
    vl->setSpacing(6);

    // Drive letter + status
    QHBoxLayout* hl = new QHBoxLayout();
    m_driveLabel = new QLabel("空闲");
    m_driveLabel->setStyleSheet("color: #ffffff; font-size: 16px; font-weight: bold;");
    m_statusLabel = new QLabel("");
    m_statusLabel->setStyleSheet("color: #9e9e9e; font-size: 11px;");
    hl->addWidget(m_driveLabel);
    hl->addStretch();
    hl->addWidget(m_statusLabel);
    vl->addLayout(hl);

    // Pie chart
    m_pie = new PieChart(this);
    vl->addWidget(m_pie, 0, Qt::AlignHCenter);

    // Size label
    m_sizeLabel = new QLabel("");
    m_sizeLabel->setStyleSheet("color: #b0b0b0; font-size: 12px;");
    m_sizeLabel->setAlignment(Qt::AlignCenter);
    vl->addWidget(m_sizeLabel);

    // Progress bar
    m_progress = new QProgressBar();
    m_progress->setVisible(false);
    m_progress->setStyleSheet(R"(
        QProgressBar {
            height: 14px;
            border-radius: 7px;
            background: #3c3c3c;
            text-align: center;
            color: #ffffff;
        }
        QProgressBar::chunk {
            background: #FF9800;
            border-radius: 7px;
        }
    )");
    vl->addWidget(m_progress);

    // Count label
    m_countLabel = new QLabel("");
    m_countLabel->setStyleSheet("color: #9e9e9e; font-size: 11px;");
    m_countLabel->setAlignment(Qt::AlignCenter);
    m_countLabel->setVisible(false);
    vl->addWidget(m_countLabel);

    m_currentFileLabel = new QLabel("");
    m_currentFileLabel->setStyleSheet("color: #808080; font-size: 10px;");
    m_currentFileLabel->setAlignment(Qt::AlignCenter);
    m_currentFileLabel->setVisible(false);
    m_currentFileLabel->setWordWrap(true);
    vl->addWidget(m_currentFileLabel);

    // Buttons
    QHBoxLayout* btnLayout = new QHBoxLayout();
    m_formatBtn = new QPushButton("格式化");
    m_formatBtn->setVisible(false);
    m_formatBtn->setStyleSheet("QPushButton { background: #F44336; color: white; border: none; border-radius: 4px; padding: 4px 8px; font-size: 12px; } QPushButton:hover { background: #e53935; }");
    connect(m_formatBtn, &QPushButton::clicked, this, [this](){ emit formatClicked(m_drive); });

    m_ejectBtn = new QPushButton("弹出");
    m_ejectBtn->setVisible(false);
    m_ejectBtn->setStyleSheet("QPushButton { background: #616161; color: white; border: none; border-radius: 4px; padding: 4px 8px; font-size: 12px; } QPushButton:hover { background: #757575; }");
    connect(m_ejectBtn, &QPushButton::clicked, this, [this](){ emit ejectClicked(m_drive); });

    m_cancelBtn = new QPushButton("取消");
    m_cancelBtn->setVisible(false);
    m_cancelBtn->setStyleSheet("QPushButton { background: #757575; color: white; border: none; border-radius: 4px; padding: 4px 8px; font-size: 12px; } QPushButton:hover { background: #9e9e9e; }");
    connect(m_cancelBtn, &QPushButton::clicked, this, [this](){ emit cancelClicked(m_drive); });

    btnLayout->addWidget(m_formatBtn);
    btnLayout->addWidget(m_ejectBtn);
    btnLayout->addWidget(m_cancelBtn);
    vl->addLayout(btnLayout);
}

void USBCard::setDrive(const QString& letter, const QString& label, qint64 total, qint64 used) {
    m_drive = letter;
    m_driveLabel->setText(letter + (label.isEmpty() ? "" : (" " + label)));
    m_sizeLabel->setText(fmtSize(used) + " / " + fmtSize(total));
    m_pie->setPercent(total > 0 ? used * 100.0 / total : 0);
    setStatus("idle");
}

void USBCard::setStatus(const QString& s) {
    m_progress->setVisible(false);
    m_cancelBtn->setVisible(false);
    m_formatBtn->setVisible(false);
    m_ejectBtn->setVisible(false);
    m_countLabel->setVisible(false);
    m_currentFileLabel->setVisible(false);
    m_currentFileLabel->setText("");

    if (s == "idle") {
        m_statusLabel->setText("空闲");
        m_statusLabel->setStyleSheet("color: #9e9e9e; font-size: 11px;");
    } else if (s == "copying") {
        m_statusLabel->setText("复制中...");
        m_statusLabel->setStyleSheet("color: #FF9800; font-size: 11px;");
        m_progress->setVisible(true);
        m_cancelBtn->setVisible(true);
        m_countLabel->setVisible(true);
    } else if (s == "done") {
        m_statusLabel->setText("待操作");
        m_statusLabel->setStyleSheet("color: #4CAF50; font-size: 11px;");
        m_formatBtn->setVisible(true);
        m_ejectBtn->setVisible(true);
    } else if (s == "formatting") {
        m_statusLabel->setText("格式化中...");
        m_statusLabel->setStyleSheet("color: #F44336; font-size: 11px;");
    }
}

void USBCard::updateProgress(int done, int total, double filePct) {
    m_progress->setMaximum(total);
    m_progress->setValue(done);
    m_countLabel->setText(QString("%1/%2 (%3%)").arg(done).arg(total).arg(int(filePct)));
    m_countLabel->setVisible(true);
    m_currentFileLabel->setVisible(true);
}

void USBCard::clear() {
    m_drive.clear();
    m_driveLabel->setText("空闲");
    m_statusLabel->setText("");
    m_sizeLabel->setText("");
    m_pie->setPercent(0);
    m_progress->setVisible(false);
    m_progress->setValue(0);
    m_countLabel->setVisible(false);
    m_countLabel->setText("");
    m_currentFileLabel->setVisible(false);
    m_currentFileLabel->setText("");
    m_formatBtn->setVisible(false);
    m_ejectBtn->setVisible(false);
    m_cancelBtn->setVisible(false);
}

void USBCard::setCurrentFile(const QString& filename) {
    m_currentFileLabel->setText(filename);
    m_currentFileLabel->setToolTip(filename);
}

QString USBCard::fmtSize(qint64 b) {
    if (b >= 1024*1024*1024) return QString::number(b/1024.0/1024/1024, 'f', 1) + "GB";
    if (b >= 1024*1024) return QString::number(b/1024.0/1024, 'f', 1) + "MB";
    if (b >= 1024) return QString::number(b/1024.0, 'f', 1) + "KB";
    return QString::number(b) + "B";
}
