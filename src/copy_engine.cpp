#include "copy_engine.h"
#include "config.h"
#include "file_record.h"
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QDebug>

CopyEngine::CopyEngine(QObject* parent)
    : QObject(parent)
{
}

CopyEngine::~CopyEngine() {
    for (QThread* t : m_threads.values()) {
        t->quit();
        t->wait(2000);
        t->deleteLater();
    }
}

void CopyEngine::startCopy(const QString& drive) {
    {
        QMutexLocker l(&m_mutex);
        if (isCopying(drive)) return;
        m_cancel[drive] = false;
    }

    QThread* t = QThread::create([this, drive]() {
        copyWorker(drive);
    });
    {
        QMutexLocker l(&m_mutex);
        m_threads[drive] = t;
    }
    connect(t, &QThread::finished, t, &QThread::deleteLater);
    t->start();
}

void CopyEngine::cancelCopy(const QString& drive) {
    QMutexLocker l(&m_mutex);
    m_cancel[drive] = true;
}

bool CopyEngine::isCopying(const QString& drive) const {
    QMutexLocker l(const_cast<QMutex*>(&m_mutex));
    QThread* t = m_threads.value(drive, nullptr);
    return t && t->isRunning();
}

void CopyEngine::copyWorker(const QString& drive) {
    Config& cfg = Config::instance();
    cfg.load();
    QStringList videoExts = cfg.videoExtensions();
    QString localBase = cfg.localPath(drive);

    // Normalize drive letter
    QString normDrive = drive;
    normDrive.replace("/", "").replace("\\", "");
    if (!normDrive.endsWith(":")) normDrive += ":";

    QDir destDir(QDir(localBase).filePath(normDrive));
    destDir.mkpath(".");

    // Collect all video files
    QList<CopyTask> tasks;
    QDir srcDir(normDrive);
    if (!srcDir.exists()) {
        qWarning() << "Source drive does not exist:" << normDrive;
        return;
    }

    // Walk directory tree
    QStringList nameFilters;
    for (const QString& ext : videoExts) {
        nameFilters << "*" + ext;
    }

    QFileInfoList files = srcDir.entryInfoList(nameFilters, QDir::Files | QDir::AllDirs | QDir::NoDotAndDotDot);
    // Recursive scan
    QQueue<QString> dirs;
    dirs.enqueue(normDrive);
    while (!dirs.isEmpty()) {
        QString curDir = dirs.dequeue();
        QDir d(curDir);
        if (!d.exists()) continue;

        bool cancelled = false;
        {
            QMutexLocker l(&m_mutex);
            cancelled = m_cancel.value(drive, false);
        }
        if (cancelled) break;

        QFileInfoList entries = d.entryInfoList(QDir::Dirs | QDir::Files | QDir::NoDotAndDotDot);
        for (const QFileInfo& fi : entries) {
            if (fi.isDir()) {
                dirs.enqueue(fi.absoluteFilePath());
            } else if (fi.isFile()) {
                QString ext = fi.suffix().toLower();
                if (!videoExts.contains("." + ext)) continue;

                QString srcPath = fi.absoluteFilePath();
                QString relPath = QDir(normDrive).relativeFilePath(srcPath);
                QString dstPath = destDir.absoluteFilePath(relPath);

                qint64 fileSize = fi.size();
                int recordId = FileRecordDB::instance().add({
                    normDrive, srcPath, dstPath, "", "pending", fileSize
                });

                CopyTask task;
                task.usbDrive = normDrive;
                task.srcPath = srcPath;
                task.dstPath = dstPath;
                task.fileSize = fileSize;
                task.recordId = recordId;
                task.status = "pending";
                tasks.append(task);
            }
        }
    }

    // Copy files
    int total = tasks.size();
    for (int i = 0; i < tasks.size(); ++i) {
        {
            QMutexLocker l(&m_mutex);
            if (m_cancel.value(drive, false)) break;
        }

        CopyTask& task = tasks[i];
        task.status = "copying";
        FileRecordDB::instance().updateStatus(task.recordId, "copying");

        try {
            // Ensure destination dir exists
            QFileInfo dstInfo(task.dstPath);
            QDir dstDir = dstInfo.absoluteDir();
            if (!dstDir.exists()) dstDir.mkpath(".");

            // Copy file
            QFile src(task.srcPath);
            if (!src.open(QIODevice::ReadOnly)) {
                task.status = "error";
                task.errorMsg = "Cannot open source file";
                FileRecordDB::instance().updateStatus(task.recordId, "error", task.errorMsg);
                emit copyProgress(drive, i + 1, total, task);
                continue;
            }

            QFile dst(task.dstPath);
            if (!dst.open(QIODevice::WriteOnly)) {
                task.status = "error";
                task.errorMsg = "Cannot create destination file";
                FileRecordDB::instance().updateStatus(task.recordId, "error", task.errorMsg);
                emit copyProgress(drive, i + 1, total, task);
                continue;
            }

            const qint64 CHUNK = 1024 * 1024; // 1MB chunks
            qint64 copied = 0;
            char buf[CHUNK];
            while (!src.atEnd()) {
                {
                    QMutexLocker l(&m_mutex);
                    if (m_cancel.value(drive, false)) {
                        src.close();
                        dst.close();
                        task.status = "error";
                        task.errorMsg = "Cancelled";
                        FileRecordDB::instance().updateStatus(task.recordId, "error", task.errorMsg);
                        break;
                    }
                }
                qint64 r = src.read(buf, CHUNK);
                if (r <= 0) break;
                dst.write(buf, r);
                copied += r;
                task.progress = (copied * 100.0) / task.fileSize;
            }
            src.close();
            dst.close();

            if (task.status != "error") {
                task.status = "copied";
                task.progress = 100;
                FileRecordDB::instance().updateStatus(task.recordId, "copied");
            }
        } catch (const std::exception& e) {
            task.status = "error";
            task.errorMsg = e.what();
            FileRecordDB::instance().updateStatus(task.recordId, "error", task.errorMsg);
        }

        emit copyProgress(drive, i + 1, total, task);
    }

    {
        QMutexLocker l(&m_mutex);
        m_threads.remove(drive);
    }

    emit copyDone(drive, tasks);
}
