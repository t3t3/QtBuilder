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

#include <QApplication>
#include <QUuid>

// ~~thread safe (no mutexes) ...
void QtBuilder::loop()
{
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
		CALL_QUEUED(m_bld, clear);
		CALL_QUEUED(m_tmp, reset);

		for	  (m_state = CreateTarget;
			   m_state < Finished;
			   m_state++)
		switch(m_state	)
		{
		case CreateTarget:
		{	if(!(m_result = createTarget(msvc, type, arch)))
				 m_state  = Finished;
		}	break;

		case CreateTemp:
		{	if(!(m_result = createTemp()))
				 m_state  = Finished;
		}	break;

		case CopySource:
		{	if(!(m_result = copySource()))
				 m_state  = Finished;
		}	break;

		case Prepare:
		{	if(!(m_result = prepare(msvc, type, arch)))
				 m_state  = Finished;
		}	break;

		case ConfClean:
		{	if(!(m_result = confClean()))
				 m_state  = Finished;
		}	break;

		case Configure:
		{	if(!(m_result = configure(msvc, type)))
				 m_state  = Finished;
		}	break;

		case JomBuild:
		{	if(!(m_result = jomBuild()))
				 m_state  = Finished;
		}	break;

		case JomClean:
		{	if(!(m_result = jomClean()))
				 m_state  = Finished;
		}	break;

		case CleanUp:
		{	if(!(m_result = cleanup()))
				 m_state  = Finished;
		}	break;

		case CopyTarget:
		{	if(!(m_result = copyTarget()))
				 m_state  = Finished;
		}	break;
		}

		if (!m_result)
			goto end;
	}

	end:
	m_result = removeTemp();
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::createTemp()
{
	log("Build step", "Creating temp infrastructure ...", AppInfo);

	m_drive = driveLetter();
	if (m_drive.isEmpty())
	{
		log("No drive letter available", Critical);
		return false;
	}

	if (!attachImdisk(m_drive))
	{
		log("Couldn't create RAM disk:", QString("\"%1:\"").arg(m_drive), Critical);
		return false;
	}

	m_drive += ":";
	if (!QDir(m_build).entryList(QDir::NoDotAndDotDot).isEmpty())
		log("Build folder not empty", "Existing content might get overwritten!", Warning);

	m_btemp = m_drive+SLASH+btemp;
	m_build = m_drive+SLASH+build;

	if (!QDir(m_btemp).exists() && !QDir().mkpath(m_btemp))
	{
		log("Couldn't create temp folder:", QDir::toNativeSeparators(m_btemp), Warning);
	}
	if (!QDir(m_build).exists() && !QDir().mkpath(m_build))
	{
		log("Couldn't create build folder:", QDir::toNativeSeparators(m_build), Critical);
		return false;
	}

	QFile(m_build+m_bld->logFile()).remove();
	clearPath(m_build+"/lib");

	CALL_QUEUED(m_tmp, setDrive, (QString, m_build));
	return true;
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::createTarget(int msvc, int type, int arch)
{
	QStringList tp;
	qString bldNat = QDir::toNativeSeparators(m_build);
	QString tgtNat, error;
	m_target = targetDir(msvc, arch, type, tgtNat, tp);

	log("Build step", "Creating target folder ...", AppInfo);

	bool exists;
	tp.removeLast();
	QString mp = tp.join(SLASH);

	QFileInfo dir(m_target);
	if ((exists = dir.symLinkTarget() == m_build))
	{
		log("Target folder already mounted:", QString("%1 -> %2").arg(bldNat, tgtNat), Warning);
	}
	else if (dir.exists())
	{
		CALL_QUEUED(m_cpy, setMaximum, (int, 0));
		CALL_QUEUED(m_cpy, show);

		log("Clearing target folder:", tgtNat);
		bool result = false;

		if ((result = removeDir(m_target)))
			 log("Target directory removed:",  tgtNat);
		else log("Couldn't clear target dir:", tgtNat, Critical);

		CALL_QUEUED(m_cpy, hide);
		if (!result)
			return false;
	}

	if (!QDir(mp).exists() && !QDir().mkpath(mp))
	{
		log("Couldn't create target dir:", tgtNat, Elevated);
		return false;
	}
	else if (!exists && !createSymlink(m_build, m_target, error))
	{
		log("Couldn't mount folder:", QString("%1 -> %2 (%3)").arg(bldNat, tgtNat, error), Critical);
		return false;
	}
	else if (!exists)
	{
		log("Target directory mounted:", QString("%1 -> %2").arg(bldNat, tgtNat));
	}

	CALL_QUEUED(m_tgt, setDrive, (QString, m_target));
	return true;
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::copySource()
{
	log("Build step", "Copying source files ...", AppInfo);
	QString native = QDir::toNativeSeparators(m_build);

	if (!checkDir(Build))
		return false;

	m_dirFilter = sfilter;
	m_extFilter = ffilter;
	m_cancelled = false;

	int count;
	if (!checkDir(Source, count))
		return false;

	log("Copying contents to:", native);
	CALL_QUEUED(m_tgt, hide);

	if (!(count = copyFolder(m_source, m_build, count)))
		log("Couldn't copy contents to:", native, Critical);

	log("Total files copied:", QString::number(count));
	CALL_QUEUED(m_tgt, show);

	return count && !m_cancelled;
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::copyTarget()
{
	log("Build step", "Copying target files ...", AppInfo);
	QString native = QDir::toNativeSeparators(m_target);

	if (!checkDir(Target))
		return false;

	if (!removeSymlink(m_target))
	{
		log("Couldn't unmount folder:", QString("%1 -> %2")
			.arg(QDir::toNativeSeparators(m_build), native), Critical);
		return false;
	}
	else if (!QDir().mkpath(m_target) || !QDir(m_target).exists())
	{
		log("Couldn't re-create folder:", native, Critical);
		return false;
	}

	log("Activating file compression:", native, Warning);
	InlineProcess(this, "compact.exe", QString("/c /i %1").arg(native));

	m_dirFilter = sfilter+tfilter;
	m_extFilter	= cfilter;
	m_cancelled = false;

	int count;
	if (!checkDir(Build, count, true))
		return false;

	log("Copying contents to:", native);
	CALL_QUEUED(m_tmp, hide);

	if (!(count = copyFolder(m_build, m_target, count, false, true)))
		log("Couldn't copy contents to:", native, Critical);

	log("Total files copied:", QString::number(++count));
	CALL_QUEUED(m_tmp, show);

	QFile(m_build+m_bld->logFile()).copy(m_target+m_bld->logFile());
	return count && !m_cancelled;
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::prepare(int msvc, int type, int arch)
{
	if (!checkDir(Source))
		return false;

	if (!checkDir(Target))
		return false;

	QString makeSpec = qMakeS.at(msvc);
	QString text = QString("<b>Version %1 ... %2 ... %3 bits ... %4</b>");
	text = text.arg(m_version, type==Shared?"shared":"static", arch==X86 ?"32":"64", makeSpec);

	log("Building Qt", text.toUpper(), Explicit);
	restart();
	//
	//	copy jom executable to build root path (from /bin);
	//	(both simplify calling it, and check for existence)
	//
	{	QString j=m_source+"/bin";
		QFile jom(j+"/jom.exe");
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

	bool result = true;
	if(!(result = setEnvironment(vcVars, makeSpec)))
	{
		cleanup();
		return false;
	}

	writeQtVars(m_target, vcVars, msvc);
	return true;
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::confClean()
{
	if (!QFileInfo(m_build+"/Makefile").exists())
		return true;

	log("Build step", "Running jom confclean ...", AppInfo);

	BuildProcess proc(this, true);
	proc.setArgs("confclean");

	if	(  !proc.result()  )
			proc.start("jom.exe");
	return	proc.result() || true;
	// ... seems to be better to not use confclean results, i.e. if the prev. build was messed up.
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::configure(int msvc, int type)
{
	log("Build step", "Running configure ...", AppInfo);

	QString qtConfig = QString("%1 %2 %3")
	   .arg(qtOpts.at(msvc), qtOpts.at(type), (global+plugins+exclude).join(" "));

	BuildProcess proc(this);
	proc.setArgs(qtConfig);
	proc.start("configure.exe");

	return proc.result() &&  m_state == Configure;
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::jomBuild()
{
	log("Build step", "Running jom make ...", AppInfo);
	//
	// TODO: this needs to go into the config sections, as it is of course connected
	// with the pre-defined configure options (whatever they are good for anyway)!!!
	//
	FOR_CONST_IT(targets)
	{
		if (m_state == JomBuild)
		{	BuildProcess proc(this);
			proc.setArgs(*IT);
			proc.start("jom.exe");
		if(!proc.result())
				 return false;
		}	else return false;
	}
	return m_state == JomBuild;
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::jomClean()
{
	log("Build step", "Running jom clean ...", AppInfo);

	BuildProcess proc(this, true);
	proc.setArgs("clean");
	proc.start("jom.exe");

	if (!proc.result())
	{	 proc.sendStdErr();
		return false;
	}	return m_state == JomClean;
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::cleanup()
{
	QLocale l(QLocale::English);
	QString mbts = l.toString(m_tmp->maxUsed());
	QString text = QString("<b>Time ... %1:%2 minutes ... %3 MB max. used temp disk space</b>");
	uint secs = elapsed()/1000;
	uint mins = secs/60;
	text = text.arg(mins).arg(secs%mins,2,FILLNUL).arg(mbts);
	log("Qt build done", text.toUpper(), Explicit);

	registerQtVersion();
	QFile(m_target+"/jom.exe"  ).remove();

	return m_state == CleanUp;
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::setEnvironment(const QString &vcVars, const QString &mkSpec)
{
	m_env.clear();
	//
	//	initial steps to create a clean build environment:
	//
	//	- clear the current (local) environment variable
	//	- get the current system envrionment from QProcess
	//
	//	- filter out any msvc references from the path, in order to avoid fallbacks
	//	 (because of existing multiple msvc versions, or multiple versions _after_
	//	  running vcvarsall.bat - since this just adds new stuff in front of it!)
	//
	//	- finally the process which runs the own get-script is going to use this...
	//
	QString value, key;
	QStringList parts, lines;
	QStringList e = QProcess::systemEnvironment();
	int count = e.count();
	for(int i = 0; i < count; i++)
	{
		value = e.at(i);
		if (!value.startsWith("path=", Qt::CaseInsensitive))
			continue;

		parts = value.split("=");
		key  = parts.takeFirst();
		value = parts.join ("="); // re-join just in case the value contained extra "=" charecters!!!

		if (key.toLower() == "path")
		{
			parts = value.split(";");
			parts.removeDuplicates();
			FOR_CONST_JT(parts)
			   if (!(*JT).contains(vc2010, Qt::CaseInsensitive) &&
				   !(*JT).contains(vc2012, Qt::CaseInsensitive) &&
				   !(*JT).contains(vc2013, Qt::CaseInsensitive) &&
				   !(*JT).contains(vc2015, Qt::CaseInsensitive))
					lines +=  (*JT);
			value = lines.join(";");
		}
		m_env.insert(key, value);
	}

	QString tgtNat = QDir::toNativeSeparators(m_target);
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
			log("Failed to write environment script:", native, Critical);
			return false;
		}
	}
	//
	//	collect the build environment by executing
	//	the above script and parsing the output...
	//	using a "clean" version of the path value!
	//
	BuildProcess prc(this, true);
	prc.start(script);
	if (!prc.result())
	{
		log("Failed to get build environment:", native, Critical);
		return false;
	}
	//
	//	additional modifications during parsing...
	//
	//	- any existing Qt related path value is filtered
	//	  build dirs for /bin and /lib are added instead
	//
	//	- QMAKESPEC value is set to the _target_ path!
	//	- the QTDIR value is set to the _target_ path!
	//
	//  - the TEMP/TMP values are set to the temp dir
	//	  (to buffer temporary files from cl/link.exe)
	//
	parts = prc.stdOut().split(anchor);
	lines = parts.last().split(_CRLF);
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
			parts = value.split(";", QString::SkipEmptyParts);
			parts.removeDuplicates();
			value.clear();
			FOR_CONST_JT(parts)
			   if (!(*JT).contains("qt", Qt::CaseInsensitive))
				  value += (*JT)+";";
			value += tgtNat+"\\bin;";
			value += tgtNat+"\\lib" ;
		}
		else if ( key.contains("qt", Qt::CaseInsensitive) ||
				value.contains("qt", Qt::CaseInsensitive))
				continue;

		m_env.insert(key, value);
	}
	if (checkDir(Temp))
	{
		m_env.insert("TEMP", tmpNat);
		m_env.insert("TMP",	 tmpNat);
	}
	{	m_env.insert("CL",	"/MP");
		m_env.insert("QTDIR",	  tgtNat);
		m_env.insert("QMAKESPEC", tgtNat+"\\mkspecs\\"+mkSpec);
	}
	return true;
}

// ~~thread safe (no mutexes) ...
void QtBuilder::writeQtVars(const QString &path, const QString &vcVars, int msvc)
{
	QString nat = QDir::toNativeSeparators(path);
	QString var;
	var += QString("@echo off\r\n"									);
	var += QString("rem\r\n"										);
	var += QString("rem This file is generated by QtBuilder\r\n"	);
	var += QString("rem\r\n"										);
	var += QString(_CRLF);
	var += QString("echo Setting up a Qt environment...\r\n"		);
	var += QString(_CRLF);
	var += QString("set QTDIR=%1\r\n"								).arg(nat);
	var += QString("echo -- QTDIR set to %1\r\n"					).arg(nat);
	var += QString("set PATH=%1\\bin;%PATH%\r\n"					).arg(nat);
	var += QString("echo -- Added %1\\bin to PATH\r\n"				).arg(nat);
	var += QString("set QMAKESPEC=%1\r\n"							).arg(qMakeS.at(msvc));
	var += QString("echo -- QMAKESPEC set to \"%1\"\r\n"			).arg(qMakeS.at(msvc));
	var += QString(_CRLF);
	var += QString("if not \"%%1\"==\"vsvars\" goto ENDVSVARS\r\n"	);
	var += QString("%1\r\n"											).arg(vcVars);
	var += QString(":ENDVSVARS\r\n"									);
	var += QString(_CRLF);
	var += QString("if not \"%%1\"==\"vsstart\" goto ENDVSSTART\r\n");
	var += QString("%1\r\n"											).arg(vcVars);
	var += QString("devenv /useenv\r\n"								);
	var += QString(":ENDVSSTART\r\n"								);

	writeTextFile(path+qtVar, var);
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::removeTemp()
{
	if (m_keepDisk)
		return true;

	log("Build step", "Removing temp infrastructure ...", AppInfo);
	return removeImdisk(false, false);
}