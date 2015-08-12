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
#include "qmath.h"

#include <QApplication>
#include <QUuid>

const bool qtBuilderConfigOnly = false;
const bool qtBuilderUseTargets = false;

QtCompile::QtCompile(QtBuilder *main) : QtBuildState(main),
	m_keepDisk(false), m_imdiskUnit(imdiskUnit)
{
	connect(main, SIGNAL(cancel()), this, SLOT(cancel()));
}

void QtCompile::sync()
{
	QtBuilder *b;
	if (!(b = qobject_cast<QtBuilder *>(parent())))
		return;

	m_confs	= b->m_confs;
	m_msvcs = b->m_msvcs;
	m_archs = b->m_archs;
	m_types = b->m_types;
	m_bopts = b->m_bopts;

	m_target	= b->m_target;
	m_source	= b->m_source;
	m_version	= b->m_version;
	m_libPath	= b->m_libPath;

	m_msvcBOpts = b->m_msvcBOpts;
}

void QtCompile::loop()
{
	if (!createTemp())
	{
		state = ErrCreateTemp;
		return;
	}

	Modes m;
	m.unite(m_msvcs);
	m.unite(m_archs);
	m.unite(m_types);
	FOR_IT(m)IT.value()=0;

	int msvc, arch, type;
	FOR_CONST_KT(m_types)
	FOR_CONST_JT(m_archs)
	FOR_CONST_IT(m_msvcs)
	{
		if (!IT.value()||!JT.value()||!KT.value())
			continue;

		msvc = IT.key();
		arch = JT.key();
		type = KT.key();

		m[msvc]=m[arch]=m[type]=true;
		emit current(m);
		m_target.clear();

		for	  (state;
			   state < Finished;
			   state+= 1)
		switch(state)
		{
		case CreateTarget:	if (!createTgt(msvc,type,arch)) state+= Error; break;
		case CopySource:	if (!copySource	 ()				 ) state+= Error; break;
		case Prepare:		if (!prepare	 (msvc,type,arch)) state+= Error; break;
		case ConfClean:		if (!confClean	 ()				 ) state+= Error; break;
		case Configure:		if (!configure	 (msvc,type)	 ) state+= Error; break;
		case Compiling:		if (!compiling	 ()				 ) state+= Error; break;
		case Cleaning:		if (!cleaning	 ()				 ) state+= Error; break;
		case Finalize:		if (!finalize	 ()				 ) state+= Error; break;
		case CopyTarget:	if (!copyTarget	 ()				 ) state+= Error; break;
		default:															 continue;
		}

		m[msvc]=m[arch]=m[type]=false;
		if	(state > Finished)
			 goto end;
		else state = Started;
	}

	end:
	emit current(m);
	if (!removeTemp())
		state = ErrRemoveTemp;

	QMutexLocker l(&mutex); // ... avoid watcher "finished" during close event signal reconnection!
}

bool QtCompile::createTemp()
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

	m_btemp = m_drive+SLASH+qtBuildTemp;
	m_build = m_drive+SLASH+qtBuildMain;

	if (!QDir(m_btemp).exists() && !QDir().mkpath(m_btemp))
	{
		log("Couldn't create temp folder:", QDir::toNativeSeparators(m_btemp), Warning);
	}
	if (!QDir(m_build).exists() && !QDir().mkpath(m_build))
	{
		log("Couldn't create build folder:", QDir::toNativeSeparators(m_build), Critical);
		return false;
	}

	QFile(logFile(m_build)).remove();
	clearPath(m_build+"/lib");

	emit tempDrive(m_build);
	return true;
}

bool QtCompile::removeTemp()
{
	if (m_keepDisk)
		return true;

	log("Build step", "Removing temp infrastructure ...", AppInfo);
	return removeImdisk(false, false);
}

bool QtCompile::createTgt(int msvc, int type, int arch)
{
	QStringList tp;
	QString bldNat = QDir::toNativeSeparators(m_build);
	QString tgtNat, error;
	m_target = targetDir(msvc, arch, type, tgtNat, tp);

	log("Build step", "Creating target folder ...", AppInfo);
	bool exists = false;

	if (qtBuilderConfigOnly)
		return true;

	tp.removeLast();
	QString mp = tp.join(SLASH);

	QFileInfo dir(m_target);
	if ((exists = dir.symLinkTarget() == m_build))
	{
		log("Target folder already mounted:", QString("%1 -> %2").arg(bldNat, tgtNat), Warning);
	}
	else if (dir.exists())
	{
		log("Clearing target folder:", tgtNat);
		bool result = false;
		diskOp(Target, true);

		if ((result = removeDir(m_target)))
			 log("Target directory removed:",  tgtNat);
		else log("Couldn't clear target dir:", tgtNat, Critical);

		diskOp();
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
	return true;
}

bool QtCompile::copySource()
{
	log("Build step", "Copying source files ...", AppInfo);
	QString native = QDir::toNativeSeparators(m_build);

	m_dirFilter = sfilter;
	m_extFilter = ffilter;

	log("Copying contents to:", native);

	int  count;
	if(!(count = copyFolder(Source, Build)))
		 log("Couldn't copy contents to:", native, Critical);

	log("Total files copied:", QString::number(count));
	return count;
}

bool QtCompile::copyTarget()
{
	log("Build step", "Copying target files ...", AppInfo);
	QString native = QDir::toNativeSeparators(m_target);

	if (qtBuilderConfigOnly)
		return true;

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
	InlineProcess(this, "compact", QString("/c /i %1").arg(native));

	m_dirFilter = sfilter+tfilter;
	m_extFilter	= cfilter;

	log("Copying contents to:", native);

	int  count;
	if(!(count = copyFolder(Build, Target, false, true)))
		 log("Couldn't copy contents to:", native, Critical);

	QFile(logFile(m_build)).copy(logFile(m_target));
	removeDir(m_target+"/%SystemDrive%");

	log("Total files copied:", QString::number(++count));
	return count;
}

bool QtCompile::prepare(int msvc, int type, int arch)
{
	if (!checkDir(Source))
		return false;

	if (!checkDir(Target))
		return false;

	QString makeSpec = qMakeS.at(msvc);
	QString text = QString("<b>Version %1 ... %2 ... %3 bits ... %4</b>");
	text = text.arg(m_version, type==Shared?"shared":"static", arch==X86 ?"32":"64", makeSpec);

	log("Building Qt", text.toUpper(), Elevated);
	restart();

	if (msBuildTool.contains("jom", Qt::CaseInsensitive))
	{	//
		//	copy jom executable to build root path (from /bin);
		//	(both simplify calling it, and check for existence)
		//
		QString b = m_source+"/bin";
		QFile bld(b+SLASH+msBuildTool);
		if ( !bld.exists() )
		{
			log(QString("Missing %1 from:").arg(msBuildTool), QDir::toNativeSeparators(b), Critical);
			return false;
		}
		else
		{
			b = m_build+SLASH+msBuildTool;
			bld.copy(b);
			bld.setFileName(b);
		}
	}

	QString vcVars = QDir::toNativeSeparators(QString("call \"%1\" %2")
			.arg(m_msvcBOpts.at(msvc)).arg(m_msvcBOpts.at(arch)));

	bool result = true;
	if(!(result = setEnvironment(vcVars, makeSpec)))
	{
		finalize();
		return false;
	}

	writeQtVars(m_target, vcVars, msvc);
	return true;
}

bool QtCompile::confClean()
{
	if (!QFileInfo(m_build+"/Makefile").exists())
		return true;

	log("Build step", QString("Running %1 confclean ...").arg(msBuildTool), AppInfo);

	BuildProcess proc(this, true);
	proc.setArgs("confclean");

	if	(  !proc.result()  )
			proc.start(msBuildTool);
	return	proc.result() || true;
	// ... seems to be better to not use confclean results, i.e. if the prev. build was messed up.
}

bool QtCompile::configure(int msvc, int type)
{
	log("Build step", QString("Running %1 ...").arg(qtConfigure), AppInfo);

	QStringList  o = globals+switches+features+plugins+exclude;
	checkOptions(o);

	QStringList  c;
	FOR_CONST_IT(m_confs)
		if (IT.value()) c.append(qtOpts.value(IT.key()));

	QString qtConfig =c.join("-and");
	qtConfig += QString(" %1 %2 %3").arg(qtOpts.at(msvc), qtOpts.at(type), o.join(" "));

	if (false)
	{	BuildProcess proc(this);
		proc.setArgs(QString("127.0.0.1 -n %1").arg(m_bopts.value(RamDisk)));
		proc.start("ping.exe");
		return proc.result();
	}

	BuildProcess proc(this);
	proc.setArgs(qtConfig);
	proc.start(qtConfigure);
	return proc.result();
}

bool QtCompile::compiling()
{
	if (qtBuilderConfigOnly)
		return true;

	log("Build step", QString("Running %1 ...").arg(msBuildTool), AppInfo);

	QString args;
	if (msBuildTool.contains("jom", Qt::CaseInsensitive))
		args = QString("/J %1 ").arg(m_bopts.value(Cores));

	if(!qtBuilderUseTargets)
	{
		BuildProcess proc(this);
		proc.setArgs(args);
		proc.start(msBuildTool);
		return proc.result();
	}
	//
	// TODO: this needs to go into the config sections, as it is of course connected
	// with the pre-defined configure options (whatever they are good for anyway)!!!
	//
	FOR_CONST_IT(targets)
	{
		if (state == Compiling)
		{	BuildProcess proc(this);
			proc.setArgs(args+ *IT);
			proc.start(msBuildTool);
		if(!proc.result())
				 return false;
		}	else return true;
	}	// note: if state was set to cancelled during processing, the local result is still "true" (since there was no process error!)
	return true;
}

bool QtCompile::cleaning()
{
return true;
// TODO: check if "clean" is actually needed; since the build folder is now synchronised
// it would be only needed, if the target copy still contains garbage...
//
	log("Build step", QString("Running %1 clean ...").arg(msBuildTool), AppInfo);

	BuildProcess proc(this, true);
	proc.setArgs("clean");
	proc.start(msBuildTool);

	if (!proc.result())
	{	 proc.sendStdErr();
		return false;
	}	return true;
}

bool QtCompile::finalize()
{
	QLocale l(QLocale::English);
	QString mbts = l.toString(qobject_cast<QtBuilder *>(parent())->maxDiskSpace());
	QString text = QString("<b>Time ... %1:%2 minutes ... %3 MB max. used temp disk space</b>");
	uint secs = elapsed()/1000;
	uint mins = qFloor(secs/60);
	text = text.arg(mins).arg(secs%60,2,10,FILLNUL).arg(mbts);

	log("Qt build done", text.toUpper(), Elevated);

	QFile(m_target+SLASH+msBuildTool).remove();
	return true;
}

void QtCompile::checkOptions(QStringList &opts)
{
	QString options;
	{	BuildProcess proc(this, true);
		proc.setArgs("-help");
		proc.start(qtConfigure);

		proc.result();
		options = proc.stdOut();
	}
	if (options.isEmpty())
		return;

	options.append("-confirm-license");
	// ... at least for 4.x this is not contained in the help output!

	QStringList o;
	QString opt;

	auto  it  = opts.begin();
	while(it != opts.end())
	{
		opt = *it;
		if (opt.startsWith("-nomake")) // apparently "-nomake X" options are not validated by configure (and not contained in the help anyway)!
		{
			++it;
			continue;
		}

		o = opt.split("-");
		QString test;

		int count = qMin(o.count(), 2); // several options are variable, but at least the 1st 2 parts of the option string is static (is it??)
		for(int i = 0; i < count; i++)
			test += o.at(i);

		if (!options.contains(test, Qt::CaseInsensitive))
		{
			it = opts.erase(it);
			log("Unknown option removed:", opt, Warning);
		}
		else ++it;
	}
}

bool QtCompile::setEnvironment(const QString &vcVars, const QString &mkSpec)
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
			   if (!(*JT).contains(m_msvcBOpts.at(MSVC2010), Qt::CaseInsensitive) &&
				   !(*JT).contains(m_msvcBOpts.at(MSVC2012), Qt::CaseInsensitive) &&
				   !(*JT).contains(m_msvcBOpts.at(MSVC2013), Qt::CaseInsensitive) &&
				   !(*JT).contains(m_msvcBOpts.at(MSVC2015), Qt::CaseInsensitive))
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

void QtCompile::writeQtVars(const QString &path, const QString &vcVars, int msvc)
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

	writeTextFile(path+qtVarScript, var);
}

bool QtCompile::writeTextFile(const QString &filePath, const QString &text)
{
	QString native = QDir::toNativeSeparators(filePath);
	QFile file(filePath);
	if ( !file.open(QIODevice::WriteOnly) ||
		 !file.write(text.toUtf8().constData()))
	{
		log("Couldn't write file", native, Elevated);
		return false;
	}
	else
	{
		log("File created", native);
		return true;
	}
}

const QString QtCompile::logFile(const QString &path) const
{
	return path+SLASH+QCoreApplication::applicationName()+".log";
}
