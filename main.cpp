#include "qtbuilder.h"
#include <QApplication>

int main(int argc, char *argv[])
{
	QCoreApplication::setOrganizationName("T3");
	QCoreApplication::setOrganizationDomain("T3forWIN");
	QCoreApplication::setApplicationName("QtBuilder");
	QCoreApplication::setApplicationVersion("0.3.1.85");

	QApplication a(argc, argv);
	a.setQuitOnLastWindowClosed(false);

	QtBuilder builder;
	  builder.exec();
			a.exec();
	return	a.property("result").toInt();
}
