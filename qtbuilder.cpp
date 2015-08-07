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
#include <QCheckBox>
#include <QLabel>
#include <QCloseEvent>
#include <QtConcurrentRun>
#include <QMessageBox>
#include <QDirIterator>
#include <QDateTime>
#include <QQueue>

const int		qtBuilderRamDiskInGB =  3;		// ... ram disk size in gigabyte - needs imdisk installed: http://www.ltr-data.se/opencode.html/#ImDisk
const QString	qtBuilderStaticDrive = "";//Y";	// ... use an existing drive letter; the ram disk part is skipped when set to anything else but ""; left-over build garbage will not get removed!

void QtBuilder::setup() /// primary setup for the current build run!!!
{
	m_version = "4.8.7";						// ... source version for the current run; needs a valid install of an according msvc qt setup - stays unmodified!!!
	m_libPath = "E:/WORK/PROG/LIBS/Qt";			// ... target path for creating the misc libs subdirs; should be of course different from the qt install location!!!

	m_msvcs << MSVC2013;						// << MSVC2010 << MSVC2012 << MSVC2013 << MSVC2015;
	m_archs << X86<<X64;						// << X86 << X64;
	m_types << Shared;				// << Shared << Static;
}



QtBuilder::QtBuilder(QWidget *parent) : QMainWindow(parent),
	m_log(NULL), m_cpy(NULL), m_tgt(NULL), m_tmp(NULL), m_state(Start),
	m_cancelled(false), m_result(true), m_imdiskUnit(imdiskUnit), m_keepDisk(false)
{
	setWindowTitle(qApp->applicationName());
	createUi();
}

QtBuilder::~QtBuilder()
{
}

void QtBuilder::closeEvent(QCloseEvent *event)
{
	m_cancelled   = true;
	if ( m_state != Finished )
		 m_state  = Finished;
	else m_cancelled = false;
	if ( m_cancelled )
		disconnect(&m_loop, 0, this,0);

	event->setAccepted(false);
	CALL_QUEUED(this, end);

	if (m_cancelled)
		emit cancelled();
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
		m_log->add("Process message", QtAppLog::clean(p->stdErr(), true), Elevated);
}

void QtBuilder::procOutput()
{
	if (QtProcess *p = qobject_cast<QtProcess *>(sender()))
		m_log->add("Process informal", QtAppLog::clean(p->stdOut(), true), Informal);
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
	t.append(paths[type]);
	t.append(paths[arch]);
	t.append(paths[msvc]);

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

void QtBuilder::exec()
{
	QSettings  s;
	QByteArray g = s.value(Q_SETTINGS_GEOMETRY).toByteArray();
	if (!g.isEmpty())
		 restoreGeometry(g);
	else setGeometry(centerRect(25));

	setup();
	show();

	m_log->addSeparator();
	m_log->add("QtBuilder started", AppInfo);

	if (!(m_result = checkSource()))
	{
		CALL_QUEUED(this, end);
		return;
	}

	connect(&m_loop, SIGNAL(finished()), this, SLOT(close()));
	m_loop.setFuture(QtConcurrent::run(this, &QtBuilder::loop));
}

void QtBuilder::end()
{
	bool error = !m_result || m_cancelled;
	m_log->add("QtBuilder ended with", QString(error?"%1":"no %1").arg("errors\r\n").toUpper(), AppInfo);

	m_cpy->hide();
	m_tgt->show();
	m_tmp->show();

	if (error)	m_bld->endFailure();
	else		m_bld->endSuccess();

	QSettings().setValue(Q_SETTINGS_GEOMETRY, saveGeometry());
	qApp->setProperty("result", error);
	endMessage();

	if (m_cancelled)
	{
		setDisabled(true);
		m_log->add("Shutting down ...", Warning);
	}
	else
	{
		hide();
		qApp->quit();
	}
}

void QtBuilder::endMessage()
{
	QString msg = !m_result ? m_cancelled ?
		"<b>Process forcefully cancelled!</b>"	:
		"<b>Process ended with errors!</b>"		:
		"<b>Process sucessfully completed.</b>" ;

	QString url("<br/><a href=\"file:///%1\">Open log file</a>");
	msg +=  url.arg(QDir::toNativeSeparators(m_log->logFile()));

	QMessageBox b(this);
	b.setText(msg);
	b.exec();
}

void QtBuilder::createUi()
{
	QWidget *wgt = new QWidget(this);
	setCentralWidget(wgt);

	QVBoxLayout *vlt = new QVBoxLayout(wgt);
	vlt->setContentsMargins(0, 0, 0, 0);
	vlt->setSpacing(0);

	QBoxLayout *lyt = vlt;
	createAddons(lyt);

	{	m_log = new QtAppLog(wgt);
		lyt->addWidget(m_log);

		connect(this, SIGNAL(log(const QString &, const QString &, int)), m_log, SLOT(add(const QString &, const QString &, int)),	Qt::BlockingQueuedConnection);
		connect(this, SIGNAL(log(const QString &, int)),				  m_log, SLOT(add(const QString &, int)),					Qt::BlockingQueuedConnection);
	}
	{	m_bld = new BuildLog(wgt);
		vlt->addWidget(m_bld);
	}
	{	m_cpy = new CopyProgress(wgt);
		vlt->addWidget(m_cpy);

		connect(this, SIGNAL(progress(int, const QString &, qreal)), m_cpy, SLOT(progress(int, const QString &, qreal)), Qt::QueuedConnection);
	}
	{	m_tmp = new DiskSpaceBar("Build ", Informal, wgt);
		vlt->addWidget(m_tmp);

		connect(this, SIGNAL(progress(int, const QString &, qreal)), m_tmp, SLOT(refresh()), Qt::QueuedConnection);
	}
	{	m_tgt = new DiskSpaceBar("Target", Elevated, wgt);
		vlt->addWidget(m_tgt);

		connect(this, SIGNAL(progress(int, const QString &, qreal)), m_tgt, SLOT(refresh()), Qt::QueuedConnection);
	}
	vlt->setStretch(0, 2);
	vlt->setStretch(1, 3);
	vlt->setStretch(3, 0);
	vlt->setStretch(4, 0);
	vlt->setStretch(5, 0);
}

bool qtBuilderParseOption(const QString &opt, QString &name, bool &isOn)
{
	name = opt;
	isOn = true;

	if (name.startsWith("-nomake"))
	{
		isOn = false;
		name.remove("-nomake");
	}
	else if (name.startsWith("-no"))
	{
		isOn = false;
		name.remove("-no");
	}
	if (!name.isEmpty())
	{
		name.replace("-"," ").trimmed();
		name.prepend(" ");

		int spc = 0;
		while((spc = name.indexOf(" ", spc)) != -1)
			name[++spc] = name[spc].toUpper();

		 return true;
	}
	else return false;
}

const QString qtBuilderHeaderStyle("QLabel { background: %1; color: white;} QLabel:disabled { background: #363636; color: #969696; border-right: 1px solid #565656; }");

void QtBuilder::createAddons(QBoxLayout *&lyt)
{
	QFont f(font());
	f.setPointSize(11);

	QBoxLayout *hlt = new QHBoxLayout();
	hlt->setContentsMargins(0, 0, 0, 0);
	hlt->setSpacing(0);
	lyt->addLayout(hlt);

	QWidget *wgt = new QWidget(lyt->parentWidget());
	wgt->setStyleSheet("QWidget:disabled { border-right: 1px solid #D8D8D8; }");
	hlt->addWidget(wgt);

	QVBoxLayout *vlt = new QVBoxLayout(wgt);
	vlt->setContentsMargins(0, 0, 0, 0);
	vlt->setSpacing(0);

	QLabel *lbl = new QLabel("OPTIONS", wgt);
	lbl->setStyleSheet(qtBuilderHeaderStyle.arg(colors.at(Warning)));
	lbl->setAlignment(Qt::AlignHCenter|Qt::AlignVCenter);
	lbl->setFixedHeight(defGuiHeight);
	lbl->setFont(f);
	vlt->addWidget(lbl);

	lyt = new QVBoxLayout();
	lyt->setContentsMargins(9, 6, 9, 9);
	lyt->setSpacing(3);
	vlt->addLayout(lyt);

	QList<QStringList> l;
	l.append(exclude);
	l.append(plugins);

	FOR_CONST_IT(l)
	{
		if (IT != l.constBegin())
		{
			QFrame *lne = new QFrame(wgt);
			lne->setStyleSheet("QFrame { border-top: 1px solid #A2A2A2; } QFrame:disabled { border-top: 1px solid #CDCDCD;}");
			lne->setFrameShape(QFrame::NoFrame);
			lne->setFixedHeight(1);
			lyt->addSpacing(6);
			lyt->addWidget(lne);
			lyt->addSpacing(6);
		}

		QMap<QString, bool> map;
		QString name;

		FOR_CONST_JT((*IT))
		{
			bool isOn;
			if (qtBuilderParseOption(*JT, name, isOn))
				map.insert(name, isOn);
		}

		QCheckBox *chb;
		FOR_CONST_JT(map)
		{
			chb = new QCheckBox(JT.key(), wgt);
			chb->setStyleSheet("QWidget { border: none; }");
			chb->setChecked(JT.value());
			chb->setDisabled(true);
			chb->setFont(f);
			lyt->addWidget(chb);
		}
	}
	vlt->addItem(new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding));

	lyt = new QVBoxLayout();
	lyt->setContentsMargins(0, 0, 0, 0);
	lyt->setSpacing(0);
	hlt->addLayout(lyt);

	lbl = new QLabel("BUILD PROGRESS LOG", lyt->parentWidget());
	lbl->setStyleSheet(qtBuilderHeaderStyle.arg(colors.at(AppInfo)));
	lbl->setAlignment(Qt::AlignHCenter|Qt::AlignVCenter);
	lbl->setFixedHeight(defGuiHeight);
	lbl->setFont(f);
	lyt->addWidget(lbl);
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
		log("Couldn't find Qt install from:", version+QString(": REG_SZ\"%1\"").arg(instDir), Elevated);
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
		 result = Informal;
	else log(QString("%1 removed").arg(name), QDir::toNativeSeparators(path), result);
	return result != Critical;
}

// ~~thread safe (no mutexes) ...
bool QtBuilder::checkDir(int which, int &count, bool skipRootFiles)
{
	count = 0;
	int dummy;

	QString path, name;
	if (!checkDir(which, dummy, path, name))
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
int QtBuilder::copyFolder(const QString &source, const QString &target, int count, bool skipRootFiles)
{
	QDir s(source);
	if (!s.exists())
		return 0;

	CALL_QUEUED(m_cpy, setMaximum, (int, count));
	CALL_QUEUED(m_cpy, show);

	QQueue<QPair<QDir, QDir> > queue;
	queue.enqueue(qMakePair(s, QDir(target)));
	bool filter =!m_extFilter.isEmpty();

	QFileInfoList sdirs, files;
	QPair<QDir, QDir>  pair;
	QString tgt, destd, nat;
	QDir srcDir, desDir;

	qreal mb = 0;
	count = 0;
	start();

	while(!queue.isEmpty())
	{
		if (m_cancelled)
			return 0;

		pair = queue.dequeue();
		srcDir = pair.first;
		desDir = pair.second;

		if (filterDir(srcDir.absolutePath()))
			continue;

		if(!desDir.exists() && !QDir().mkpath(desDir.absolutePath()))
			return 0;

		destd = desDir.absolutePath()+SLASH;
		files = srcDir.entryInfoList(QDir::Files);
		QFileInfo src, des;

		if (skipRootFiles)
			skipRootFiles = false;

		else FOR_CONST_IT(files)
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
				count++;
				des =  QFileInfo(tgt);
				mb +=src.size()/MBYTE;
				quint64 e = elapsed();

				if  (!((e)%150))
					emit progress(count, nat, mb*1000/e);
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
				log("Couldn't replace file", nat, Elevated);
				return 0;
			}
			else if (!QFile::copy(src.absoluteFilePath(), tgt))
			{
				log("Couldn't copy file", nat, Elevated);
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

	CALL_QUEUED(m_cpy, hide);
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
