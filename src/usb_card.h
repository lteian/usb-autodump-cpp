#ifndef USD_CARD_H
#define USD_CARD_H

#include <QFrame>
#include <QLabel>
#include <QProgressBar>
#include <QPushButton>
#include <QPainter>

// ── Pie Chart ────────────────────────────────────────────
class PieChart : public QWidget {
    Q_OBJECT
    Q_PROPERTY(double percent READ percent WRITE setPercent)
public:
    PieChart(QWidget* parent = nullptr) : QWidget(parent), m_percent(0) {
        setMinimumSize(80, 80);
    }
    double percent() const { return m_percent; }
    void setPercent(double v) { m_percent = qBound(0.0, v, 100.0); update(); }

protected:
    void paintEvent(QPaintEvent*) override {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        int w = width(), h = height();
        int side = qMin(w, h);
        QRectF rect((w-side)/2.0, (h-side)/2.0, side, side);

        // Background
        p.setPen(Qt::NoPen);
        p.setBrush(QColor("#3c3c3c"));
        p.drawEllipse(rect);

        // Used arc
        if (m_percent > 0) {
            p.setBrush(QColor("#FF9800"));
            int span = -int(m_percent * 16 * 360 / 100);
            p.drawPie(rect.toRect(), 90*16, span);
        }

        // Border
        p.setPen(QPen(QColor("#666666"), 2));
        p.setBrush(Qt::NoBrush);
        p.drawEllipse(rect.toRect());

        // Center text
        p.setPen(Qt::white);
        QFont f = p.font();
        f.setBold(true);
        f.setPointSize(10);
        p.setFont(f);
        p.drawText(rect, Qt::AlignCenter, QString("%1%").arg(int(m_percent)));
    }

private:
    double m_percent;
};

// ── USBCard ──────────────────────────────────────────────
class USBCard : public QFrame {
    Q_OBJECT
public:
    explicit USBCard(QWidget* parent = nullptr);
    void setDrive(const QString& letter, const QString& label, qint64 total, qint64 used);
    void setStatus(const QString& s); // idle/copying/done/formatting
    void updateProgress(int done, int total, double filePct);
    void setCurrentFile(const QString& filename);
    void clear();

signals:
    void formatClicked(const QString& drive);
    void ejectClicked(const QString& drive);
    void cancelClicked(const QString& drive);

private:
    QString m_drive;
    QLabel* m_driveLabel = nullptr;
    QLabel* m_statusLabel = nullptr;
    PieChart* m_pie = nullptr;
    QLabel* m_sizeLabel = nullptr;
    QProgressBar* m_progress = nullptr;
    QLabel* m_countLabel = nullptr;
    QLabel* m_currentFileLabel = nullptr;
    QPushButton* m_formatBtn = nullptr;
    QPushButton* m_ejectBtn = nullptr;
    QPushButton* m_cancelBtn = nullptr;

    static QString fmtSize(qint64 b);
};

#endif // USD_CARD_H
