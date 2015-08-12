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
#include <QElapsedTimer>
#include <QProgressDialog>
#include <QProgressBar>
#include <QTextBrowser>
#include <QTextEdit>
#include <QCheckBox>
#include <QSlider>
#include <QLabel>
#include <QMenu>
#include <QBoxLayout>
#include <QPushButton>
#include <QProxyStyle>
#include <QSignalMapper>
#include <QFutureWatcher>
#include <QSettings>
#include <QProcess>
#include <QPaintEvent>
#include <QFileInfo>
#include <QDir>

struct Range
{
	Range()
	{
		minimum = 0;
		maximum = 0;
	}
	Range(int min, int max)
	{
		minimum = min;
		maximum = max;
	}
	Range(const Range &other)
	{
		minimum = other.minimum;
		maximum = other.maximum;
	}
	Range operator=(const Range &other)
	{
		minimum = other.minimum;
		maximum = other.maximum;
		return *this;
	}
	bool operator==(const Range &other)
	{
		return	minimum == other.minimum &&
				maximum == other.maximum;
	}
	int minimum;
	int maximum;
};
typedef QMap<int, Range> Ranges;
typedef QMap<int, bool>  Modes;
Q_DECLARE_METATYPE(Modes)

class QtAppLog : public QTextBrowser
{
	Q_OBJECT

public:
	explicit QtAppLog(QWidget *parent = 0);

	void addSeparator();
	const QString logFile();
	static const QString clean(QString text, bool extended = false);

public slots:
	void add(const QString &msg, const QString &text, int type);
	void add(const QString &msg, int type = Informal);

protected:
	void paintEvent(QPaintEvent *event);

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

class QtProgress : public QProgressBar
{
	Q_OBJECT

public:
	explicit QtProgress(int color, QWidget *parent = 0);
};

class DiskSpaceBar : public QtProgress
{
	Q_OBJECT

public:
	explicit DiskSpaceBar(QWidget *parent, int color, const QString &name);
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

class CopyProgress : public QtProgress
{
	Q_OBJECT

public:
	explicit CopyProgress(QWidget *parent);

public slots:
	void setMaximum(int maximum);
	void progress(int count, const QString &file, qreal mbs);
};

class Selections : public QWidget, public QList<QList<QCheckBox *> >
{
	Q_OBJECT

signals:
	void selected(int);
	void checked(bool);

public:
	explicit Selections(const QString &label, int color, QWidget *parent);

	inline void setDisabled	(bool on) {  m_disabled  = on; }
	inline void setSorting	(bool on) {  m_sorting   = on; }
	inline void setAutoSplit(bool on) {  m_autoSplit = on; }
	inline void setTriState	(bool on) {  m_triState  = on; }

	void addModes(const Modes &modes, const QStringList &inf = QStringList());
	void addOpts(const QStringList &opts);
	void create();

public slots:
	void disable(bool disable);
	void activate(const Modes &modes);

protected slots:
	void toggled(const QString &name);

protected:
	QCheckBox *add(const QString &option, const QString &toolTip, bool disable);
	bool parseOption(const QString &opt, QString &label, Qt::CheckState &state);
	bool nextCharUppper(const QChar &c) const;
	void addSpacer();

private:
	QSignalMapper *m_mapper;
	QVBoxLayout *m_main;

	bool m_disabled;
	bool m_sorting;
	bool m_autoSplit;
	bool m_triState;
};

class QtHeader : public QLabel
{
	Q_OBJECT

public:
	explicit QtHeader(const QString &text, int color, QWidget *parent = 0);
};

class QtButton : public QtHeader
{
	Q_OBJECT

signals:
	void isOn(bool isOn);

public:
	explicit QtButton(const QString &off, const QString &on, QWidget *parent = 0, bool resetExternal = false);
	void setOff(bool off = true);

protected:
	void mouseReleaseEvent(QMouseEvent *event);

private:
	QStringList m_text;
	bool m_resetExtern;
	bool m_isOn;
};

class QtSlider : public QSlider
{
	Q_OBJECT

signals:
	void optionChanged(int option, int value);

public:
	explicit QtSlider(int option, int bgnd, int color, QWidget *parent);

	void setOrientation(Qt::Orientation o);
	QSize minimumSizehint() const;

protected:
	int  valueFromPosition(const QPoint &pos) const;
	bool isOnHandle(const QPoint &pos) const;
	bool mouseLocked() const { return m_clickSet || m_sliding; }

	void mouseMoveEvent(QMouseEvent *event);
	void mousePressEvent(QMouseEvent *event);
	void mouseReleaseEvent(QMouseEvent *event);
	void keyPressEvent(QKeyEvent *event);
	void keyReleaseEvent(QKeyEvent *event);

private:
	int  m_option;
	bool m_clickSet;
	bool m_sliding;
};

class SliderProxyStyle : public QProxyStyle
{
public:
	explicit SliderProxyStyle(QStyle *baseStyle);

	void drawComplexControl(ComplexControl ctrl, const QStyleOptionComplex *opt, QPainter *p, const QWidget *wgt = 0) const;
	QRect subControlRect(ComplexControl ctrl, const QStyleOptionComplex *opt, SubControl sub, const QWidget *wgt = 0) const;
	int pixelMetric(PixelMetric pMetric, const QStyleOption *opt = 0, const QWidget *wgt = 0) const;
};

class DirMenu : public QMenu
{
	Q_OBJECT

signals:
	void dirSelected(const QString &path, const QString &ver);

public:
	explicit DirMenu(const QString &path, int dir, int color, QWidget *parent);

public slots:
	void browseDisk();

protected slots:
	void action(QAction *act);

protected:
	bool predefined();

private:
	QString  m_last;
	const int m_dir;
};

class DirSelect : public QPushButton
{
	Q_OBJECT

signals:
	void dirSelected(const QString &path, const QString &ver);

public:
	explicit DirSelect(const QString &path, int dir, int color, QWidget *parent);

protected slots:
	void updateDir(const QString &path, const QString &ver = QString());

protected:
	void resizeEvent(QResizeEvent *event);
	void paintEvent(QPaintEvent *event);
	bool eventFilter(QObject *object, QEvent *event);

private:
	const int m_dir;
	bool m_isElided;
};

class QtAtomicInt : public QAtomicInt
{
public:
	explicit QtAtomicInt() { }
	void operator =(int value)
	{
		QAtomicInt::operator=(value);
	}
	void operator+=(int value)
	{
		QAtomicInt::operator=(*this+value);
	}
};

class QtBuilder;
class QtBuilderBase
{
public:
	enum Dirs { Source = 0x1001, Target, Build, Temp };

protected:
	Modes	m_confs;
	Modes	m_msvcs;
	Modes	m_archs;
	Modes	m_types;

	QString m_target;
	QString m_source;
	QString m_version;
	QString m_libPath;

	QMap<int, int>	m_bopts;
	QStringList m_msvcBOpts;
	QStringList m_dirFilter;
	QStringList m_extFilter;
};

class QtBuildState : public QObject, public QElapsedTimer
{
	Q_GADGET
	Q_ENUMS(States)
	Q_ENUMS(Options)

public:
	QtBuildState(QObject *parent) : QObject(parent) { state = NotStarted; }

	enum States
	{//	Processing...
		NotStarted		= -1,
		Started			=  0,
		CreateTarget	=  1,
		CopySource		=  2,
		Prepare			=  3,
		ConfClean		=  4,
		Configure		=  5,
		Compiling		=  6,
		Cleaning		=  7,
		Finalize		=  8,
		CopyTarget		=  9,
	 //	Results...
		Finished		= 10,
		Cancel			= 50,
		Cancelled		= 51,
		ErrCheckSource	= 61,
		ErrCheckTarget	= 62,
		ErrCreateTemp	= 71,
		ErrRemoveTemp	= 72,
		Error			= 80,
		ErrCreateTarget = 81,
		ErrCopySource	= 82,
		ErrPrepare		= 83,
		ErrConfClean	= 84,
		ErrConfigure	= 85,
		ErrCompiling	= 86,
		ErrCleaning		= 87,
		ErrFinalize		= 88,
		ErrCopyTarget	= 89,
	};
	enum Options { Cores, RamDisk, };

	QtAtomicInt state;
	inline bool ready()		  const { return state <  Started;   }
	inline bool failed()	  const { return state  > Cancelled; }
	inline bool working()	  const { return state <  Cancelled && state > Started; }
	inline bool cancelled()	  const { return state == Cancelled || state == Cancel; }

	const QString lastState() const;
	const QString optName(int option) const;
};

class QtCompile : public QtBuildState, public QtBuilderBase
{
	Q_OBJECT

signals:
	void current(const Modes &modes);
	void progress(int count, const QString &file, qreal mbs);
	void diskOp(int to = -1, bool start = false, int count = 0);
	void tempDrive(const QString &path);

	void log(const QString &msg, int type);
	void log(const QString &msg, const QString &text = QString(), int type = Informal);

public:
	explicit QtCompile(QtBuilder *main);

	inline const QProcessEnvironment &environment() const { return m_env ; }
	inline const QString buildLogFile() const { return  logFile(m_target); }
	inline const QString targetFolder() const { return			m_target ; }

	void  sync();
	void  loop();
	QMutex mutex;

protected slots:
	void cancel() { state = Cancel; }

protected:
	bool removeTemp();
	bool createTemp();
	bool createTgt (int msvc, int type, int arch);
	bool copyTarget();
	bool copySource();

	bool prepare  (int msvc, int type, int arch);
	bool confClean();
	bool configure(int msvc, int type);
	bool compiling();
	bool cleaning ();
	bool finalize ();

	void checkOptions(QStringList &opts);
	bool setEnvironment(const QString &vcVars, const QString &mkSpec);
	void writeQtVars(const QString &path, const QString &vcVars, int msvc);

	bool checkDir(int which);
	bool checkDir(int which, QString &path);
	bool checkDir(int which, QString &path, int &count, bool skipRootFiles = false);

	bool attachImdisk(QString &letter);
	bool removeImdisk(bool silent, bool force);
	bool writeTextFile(const QString &filePath, const QString &text);

	int copyFolder(int fr,  int to, bool synchronize = true, bool skipRootFiles = false);
	void clearPath(const QString &dirPath);
	bool removeDir(const QString &dirPath, const QStringList &inc = QStringList());
	bool filterDir(const QString &dirPath, bool isRoot);
	bool filterExt(const QFileInfo &info);
	bool filterPath(const QString &eLine);

	const QString logFile(const QString &path) const;
	const QString driveLetter();
	const QString targetDir(int msvc, int arch, int type, QString &native, QStringList &t = QStringList());

private:
	QProcessEnvironment	m_env;
	uint m_imdiskUnit;
	bool m_keepDisk;
	QString m_drive;
	QString m_build;
	QString m_btemp;
};

class QtBuilder : public QMainWindow, public QtBuilderBase
{
	Q_OBJECT

	friend class QtCompile;

signals:
	void cancel();

public:
	explicit QtBuilder(QWidget *parent = 0);
	virtual ~QtBuilder();

	void show();
	void setupDefaults();

	bool cancelled() const { return m_qtc->cancelled(); }
	int maxDiskSpace() const;

public slots:
	void procOutput();
	void procError();
	void procLog();

protected slots:
	void process(bool start);
	void processed();

	void end();
	void message();

	void setup(int option);
	void option(int opt, int value);
	void disable(bool disable = true);

	void setSourceDir(const QString &path, const QString &ver);
	void setTargetDir(const QString &path, const QString &ver);

	void nextBuild();
	void diskOp(int to, bool start, int count);

protected:
	void createUi();
	void createConfig(QBoxLayout *&lyt);
	void createCfgOpt(QBoxLayout * lyt);
	void createBldOpt(QBoxLayout * lyt);
	void createAppOpt(QBoxLayout * lyt);

	void endProcess();
	void checkVsInstalls();
	void registerQtVersion();

	void closeEvent(QCloseEvent *event);

	void log(const QString &msg, int type);
	void log(const QString &msg, const QString &text = QString(), int type = Informal);

private:
	QFutureWatcher<void> m_loop;
	QList<Modes *> m_opts;
	QtCompile *m_qtc;

	CopyProgress *m_cpy;
	DiskSpaceBar *m_tgt;
	DiskSpaceBar *m_tmp;
	Selections *m_sel;
	BuildLog *m_bld;
	QtAppLog *m_log;
	QWidget	 *m_opt;
	QtButton *m_go;
	Ranges m_range;
};

class QtProcess : public QProcess
{
	Q_OBJECT

public:
	explicit QtProcess(QObject *main, bool blockOutput = false);
	virtual ~QtProcess();

	void sendStdOut();
	void sendStdErr();

	const QString stdOut() { return readAllStandardOutput(); }
	const QString stdErr() { return readAllStandardError();  }

	inline bool normalExit() const { return m_cancelled || exitCode() ==  NormalExit; }

protected slots:
	void cancel() { m_cancelled = true; }
	void checkQuit();

protected:
	QtBuilder *m_bld;
	bool m_cancelled;
};

class InlineProcess : public QtProcess
{
public:
	explicit InlineProcess(QtCompile *compile, const QString &prog, const QString &args, bool blockOutput = false);
	virtual ~InlineProcess();

private:
	bool m_blockStdOutput;
};

class BuildProcess : public QtProcess
{
public:
	explicit BuildProcess(QtCompile *compile, bool blockOutput = false);
	virtual ~BuildProcess();

	void setArgs(const QString &args);
	void start(const QString &prog);
	bool result();
};

#endif // QTBUILDER_H
