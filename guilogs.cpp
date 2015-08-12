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

#include <QApplication>
#include <QPainter>
#include <QDateTime>
#include <QTimer>

const QString qtBuilderTableBody("<html><header><style>TD{padding:0px 6px;}</style></header><body style=font-size:11pt;font-family:Calibri;><table>");
const QString qtBuilderTableLine("<tr><td style=font-size:10pt>%1</td><td><b style=color:%2>%3</b></td><td>%4</td></tr>");
const QString qtBuilderTableHtml("</table><a name=\"end\"><br/></body></html>");
const QString qtBuilderLogLine("%1\t%2\t%3%4\r\n");
const QString qtBuilderBuildLogTabs = QString(___LF)+QString(__TAB).repeated(9);

QtAppLog::QtAppLog(QWidget *parent) : QTextBrowser(parent)
{
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setFrameStyle(QFrame::NoFrame);
	setFocusPolicy(Qt::NoFocus);
	setMinimumWidth(640);

	m_info.insert(AppInfo,	"AppInfo ");
	m_info.insert(Process,	"Process ");
	m_info.insert(Warning,	"Warning ");
	m_info.insert(Informal,	"Informal");
	m_info.insert(Elevated,	"Elevated");
	m_info.insert(Critical,	"Critical");
}

const QString QtAppLog::logFile()
{
	if (	m_logFile.isEmpty())
			m_logFile = qApp->applicationDirPath()+SLASH+qApp->applicationName()+".log";
	return	m_logFile;

}

const QString QtAppLog::clean(QString text, bool extended)
{
	text = text.remove(__TAB).trimmed();
	if (!extended)
		return text;

	QStringList t = text.split(_CRLF, QString::SkipEmptyParts);
				t+= text.split(___LF, QString::SkipEmptyParts);

	int count = t.count();
	for(int i = 0; i < count; i++)
	{
		text = t[i].simplified();
		if (text.isEmpty())
		{
			t.removeAt(i);
			count--;
		}
		else t[i] = text;
	}

	t.removeDuplicates();
	return t.join(_CRLF);
}

void QtAppLog::addSeparator()
{
	QFile log(logFile());
	if (log.open(QIODevice::Append))
		log.write(QString("\r\n%1\r\n").arg(QString("*").repeated(120)).toUtf8().constData());
}

void QtAppLog::add(const QString &msg, int type)
{
	add(msg, QString(), type);
}

void QtAppLog::add(const QString &msg, const QString &text, int type)
{
	QString ts = QDateTime::currentDateTime().toString(Qt::ISODate);
	QString message = msg;

	m_timestamp.append(ts);
	m_message.append(message);
	m_description.append(text);
	m_type.append(type);

	QSet<uint> hashes;
	QString tline;

	QString h = qtBuilderTableBody;
	int count = m_type.count();
	for(int i = 0; i < count; i++)
	{
		tline = qtBuilderTableLine.arg(m_timestamp.at(i), colors.at(m_type.at(i)), m_message.at(i), m_description.at(i));
		uint hash = qHash(tline); // ... filter out duplicates for process messages posted to stdout AND stdErr!!!

		if (!hashes.contains(hash))
		{	 hashes.insert(hash);
			 h.append(tline);
		}
	}
	h.append(qtBuilderTableHtml);

	setText(h);
	scrollToAnchor("end");

	if (type == Process)
		if (int len = qMax(0, 30-msg.length()))
			message += QString(" %1 ").arg(QString("*").repeated(len));

	QFile log(logFile());
	if (log.open(QIODevice::Append))
		log.write(qtBuilderLogLine.arg(ts, m_info.value(type), message.leftJustified(32),
			QString(text).replace(___LF, qtBuilderBuildLogTabs)).toUtf8().constData());
}

void QtAppLog::paintEvent(QPaintEvent *event)
{
	QPainter painter(viewport());
	QPixmap p(":/graphics/icon.png");
	p = p.scaledToWidth(p.width()/2);

	painter.save();
	painter.setOpacity(0.25);

	QRect r = event->rect();
	r.setLeft(qAbs(p.width()-r.width()));
	r.setTop(qAbs(p.height()-r.height()));
	r.moveTo(r.topLeft()-QPoint(0,r.height()/8));

	painter.setBrushOrigin(r.topLeft());
	painter.fillRect(r, p);
	painter.restore();

	QTextBrowser::paintEvent(event);
}


const bool	  qtBuilderWriteBldLog = true;	// creates a build.log with all the stdout from configure/jom; is copied to the target folder.
const QString qtBuilderCommandStyle("QTextEdit { border: none; color: white; background: #181818; } QTextEdit:disabled { background: #323232; color: #969696; }");

BuildLog::BuildLog(QWidget *parent) : QTextEdit(parent),
	m_lineCount(0)
{
	setTextInteractionFlags(Qt::TextSelectableByMouse|Qt::TextSelectableByKeyboard);
	setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
	setHorizontalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setVerticalScrollBarPolicy(Qt::ScrollBarAlwaysOff);
	setStyleSheet(qtBuilderCommandStyle);
	setLineWrapMode(QTextEdit::NoWrap);
	setFrameStyle(QFrame::NoFrame);
	setFontFamily("Consolas");
	setMinimumWidth(960);
	setFontPointSize(9);
	setFocus();
}

const QString BuildLog::logFile()
{
	if (	m_logFile.isEmpty())
			m_logFile = SLASH+qApp->applicationName()+".log";
	return	m_logFile;
}

void BuildLog::append(const QString &text, const QString &path)
{
	if (text.simplified().trimmed().isEmpty())
		return;

	if (!m_lineCount)
		QTextEdit::append(text.trimmed());

	if (!qtBuilderWriteBldLog)
		return;

	QFile log(path+logFile());
	if (log.open(QIODevice::Append))
		log.write(text.toUtf8().constData());
}

void BuildLog::endFailure()
{
	if (!m_lineCount)
		QTextEdit::append(___LF+___LF);

	QStringList a = QString(qUncompress(__ARR)).split(___LF);
	if (a.count() > m_lineCount)
	{
		QTextEdit::append(a.at(m_lineCount++));
		QTimer::singleShot(50, this, SLOT(endFailure()));
	}
	else if (a.count() == m_lineCount)
	{
		QTextEdit::append(___LF);
		m_lineCount =  0;
	}
	ensureCursorVisible();
}

void BuildLog::endSuccess()
{
	if (!m_lineCount)
		QTextEdit::append(___LF+___LF);

	QStringList h = QString(qUncompress(__HRR)).split(___LF);
	if (h.count() > m_lineCount)
	{
		QTextEdit::append(h.at(m_lineCount++));
		QTimer::singleShot(50, this, SLOT(endSuccess()));
	}
	else if (h.count() == m_lineCount)
	{
		QTextEdit::append(___LF);
		m_lineCount =  0;
	}
	ensureCursorVisible();
}

