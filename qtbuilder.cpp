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

Q_REGISTER_METATYPE(Modes)

class QtBuildSettings
{
public:
	QtBuildSettings()
	{
		settings = Q_SET_GET(SETTINGS_BUILDOPT).toStringList();
	}
	void set(Modes &modes, int opt, bool def)
	{
		modes.insert(opt, settings.contains(QString::number(opt)) || def);
	}
private:
	QStringList settings;
};

void QtBuilder::setupDefaults()
{
	int cores = QThread::idealThreadCount();
	typedef QtBuildState S;
	QtBuildSettings b;

	b.set(m_msvcs, MSVC2010,	false);
	b.set(m_msvcs, MSVC2012,	false);
	b.set(m_msvcs, MSVC2013,	true );
	b.set(m_msvcs, MSVC2015,	false);

	b.set(m_types, Shared,		true );
	b.set(m_types, Static,		true );

	b.set(m_archs, X86,			true );
	b.set(m_archs, X64,			true );

	b.set(m_confs, Debug,		true );
	b.set(m_confs, Release,		true );

	m_bopts.insert(S::RamDisk,	 4);
	m_bopts.insert(S::Cores,	 qMax(cores-1, 1));

	m_range.insert(S::RamDisk,	 Range(3, 10));
	m_range.insert(S::Cores,	 Range(1, cores));

	QSettings	s;
	m_version = s.value(SETTINGS_LVERSION, "4.8.7"				).toString();
	m_source  = s.value(SETTINGS_L_SOURCE, "C:/Qt/4.8.7"		).toString();
	m_libPath = s.value(SETTINGS_L_TARGET, "C:/Qt/4.8.7/builds"	).toString();

	m_source  = QDir::cleanPath(m_source);
	m_libPath = QDir::cleanPath(m_libPath);
}



QtBuilder::QtBuilder(QWidget *parent) : QMainWindow(parent),
	m_log(NULL), m_cpy(NULL), m_tgt(NULL), m_tmp(NULL)
{
	m_qtc = new QtCompile(this);

	m_opts.append(&m_confs);
	m_opts.append(&m_archs);
	m_opts.append(&m_types);
	m_opts.append(&m_msvcs);

	setWindowIcon(QIcon(":/graphics/icon.png"));
	setWindowTitle(qApp->applicationName());

	setupDefaults();
	createUi();

	connect(m_qtc,	 SIGNAL(diskOp(int, bool, int)), this, SLOT(diskOp(int,bool,int)),	Qt::BlockingQueuedConnection);
	connect(m_qtc,	 SIGNAL(current(const Modes &)), this, SLOT(nextBuild()),			Qt::BlockingQueuedConnection);
	connect(&m_loop, SIGNAL(finished()),			 this, SLOT(processed()));
}

QtBuilder::~QtBuilder()
{
}

void QtBuilder::closeEvent(QCloseEvent *event)
{
	event->setAccepted(false);

	if (m_qtc->working())
	{
		QMutexLocker l(&m_qtc->mutex); // ... avoid watcher "finished" during signal reconnection!

		disconnect(&m_loop, 0,					this, 0);
		   connect(&m_loop, SIGNAL(finished()), this, SLOT(end()));

		 cancel();
		disable();
	}
	else CALL_QUEUED(this, end);
}

void QtBuilder::end()
{
	QSettings().setValue(SETTINGS_GEOMETRY, saveGeometry());
	qApp->setProperty("result", (int)m_qtc->state);

	hide();
	qApp->quit();
}

void QtBuilder::show()
{
	QSettings  s;
	QByteArray g = s.value(SETTINGS_GEOMETRY).toByteArray();
	if (!g.isEmpty())
		 restoreGeometry(g);
	else setGeometry(centerRect(25));

	QMainWindow::show();

	m_log->addSeparator();
	m_log->add("QtBuilder started", AppInfo);

	if(QDir(m_libPath).exists())
		 m_tgt->setDrive(m_libPath);
	else m_log->add("Target path missing:", QDir::toNativeSeparators(m_libPath), Critical);
}

void QtBuilder::option(int opt, int value)
{
	m_bopts[opt] = value;
}

void QtBuilder::setup(int option)
{
	QStringList opts;
	FOR_IT(m_opts)
	{
		for(auto JT=(*IT)->begin();JT!=(*IT)->end();++JT)
		{	if ( JT.key() == option)
				*JT = option;
			if ( JT.value())
				opts.append(QString::number(JT.key()));
		}
	}
	Q_SET_SET(SETTINGS_BUILDOPT, opts);
}

void QtBuilder::disable(bool disable)
{
	if (disable)
		m_log->add("Shutting down ...", Warning);

	m_opt->setDisabled(disable);
	m_sel->disable(disable);
}

void QtBuilder::process(bool start)
{
	if (m_qtc->working())
	{
		 cancel();
		disable();
	}
	else if (start)
	{
		if (!QFileInfo(m_source+SLASH+qtConfigure).exists())
		{
			log("Qt sources path mismatch:", QDir::toNativeSeparators(m_source), Warning);
			m_go->setOff();
			return;
		}
		else if (!QDir(m_libPath).exists())
		{
			log("Build target path mismatch:", QDir::toNativeSeparators(m_target), Warning);
			m_go->setOff();
			return;
		}

		bool ok = true;
		ok &= !m_confs.keys(true).isEmpty();
		ok &= !m_types.keys(true).isEmpty();
		ok &= !m_archs.keys(true).isEmpty();
		ok &= !m_msvcs.keys(true).isEmpty();

		m_qtc->state = ok ? QtBuildState::Started : QtBuildState::NotStarted;
		if (ok)
		{
			m_qtc->sync();
			m_loop.setFuture(QtConcurrent::run(m_qtc, &QtCompile::loop));
			m_opt->setDisabled(true);
		}
		else m_go->setOff();
	}
}

void QtBuilder::processed()
{
	QString msg("QtBuilder ended with:");
	if (m_qtc->failed())
	{
		QString errn = PLCHD.arg(m_qtc->state,4,10,FILLNUL);
		QString text = QString("Error %2 (%3)\r\n").arg(errn, m_qtc->lastState());

		m_bld->endFailure();
		m_log->add(msg, text.toUpper(), Elevated);
	}
	else if (m_qtc->cancelled())
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
	if	 (m_qtc->cancelled()) msg = "<b>Process forcefully cancelled!</b>";
	else if (m_qtc->failed()) msg = "<b>Process ended with errors!</b>";
	else					  msg = "<b>Process sucessfully completed.</b>";

	QString url("<br/><a href=\"file:///%1\">Open app log file</a>");
	msg +=  url.arg(QDir::toNativeSeparators(m_log->logFile()));

	if (m_qtc->failed())
	{
		QString url("<br/><a href=\"file:///%1\">Open last buil log</a>");
		msg +=  url.arg(QDir::toNativeSeparators(m_qtc->buildLogFile()));
	}

	QMessageBox b(this);
	b.setText(msg);
	b.exec();
}

void QtBuilder::diskOp(int to, bool start, int count)
{
	if (start)
	{
		switch(to)
		{
		case Build:  m_tgt->hide(); break;
		case Target: m_tmp->hide(); break;
		}

		m_cpy->setMaximum(count);
	}
	else
	{
		m_cpy->hide();
		m_tmp->show();
		m_tgt->show();
	}
}

void QtBuilder::procLog()
{
	CALL_QUEUED(m_tmp, refresh);
	if (QtProcess *p = qobject_cast<QtProcess *>(sender()))
		m_bld->append(QtAppLog::clean(p->stdOut()), p->workingDirectory());
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

void QtBuilder::nextBuild()
{
	m_bld->clear();
	m_tmp->reset();
}

void QtBuilder::setSourceDir(const QString &path, const QString &ver)
{	//
	// note: these should be - and usually are - verified!
	//
	QString native = QDir::toNativeSeparators(path);
	m_source = path;
	m_version = ver;
	m_log->add("Source path selected:", native, Elevated);

	QSettings().setValue(SETTINGS_L_SOURCE, native);
	QSettings().setValue(SETTINGS_LVERSION, m_version);
}

void QtBuilder::setTargetDir(const QString &path, const QString &ver)
{	//
	// note: this should be - and usually is - verified!
	//
	QString native = QDir::toNativeSeparators(path);
	m_libPath = path;
	m_tgt->setDrive(m_libPath);
	m_log->add("Target path selected:", native, Elevated);

	QSettings().setValue(SETTINGS_L_TARGET, native);
	Q_UNUSED(ver);
}

void QtBuilder::registerQtVersion()
{
	// TODO: make compiled Qt versions(s) available to Qt Creator auto detect...
}

void QtBuilder::log(const QString &msg, int type)
{
	log(msg, QString(), type);
}

void QtBuilder::log(const QString &msg, const QString &text, int type)
{
	m_log->add(msg, text, type);
}

int QtBuilder::maxDiskSpace() const
{
	return m_tmp ? m_tmp->maxUsed() : 0;
}



const QString QtBuildState::optName(int option) const
{
	return META_ENUM(Options).key(option);
}

const QString QtBuildState::lastState() const
{
	return META_ENUM(States).key(state);
}
