#include "ftp_uploader.h"
#include "config.h"
#include "file_record.h"
#include "crypto.h"
#include <QFile>
#include <QTimer>
#include <QDir>
#include <QDebug>
#include <QAuthenticator>

FTPUploader::FTPUploader(QObject* parent)
    : QObject(parent)
{
    m_manager = new QNetworkAccessManager(this);
}

FTPUploader::~FTPUploader() {
    stop();
}

void FTPUploader::start() {
    {
        QMutexLocker l(&m_mutex);
        if (m_running) return;
        m_running = true;
    }
    connectFtp();
    QTimer* t = new QTimer(this);
    connect(t, &QTimer::timeout, this, &FTPUploader::onTimeout);
    t->start(2000);
}

void FTPUploader::stop() {
    {
        QMutexLocker l(&m_mutex);
        m_running = false;
    }
    disconnectFtp();
}

bool FTPUploader::connectFtp() {
    Config& cfg = Config::instance();
    cfg.load();
    QJsonObject ftp = cfg.ftpConfig();

    QString host = ftp.value("host").toString();
    if (host.isEmpty()) {
        emit logMessage("[FTP] 未配置服务器");
        m_connected = false;
        emit connectedChanged(false);
        return false;
    }

    int port = ftp.value("port").toInt(21);
    QString user = ftp.value("username").toString();
    QString encryptedPass = ftp.value("password").toString();
    QString pass = encryptedPass;

    // Decrypt password if encrypted
    if (!encryptedPass.isEmpty() && cfg.isPasswordSet()) {
        pass = Crypto::decrypt(encryptedPass, cfg.encryptionPassword());
    }

    QString urlStr = QString("ftp://%1:%2@%3:%4/")
                         .arg(user)
                         .arg(pass)
                         .arg(host)
                         .arg(port);
    m_ftpUrl = QUrl(urlStr);
    m_ftpUrl.setPath("/");

    m_connected = true;
    emit connectedChanged(true);
    emit logMessage(QString("[FTP] 已连接: %1:%2").arg(host).arg(port));
    return true;
}

void FTPUploader::disconnectFtp() {
    if (m_currentReply) {
        m_currentReply->abort();
        m_currentReply->deleteLater();
        m_currentReply = nullptr;
    }
    m_connected = false;
    emit connectedChanged(false);
}

void FTPUploader::enqueue(const UploadTask& task) {
    QMutexLocker l(&m_mutex);
    m_queue.enqueue(task);
}

int FTPUploader::queueSize() const {
    return m_queue.size();
}

void FTPUploader::onTimeout() {
    if (!m_running) return;

    // Load pending from DB if queue is empty
    {
        QMutexLocker l(&m_mutex);
        if (m_queue.isEmpty()) {
            // Will be loaded below
        }
    }

    loadPendingFromDB();

    if (m_currentReply) return; // already uploading

    uploadNext();
}

void FTPUploader::loadPendingFromDB() {
    QMutexLocker l(&m_mutex);
    if (!m_queue.isEmpty()) return;

    Config& cfg = Config::instance();
    cfg.load();
    QString subPath = cfg.ftpConfig().value("sub_path").toString("/");

    QList<FileRecord> pending = FileRecordDB::instance().pendingRecords();
    for (const FileRecord& rec : pending) {
        UploadTask task;
        task.recordId = rec.id;
        task.localPath = rec.localPath;
        task.remotePath = subPath + "/" + QFileInfo(rec.localPath).fileName();
        task.fileSize = rec.fileSize;
        task.status = "pending";
        m_queue.enqueue(task);
    }
}

void FTPUploader::uploadNext() {
    Config& cfg = Config::instance();

    UploadTask task;
    {
        QMutexLocker l(&m_mutex);
        if (m_queue.isEmpty()) return;
        task = m_queue.dequeue();
    }

    if (!QFile::exists(task.localPath)) {
        task.status = "error";
        task.errorMsg = "本地文件不存在";
        FileRecordDB::instance().updateStatus(task.recordId, "error", task.errorMsg);
        emit uploadDone(task);
        return;
    }

    if (!m_connected) {
        if (!connectFtp()) {
            // Re-queue
            {
                QMutexLocker l(&m_mutex);
                m_queue.enqueue(task);
            }
            return;
        }
    }

    task.status = "uploading";
    FileRecordDB::instance().updateStatus(task.recordId, "uploading");

    QUrl url = m_ftpUrl;
    url.setPath(task.remotePath);

    QNetworkRequest req(url);
    req.setHeader(QNetworkRequest::KnownHeaders::ContentLengthHeader, task.fileSize);

    QFile* file = new QFile(task.localPath);
    if (!file->open(QIODevice::ReadOnly)) {
        task.status = "error";
        task.errorMsg = "Cannot open file for upload";
        FileRecordDB::instance().updateStatus(task.recordId, "error", task.errorMsg);
        emit uploadDone(task);
        delete file;
        return;
    }

    m_currentTask = task;
    m_currentReply = m_manager->put(req, file);
    file->setParent(m_currentReply); // auto-delete

    connect(m_currentReply, &QNetworkReply::finished, this, &FTPUploader::onUploadFinished);
    emit logMessage(QString("[FTP] 上传: %1").arg(task.localPath));
}

void FTPUploader::onUploadFinished() {
    UploadTask task = m_currentTask;
    m_currentTask = UploadTask();
    QNetworkReply* reply = m_currentReply;
    m_currentReply = nullptr;

    if (!reply) return;

    bool success = (reply->error() == QNetworkReply::NoError);
    if (success) {
        task.status = "uploaded";
        FileRecordDB::instance().updateStatus(task.recordId, "uploaded");
        FileRecordDB::instance().updateFtpPath(task.recordId, task.remotePath);
        emit logMessage(QString("[FTP] 上传成功: %1").arg(task.localPath));

        // Delete local file if configured
        if (Config::instance().autoDeleteLocal()) {
            if (QFile::remove(task.localPath)) {
                FileRecordDB::instance().updateStatus(task.recordId, "deleted");
                emit logMessage(QString("[FTP] 已删除本地: %1").arg(task.localPath));
            }
        }
    } else {
        int maxRetry = Config::instance().maxRetry();
        task.retryCount++;
        if (task.retryCount < maxRetry) {
            task.status = "pending";
            QMutexLocker l(&m_mutex);
            m_queue.enqueue(task);
            emit logMessage(QString("[FTP] 上传失败，重试 %1/%2: %3")
                                .arg(task.retryCount).arg(maxRetry).arg(reply->errorString()));
        } else {
            task.status = "error";
            task.errorMsg = reply->errorString();
            FileRecordDB::instance().updateStatus(task.recordId, "error", task.errorMsg);
            emit logMessage(QString("[FTP] 上传失败: %1 - %2").arg(task.localPath).arg(reply->errorString()));
        }
    }

    reply->deleteLater();
    emit uploadDone(task);
}
