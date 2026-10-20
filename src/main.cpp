#include "widget.h"
#include <QApplication>
#include "mymainwindow.h"
Widget *my_widget;
int main(int argc, char *argv[])
{
    QApplication a(argc, argv);
    MyMainWindow w;
    Widget* myW = new Widget();
    my_widget = myW;
    w.setCentralWidget(myW);
    w.showMaximized();
    w.view = myW->view;
    w.central_w = myW;
    w.DoConnections();
    return a.exec();
}
