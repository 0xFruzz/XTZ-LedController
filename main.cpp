#include "XTZLedController.h"
#include <QtWidgets/QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    XTZLedController window;
    //window.show();
    return app.exec();
}
