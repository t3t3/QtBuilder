#include "qtbuilder.h"
#include "appinfo.h"
#include <QApplication>

int main(int argc, char *argv[])
{
	QString app_version = QString("%1.%2.%3")
	   .arg(APP_VERSION_MAJOR)
	   .arg(APP_VERSION_MINOR)
	   .arg(APP_VERSION_REVSN)
	   .arg(APP_VERSION_BUILD);

	QCoreApplication::setOrganizationDomain (APP_INFO_DOMAIN);
	QCoreApplication::setOrganizationName   (APP_INFO_ORG);
	QCoreApplication::setApplicationName    (APP_INFO_NAME);
	QCoreApplication::setApplicationVersion (app_version);

	QApplication a(argc, argv);
	a.setQuitOnLastWindowClosed(false);

	QtBuilder builder;
	  builder.exec();
			a.exec();
	return	a.property("result").toInt();
}
