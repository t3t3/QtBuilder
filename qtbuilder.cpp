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
#include <QMessageBox>
#include <QtConcurrentRun>

const QStringList  qtBuilderQtDirs = QStringList() << "";	// ... override detection from registry!

void QtBuilder::setup() /// defaults & some still hard-coded setup...
{
	QMutexLocker locker(&m_mutex);

	m_msvcs.insert(MSVC2010, false);	// ... default build requests...
	m_msvcs.insert(MSVC2012, false);
	m_msvcs.insert(MSVC2013, true );
	m_msvcs.insert(MSVC2015, false);

	m_types.insert(Shared,	 true );
	m_types.insert(Static,	 true );

	m_archs.insert(X86,		 true );
	m_archs.insert(X64,		 true );

	m_confs.insert(Debug,	 true );
	m_confs.insert(Release,	 true );

	m_bopts.insert(RamDisk,	 4);
	m_bopts.insert(Cores,	 qMax(QThread::idealThreadCount()-1, 1));

	m_version = "4.8.7";
	m_source  = "C:/Qt/4.8.7";
	m_libPath = "C:/Qt/4.8.7/builds";
}



QtBuilder::QtBuilder(QWidget *parent) : QMainWindow(parent),
	m_log(NULL), m_cpy(NULL), m_tgt(NULL), m_tmp(NULL), m_keepDisk(false), m_imdiskUnit(imdiskUnit)
{
	m_state = NotStarted;
	m_range.insert(Cores,	qMakePair(1, QThread::idealThreadCount()));
	m_range.insert(RamDisk, qMakePair(3, 10));

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
	if(QDir(tdrive).exists())
		 m_tgt->setDrive(tdrive);
	else m_log->add("Target drive doesn't exist:", QDir::toNativeSeparators(tdrive), Critical);
}

void QtBuilder::option(int opt, int value)
{
	m_bopts[opt] = value;
}

void QtBuilder::setup(int option)
{
	switch(option)
	{
	case Debug:
	case Release:
		m_confs[option] = !m_confs[option];
		break;

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
	m_log->add("Shutting down ...", Warning);
	m_opt->setDisabled(disable);
	m_sel->disable(disable);
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
		if (!QFileInfo(m_source+SLASH+qtConfigure).exists())
		{
			doLog("Qt sources path mismatch:", QDir::toNativeSeparators(m_source), Warning);
			m_go->setOff();
			return;
		}
		else if (!QDir(m_libPath).exists())
		{
			doLog("Build target path mismatch:", QDir::toNativeSeparators(m_target), Warning);
			m_go->setOff();
			return;
		}

		bool ok = true;
		ok &= !m_confs.keys(true).isEmpty();
		ok &= !m_types.keys(true).isEmpty();
		ok &= !m_archs.keys(true).isEmpty();
		ok &= !m_msvcs.keys(true).isEmpty();

		m_state = ok ? Started : NotStarted;
		if (ok)
		{
			 m_loop.setFuture(QtConcurrent::run(this, &QtBuilder::loop));
			 m_opt->setDisabled(true);
		}
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

void QtBuilder::sourceDir(const QString &path, const QString &ver)
{	//
	// note: these should be - and usually are - verified!
	//
	m_source = path;
	m_version = ver;
	m_log->add("Source path selected:", QDir::toNativeSeparators(m_source), Elevated);
}

void QtBuilder::tgtLibDir(const QString &path, const QString &ver)
{	//
	// note: this should be - and usually is - verified!
	//
	m_libPath = path;
	m_tgt->setDrive(path.left(3));
	m_log->add("Target path selected:", QDir::toNativeSeparators(m_libPath), Elevated);

	Q_UNUSED(ver);
}

#define META_ENUM(ID) (metaObject()->enumerator(metaObject()->indexOfEnumerator(#ID)))

const QString QtBuilder::enumName(int enumId) const
{
	return META_ENUM(Options).key(enumId);
}

const QString QtBuilder::lastState() const
{
	return META_ENUM(States).key(m_state);
}

Q_REGISTER_METATYPE(Modes)
