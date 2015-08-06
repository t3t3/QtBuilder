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
#ifndef QTBUILDER_H
#define QTBUILDER_H
#include "definitions.h"

#include <QMainWindow>
#include <QTextBrowser>
#include <QTextEdit>
#include <QProgressDialog>
#include <QProgressBar>
#include <QProcess>
#include <QSettings>
#include <QFutureWatcher>
#include <QFileInfo>
#include <QDir>

class AppLog : public QTextBrowser
{
	Q_OBJECT

public:
	explicit AppLog(QWidget *parent = 0);
	const QString logFile();

	static const QString clean(QString text, bool extended = false);

public slots:
	void add(const QString &msg, const QString &text, int type = Informal);
	void add(const QString &msg, int type = Informal);

private:
	QMap<int, QString> m_info;
	QStringList m_timestamp;
	QStringList m_message;
	QStringList m_description;
	QList<int>  m_type;
};

class BuildLog : public QTextEdit
{
	Q_OBJECT

public:
	explicit BuildLog(QWidget *parent = 0);

public slots:
	void append(const QString &text, const QString &path);
};

class Progress : public QProgressDialog
{
	Q_OBJECT

public:
	explicit Progress(QWidget *parent = 0);

public slots:
	void setMaximum(int max);
};

class BuildProcess;

class QtBuilder : public QMainWindow
{
	Q_OBJECT

signals:
	void log(const QString &msg, const QString &text = QString(), int type = Informal);
	void log(const QString &msg, int type);

	void copyProgress(int count, const QString &file);
	void newCopyCount(int count = -1);

	void buildFinished();

public:
	explicit QtBuilder(QWidget *parent = 0);
	virtual ~QtBuilder();

	void exec();
	void setup();
	bool result() { return m_result; }

public slots:
	void progress(int count, const QString &file);

protected slots:
	void end();
	void procError();
	void procOutput();

protected:
	void createUi();
	bool createTemp();
	bool createTarget();
	bool cleanTemp();
	bool clearTarget(int msvc, int arch, int type);
	bool copyTemp();
	bool copyTarget(bool removeBuiltLibs);
	bool removeTemp();
	bool checkSource();

	bool buildQt(int msvc, int arch, int type);
	bool setEnvironment(QProcessEnvironment &env, const QString &vcVars, const QString &mkSpec);
	void writeQtVars(const QString &path, const QString &vcVars, int msvc);
	bool writeTextFile(const QString &filePath, const QString &text);
	void registerQtVersion();

	bool checkDir(int which);
	bool checkDir(int which, int &result, QString &path, QString &name);
	bool checkDir(int which, int &count);

	bool attachImdisk(QString &letter);
	bool removeImdisk(bool silent, bool force);

	void loop();
	void endProcess();
	void closeEvent(QCloseEvent *event);

	int copyFolder(const QString &source,  const QString &target);
	void clearPath(const QString &dirPath);
	bool removeDir(const QString &dirPath, const QStringList &inc = QStringList());
	bool filterDir(const QString &dirPath);
	bool filterExt(const QFileInfo &info);

	const QString driveLetter();
	const QString targetDir(int msvc, int arch, int type, QString &native);
	bool qtSettings(QStringList &versions, QString &instDir);

	bool normalExit() { return m_prc && m_prc->exitCode() ==  QProcess::NormalExit; }
	QString stdOut()  { return m_prc  ? m_prc->readAllStandardOutput() : QString(); }
	QString stdErr()  { return m_prc  ? m_prc->readAllStandardError()  : QString(); }

private:
	enum Dirs { Source, Target, Build, Temp, };
	enum BuildSteps
	{
		Start		 = 0,
		ClearTarget	 = 1,
		CreateTarget = 2,
		CopyTemp	 = 3,
		CopySource	 = 4,
		BuildQt		 = 5,
		CopyTarget	 = 6,
		Finished	 = 7,
	};
	QFutureWatcher<void> m_loop;
	QProgressBar *m_spc;
	QProcess *m_prc;
	Progress *m_prg;
	BuildLog *m_jom;
	AppLog *m_log;
	QString m_btemp;
	QString m_build;
	QString m_target;
	QString m_source;
	QString m_version;
	QString m_libPath;

	QStringList m_dirFilter;
	QStringList m_extFilter;

	QList<int> m_msvcs;
	QList<int> m_archs;
	QList<int> m_types;

	uint m_imdiskUnit;
	uint m_diskSpace;

	volatile int  m_state;
	volatile bool m_result;
	volatile bool m_building;
	volatile bool m_cancelled;


	class InlineProcess
	{
	public:
		explicit InlineProcess(QtBuilder *builder, const QString &prog, const QString &args, bool blockOutput = false)
			: m_builder(builder), m_buildPrc(NULL), m_unblockOutput(false)
		{
			if (!m_builder || !(m_buildPrc = m_builder->m_prc))
				return;

			if (	blockOutput)
			{	m_unblockOutput = !m_buildPrc->signalsBlocked();
				m_buildPrc->blockSignals(true);
			}
			{	m_buildPrc->setNativeArguments(args);
				m_buildPrc->start(prog);
				m_buildPrc->waitForFinished(-1);
			}
		}
		virtual ~InlineProcess()
		{
			if (m_buildPrc)
				m_buildPrc->close();

			if (m_unblockOutput)
				m_buildPrc->blockSignals(false);
		}

	private:
		QtBuilder *m_builder;
		QProcess *m_buildPrc;
		bool m_unblockOutput;
	};
	class BuildProcess
	{
	public:
		explicit BuildProcess(QtBuilder *builder, const QProcessEnvironment &env, bool blockOutput = false)
			: m_builder(builder), m_buildPrc(NULL), m_unblockOutput(false), m_unsetBuilding(false)
		{
			if (!m_builder || !(m_buildPrc = m_builder->m_prc))
				return;

			if (!m_builder->m_building)
			{	 m_builder->m_building = true;
				 m_unsetBuilding = true;
			}
			if (	blockOutput)
			{	m_unblockOutput = !m_buildPrc->signalsBlocked();
				m_buildPrc->blockSignals(true);
			}
			{	m_buildPrc->setWorkingDirectory(m_builder->m_build);
				m_buildPrc->setNativeArguments(QString());
			}
			if(!env.isEmpty())
				m_buildPrc->setProcessEnvironment(env);
		}
		virtual ~BuildProcess()
		{
			if (m_buildPrc)
			{	m_buildPrc->close();

			if (m_unsetBuilding)
				m_builder->m_building = false;

			if (m_unblockOutput)
				m_buildPrc->blockSignals(false);
			}
		}
		inline void setArgs(const QString &args = QString())
		{
			if (m_buildPrc)
				m_buildPrc->setNativeArguments(args);
		}
		inline void start(const QString &prog)
		{
			if (m_builder)
				QDir::setCurrent(m_builder->m_build);

			if (m_buildPrc)
				m_buildPrc->start(prog);
		}
		inline bool result()
		{
			if (m_buildPrc)
			{	m_buildPrc->waitForFinished(-1);
			return
				m_builder->normalExit();
			}
			return false;
		}

	private:
		QtBuilder *m_builder;
		QProcess *m_buildPrc;
		bool m_unblockOutput;
		bool m_unsetBuilding;
	};
};


#endif // QTBUILDER_H
