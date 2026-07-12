#include "widget.h"
#include <QApplication>
#include "mymainwindow.h"
Widget *myWidget;
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MyMainWindow w;
    Widget* myW = new Widget();
    myWidget = myW;
    w.setCentralWidget(myW);
    w.showMaximized();
    w.view = myW->view;
    w.central_w = myW;
    w.DoConnections();
    return a.exec();
}
