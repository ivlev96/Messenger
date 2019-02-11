#include "widgets/mainwindow/mainwindow.h"
#include "controllers/globalcontroller.h"


using namespace Widgets;

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);

	Controllers::GlobalController controller;
	controller.run();

	return a.exec();
}
