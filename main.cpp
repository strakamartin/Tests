#include <QApplication>
#include "mainwindow.h"
#include <QStringList>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    // detect teacher mode by presence of "-u" parameter (as requested)
    QStringList args = a.arguments();
    bool teacherMode = args.contains(QStringLiteral("-u"));

    MainWindow w(teacherMode);
    w.show();
    return a.exec();
}
