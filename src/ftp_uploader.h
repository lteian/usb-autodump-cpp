#ifndef FTP_UPLOADER_H
#define FTP_UPLOADER_H

#include <QObject>
#include <QQueue>
#include <QMutex>
#include <QString>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QUrl>

struct UploadTask {
    int recordId = 0;
    QString localPath;
    QString remotePath;
    qint64 fileSize = 0;
    int retryCount = 0;
    QString status; // pending/uploading/uploaded/error
    QString errorMsg;
};

class FTPUploader : public QObject {
    Q_OBJECT
public:
    explicit FTPUploader(QObject* parent = nullptr);
    ~FTPUploader();

    void start();
    void stop();
    bool isConnected() const { return m_connected; }
    int queueSize() const;
    void enqueue(const UploadTask& task);

signals:
    void uploadDone(const UploadTask& task);
    void connectedChanged(bool connected);
    void logMessage(const QString& msg);

private slots:
    void onUploadFinished();
    void onTimeout();
    void onConnected();

private:
    void loadPendingFromDB();
    void uploadNext();
    bool connectFtp();
    void disconnectFtp();

    QQueue<UploadTask> m_queue;
    QMutex m_mutex;
    bool m_running = false;
    bool m_connected = false;
    QNetworkAccessManager* m_manager = nullptr;
    QNetworkReply* m_currentReply = nullptr;
    UploadTask m_currentTask;
};

#endif // FTP_UPLOADER_H
