/*
	The MIT License (MIT)

	Copyright (c) 2015, Gerald Gstaltner

	Permission is hereby granted, free of charge, to any person obtaining a copy
	of this software and associated documentation files (the "Software"), to deal
	in the Software without restriction, including without limitation the rights
	to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
	copies of the Software, and to permit persons to whom the Software is
	furnished to do so, subject to the following conditions:

	The above copyright notice and this permission notice shall be included in all
	copies or substantial portions of the Software.

	THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
	IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
	FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
	AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
	LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
	OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
	SOFTWARE.
*/
#include "qtbuilder.h"
#include "helpers.h"
#include <stdlib.h>
#ifdef _WIN32
#include "Windows.h"
#endif
#include <QApplication>
#include <QPlastiqueStyle>
#include <QVBoxLayout>
#include <QDateTime>
#include <QQueue>
#include <QtConcurrentRun>
#include <QDirIterator>
#include <QMessageBox>
#include <QElapsedTimer>
#include <QUuid>

const int		qtBuilderRamDiskInGB = 6;		// ... ram disk size in gigabyte - needs imdisk installed: http://www.ltr-data.se/opencode.html/#ImDisk
const QString	qtBuilderStaticDrive = "";//Y";	// ... use an existing drive letter; the ram disk part is skipped when set to anything else but "".
const bool		qtBuilderWriteBldLog = true;	// creates a build.log with all the stdout from configure/jom; is copied to the target folder.
const bool		qtBuilderClearTarget = false;	// removy any target folder contents; if only managed by this app, this can be skipped.

void QtBuilder::setup() /// primary setup for the current build run!!!
{
	m_version = "4.8.7";						// ... source version for the current run; needs a valid install of an according msvc qt setup - stays unmodified!!!
	m_libPath = "E:/WORK/PROG/LIBS/Qt";			// ... target path for creating the misc libs subdirs; should be of course different from the qt install location!!!

	m_msvcs << MSVC2013;						// << MSVC2010 << MSVC2012 << MSVC2013 << MSVC2015;
	m_archs << X86;						// << X86 << X64;
	m_types << Static;				// << Shared << Static;
}



QtBuilder::QtBuilder(QWidget *parent) : QMainWindow(parent),
	m_log(NULL), m_prg(NULL), m_prc(NULL), m_diskSpace(0), m_state(Start),
	m_building(false), m_cancelled(false), m_result(true), m_imdiskUnit(imdiskUnit)
{
	createUi();

	connect(this, SIGNAL(copyProgress(int, const QString &)),
			this,		SLOT(progress(int, const QString &)), Qt::BlockingQueuedConnection);
}

QtBuilder::~QtBuilder()
{
}

void QtBuilder::closeEvent(QCloseEvent *event)
{
	QMainWindow::closeEvent(event);
	connect(m_prc, SIGNAL(destroyed()), qApp, SLOT(quit()), Qt::BlockingQueuedConnection);
	m_state = Finished;
}

void QtBuilder::procError()
{
	QString text = AppLog::clean(stdErr(), true);
	if (text.length() > 12)
		m_log->add("Process message", text, Elevated);
}

void QtBuilder::procOutput()
{
	if (m_building)
	{
		uint total, free;
		if (getDiskSpace(m_build, total, free))
			m_diskSpace=qMax(m_diskSpace,total-free);

		if (!m_spc->isVisible())
		{	 m_spc->show();
			 m_spc->setMaximum(total);
		}	 m_spc->setValue(m_diskSpace);

		m_jom->append(AppLog::clean(stdOut()), m_build);
	}
	else
	{
		QString text = AppLog::clean(stdOut(), true);
		if (text.length() > 12)
			m_log->add("Process info", text, Informal);
	}
}

// ~~thread safe (no mutexes) ...
const QString QtBuilder::targetDir(int msvc, int arch, int type, QString &native)
{
	m_libPath = QDir::cleanPath(m_libPath);
	QString target("%1/%2/%3/%4/%5");
	//
	//	USER_SET_LIB_PATH	...	something like				"D:\3d-Party\Qt"
	//	  \VERSION			... currently processed version	i.e. "4.8.7"
	//	    \TYPE			...	library type:				"shared" or "static"
	//	      \ARCHITECTURE	... MSVC $(Platform) compatible	"Win32" or "x64"
	//	        \MSVC_VER	...	MSVC $(PlatformToolset)		"v100" or "v110" or "v120" or "v130"
	//
			target = target.arg(m_libPath, m_version, paths[type], paths[arch], paths[msvc]);
			native = QDir::toNativeSeparators(target);
	return	target;
}

// ~~thread safe (no mutexes) ...
const QString QtBuilder::driveLetter()
{
	if (!qtBuilderStaticDrive.isEmpty())
		return qtBuilderStaticDrive;

	QFileInfoList drives = QDir::drives();
	QStringList aToZ;

	int a = (int)QChar('A').toLatin1();
	int z = (int)QChar('Z').toLatin1();
	for(int i = a; i <= z; i++)
		aToZ.append(QChar::fromLatin1(i));

	FOR_CONST_IT(drives)
		aToZ.removeOne((*IT).absolutePath().left(1).toUpper());
	if (aToZ.isEmpty())
		return QString();

	return aToZ.last();
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::qtSettings(QStringList &versions, QString &instDir)
{
	if (m_version.isEmpty())
	{
		log("No Qt version given", Elevated);
		return false;
	}

	versions+= "HKEY_CURRENT_USER\\Software\\Trolltech\\Versions\\%1";
	versions+= "HKEY_CURRENT_USER\\Software\\Digia\\Versions\\%1";
	instDir  = "InstallDir";
	return true;
}

void QtBuilder::exec()
{
	setGeometry(centerRect(25));
	setup();
	show();

	m_prg->setFixedWidth(width()/2);
	m_log->add("\r\nQtBuilder started", AppInfo);

	if (!(m_result = checkSource()))
	{
		CALL_QUEUED(this, end);
		return;
	}

	connect(&m_loop, SIGNAL(finished()), this, SLOT(end()));
	m_loop.setFuture(QtConcurrent::run(this, &QtBuilder::loop));
}

void QtBuilder::end()
{
	m_state = Finished;

	QString msg = m_result ?
		"<b>Process sucessfully completed.</b>" :
		"<b>Process ended with errors!</b>" ;

	QString url("<br/><a href=\"file:///%1\">Open log file</a>");
	msg +=  url.arg(QDir::toNativeSeparators(m_log->logFile()));

	QMessageBox b;
	b.setText(msg);
	b.exec();

	m_log->add("QtBuilder ended with", QString(m_result ? "no %1" : "%1").arg("errors\r\n"), AppInfo);
	close();
}

// ~~thread safe (no mutexes) ...
void QtBuilder::endProcess()
{
	if (!m_prc->isOpen())
		return;

	log("Process active", "waiting max. 30secs for process to end", Warning);
	CALL_QUEUED(this, disable);
#ifdef _WIN32
	GenerateConsoleCtrlEvent(0, (DWORD)m_prc->pid());
	GenerateConsoleCtrlEvent(1, (DWORD)m_prc->pid());
#endif
	m_prc->terminate();
	m_prc->waitForFinished(20000);

	if (m_prc->state() == QProcess::Running)
	{	m_prc->kill();
		m_prc->waitForFinished(1000);
		qDebug()<<"Qt kill running process";
	}
	if (m_prc->state() == QProcess::Running)
	{
		QProcess().execute(QString("taskkill /PID %1").arg((int)m_prc->pid()));
		qDebug()<<"Win kill running process";
		m_prc->close();
	}
	delete m_prc;
	m_prc = NULL;
}

// ~~thread safe (no mutexes) ...
void QtBuilder::loop()
{
	m_prc = new QProcess();
	connect(m_prc, SIGNAL(readyReadStandardError()),  this, SLOT(procError()),	Qt::BlockingQueuedConnection);
	connect(m_prc, SIGNAL(readyReadStandardOutput()), this, SLOT(procOutput()), Qt::BlockingQueuedConnection);

	if (!(m_result = createTemp()))
		return;

	int msvc, arch, type;
	FOR_CONST_IT(m_msvcs)
	FOR_CONST_JT(m_archs)
	FOR_CONST_KT(m_types)
	{
		msvc = *IT;
		arch = *JT;
		type = *KT;

		m_target.clear();
		m_dirFilter.clear();
		m_extFilter.clear();
		m_diskSpace = 0;

		for	  (m_state = ClearTarget;
			   m_state < Finished;
			   m_state++)
		switch(m_state	)
		{
		case ClearTarget:
		{	if(!(m_result = clearTarget(msvc, arch, type)))
				 m_state  = Finished;
		}	break;

		case CreateTarget:
		{	if(!(m_result = createTarget()))
				 m_state  = Finished;
		}	break;

		case CopyTarget:
		{	if(!(m_result = copyTarget(true)))
				 m_state  = Finished;
		}	break;

		case CopySource:
		{	if(!(m_result = copyTarget(false)))
				 m_state  = Finished;
		}	break;

		case CopyTemp:
		{	if(!(m_result = copyTemp()))
				 m_state  = Finished;
		}	break;

		case BuildQt:
		{	if(!(m_result = buildQt(msvc, arch, type)))
				 m_state  = Finished;
		}	break;
		}

		if (!m_result)
			goto end;
	}

	end:
	endProcess();
	m_result = removeTemp();
}

void QtBuilder::createUi()
{
	m_prg = new Progress(this);
	connect(this, SIGNAL(newCopyCount(int)), m_prg, SLOT(setMaximum(int)), Qt::BlockingQueuedConnection);

	QWidget *wgt = new QWidget(this);
	setCentralWidget(wgt);

	QVBoxLayout *lyt = new QVBoxLayout(wgt);
	lyt->setContentsMargins(0, 0, 0, 0);
	lyt->setSpacing(0);

	{	m_log = new AppLog(wgt);
		lyt->addWidget(m_log);

		connect(this, SIGNAL(log(const QString &, const QString &, int)), m_log, SLOT(add(const QString &, const QString &, int)),	Qt::BlockingQueuedConnection);
		connect(this, SIGNAL(log(const QString &, int)),				  m_log, SLOT(add(const QString &, int)),					Qt::BlockingQueuedConnection);
	}
	{	m_spc = new QProgressBar(wgt);
		m_spc->setAlignment(Qt::AlignHCenter);
		m_spc->setFormat("Temp disk usage: %v MB");
		m_spc->setStyle(new QPlastiqueStyle());
		m_spc->hide();
		lyt->addWidget(m_spc);

		connect(this, SIGNAL(buildFinished()), m_spc, SLOT(hide()), Qt::BlockingQueuedConnection);
	}
	{	m_jom = new BuildLog(wgt);
		lyt->addWidget(m_jom);

		connect(this, SIGNAL(buildFinished()), m_jom, SLOT(clear()), Qt::BlockingQueuedConnection);
	}
	lyt->setStretch(0, 2);
	lyt->setStretch(1, 0);
	lyt->setStretch(2, 1);
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::createTemp()
{
	log("Build step", "Creating temp infrastructure", AppInfo);

	QString drive = driveLetter();
	if (drive.isEmpty())
	{
		log("No temp drive letter available", Critical);
		return false;
	}

//	removeImDisk(true, true);
	if (!attachImdisk(drive))
	{
		log("Could not create RAM disk", QDir::toNativeSeparators(m_btemp), Critical);
		return false;
	}

	m_btemp = QString("%1:/%2").arg(drive, btemp);
	QString native = QDir::toNativeSeparators(m_btemp);

	if (!QDir(m_btemp).exists() && !QDir().mkpath(m_btemp))
		log("Could not create temp dir", native, Warning);

	m_build = QString("%1:/%2").arg(drive, build);
	native = QDir::toNativeSeparators(m_build);

	if (QDir(m_build).exists())
	{
		log("Build dir exists", native, Warning);
	//
	//	note: "copyFolder" is going to overwrite any modified file anyway;
	//	usually there should be no need to explicitly clear the build dir!
	//
	//	log("Build dir exists - clearing", native, Warning);
	//	emit newCopyCount(0);
	//	removeDir(m_build);
	//	emit newCopyCount();
	}
	if (!QDir().mkpath(m_build))
	{
		log("Could not create build dir", native, Critical);
		return false;
	}
	return true;
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::clearTarget(int msvc, int arch, int type)
{
	QString native;
	m_target = targetDir(msvc, arch, type, native);

	if (!qtBuilderClearTarget |
		!QDir(m_target).exists())
		return true;

	log("Build step", QString("Clearing target folder: %1").arg(native), AppInfo);

	emit newCopyCount(0);
	bool result = false;

	if ((result = removeDir(m_target)))
		 log("Target directory cleared", native);
	else log("Could not clear target dir", native, Elevated);

	emit newCopyCount();
	return result;
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::createTarget()
{
	QString native = QDir::toNativeSeparators(m_target);
	log("Build step", QString("Creating target folder: %1").arg(native), AppInfo);

	if (QDir(m_target).exists())
	{
		log("Target directory exists", native, Elevated);
		return true;
	}
	else if (QDir().mkpath(m_target))
	{
		log("Target directory created", native);
		log("Activating file compression", native, Warning);

		InlineProcess(this, "compact.exe", QString("/c /i %1").arg(native));
	}
	else
	{
		log("Could not create target dir", native, Elevated);
		return false;
	}
	return true;
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::copyTarget(bool removeBuiltLibs)
{
	QString native = QDir::toNativeSeparators(m_target);
	log("Build step", QString("Copying files to target folder: %1").arg(native), AppInfo);

	int count;
	if (!checkDir(Build, count))
		return false;

	if (!checkDir(Target))
		return false;

	log("Copying contents to:", native);
	emit newCopyCount(count);

	m_extFilter.clear();
	m_dirFilter = tfilter;
	m_cancelled = false;
	bool result = true;
	if (!(count = copyFolder(m_build, m_target)))
	{
		log("Could not copy contents to:", native, Critical);
		result = false;
	}

	emit newCopyCount();
	log("Total files copied", QString::number(count));

	if (removeBuiltLibs)
		removeDir(m_build+"/lib");

	return result;
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::copyTemp()
{
	QString native = QDir::toNativeSeparators(m_build);
	log("Build step", QString("Copying files to temp folder: %1").arg(native), AppInfo);

	int count;
	if (!checkDir(Source, count))
		return false;

	if (!checkDir(Build))
		return false;

	log("Copying contents to:", native);
	emit newCopyCount(count);

	m_extFilter = ffilter;
	m_dirFilter = sfilter;
	m_cancelled = false;
	bool result = true;
	if (!(count = copyFolder(m_source, m_build)))
	{
		log("Could not copy contents to:", native, Critical);
		result = false;
	}

	emit newCopyCount();
	log("Total files copied", QString::number(count));

	QFile(m_build+"/build.log").remove();
	clearPath(m_build+"/lib");
	return result;
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::buildQt(int msvc, int arch, int type)
{
	log("Build step", "Preparing Qt build", AppInfo);

	if (!checkDir(Source))
		return false;

	if (!checkDir(Target))
		return false;

	if (!checkDir(Build))
		return false;
	//
	//	copy jom executable to build root path (from /bin);
	//	(both simplify calling it, and check for existence)
	//
	QFile jom;
	{	QString j=m_source+"/bin";
		jom.setFileName(j+"/jom.exe");
		if (!jom.exists())
		{
			log("Missing jom.exe from:", QDir::toNativeSeparators(j), Critical);
			return false;
		}
		else
		{
			j = m_build+"/jom.exe";
			jom.copy(j);
			jom.setFileName(j);
		}
	}
	//
	//
	//
	QString vcVars;
	{		vcVars = builds.at(msvc)+"/VC/vcvarsall.bat";

		if (!QFileInfo(vcVars).exists())
		{
			log("Missing Visual Studio:", QDir::toNativeSeparators(vcVars), Critical);
			return false;
		}
		vcVars = QDir::toNativeSeparators(
			QString("call \"%1\" %2").arg(vcVars).arg(builds.at(arch)));
	}
	//
	//	the basic configuration is completed by
	//	- selecting a make spec from the list, according to the msvc version
	//	- selecting arguments for configure.exe according to msvc version and build type
	//	- joining the static configure options together and adding the to the args;
	//
	QString makeSpec = qMakeS.at(msvc);
	QString qtConfig = QString("-prefix %1 %2 %3 %4").arg(
			QDir::toNativeSeparators(m_target),
			qtOpts.at(msvc), qtOpts.at(type),
			(global+plugins+exclude).join(" "));

	QProcessEnvironment env;
	QString text;

	QElapsedTimer timer;
	timer.start();

	bool result = true;
	if(!(result = setEnvironment(env, vcVars, makeSpec)))
		goto end;

	writeQtVars(m_build, vcVars, msvc);

	{	text = QString("Version %1 ... %2 ... %3 bits ... %4");
		text = text.arg(m_version, type == Shared ?"shared":"static", arch==X86 ?"32":"64", makeSpec);
		log("Building Qt ...", text.toUpper(), Warning);
	}
	if (QFileInfo(m_build+"/Makefile").exists())
	{	log("Running jom confclean ...");

		BuildProcess proc(this, env, true);
		proc.setArgs("confclean");
		proc.start("jom.exe");

		if (!(result = proc.result()))
		{
			CALL_QUEUED(this, procError);
			goto end;
		}
	}
	{	log("Running configure ...");

		BuildProcess proc(this, env);
		proc.setArgs(qtConfig);
		proc.start("configure.exe");

		if (!(result = proc.result()))
			goto end;
	}
	{	log("Running jom make ...");

		BuildProcess proc(this, env);
		proc.start("jom.exe");

		if (!(result = proc.result()))
			goto end;
	}
	{	log("Running jom install ...");

		BuildProcess proc(this, env);
		proc.setArgs("install");
		proc.start("jom.exe");

		if (!(result = proc.result()))
			goto end;
	}
	{	log("Running jom clean ...");

		BuildProcess proc(this, env, true);
		proc.setArgs("clean");
		proc.start("jom.exe");

		if (!(result = proc.result()))
		{
			CALL_QUEUED(this, procError);
			goto end;
		}
	}

	end:
	{
		text = QString("Time: %1 minutes ... %2 MB max. used temp disk space");
		text = text.arg(timer.elapsed()/60000).arg(m_diskSpace);
		log("Qt build done...", text.toUpper(), Warning);
		emit buildFinished();
	}

	registerQtVersion();
	jom.remove();
	return result;
}

// ~~thread safe (no mutexes) ...
void QtBuilder::registerQtVersion()
{
	return; // a) not working, b) not usefual in particular for having Qt Creator showing the new versions

	QStringList versions;
	QString version, instDir;
	if (!qtSettings(versions, instDir))
		return;

	FOR_CONST_IT(versions)
	{
		QString native = QDir::toNativeSeparators(m_target);
		QString name = QString(m_target).remove(m_libPath+SLASH).replace(SLASH,"-");

		  version = (*IT).arg(name);
		QSettings s(version);
		qDebug() << version;

		if (s.isWritable())
		{	s.setValue(instDir, native);
			s.sync();
		if (s.value(instDir).toString() == native)
			return;
		}
		log("Could not register Qt in:", version+QString(": REG_SZ\"%1\"").arg(instDir), Elevated);
	}
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::setEnvironment(QProcessEnvironment &env, const QString &vcVars, const QString &mkSpec)
{
	QString tgtNat = QDir::toNativeSeparators(m_target);
	QString bldNat = QDir::toNativeSeparators(m_build);
	QString tmpNat = QDir::toNativeSeparators(m_btemp);
	//
	//	create cmd script to set & gather the msvc build environment
	//
	QString  anchor = QUuid::createUuid().toString();
	QString  script, native;
	{	QString cmd;
				cmd += QString("%1\r\n"		 ).arg(vcVars);
				cmd += QString("#echo %1\r\n").arg(anchor);
				cmd += QString("set"		 );

		script = m_btemp+"/vcvars.cmd";
		native = QDir::toNativeSeparators(script);

		if (!writeTextFile(script, cmd))
		{
			log("Failed to set build environment", QString("Write: ").arg(native), Critical);
			return false;
		}
	}
	//
	//	collect the build environment by executing
	//	the above script and parsing the output...
	//
	BuildProcess proc(this, env, true);
	proc.start(script);

	if (!proc.result())
	{
		log("Failed to set build environment", QString("Execute: ").arg(native), Critical);
		return false;
	}
	//
	//	some modifications during parsing the environment:
	//
	//	- any existing qt related path value is filtered
	//	_build_ dirs for /bin and /lib are added instead
	//
	//	- QMAKESPEC value is set to the _target_ path!
	//	- the QTDIR value is set to the _target_ path!
	//
	//  - the TEMP/TMP values are set to the temp dir
	//	  (since they are used by cl.exe and link.exe)
	//
	QString key, value;
	QStringList  parts = stdOut().split(anchor);
	QStringList  lines = parts.last().split("\r\n");
	FOR_CONST_IT(lines)
	{
		value = *IT;
		if (!value.contains("="))
			continue;

		parts = value.split("=");
		key  = parts.takeFirst();
		value = parts.join ("="); // re-join just in case the value contained extra "=" charecters!!!

		if (key.toLower() == "path")
		{
			parts = value.split(";");
			parts.removeDuplicates();
			value.clear();
			FOR_CONST_JT(parts)
			   if (!(*JT).contains("qt", Qt::CaseInsensitive))
				  value += (*JT)+";";
			value += bldNat+"\\bin;";
			value += bldNat+"\\lib;";
		}
		else if ( key.contains("qt", Qt::CaseInsensitive) ||
				value.contains("qt", Qt::CaseInsensitive))
				continue;

		env.insert(key, value);
	}
	if (checkDir(Temp))
	{
		env.insert("TEMP",		tmpNat);
		env.insert("TMP",		tmpNat);
	}
	{	env.insert("QTDIR",		tgtNat);
		env.insert("QMAKESPEC", tgtNat+"\\mkspecs\\"+mkSpec);
	}
	return true;
}

// ~~thread safe (no mutexes) ...
void QtBuilder::writeQtVars(const QString &path, const QString &vcVars, int msvc)
{
	QString nat = QDir::toNativeSeparators(m_target);
	QString var;
	var += QString("@echo off\r\n"									);
	var += QString("rem\r\n"										);
	var += QString("rem This file is generated by QtBuilder\r\n"	);
	var += QString("rem\r\n"										);
	var += QString("\r\n"											);
	var += QString("echo Setting up a Qt environment...\r\n"		);
	var += QString("\r\n"											);
	var += QString("set QTDIR=%1\r\n"								).arg(nat);
	var += QString("echo -- QTDIR set to %1\r\n"					).arg(nat);
	var += QString("set PATH=%1\\bin;%PATH%\r\n"					).arg(nat);
	var += QString("echo -- Added %1\\bin to PATH\r\n"				).arg(nat);
	var += QString("set QMAKESPEC=%1\r\n"							).arg(qMakeS.at(msvc));
	var += QString("echo -- QMAKESPEC set to \"%1\"\r\n"			).arg(qMakeS.at(msvc));
	var += QString("\r\n"											);
	var += QString("if not \"%%1\"==\"vsvars\" goto ENDVSVARS\r\n"	);
	var += QString("%1\r\n"											).arg(vcVars);
	var += QString(":ENDVSVARS\r\n"									);
	var += QString("\r\n"											);
	var += QString("if not \"%%1\"==\"vsstart\" goto ENDVSSTART\r\n");
	var += QString("%1\r\n"											).arg(vcVars);
	var += QString("devenv /useenv\r\n"								);
	var += QString(":ENDVSSTART\r\n"								);

	writeTextFile(path+qtVars, var);
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::writeTextFile(const QString &filePath, const QString &text)
{
	QString native = QDir::toNativeSeparators(filePath);
	QFile file(filePath);
	if ( !file.open(QIODevice::WriteOnly) ||
		 !file.write(text.toUtf8().constData()))
	{
		log("Could not write file", native, Elevated);
		file.close();
		return false;
	}
	else
	{
		log("File created", native);
		file.close();
		return true;
	}
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::removeTemp()
{
	if (!qtBuilderStaticDrive.isEmpty())
		return true;

	log("Build step", "Removing temp infrastructure", AppInfo);
	return removeImdisk(false, false);
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::checkSource()
{
	QStringList versions;
	QString version, instDir;
	if (!qtSettings(versions, instDir))
		return false;

	bool result = false;
	FOR_CONST_IT(versions)
	{
		version = (*IT).arg(m_version);
		QSettings s(version, QSettings::NativeFormat);
		QString dir = s.value(instDir).toString().trimmed();
		if ((result = s.contains(instDir) && !dir.isEmpty()))
		{	QDir  d = QDir(dir);
			if ( !d.exists())
			{
				log("Qt install path not on disk:", QDir::toNativeSeparators(m_source), Elevated);
				return false;
			}

			m_source = d.absolutePath();
			break;
		}
	}
	if (!result)
	{
		log("Could not find Qt install from:", version+QString(": REG_SZ\"%1\"").arg(instDir), Elevated);
		return false;
	}
	return true;
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::checkDir(int which)
{
	int result;
	return checkDir(which, result, QString(), QString());
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::checkDir(int which, int &result, QString &path, QString &name)
{
	result = Critical;
	switch(which)
	{
	case Source:
		path = m_source;
		name = "Source";
		break;

	case Target:
		path = m_target;
		name = "Target";
		break;
	case Build:
		path = m_build;
		name = "Build";
		break;

	case Temp:
		path = m_btemp;
		name = "Temp";
		result = Warning;
		break;

	default:
		return false;
	}

	if (QDir(path).exists())
		 result = Informal;
	else log(QString("%1 directory removed").arg(name), QDir::toNativeSeparators(path), result);
	return result != Critical;
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::checkDir(int which, int &count)
{
	count = 0;

	int result;
	QString path, name;
	if (!checkDir(which, result, path, name))
		return false;

	bool filter =!m_extFilter.isEmpty();
	QDirIterator it(QDir(path), QDirIterator::Subdirectories);
	QFileInfo info;

	while (it.hasNext())
	{
		it.next();
		info = it.fileInfo();

		bool d = info.isDir();
		if ( d && filterDir(it.filePath()+SLASH))
			continue;

		else if (!filter || !filterExt(info))
			count++;
	}

	if (!count && result == Critical)
	{
		log(QString("%1 directory empty").arg(name), QDir::toNativeSeparators(path), Elevated);
		return false;
	}
	return true;
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::attachImdisk(QString &letter)
{
	if (!qtBuilderStaticDrive.isEmpty())
		return true;

	QString out;
	{	InlineProcess p(this, "imdisk.exe", "-l -n", true);
		out = stdOut();
	}
	if (out.contains(QString::number(m_imdiskUnit)))
	{
		InlineProcess p(this, "imdisk.exe", QString("-l -u %1").arg(m_imdiskUnit), true);
		QString inf = stdOut();
		if (inf.contains(imdiskLetter))
		{	//
			// TODO: in this case it would also be needed to extend the disk if needed
			//
			letter	 = getValueFrom(inf, imdiskLetter, "\n");
			int size = getValueFrom(inf, imdiskSizeSt, " ").toULongLong() /1024 /1024 /1024;
			log("Using existing RAM disk", QString("Drive letter %1, %2GB").arg(letter).arg(size));
			return true;
		}
		else while(out.contains(QString::number(m_imdiskUnit)))
		{
			m_imdiskUnit++;
		}
	}

	log("Trying to attach RAM disk", QString("Drive letter %1, %2GB").arg(letter).arg(qtBuilderRamDiskInGB));

	QString args = QString("-a -m %1: -u %2 -s %3G -o rem -p \"/fs:ntfs /q /y\"");
	args  = args.arg(letter).arg(m_imdiskUnit).arg(qtBuilderRamDiskInGB);

	InlineProcess(this, "imdisk.exe", args, false);
	return normalExit();
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::removeImdisk(bool silent, bool force)
{
	if (!qtBuilderStaticDrive.isEmpty())
		return true;

	if (!silent)
		log("Trying to remove RAM disk");

	QString args = QString("-%1 -u %2");
	args  = args.arg(force ? "D" : "d").arg(m_imdiskUnit);

	InlineProcess(this, "imdisk.exe", args, silent);
	return normalExit();
}

// ~~thread safe (no mutexes) ...
int QtBuilder::copyFolder(const QString &source, const QString &target)
{
	QQueue<QPair<QDir, QDir> > queue;
	queue.enqueue(qMakePair(QDir(source), QDir(target)));
	bool filter =!m_extFilter.isEmpty();

	QFileInfoList sdirs, files;
	QPair<QDir, QDir>  pair;
	QString tgt, destd, nat;
	QDir srcDir, desDir;
	int count = 0;

	while(!queue.isEmpty())
	{
		if (m_cancelled)
			return 0;

		pair = queue.dequeue();
		if(!pair.first.exists())
			continue;

		srcDir = pair.first;
		desDir = pair.second;

		if (filterDir(srcDir.absolutePath()))
			continue;

		if(!desDir.exists() && !QDir().mkpath(desDir.absolutePath()))
			return 0;

		destd = desDir.absolutePath()+SLASH;
		files = srcDir.entryInfoList(QDir::Files);
		QFileInfo src, des;

		FOR_CONST_IT(files)
		{
			if (m_cancelled)
				return 0;

			src = *IT;
			tgt = destd+src.fileName();
			nat = QDir::toNativeSeparators(tgt);

			if (filter && filterExt(src))
				continue;

			if (tgt.length() > 256)
			{
				log("Path length exceeded", nat, Elevated);
				return 0;
			}
			else if (!m_cancelled)
			{
				if  (!((++count)%10))
					emit copyProgress(count, nat);

				des = QFileInfo(tgt);
			}

			bool exists;
			if ((exists  = des.exists()) &&
				des.size() == src.size() &&
				des.lastModified() == src.lastModified())
			{
				continue;
			}
			else if (exists && !QFile(tgt).remove())
			{
				log("Could not replace file", nat, Elevated);
				return 0;
			}
			else if (!QFile::copy(src.absoluteFilePath(), tgt))
			{
				log("Could not copy file", nat, Elevated);
				return 0;
			}
		}

		sdirs = srcDir.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot);
		destd = desDir.absolutePath()+SLASH;

		FOR_CONST_IT(sdirs)
		{
			if (m_cancelled)
				return 0;

			srcDir = (*IT).absoluteFilePath();
			queue.enqueue(qMakePair(srcDir, QDir(destd+srcDir.dirName())));
		}
	}
	return count;
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::removeDir(const QString &dirPath, const QStringList &inc)
{
	bool result = true;
	QDir dir(dirPath);

	if (!dir.exists(dirPath))
		return result;

	bool i = !inc.isEmpty();

	QFileInfo info;
	QFileInfoList infos = dir.entryInfoList(QDir::NoDotAndDotDot|QDir::System|QDir::Hidden|QDir::AllDirs|QDir::Files, QDir::DirsFirst);
	FOR_CONST_IT( infos )
	{		 info = *IT;
		if ( info.isDir())
		{
			if (!i  ||  inc.contains(info.fileName()))
			{	 result &= removeDir(info.absoluteFilePath());
			}
			else continue;
		}
		else if (!i ||  inc.contains((QDir(info.absolutePath()).dirName())))
			 result &= QFile::remove(info.absoluteFilePath());
		if (!result)
			return result;
	}
	return result && dir.rmdir(dirPath);
}

// ~~thread safe (no mutexes) ...
void QtBuilder::clearPath(const QString &dirPath)
{
	QDir dir(dirPath);
	if (!dir.exists(dirPath))
		return;

	QFileInfoList infos = dir.entryInfoList(QDir::System|QDir::Hidden|QDir::Files);
	FOR_CONST_IT( infos )
		QFile((*IT).absoluteFilePath()).remove();
}

// ~~thread safe (no mutexes) ...
const QString qtBuilderCoreLib("corelib");
const QString qtBuilderHelpCnv("qhelpconverter");
bool QtBuilder::filterDir(const QString &dirPath)
{
	QString d = dirPath.toLower();
	if (d.contains(qtBuilderCoreLib) ||
		d.contains(qtBuilderHelpCnv))
		return false;

	QString p = dirPath+SLASH;
	bool skip = false;
	FOR_CONST_IT(m_dirFilter)
		if ((skip = p.contains(*IT, Qt::CaseInsensitive)))
			break;
	return	skip;
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::filterExt(const QFileInfo &info)
{
	return m_extFilter.contains(info.suffix().toLower());
}

void QtBuilder::progress(int count, const QString &file)
{
	if (m_cancelled)
		return;

	m_prg->setValue(count);
	m_prg->setLabelText(file);

	if (m_prg->wasCanceled())
	{
		m_log->add("Copying cancelled", Warning);
		m_prg->hide();
		m_cancelled = true;
	}
}



Progress::Progress(QWidget *parent) : QProgressDialog(parent, Qt::WindowStaysOnTopHint)
{
	setStyleSheet("QLabel { qproperty-alignment: 'AlignLeft | AlignBottom'; }");
	setWindowTitle("Copying Files");
	setFixedWidth(parent->width()/2);
	setAutoReset(false);
	setAutoClose(false);

	QProgressDialog::setMaximum(INT_MAX);
}

void Progress::setMaximum(int max)
{
	if (max == -1)
	{
		hide();
		QProgressDialog::setMaximum(INT_MAX);
	}
	else if (max != maximum())
	{
		QProgressDialog::setMaximum(max);
		show();
	}
}



const QString qtBuilderTableBody("<html><header><style>TD{padding:0px 6px;}</style></header><body style=font-size:11pt;font-family:Calibri;><table>");
const QString qtBuilderTableLine("<tr><td style=font-size:10pt>%1</td><td><b style=color:%2>%3</b></td><td style=white-space:pre>%4</td></tr>");
const QString qtBuilderTableHtml("</table><br/><a name=\"end\"></body></html>");
const QString qtBuilderLogLine("%1\t%2\t%3\t%4\r\n");
const QString qtBuilderBuildLogTabs = QString("\n")+QString("\t").repeated(10);



AppLog::AppLog(QWidget *parent) : QTextBrowser(parent)
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);

	m_info.insert(AppInfo,	"AppInfo ");
	m_info.insert(Elevated,	"Elevated");
	m_info.insert(Warning,	"Warning ");
	m_info.insert(Critical,	"Critical");
	m_info.insert(Informal,	"Informal");
}

const QString AppLog::logFile()
{
	return qApp->applicationDirPath()+SLASH+qApp->applicationName()+".log";
}

const QString AppLog::clean(QString text, bool extended)
{
	text = text.remove("\t").trimmed();
	if (!extended)
		return text;

	QStringList t = text.split("\r\n", QString::SkipEmptyParts);
				t+= text.split(  "\n", QString::SkipEmptyParts);

	int count = t.count();
	for(int i = 0; i < count; i++)
	{
		text = t[i].simplified();
		if (text.isEmpty())
		{
			t.removeAt(i);
			count--;
		}
		else t[i] = text;
	}

	t.removeDuplicates();
	return t.join("\r\n");
}

void AppLog::add(const QString &msg, int type)
{
	add(msg, QString(), type);
}

void AppLog::add(const QString &msg, const QString &text, int type)
{
	QString ts = QDateTime::currentDateTime().toString(Qt::ISODate);

	m_timestamp.append(ts);
	m_message.append(msg);
	m_description.append(text);
	m_type.append(type);

	QSet<uint> hashes;
	QString tline;

	QString h = qtBuilderTableBody;
	int count = m_type.count();
	for(int i = 0; i < count; i++)
	{
		tline = qtBuilderTableLine.arg(m_timestamp.at(i), colors.at(m_type.at(i)), m_message.at(i), m_description.at(i));
		uint hash = qHash(tline); // ... filter out duplicates for process messages posted to stdout AND stdErr!!!

		if (!hashes.contains(hash))
		{	 hashes.insert(hash);
			 h.append(tline);
		}
	}
	h.append(qtBuilderTableHtml);

	setText(h);
	scrollToAnchor("end");

	QFile log(logFile());
	if (log.open(QIODevice::Append))
		log.write(qtBuilderLogLine.arg(ts, m_info.value(type), msg.leftJustified(32),
			QString(text).replace("\n", qtBuilderBuildLogTabs)).toUtf8().constData());
}



BuildLog::BuildLog(QWidget *parent) : QTextEdit(parent)
{
	setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOn);
	setLineWrapMode(QTextEdit::NoWrap);
	setFrameStyle(QFrame::NoFrame);
	setFontFamily("Consolas");
	setFontPointSize(9);
}

void BuildLog::append(const QString &text, const QString &path)
{	QTextEdit::append(text);

	if (!qtBuilderWriteBldLog)
		return;

	QFile log(path+SLASH+"build.log");
	if (log.open(QIODevice::Append))
		log.write(text.toUtf8().constData());
}
