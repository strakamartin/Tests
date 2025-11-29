#include <QApplication>
#include "mainwindow.h"
#include "dbmanager.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // Optional: initialize a default DB file in the app working directory
    QString defaultDb = "questions.db";
    QString err;
    if (!DBManager::instance().openDatabase(defaultDb, &err)) {
        // If DB cannot be created/opened, show a warning but still run app
        qWarning("DB open failed: %s", qPrintable(err));
    }

    MainWindow w;
    w.resize(1000, 600);
    w.show();
    return a.exec();
}
