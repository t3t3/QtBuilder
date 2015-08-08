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
#include <QBoxLayout>
#include <QElapsedTimer>
#include <QTextBrowser>
#include <QTextEdit>
#include <QProgressDialog>
#include <QProgressBar>
#include <QProcess>
#include <QSettings>
#include <QFutureWatcher>
#include <QFileInfo>
#include <QDir>

class QtAppLog : public QTextBrowser
{
	Q_OBJECT

public:
	explicit QtAppLog(QWidget *parent = 0);

	void addSeparator();
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
	QString		m_logFile;
	QList<int>  m_type;
};

class BuildLog : public QTextEdit
{
	Q_OBJECT

public:
	explicit BuildLog(QWidget *parent = 0);
	const QString logFile();

public slots:
	void append(const QString &text, const QString &path);

	void endFailure();
	void endSuccess();

private:
	QString m_logFile;
	int m_lineCount;
};

class Progress : public QProgressDialog
{
	Q_OBJECT

public:
	explicit Progress(QWidget *parent = 0);

public slots:
	void setMaximum(int max);
};

class DiskSpaceBar : public QProgressBar
{
	Q_OBJECT

public:
	DiskSpaceBar(const QString &name, int color, QWidget *parent);
	inline int maxUsed() const	{ return m_diskSpace; }

public slots:
	void setDrive(const QString &path);
	void reset() { m_diskSpace = 0; }
	void refresh();

protected:
	void showEvent(QShowEvent *event);

private:
	QString m_name;
	QString m_path;
	uint m_diskSpace;
};

class CopyProgress : public QProgressBar
{
	Q_OBJECT

public:
	CopyProgress(QWidget *parent);

public slots:
	void progress(int count, const QString &file, qreal mbs);
};

class QtBuilder : public QMainWindow, private QElapsedTimer
{
	Q_OBJECT

signals:
	void log(const QString &msg, const QString &text = QString(), int type = Informal);
	void log(const QString &msg, int type);

	void progress(int count, const QString &file, qreal mbs);
	void cancelled();

public:
	explicit QtBuilder(QWidget *parent = 0);
	virtual ~QtBuilder();

	void exec();
	void setup();

	inline const QProcessEnvironment &environment() const { return m_env; }
	inline const QString			&targetFolder() const { return m_target; }

public slots:
	void procOutput();
	void procError();
	void procLog();

protected slots:
	void end();
	void endMessage();

protected:
	void createUi();
	void createAddons(QBoxLayout *&lyt);

	bool createTarget(int msvc, int type, int arch);
	bool createTemp	 ();
	bool checkSource ();
	bool copySource	 ();
	bool copyTarget	 ();
	bool removeTemp	 ();

	bool prepare	 (int msvc, int type, int arch);
	bool confClean	 ();
	bool configure	 (int msvc, int type);
	bool jomBuild	 ();
	bool jomClean	 ();
	bool finished	 ();
	bool install	 ();

	bool setEnvironment(const QString &vcVars, const QString &mkSpec);
	void writeQtVars(const QString &path, const QString &vcVars, int msvc);
	bool writeTextFile(const QString &filePath, const QString &text);
	void registerQtVersion();

	bool checkDir(int which);
	bool checkDir(int which, QString &path);
	bool checkDir(int which, QString &path, int &count, bool skipRootFiles = false);

	void diskOp(int which = -1, bool start = false, int count = 0);

	bool attachImdisk(QString &letter);
	bool removeImdisk(bool silent, bool force);

	void loop();
	void endProcess();
	void closeEvent(QCloseEvent *event);

	int copyFolder(int fr,  int to, bool synchronize = true, bool skipRootFiles = false);
	void clearPath(const QString &dirPath);
	bool removeDir(const QString &dirPath, const QStringList &inc = QStringList());
	bool filterDir(const QString &dirPath);
	bool filterExt(const QFileInfo &info);

	const QString driveLetter();
	const QString targetDir(int msvc, int arch, int type, QString &native, QStringList &t = QStringList());
	bool qtSettings(QStringList &versions, QString &instDir);

private:
	enum Dirs { Source, Target, Build, Temp };
	enum BuildSteps
	{
		Start		 =  0,
		CreateTarget =  1,
		CreateTemp	 =  2,
		CopySource	 =  3,
		Prepare		 =  4,
		ConfClean	 =  5,
		Configure	 =  6,
		JomBuild	 =  7,
		JomClean	 =  8,
		CleanUp		 =  9,
		CopyTarget	 = 10,
		Finished	 = 11,
	};

	QFutureWatcher<void> m_loop;
	QProcessEnvironment	 m_env;
	CopyProgress *m_cpy;
	DiskSpaceBar *m_tgt;
	DiskSpaceBar *m_tmp;
	QtAppLog *m_log;
	BuildLog *m_bld;

	QString m_drive;
	QString m_build;
	QString m_btemp;
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
	bool m_keepDisk;

	volatile int  m_state;
	volatile bool m_result;
	volatile bool m_cancelled;
};

class QtProcess : public QProcess
{
	Q_OBJECT

public:
	explicit QtProcess(QtBuilder *builder, bool blockOutput = false);
	virtual ~QtProcess();

	void sendStdOut();
	void sendStdErr();

	const QString stdOut() { return readAllStandardOutput(); }
	const QString stdErr() { return readAllStandardError();  }

	inline bool normalExit() const { return !m_cancelled && exitCode() ==  NormalExit; }

public slots:
	void itStheEndOfTheWorldAsWeKnowIt();

private:
	QtBuilder *m_bld;
	bool m_cancelled;
};

class InlineProcess : public QtProcess
{
public:
	explicit InlineProcess(QtBuilder *builder, const QString &prog, const QString &args, bool blockOutput = false);
	virtual ~InlineProcess();

private:
	bool m_blockStdOutput;
};

class BuildProcess : public QtProcess
{
public:
	explicit BuildProcess(QtBuilder *builder, bool blockOutput = false);
	virtual ~BuildProcess();

	void setArgs(const QString &args);
	void start(const QString &prog);
	bool result();
};

#endif // QTBUILDER_H
