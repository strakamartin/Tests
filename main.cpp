#include <QApplication>
#include "mainwindow.h"
#include <QStringList>

// TOTO
// pri otazke s vice moznostami posledni pridana polozka neobsahuje text po zobrazeni.
// asi sa nespravne anebo vubec neulozi do db


int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // detect teacher mode by presence of "-u" parameter
    QStringList args = a.arguments();
    bool teacherMode = args.contains(QStringLiteral("-t"));

    MainWindow w(teacherMode);
    w.show();
    return a.exec();
}
