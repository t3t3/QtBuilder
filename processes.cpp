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
#ifdef _WIN32
#include "Windows.h"
#endif

QtProcess::QtProcess(QtBuilder *builder, bool blockOutput) : QProcess(),
	m_bld(builder), m_cancelled(false)
{
	if (!m_bld)
		return;

	setWorkingDirectory(m_bld->targetFolder());
	QProcessEnvironment e=m_bld->environment();
	if (!e.isEmpty()) setProcessEnvironment(e);

	if (!blockOutput)
	{
		connect(this,	SIGNAL(readyReadStandardOutput()), m_bld, SLOT(procOutput()), Qt::BlockingQueuedConnection);
		connect(this,	SIGNAL(readyReadStandardError()),  m_bld, SLOT(procError()),  Qt::BlockingQueuedConnection);
	}
	{	connect(this,	SIGNAL(readyReadStandardOutput()),	this, SLOT(checkQuit()));
		connect(this,	SIGNAL(readyReadStandardError()),	this, SLOT(checkQuit()));
	}
}

QtProcess::~QtProcess()
{
	close();
}

void QtProcess::sendStdOut()
{
	if (m_bld)
		CALL_QUEUED(m_bld, procOutput);
}

void QtProcess::sendStdErr()
{
	if (m_bld)
		CALL_QUEUED(m_bld, procError);
}

void QtProcess::checkQuit()
{
	if (!m_bld->cancelled())
		return;

	m_cancelled = true;
	if (state() != Running)
	{
		close();
		return;
	}
	for(int i = 0; i < 5; i++)
	{
#ifdef _WIN32
		GenerateConsoleCtrlEvent(0, (DWORD)pid());
		GenerateConsoleCtrlEvent(1, (DWORD)pid());
#endif
		if (!waitForFinished(500))
		{
			closeWriteChannel();
			closeReadChannel(StandardError);
			closeReadChannel(StandardOutput);
		}

		if (!waitForFinished(500)) terminate();
		if (!waitForFinished(500)) kill();
	}
	if (state() == Running)
	{
		QProcess p;
		p.execute(QString("taskkill /PID %1").arg((int)pid()));
		waitForFinished(5000);
	}
	close();
}



InlineProcess::InlineProcess(QtBuilder *builder, const QString &prog, const QString &args, bool blockOutput) : QtProcess(builder, blockOutput)
{
	if (!builder)
		return;

	setNativeArguments(args);
	start(prog);
	waitForFinished(-1);
}

InlineProcess::~InlineProcess()
{
}



BuildProcess::BuildProcess(QtBuilder *builder, bool blockOutput) : QtProcess(builder, true)
{
	if (!builder)
		return;

	setWorkingDirectory(builder->targetFolder());
	QProcessEnvironment e=builder->environment();
	if (!e.isEmpty())	setProcessEnvironment(e);

	if (!blockOutput)
	{
		connect(this, SIGNAL(readyReadStandardOutput()), builder, SLOT(procLog()),	 Qt::BlockingQueuedConnection);
		connect(this, SIGNAL(readyReadStandardError()),  builder, SLOT(procError()), Qt::BlockingQueuedConnection);
	}
}

BuildProcess::~BuildProcess()
{
}

void BuildProcess::setArgs(const QString &args)
{
	if (!args.isEmpty())
		setNativeArguments(args);
}

void BuildProcess::start(const QString &prog)
{
	QDir::setCurrent(workingDirectory());
	QProcess::start(prog);
}

bool BuildProcess::result()
{
	waitForFinished(-1);
	return normalExit();
}
