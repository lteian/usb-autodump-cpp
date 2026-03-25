#include "usb_monitor.h"
#include <QProcess>
#include <QDebug>
#include <QRegularExpression>

#ifdef _WIN32
#include <windows.h>
#endif

USBMonitor::USBMonitor(QObject* parent)
    : QObject(parent)
{
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &USBMonitor::poll);
    m_timer->start(1000);

    // Initial detection
    m_lastDevices = QMap<QString, USBDevice>();
    QList<USBDevice> initial = detectDevices();
    for (const USBDevice& dev : initial) {
        m_lastDevices[dev.driveLetter] = dev;
    }
}

USBMonitor::~USBMonitor() {
    m_timer->stop();
}

QList<USBDevice> USBMonitor::currentDevices() {
    return detectDevices();
}

void USBMonitor::poll() {
    QList<USBDevice> current = detectDevices();
    QMap<QString, USBDevice> currentMap;
    for (const USBDevice& dev : current) {
        currentMap[dev.driveLetter] = dev;
    }

    // Detect inserted
    for (const QString& letter : currentMap.keys()) {
        if (!m_lastDevices.contains(letter)) {
            qDebug() << "USB inserted:" << letter;
            emit deviceInserted(currentMap[letter]);
        }
    }

    // Detect removed
    for (const QString& letter : m_lastDevices.keys()) {
        if (!currentMap.contains(letter)) {
            qDebug() << "USB removed:" << letter;
            emit deviceRemoved(letter);
        }
    }

    m_lastDevices = currentMap;
}

QList<USBDevice> USBMonitor::detectDevices() {
    QList<USBDevice> list;

#ifdef _WIN32
    // Use wmic on Windows
    QProcess p;
    p.start("wmic", QStringList() << "logicaldisk" << "where" << "DriveType=2" << "get" << "DeviceID,VolumeName,Size,FreeSpace");
    p.waitForFinished(5000);
    QString output = QString::fromLocal8Bit(p.readAll());

    QStringList lines = output.split('\n', Qt::SkipEmptyParts);
    for (const QString& line : lines) {
        QString trimmed = line.trimmed();
        if (trimmed.isEmpty() || trimmed.startsWith("DeviceID")) continue;

        QStringList parts = trimmed.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
        if (parts.size() >= 3) {
            USBDevice dev;
            dev.driveLetter = parts[0];
            dev.label = parts.size() >= 4 ? parts[1] : "U盘";
            dev.totalSize = parts.size() >= 3 ? parts[parts.size()-2].toLongLong() : 0;
            dev.freeSpace = parts.size() >= 4 ? parts[parts.size()-1].toLongLong() : 0;
            if (dev.totalSize > 0) {
                list << dev;
            }
        }
    }
#else
    // Linux: use lsblk
    QProcess p;
    p.start("lsblk", QStringList() << "-o" << "NAME,MOUNTPOINT,SIZE,TYPE" << "-J" << "-l");
    p.waitForFinished(5000);
    QString output = QString::fromLocal8Bit(p.readAll());

    // Simple parsing - look for /dev/sd* that are mounted
    QProcess p2;
    p2.start("df", QStringList() << "-T");
    p2.waitForFinished(3000);
    QString dfOutput = QString::fromLocal8Bit(p2.readAll());
    QStringList dfLines = dfOutput.split('\n', Qt::SkipEmptyParts);

    for (const QString& l : dfLines) {
        if (l.startsWith("/dev/sd")) {
            QStringList cols = l.split(QRegularExpression("\\s+"), Qt::SkipEmptyParts);
            if (cols.size() >= 3) {
                QString path = cols[0];
                QString mount = cols[1];
                QString total = cols[2];
                if (mount.startsWith("/media") || mount.startsWith("/mnt") || mount.startsWith("/run/media")) {
                    USBDevice dev;
                    dev.driveLetter = path; // e.g. /dev/sdb1
                    dev.totalSize = total.toLongLong() * 1024; // df gives KB
                    list << dev;
                }
            }
        }
    }
#endif

    return list;
}
