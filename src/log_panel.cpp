#include "log_panel.h"
#include <QDateTime>
#include <QTextCharFormat>
#include <QDebug>

LogPanel::LogPanel(QWidget* parent)
    : QTextEdit(parent)
{
    setReadOnly(true);
    setMaximumHeight(160);
    setStyleSheet(R"(
        QTextEdit {
            background: #1e1e1e;
            color: #d4d4d4;
            font-family: 'Consolas', 'Courier New', monospace;
            font-size: 12px;
            border: 1px solid #3c3c3c;
            border-radius: 4px;
        }
    )");
}

QString LogPanel::ts() const {
    return QDateTime::currentDateTime().toString("HH:mm:ss");
}

void LogPanel::log(const QString& level, const QString& msg, const QString& color) {
    QString html = QString(
        "<span style=\"color: #808080;\">[%1]</span> "
        "<span style=\"color: %2;\">%3</span> "
        "<span style=\"color: #d4d4d4;\">%4</span>"
    ).arg(ts()).arg(color).arg(level.toUpper()).arg(msg);
    append(html);
}

void LogPanel::appendInfo(const QString& msg)    { log("INFO", msg, "#4CAF50"); }
void LogPanel::appendWarning(const QString& msg) { log("WARN", msg, "#FF9800"); }
void LogPanel::appendError(const QString& msg)  { log("ERROR", msg, "#F44336"); }
void LogPanel::appendDebug(const QString& msg)  { log("DEBUG", msg, "#9E9E9E"); }
