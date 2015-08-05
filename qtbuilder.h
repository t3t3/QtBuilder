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

	friend class BuildProcess;

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
	bool copyTarget();
	bool removeTemp();
	bool checkSource();

	bool buildQt(int msvc, int arch, int type);
	void registerQtVersion();
	bool setEnvironment(QProcessEnvironment &env, const QString &vcVars, const QString &mkSpec);
	void writeQtVars(const QString &path, const QString &vcVars, int msvc);
	bool writeTextFile(const QString &filePath, const QString &text);

	bool checkDir(int which);
	bool checkDir(int which, int &result, QString &path, QString &name);
	bool checkDir(int which, int &count);

	bool attachImDisk(const QString &letter);
	bool removeImDisk(bool silent, bool force);

	void loop();
	void closeEvent(QCloseEvent *event);

	int copyFolder(const QString &source,  const QString &target);
	void clearPath(const QString &dirPath);
	bool removeDir(const QString &dirPath, const QStringList &inc = QStringList());
	bool filterDir(const QString &dirPath);
	bool filterExt(const QFileInfo &info);

	const QString driveLetter();
	const QString targetDir(int msvc, int arch, int type, QString &native);
	bool qtSettings(QString &versions, QString &instDir);

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
	Progress *m_prg;
	QProcess *m_prc;
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

	uint m_diskSpace;

	volatile int  m_state;
	volatile bool m_result;
	volatile bool m_building;
	volatile bool m_cancelled;
};

class BuildProcess
{
public:
	explicit BuildProcess(QtBuilder *builder, const QProcessEnvironment &env, bool blockStdErrors = false)
		: m_builder(NULL)
	{
		if (  builder->m_prc &&
			! builder->m_building)
		{	  builder->m_building = true;
			m_builder = builder;

			if (builder->m_prc)
			{
			if (blockStdErrors)
				builder->m_prc->blockSignals(true);

				builder->m_prc->setWorkingDirectory(builder->m_build);
				builder->m_prc->setNativeArguments(QString());

			if(!env.isEmpty())
				builder->m_prc->setProcessEnvironment(env);
			}
		}
	}
	virtual ~BuildProcess()
	{
		if (m_builder)
		{	m_builder->m_building = false;

			if (m_builder->m_prc)
			{	m_builder->m_prc->blockSignals(false);
				m_builder->m_prc->close();;
			}
		}
	}
	inline void setArgs(const QString &args = QString())
	{
		if (m_builder)
			m_builder->m_prc->setNativeArguments(args);
	}
	inline void start(const QString &app)
	{
		QDir::setCurrent(m_builder->m_build);
		if (m_builder)
			m_builder->m_prc->start(app);
	}
	inline bool result()
	{
		if (m_builder)
		{	m_builder->m_prc->waitForFinished(-1);

			return
			m_builder->m_prc->exitStatus() == QProcess::NormalExit;
		}
		return false;
	}

private:
	QtBuilder *m_builder;
};

#endif // QTBUILDER_H
