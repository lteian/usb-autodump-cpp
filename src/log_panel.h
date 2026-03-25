#ifndef LOG_PANEL_H
#define LOG_PANEL_H

#include <QTextEdit>

class LogPanel : public QTextEdit {
    Q_OBJECT
public:
    explicit LogPanel(QWidget* parent = nullptr);

    void appendInfo(const QString& msg);
    void appendWarning(const QString& msg);
    void appendError(const QString& msg);
    void appendDebug(const QString& msg);

private:
    void log(const QString& level, const QString& msg, const QString& color);
    QString ts() const;
};

#endif // LOG_PANEL_H
