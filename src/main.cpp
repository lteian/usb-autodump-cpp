#include <QApplication>
#include <QMessageBox>
#include "mainwindow.h"
#include "config.h"
#include "file_record.h"

int main(int argc, char *argv[]) {
    QApplication a(argc, argv);
    a.setApplicationName("USB自动转储工具");
    a.setOrganizationName("usb-autodump");

    // Ensure data dir exists
    Config::instance().load();

    MainWindow w;
    w.show();
    return a.exec();
}
