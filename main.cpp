#include "qtbuilder.h"
#include <QApplication>

int main(int argc, char *argv[])
{
	QApplication a(argc, argv);
	a.setQuitOnLastWindowClosed(false);

	QtBuilder builder;
	builder.exec();

	int result = a.exec() + (int)!builder.result();
	return result;
}
