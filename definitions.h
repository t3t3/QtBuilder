#ifndef DEFINITIONS_H
#define DEFINITIONS_H
#include <QStringList>

const QString vc2010("D:/BIZ/MICROSOFT/Visual Studio 2010");
const QString vc2012("D:/BIZ/MICROSOFT/Visual Studio 2012");
const QString vc2013("D:/BIZ/MICROSOFT/Visual Studio 2013");
const QString vc2015("D:/BIZ/MICROSOFT/Visual Studio 2015");

const QString qtVars("/bin/qtvars.bat");

const QStringList global = QStringList()	<<
	"-confirm-license"						<<
	"-opensource"							<<
	"-arch windows"							<<
	"-debug-and-release"					<<
	"-ltcg"									<<
	"-mmx"									<<
	"-3dnow"								<<
	"-sse"									<<
	"-sse2"									<<
	"-fast"									<<
	"-mp"									;

const QStringList plugins = QStringList()	<<
	"-qt-zlib"								<<
	"-qt-libjpeg"							<<
	"-qt-libtiff"							<<
	"-qt-libmng"							<<
	"-qt-libpng"							<<
	"-qt-sql-sqlite"						<<
	"-qt-style-windowsvista"				;

const QStringList exclude = QStringList()	<<
	"-nomake demos"							<<
	"-nomake docs"							<<
	"-nomake examples"						<<
	"-no-dbus"								<<
	"-no-accessibility"						<<
	"-no-declarative"						<<
	"-no-neon"								<<
	"-no-openvg"							<<
	"-no-openssl"							<<
	"-no-phonon"							<<
	"-no-phonon-backend"					<<
	"-no-qt3support"						<<
	"-no-script"							<<
	"-no-scripttools"						<<
	"-no-webkit"							;

const QStringList ffilter = QStringList()	<<
	"dll"									<<
	"lib"									<<
	"pdb"									<<
	"prl"									;

const QStringList sfilter = QStringList()	<<
	"/dbus"									<<
	"/demos"								<<
	"/doc/"									<<
	"/docs/"								<<
	"/examples"								<<
	"/javascript"							<<
	"/phonon"								<<
	"openvg/"								<<
	"/qt3support/"							<<
	"scripttools/"							<<
	"webkit/"								;

const QStringList tfilter = QStringList()	<<
	"/tmp"									<<
	"/config.profiles"						<<
	"/config.tests"							<<
	"/translations"							<<
	"/examples"								<<
	"/imports"								<<
	"/util"									;

enum Modes
{
	X86,
	X64,
	MSVC2010,
	MSVC2012,
	MSVC2013,
	MSVC2015,
	Static,
	Shared
};

const QStringList paths	 = QStringList() << "Win32"
										 << "x64"
										 << "v100"
										 << "v110"
										 << "v120"
										 << "v130"
										 << "static"
										 << "shared";

const QStringList builds = QStringList() << "x86"
										 << "amd64"
										 << vc2010
										 << vc2012
										 << vc2013
										 << vc2015
										 << ""
										 << "";

const QStringList qMakeS = QStringList() << ""
										 << ""
										 << "win32-msvc2010"
										 << "win32-msvc2012"
										 << "win32-msvc2013"
										 << "win32-msvc2015"
										 << ""
										 << "";

const QStringList qtOpts = QStringList() << ""
										 << ""
										 << QString("-platform %1").arg(qMakeS.at(MSVC2010))
										 << QString("-platform %1").arg(qMakeS.at(MSVC2012))
										 << QString("-platform %1").arg(qMakeS.at(MSVC2013))
										 << QString("-platform %1").arg(qMakeS.at(MSVC2015))
										 << "-static"
										 << "-shared";

const QStringList colors = QStringList() << "#60BF4D"	/* Step		*/
										 << "#C558E0"	/* Error	*/
										 << "#EBA421"	/* Warning	*/
										 << "#DB4242"	/* Critical */
										 << "#418ECC";	/* Informal	*/
enum MessageType
{
	AppInfo = 0,
	Elevated,
	Warning,
	Critical,
	Informal,
};

const QString build("_build"); // DON'T use a $ char in the temp folders as it WILL mess up qmake!!!
const QString btemp("_btemp"); // DON'T use a $ char in the temp folders as it WILL mess up qmake!!!
const QString imdiskLetter("Drive letter:");
const QString imdiskSizeSt("Size:");
const int imdiskUnit = 16841 ;
#endif // DEFINITIONS_H
