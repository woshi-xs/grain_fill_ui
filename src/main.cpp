#include "widget.h"

#include <QApplication>

#include <QImageReader>

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    Widget w;
    w.show();
    return QApplication::exec();
}
