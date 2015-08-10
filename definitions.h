#ifndef DEFINITIONS_H
#define DEFINITIONS_H
#include <QStringList>
//
// user config section...
//
const QString msBuildTool("jom.exe"); // ... or "nmake.exe"
const QString vcInst("D:/BIZ/MICROSOFT/" ); // TODO: replace by querying the registry!

const QString vc2010("Visual Studio 2010");
const QString vc2012("Visual Studio 2012");
const QString vc2013("Visual Studio 2013");
const QString vc2015("Visual Studio 2015");

const QString _dbAndRl("-debug-and-release");
const QString _debuglb("-debug");
const QString _release("-release");

const QStringList globals = QStringList()
	<< _dbAndRl
	<< "-confirm-license"
	<< "-opensource"
;
const QStringList switches = QStringList()
	<< "-mp"
	<< "-ltcg"
;
const QStringList features = QStringList()
	<< "-3dnow"
	<< "-mmx"
	<< "-sse"
	<< "-sse2"
;
const QStringList plugins = QStringList()
	<< "-qt-zlib"
	<< "-qt-libjpeg"
	<< "-qt-libtiff"
	<< "-qt-libmng"
	<< "-qt-libpng"
	<< "-qt-sql-sqlite"
	<< "-qt-style-windowsvista"
;
const QStringList exclude = QStringList()
	<< "-no-accessibility"
	<< "-no-dbus"
	<< "-no-declarative"
	<< "-no-neon"
	<< "-no-openvg"
	<< "-no-openssl"
	<< "-no-phonon"
	<< "-no-phonon-backend"
	<< "-no-qt3support"
	<< "-no-script"
	<< "-no-scripttools"
	<< "-no-webkit"
	<< "-nomake demos"
	<< "-nomake docs"
	<< "-nomake examples"
	<< "-nomake tools"
	<< "-nomake translations"
;
const QStringList sfilter = QStringList() /* lower case! */
	<< "/demos"
	<< "/doc"
	<< "/examples"
;
const QStringList tfilter = QStringList() /* lower case! */
	<< "/activeqt"
	<< "/dbus"
	<< "/docs"
	<< "/generated"
	<< "/javascript"
	<< "/openvg"
	<< "/phonon"
	<< "/qdoc3"
	<< "/qt3support"
	<< "/scripttools"
	<< "/translations"
	<< "/webkit"
	<< "/config.profiles"
	<< "/config.tests"
	<< "/imports"
	<< "/tmp"
	<< "/%systemdrive%"
;
const QStringList ffilter = QStringList() /* lower case! */
	<< "dll"
	<< "lib"
	<< "pdb"
	<< "prl"
;
const QStringList cfilter = QStringList() /* lower case! */
	<< "c"
	<< "cpp"
;
const QStringList targets = QStringList()
	<< "sub-tools-bootstrap"
	<< "sub-moc"
	<< "sub-rcc"
	<< "sub-uic"
	<< "sub-winmain"
	<< "sub-corelib"
	<< "sub-xml"
	<< "sub-network"
	<< "sub-sql"
/*	<< "sub-testlib"		*/
	<< "sub-gui"
/*	<< "sub-qt3support"		*/
/*	<< "sub-uic3"			*/
	<< "sub-idc"
/*	<< "sub-activeqt"		*/
	<< "sub-opengl"
/*	<< "sub-xmlpatterns"	*/
/*	<< "sub-phonon"			*/
	<< "sub-multimedia"
	<< "sub-svg"
/*	<< "sub-script"			*/
/*	<< "sub-declarative"	*/
/*	<< "sub-webkit"			*/
/*	<< "sub-scripttools"	*/
	<< "sub-plugins"
/*	<< "sub-imports"		*/
/*	<< "sub-tools"			*/
/*	<< "sub-translations"	*/
/*	<< "sub-examples"		*/
/*	<< "sub-demos"			*/
;
//
// persistent configs...
//
enum Mode
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

const QString vc2010Inst(vcInst+vc2010); // TODO: replace by querying the registry!
const QString vc2012Inst(vcInst+vc2012);
const QString vc2013Inst(vcInst+vc2013);
const QString vc2015Inst(vcInst+vc2015);

const QStringList modeLabels = QStringList()
	<< "Windows (Win32)"
	<< "Windows (x64)"
	<< "Visual Studio 2010"
	<< "Visual Studio 2012"
	<< "Visual Studio 2013"
	<< "Visual Studio 2015"
	<< "Static Build"
	<< "Shared Build"
;
const QStringList bPaths = QStringList()
	<< "Win32"
	<< "x64"
	<< "v100"
	<< "v110"
	<< "v120"
	<< "v130"
	<< "static"
	<< "shared"
;
const QStringList builds = QStringList()
	<< "x86"
	<< "amd64"
	<< vc2010Inst
	<< vc2012Inst
	<< vc2013Inst
	<< vc2015Inst
	<< ""
	<< ""
;
const QStringList qMakeS = QStringList()
	<< ""
	<< ""
	<< "win32-msvc2010"
	<< "win32-msvc2012"
	<< "win32-msvc2013"
	<< "win32-msvc2015"
	<< ""
	<< ""
;
const QStringList qtOpts = QStringList()
	<< ""
	<< ""
	<< QString("-platform %1").arg(qMakeS.at(MSVC2010))
	<< QString("-platform %1").arg(qMakeS.at(MSVC2012))
	<< QString("-platform %1").arg(qMakeS.at(MSVC2013))
	<< QString("-platform %1").arg(qMakeS.at(MSVC2015))
	<< "-static"
	<< "-shared"
;
const QStringList colors = QStringList()
	<< "#60BF4D"	/* AppInfo	*/
	<< "#C558E0"	/* Elevated	*/
	<< "#418ECC"	/* Explicit	*/
	<< "#EBA421"	/* Warning	*/
	<< "#DB4242"	/* Critical */
	<< "#555853"	/* Informal	*/
;
enum MessageType
{
	AppInfo = 0,
	Elevated,
	Process,
	Warning,
	Critical,
	Informal,
};

const QString btemp("_btmp");
const QString build("_build");
const QString qtVar("/bin/qtvars.bat");
const QString imdiskLetter("Drive letter:");
const QString imdiskSizeSt("Size:");
const int imdiskUnit = 16841 ;
const int defGuiHeight = 28;

const QStringList globalOptions = QStringList()
	<< "-opensource"
	<< "-commercial"
	<< "-developer-build"
;
const QStringList compileSwitches = QStringList() /* with or without "-no" prefix */
	<< "-mp"
	<< "-ltcg"
	<< "-fast"
	<< "-dsp"
	<< "-vcproj"
	<< "-plugin-manifests"
	<< "-process"
	<< "-incredibuild-xge"
/*	<< "-qmake" ... no chance to build from source without qmake! */
;
const QStringList featureSwitches = QStringList() /* with or without "-no" prefix */
	<< "-exceptions"
	<< "-stl"
	<< "-rtti"
	<< "-mmx"
	<< "-3dnow"
	<< "-sse"
	<< "-sse2"
;
const QStringList targetSwitches = QStringList() /* with or without "-no" prefix */
/*	<< "-declarative-debug"	... dependency needed */
	<< "-accessibility"
	<< "-openssl"
/*	<< "-openssl-linked"	... dependency needed */
	<< "-dbus"
/*	<< "-dbus-linked"		... dependency needed */
	<< "-webkit"
/*	<< "-webkit-debug"		... dependency needed */
	<< "-phonon"
	<< "-phonon-backend"
	<< "-multimedia"
	<< "-audio-backend"
	<< "-openvg"
	<< "-script"
	<< "-scripttools"
	<< "-declarative"
	<< "-native-gestures"
	<< "-system-proxies"
;
/*
-release
-debug
-debug-and-release
-opensource
-commercial
-developer-build

-ltcg				-no-ltcg
-fast				-no-fast
-exceptions			-no-exceptions

-dsp				-no-dsp
-vcproj				-no-vcproj
-plugin-manifests	-no-plugin-manifests
-qmake				-no-qmake
-process			-dont-process
-incredibuild-xge	-no-incredibuild-xge

-stl				-no-stl
-rtti				-no-rtti
-mmx				-no-mmx
-3dnow				-no-3dnow
-sse				-no-sse
-sse2				-no-sse2

-qt-zlib							-system-zlib
					-no-gif
-qt-libpng			-no-libpng		-system-libpng
-qt-libmng			-no-libmng		-system-libmng
-qt-libtiff			-no-libtiff		-system-libtiff
-qt-libjpeg			-no-libjpeg		-system-libjpeg

					-no-qt3support
-opengl <api>		-no-opengl
-qt-sql-<drv>		-no-sql-<drv>	-plugin-sql-<drv>
									-system-sqlite

-declarative-debug	-no-declarative-debug
-accessibility		-no-accessibility
-openssl			-no-openssl		-openssl-linked
-dbus				-no-dbus		-dbus-linked
-webkit				-no-webkit		-webkit-debug
-phonon				-no-phonon
-phonon-backend		-no-phonon-backend
-multimedia			-no-multimedia
-audio-backend		-no-audio-backend
-openvg				-no-openvg
-script				-no-script
-scripttools		-no-scripttools
-declarative		-no-declarative
-qt-style-<style>	-no-style-<style>
-native-gestures	-no-native-gestures
-system-proxies		-no-system-proxies
*/
const QByteArray __HRR = QByteArray::fromBase64("AAADX3icbZLBkcMwCEXvWwXji+wZEPc4pTCLOtgGVPyCkCzLCZcYeIIPAeAyBUUiygo/I4SUMPXvYhmdOaYzwWplPmXhGbewVcf0B9LzuwGjq/VMgtZnKxmOUaD2vL9yk9aMWS4gfkYZ4CaX0raZ0AG9elK0tWIfaMq9jyoz6shNfdgxl2IyQUobCvp2tJuPMUGbnKjA4NAdVXHuXDAvZ1mnsOgRCiy0QiA7nD54NTWxxkK8IqHfkE5A/SSiRL073wk0UTkc/YaUzG1RJtjHPD4YrX2Tthdo1/REMFXd/YPpnW19ppfwCdUxl19nbIKvU1qpMI6/02/sgSH9xon3F9IEPrjbkYjRzYt7NrfkSS5XIgBPV+AfaJGO1Q==");
const QByteArray __ARR = QByteArray::fromBase64("AAAELHichZJLbsUwCEXnbxUoE9sVxPNU3QkSLMSLL45/OFFVBollTi5wA8B/IQKfP3OAIoIUDqQfOnewJsiQmpckAPE4NNApDOpRwXBXKfY21I4J7HDn8gllohiqosF2V6MeJeUppNxRDFWynca31gOq6+6a7MgPMiLFfczLD+YlAZsLLr4dO1tDmzor0SzS058XGc1PSdFQszOtkp8nCL1HPc3nlmLIOMDLt2TVZTWZRThPV8trMK2l7yDkZf9UXC1YcH2Y/JqF5yAvrrhV4lFVCeHLSUtBdh7ybE17W/cahJabXBTulxj4SEQYptk271Ow1crZ6G5A2yzdl7iXVMrAoXkluP6hPja+8jL8MRvDZpVvdwT6hSh77o3vcg/8yeO+bG8+b3yGZ+y8qf0Cqx+k6w==");
#endif // DEFINITIONS_H
