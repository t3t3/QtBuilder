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

#include <QApplication>
#include <QMetaEnum>
#include <QCloseEvent>
#include <QtConcurrentRun>
#include <QMessageBox>
#include <QDirIterator>
#include <QDateTime>
#include <QQueue>

const int	  qtBuilderRamDiskInGB =  4;		// ... ram disk size in gigabyte - needs imdisk installed: http://www.ltr-data.se/opencode.html/#ImDisk
const QString qtBuilderStaticDrive = "";//Y";	// ... use an existing drive letter; the ram disk part is skipped when set to anything else but ""; left-over build garbage will not get removed!
const QStringList  qtBuilderQtDirs = QStringList() << "";	// ... override detection from registry!

void QtBuilder::setup() /// primary setup for the current build run!!!
{
	QMutexLocker locker(&m_mutex);

//	m_version = "4.8.7";						// ... source version for the current run; needs a valid install of an according msvc qt setup;
	m_version = "D:/BIZ/Qt/4.8.7_src";			// ... passing a full path to a src path instead will override detection from registry entries.
	m_libPath = "E:/WORK/PROG/LIBS/Qt";			// ... target path for creating the misc libs subdirs; different from the qt install location!!

	m_msvcs.insert(MSVC2010, false);			// ... default build requests...
	m_msvcs.insert(MSVC2012, false);
	m_msvcs.insert(MSVC2013, false);
	m_msvcs.insert(MSVC2015, false);
	m_types.insert(Shared,	 false);
	m_types.insert(Static,	 false);
	m_archs.insert(X86,		 false);
	m_archs.insert(X64,		 false);
}



QtBuilder::QtBuilder(QWidget *parent) : QMainWindow(parent),
	m_log(NULL), m_cpy(NULL), m_tgt(NULL), m_tmp(NULL), m_imdiskUnit(imdiskUnit), m_keepDisk(false)
{
	m_state = NotStarted;

	setWindowIcon(QIcon(":/graphics/icon.png"));
	setWindowTitle(qApp->applicationName());

	setup();
	createUi();

	connect(&m_loop, SIGNAL(finished()), this, SLOT(processed()));
}

QtBuilder::~QtBuilder()
{
}

void QtBuilder::closeEvent(QCloseEvent *event)
{
	event->setAccepted(false);

	if (working())
	{
		QMutexLocker locker(&m_mutex); // ... avoid watcher "finished" during signal reconnection!

		disconnect(&m_loop, 0,					this, 0);
		   connect(&m_loop, SIGNAL(finished()), this, SLOT(end()));

		 cancel();
		disable();

		m_log->add("Shutting down ...", Warning);
	}
	else CALL_QUEUED(this, end);
}

void QtBuilder::end()
{
	QSettings().setValue(Q_SETTINGS_GEOMETRY, saveGeometry());
	qApp->setProperty("result", (int)m_state);

	hide();
	qApp->quit();
}

void QtBuilder::show()
{
	QSettings  s;
	QByteArray g = s.value(Q_SETTINGS_GEOMETRY).toByteArray();
	if (!g.isEmpty())
		 restoreGeometry(g);
	else setGeometry(centerRect(25));

	QMainWindow::show();

	m_log->addSeparator();
	m_log->add("QtBuilder started", AppInfo);

	QString tdrive = m_libPath.left(3);
	if (!QDir(tdrive).exists())
	{
		log("Target drive doesn't exist:", QDir::toNativeSeparators(tdrive), Critical);
		disable();
		return;
	}
	if (!checkSource())
	{
		disable();
		return;
	}
	m_tgt->setDrive(tdrive);
}

void QtBuilder::setup(int option)
{
	switch(option)
	{
	case Shared:
	case Static:
		m_types[option] = !m_types[option];
		break;

	case X86:
	case X64:
		m_archs[option] = !m_archs[option];
		break;

	case MSVC2010:
	case MSVC2012:
	case MSVC2013:
	case MSVC2015:
		m_msvcs[option] = !m_msvcs[option];
		break;
	}
}

void QtBuilder::disable(bool disable)
{
	m_sel->setDisabled(disable);
}

void QtBuilder::cancel()
{
	m_state = Cancel;
	emit cancelling();
}

void QtBuilder::process(bool start)
{
	if (working())
	{
		 cancel();
		disable();
	}
	else if (start)
	{
		bool ok = true;
		ok &= !m_types.keys(true).isEmpty();
		ok &= !m_archs.keys(true).isEmpty();
		ok &= !m_msvcs.keys(true).isEmpty();

		m_state = ok ? Started : NotStarted;
		if (ok)
			 m_loop.setFuture(QtConcurrent::run(this, &QtBuilder::loop));
		else m_go->setOff();
	}
}

void QtBuilder::processed()
{
	QString msg("QtBuilder ended with:");
	if (failed())
	{
		QString errn = PLCHD.arg(m_state,4,10,FILLNUL);
		QString text = QString("Error %2 (%3)\r\n").arg(errn, lastState());

		m_bld->endFailure();
		m_log->add(msg, text.toUpper(), Elevated);
	}
	else if (cancelled())
	{
		m_bld->endFailure();
		m_log->add("QtBuilder was cancelled.", Warning);
	}
	else
	{
		m_bld->endSuccess();
		m_log->add(msg, QString("No Errors\r\n").toUpper(), AppInfo);
	}

	message();
	disable(false);
	m_go->setOff();
}

void QtBuilder::message()
{
	QString msg;
	if	 (cancelled())	msg = "<b>Process forcefully cancelled!</b>";
	else if (failed())	msg = "<b>Process ended with errors!</b>";
	else				msg = "<b>Process sucessfully completed.</b>";

	QString url("<br/><a href=\"file:///%1\">Open app log file</a>");
	msg +=  url.arg(QDir::toNativeSeparators(m_log->logFile()));

	if (failed())
	{
		QString url("<br/><a href=\"file:///%1\">Open last buil log</a>");
		msg +=  url.arg(QDir::toNativeSeparators(m_build+m_bld->logFile()));
	}

	QMessageBox b(this);
	b.setText(msg);
	b.exec();
}

void QtBuilder::procLog()
{
	CALL_QUEUED(m_tmp, refresh);
	if (QtProcess *p = qobject_cast<QtProcess *>(sender()))
		m_bld->append(QtAppLog::clean(p->stdOut()),m_build);
}

void QtBuilder::procError()
{
	if (QtProcess *p = qobject_cast<QtProcess *>(sender()))
		m_log->add("Process message", QtAppLog::clean(p->stdErr(), true), Process);
}

void QtBuilder::procOutput()
{
	if (QtProcess *p = qobject_cast<QtProcess *>(sender()))
		m_log->add("Process informal", QtAppLog::clean(p->stdOut(), true), Informal);
}

// ~~thread safe (no mutexes) ...
void QtBuilder::diskOp(int to, bool start, int count)
{
	if (start)
	{
		switch(to)
		{
		case Build:  CALL_QUEBLK(m_tgt, hide); break;
		case Target: CALL_QUEBLK(m_tmp, hide); break;
		}

		CALL_QUEBLK(m_cpy, setMaximum, (int, count));
	}
	else
	{
		CALL_QUEBLK(m_cpy, hide);
		CALL_QUEBLK(m_tmp, show);
		CALL_QUEBLK(m_tgt, show);
	}
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::checkSource()
{
	if (QDir(m_version).exists())
	{
		QStringList n;
		bool ok=false;  int v;
		foreach(const QChar &c, m_version)
			if (c.isDigit()&&(ok||(ok=(v=c.digitValue())>3||v<6)))
				n.append(c);

		m_source  = m_version;
		m_version = n.join(".");
		return true;
	}

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
				log("Qt install path not on disk:", QDir::toNativeSeparators(m_source), Critical);
				return false;
			}

			m_source = d.absolutePath();
			break;
		}
	}
	if (!result)
	{
		log("Couldn't find Qt install from:", version+QString(": REG_SZ\"%1\"").arg(instDir), Elevated);
		return false;
	}
	return true;
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::checkDir(int which)
{
	return checkDir(which, QString());
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::checkDir(int which, QString &path)
{
	int result = Critical;
	QString name;
	switch(which)
	{
	case Source:
		path = m_source;
		name = "Source directory";
		break;

	case Target:
		path = m_target;
		name = "Target directory";
		break;
	case Build:
		path = m_build;
		name = "Temporary drive";
		break;

	case Temp:
		path = m_btemp;
		name = "Temp directory";
		result = Warning;
		break;

	default:
		return false;
	}
	if (QDir(path).exists())
			result  = Informal;
	else	log(QString("%1 removed").arg(name), QDir::toNativeSeparators(path), result);
	return	result != Critical;
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::checkDir(int which, QString &path, int &count, bool skipRootFiles)
{
	count = 0;
	if (!checkDir(which, path))
		return false;

	QQueue<QDir> queue;
	queue.enqueue(QDir(path));
	bool filter =!m_extFilter.isEmpty();

	QFileInfoList files, dirs;
	QDir dir;

	while(!queue.isEmpty())
	{
		dir = queue.dequeue();
		if (filterDir(dir.absolutePath()))
			continue;

		files = dir.entryInfoList(QDir::Files);

		if (skipRootFiles)
			skipRootFiles = false;

		else FOR_CONST_IT(files)
			if (!filter || !filterExt(*IT))
				count++;

		dirs = dir.entryInfoList(QDir::AllDirs|QDir::NoDotAndDotDot);
		FOR_CONST_IT(dirs)
			queue.enqueue(QDir((*IT).absoluteFilePath()));
	}
	return count;
}

// ~~thread safe (no mutexes) ...
int QtBuilder::copyFolder(int fr, int to, bool synchronize, bool skipRootFiles)
{
	int count = 0;
	QString source, target;
	if (!checkDir(to, target) ||
		!checkDir(fr, source, count, skipRootFiles))
		return 0;

	diskOp(to, true, count);

	QQueue<QPair<QDir, QDir> > queue;
	queue.enqueue(qMakePair(QDir(source), QDir(target)));
	bool filter =!m_extFilter.isEmpty();

	QFileInfoList sinfo, dinfo;
	QPair<QDir, QDir>  pair;
	QString tgt, destd, nat, nme;
	QStringList compare;
	QDir srcDir, desDir;
	QFileInfo src, des;

	qreal mb = 0;
	start();

	while(!queue.isEmpty())
	{
		if (cancelled())
			goto end;

		pair = queue.dequeue();
		srcDir = pair.first;
		desDir = pair.second;

		if (filterDir(srcDir.absolutePath()))
		{
			if (synchronize && desDir.exists() &&
			   !removeDir(desDir.absolutePath()))
			{
				log("Synchronize faild; couldn't remove:",
					QDir::toNativeSeparators(desDir.absolutePath()), Elevated);
				goto error;
			}
			continue;
		}

		if(!desDir.exists() && !QDir().mkpath(desDir.absolutePath()))
			goto error;

		destd = desDir.absolutePath()+SLASH;
		sinfo = srcDir.entryInfoList(QDir::Files);

		if (skipRootFiles)
		{	skipRootFiles = false;
		}
		else FOR_CONST_IT(sinfo)
		{
			if (cancelled())
				goto end;

			src = *IT;
			nme = src.fileName();
			tgt = destd+nme;
			des = QFileInfo(tgt);
			nat = QDir::toNativeSeparators(tgt);

			if (filter && filterExt(src))
				continue;

			if (tgt.length() > 256)
			{
				log("Path length exceeded", nat, Elevated);
				goto error;
			}
			{	count++;
				mb +=src.size()/MBYTE;
				quint64 e = elapsed();

				if (e && !((e)%10))
					emit progress(count, nat, mb*1000/e);
			}

			bool exists;
			if ((exists  = des.exists()) &&
				des.size() == src.size() &&
				des.lastModified() == src.lastModified())
			{
				if (synchronize)
					compare.append(nme);
				continue;
			}
			else if (exists && !QFile(tgt).remove())
			{
				log("Couldn't replace old file:", nat, Elevated);
				goto error;
			}
			else if (!QFile::copy(src.absoluteFilePath(), tgt))
			{
				log("Couldn't copy new file:", nat, Elevated);
				goto error;
			}
			else if (synchronize)
			{
				compare.append(nme);
			}
		}

		if (synchronize)
		{
			dinfo = desDir.entryInfoList(QDir::Files);
			FOR_CONST_IT(dinfo)
			{
				if (cancelled())
					goto end;

				des = *IT;
				if (!compare.contains(des.fileName()) &&
					!QFile(des.absoluteFilePath()).remove())
				{
					log("Synchronize failed; couldn't remove:",
						QDir::toNativeSeparators(des.absoluteFilePath()), Elevated);
					goto error;
				}
			}
			compare.clear();
		}

		sinfo = srcDir.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot);
		destd = desDir.absolutePath()+SLASH;

		FOR_CONST_IT(sinfo)
		{
			if (cancelled())
				goto end;

			srcDir = (*IT).absoluteFilePath();
			nme = srcDir.dirName();
			queue.enqueue(qMakePair(srcDir, QDir(destd+nme)));

			if (synchronize)
				compare.append(nme);
		}

		if (synchronize)
		{
			dinfo = desDir.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot);
			FOR_CONST_IT(dinfo)
			{
				if (cancelled())
					goto end;

				des = *IT;
				if (!compare.contains(des.fileName()) &&
					!removeDir(des.absoluteFilePath()))
				{
					log("Synchronize failed; couldn't remove:",
						QDir::toNativeSeparators(des.absoluteFilePath()), Elevated);
					goto error;
				}
			}
		}
	}

	goto end;
	error:
	count=0;
	end:
	diskOp();
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
bool QtBuilder::attachImdisk(QString &letter)
{
	if (!qtBuilderStaticDrive.isEmpty())
	{
		letter = qtBuilderStaticDrive;
		m_keepDisk = true;
		return true;
	}

	QString out;
	{	InlineProcess p(this, "imdisk.exe", "-l -n", true);
		out = p.stdOut();
	}
	if (out.contains(QString::number(m_imdiskUnit)))
	{
		InlineProcess p(this, "imdisk.exe", QString("-l -u %1").arg(m_imdiskUnit), true);
		QString inf = p.stdOut();
		if (inf.contains(imdiskLetter))
		{	//
			// TODO: in this case it would also be needed to extend the disk if needed
			//
			letter	 = getValueFrom(inf, imdiskLetter, ___LF);
			int size = getValueFrom(inf, imdiskSizeSt, " ").toULongLong() /1024 /1024 /1024;
			log("Using existing RAM disk", QString("Drive letter %1, %2GB").arg(letter).arg(size));

			m_keepDisk = true;
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

	InlineProcess p(this, "imdisk.exe", args, false);
	return p.normalExit();
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::removeImdisk(bool silent, bool force)
{
	if (m_keepDisk)
		return true;

	if (!silent)
		log("Trying to remove RAM disk");

	QString args = QString("-%1 -u %2");
	args  = args.arg(force ? "D" : "d").arg(m_imdiskUnit);

	InlineProcess p(this, "imdisk.exe", args, silent);
	return p.normalExit();
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::writeTextFile(const QString &filePath, const QString &text)
{
	QString native = QDir::toNativeSeparators(filePath);
	QFile file(filePath);
	if ( !file.open(QIODevice::WriteOnly) ||
		 !file.write(text.toUtf8().constData()))
	{
		log("Couldn't write file", native, Elevated);
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
const QString qtBuilderCoreLib("corelib");
const QString qtBuilderHelpCnv("qhelpconverter");
bool QtBuilder::filterDir(const QString &dirPath)
{
	QString d = dirPath.toLower();
	if (d.contains(qtBuilderCoreLib) ||
		d.contains(qtBuilderHelpCnv))
		return false;

	QString p = dirPath;
	bool skip = false;
	FOR_CONST_IT(m_dirFilter)
		if ((skip = p.endsWith(*IT)))
			break;
	return	skip;
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::filterExt(const QFileInfo &info)
{
	return m_extFilter.contains(info.suffix().toLower());
}

// ~~thread safe (no mutexes) ...
const QString QtBuilder::targetDir(int msvc, int arch, int type, QString &native, QStringList &t)
{
	m_libPath = QDir::cleanPath(m_libPath);
	//
	//	USER_SET_LIB_PATH	...	something like				"D:\3d-Party\Qt"
	//	  \VERSION			... currently processed version	i.e. "4.8.7"
	//	    \TYPE			...	library type:				"shared" or "static"
	//	      \ARCHITECTURE	... MSVC $(Platform) compatible	"Win32" or "x64"
	//	        \MSVC_VER	...	MSVC $(PlatformToolset)		"v100" or "v110" or "v120" or "v130"
	//
	t.clear();
	t.append(m_libPath);
	t.append(m_version);
	t.append(bPaths[type]);
	t.append(bPaths[arch]);
	t.append(bPaths[msvc]);

	QString target = t.join(SLASH);
			native = QDir::toNativeSeparators(target);
	return	target;
}

// ~~thread safe (no mutexes) ...
const QString QtBuilder::driveLetter()
{
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

// ~~thread safe (no mutexes) ...
void QtBuilder::registerQtVersion()
{
	return; // a) not working, b) not useful in particular for having Qt Creator showing the new versions

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
		log("Couldn't register Qt in:", version+QString(": REG_SZ\"%1\"").arg(instDir), Elevated);
	}
}

const QString QtBuilder::lastState() const
{
	int index = metaObject()->indexOfEnumerator("States");
	QMetaEnum metaEnum = metaObject()->enumerator(index);
	return metaEnum.key(m_state);
}

Q_REGISTER_METATYPE(Modes)
