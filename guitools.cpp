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
#include <QMotifStyle>
#include <QStyleOptionSlider>
#include <QPainter>
#include <QMouseEvent>
#include <QDateTime>
#include <QTimer>

Progress::Progress(QWidget *parent) : QProgressDialog(parent, Qt::WindowStaysOnTopHint)
{
	setStyleSheet("QLabel { qproperty-alignment: 'AlignLeft | AlignBottom'; }");
	setWindowTitle("Copying Files");
	setFixedWidth(parent->width()/2);
	setFocusPolicy(Qt::NoFocus);
	setAutoReset(false);
	setAutoClose(false);

	QProgressDialog::setMaximum(INT_MAX);
}

void Progress::setMaximum(int max)
{
	if (max == -1)
	{
		hide();
		QProgressDialog::setMaximum(INT_MAX);
	}
	else if (max != maximum())
	{
		QProgressDialog::setMaximum(max);
		show();
	}
}



const QString qtBuilderProgressTempl(" %1 MB/s ... %2");
const QString qtBuilderDiskSpaceTmpl("  %1 disk usage: %2 GB /%3%");
const QString qtBuilderProgressStyle(
	 "QProgressBar			{ color: #FFFFFF; background: #181818; border: none; text-align: left; vertical-align: center; }"
	 "QProgressBar:disabled { color: #BBBBBB; background: #323232; }");

QtProgress::QtProgress(int color, QWidget *parent) : QProgressBar(parent)
{
	setStyleSheet(qtBuilderProgressStyle);
	setStyle(new QMotifStyle());
	setFocusPolicy(Qt::NoFocus);
	setFixedHeight(defGuiHeight);
	setMinimum(0);
	setMaximum(0);

	QPalette p(palette());
	p.setColor(QPalette::Highlight, colors.at(color));
	p.setColor(QPalette::Disabled, QPalette::Highlight, "#282828");
	setPalette(p);

	QFont f(font());
	f.setPixelSize(12);
	f.setFamily("Consolas");
	setFont(f);
}

CopyProgress::CopyProgress(QWidget *parent) : QtProgress(Critical, parent)
{
	hide();
}

void CopyProgress::setMaximum(int maximum)
{
	QProgressBar::setMaximum(maximum);
	QProgressBar::show();
}

void CopyProgress::progress(int count, const QString &file, qreal mbs)
{
	if (!maximum())
		return;

	QString text = qtBuilderProgressTempl.arg(mbs,6,FMT_F,2,FILLSPC).arg(file);

	setFormat(text);
	setValue(count);
}

DiskSpaceBar::DiskSpaceBar(QWidget *parent, int color, const QString &name) : QtProgress(color, parent),
	m_diskSpace(0), m_name(name)
{
}

void DiskSpaceBar::showEvent(QShowEvent *event)
{
	QtProgress::showEvent(event);
	refresh();
}

void DiskSpaceBar::setDrive(const QString &path)
{
	uint total, free;
	if (getDiskSpace(path, total, free) && total)
	{
		m_diskSpace = qMax(m_diskSpace, total-free);
		m_path = path;

		setValue(m_diskSpace);
		setMaximum(total);
		show();
	}
}

void DiskSpaceBar::refresh()
{
	if (!isVisible())
		return;

	uint total, free;
	if (!getDiskSpace(m_path, total, free))
		return;

	uint usedmb = total-free;
	m_diskSpace = qMax(m_diskSpace, usedmb);
	qreal gbyte = usedmb/1024.0;
	uint percnt = usedmb*100/total;
	QString gbt = PLCHD.arg(gbyte,7,FMT_F,2,FILLSPC);
	QString pct = QString::number(percnt).rightJustified(3,FILLSPC);

	setValue(usedmb);
	setFormat(qtBuilderDiskSpaceTmpl.arg(m_name, gbt, pct));
}



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



QtSlider::QtSlider(int bgnd, int color, QWidget *parent) : QSlider(parent),
	m_clickSet(false), m_sliding(false)
{
	setStyle(new SliderProxyStyle(style()));
	setOrientation(Qt::Horizontal);
	setFocusPolicy(Qt::NoFocus);
	setAttribute(Qt::WA_Hover);
	setCursor(Qt::ArrowCursor);
	setMouseTracking(true);

	QPalette p(palette());
	p.setColor(QPalette::Highlight, colors.at(color));
	p.setColor(QPalette::Background, colors.at(bgnd));
	setPalette(p);
}

void QtSlider::setOrientation(Qt::Orientation o)
{
	QSlider::setOrientation(o);
	if (o == Qt::Horizontal)
	{
		setSizePolicy(QSizePolicy::Minimum, QSizePolicy::Fixed);
		setMinimumHeight(defGuiHeight);
		setMaximumHeight(defGuiHeight);
		setMinimumWidth (defGuiHeight);
		setMaximumWidth (QT_MAX_UI);
	}
	else
	{
		setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Minimum);
		setMinimumWidth (defGuiHeight);
		setMaximumWidth (defGuiHeight);
		setMinimumHeight(defGuiHeight);
		setMaximumHeight(QT_MAX_UI);
	}
}

void QtSlider::mouseMoveEvent(QMouseEvent *event)
{
	if (m_sliding)
	{
		setValue(valueFromPosition(event->pos()));
	}
	else if (isOnHandle(event->pos()))
	{
		setCursor(Qt::PointingHandCursor);
	}
	else
	{
		setCursor(Qt::ArrowCursor);
	}
	event->accept();
}

void QtSlider::mousePressEvent(QMouseEvent *event)
{
	if (mouseLocked())
	{
		event->ignore();
	}
	else if (event->button() == Qt::LeftButton)
	{
		if ((m_clickSet = !isOnHandle(event->pos())))
		{
			event->ignore();
		}
		else
		{
			if (cursor().shape()!=Qt::PointingHandCursor)
						setCursor(Qt::PointingHandCursor);

			m_sliding = true;
			event->accept();
		}
		emit sliderPressed();
	}
}

void QtSlider::mouseReleaseEvent(QMouseEvent *event)
{
	if (m_clickSet)
		setValue(valueFromPosition(event->pos()));

	bool wasLocked = mouseLocked();
	m_sliding  = false;
	m_clickSet = false;

	if (wasLocked)
	{
		if (event->button() == Qt::LeftButton)
			emit sliderReleased();

		event->ignore();
	}

	if (cursor().shape()!=Qt::ArrowCursor)
				setCursor(Qt::ArrowCursor);
	repaint();
}

void QtSlider::keyPressEvent(QKeyEvent *event)
{
	event->ignore();
}

void QtSlider::keyReleaseEvent(QKeyEvent *event)
{
	event->ignore();
}

int QtSlider::valueFromPosition(const QPoint &pos) const
{
	QStyleOptionSlider opt;
	initStyleOption ( &opt );

	qreal s = style()->pixelMetric(QStyle::PM_SliderControlThickness,&opt,this)/2.0;
	qreal min = minimum();
	qreal max = maximum();

	if (orientation() == Qt::Horizontal)
	{
		qreal w = width();
		return qRound(Map((qreal)pos.x(),s,w-s,min,max));
	}
	else
	{
		qreal h = height();
		return qRound(Map((qreal)pos.y(),s,h-s,min,max));
	}
}

bool QtSlider::isOnHandle(const QPoint &pos) const
{
	QStyleOptionSlider opt;
	initStyleOption ( &opt );

	QRect  r = style()->subControlRect(QStyle::CC_Slider, &opt, QStyle::SC_SliderHandle, this);
	return r.contains(pos);
}

QSize QtSlider::minimumSizehint() const
{
	if (orientation() == Qt::Horizontal)
		return QSize(QSlider::minimumSizeHint().width(), defGuiHeight);

	return QSize(defGuiHeight, QSlider::minimumSizeHint().height());
}



SliderProxyStyle::SliderProxyStyle(QStyle *baseStyle) : QProxyStyle(baseStyle)
{
}

int SliderProxyStyle::pixelMetric(PixelMetric pMetric, const QStyleOption *opt, const QWidget *wgt) const
{	//
	// note: apparently all these values only deal with the height (for a horizontal slider)
	// or the width (for a vertical slider), of certain elements, but not the "sliding span"
	// ... except the "PM_SliderLength" value which effects the lenght of the _handle_ !!!!!
	//
	switch(pMetric)
	{
	case PM_SliderLength:			//...the lenght of the _handle_ (not just the slider)
	case PM_SliderThickness:		//...this is the slider thickness without tick marks!
	case PM_SliderSpaceAvailable:	//...this is the thickness inc. the tick marks space!
	case PM_SliderControlThickness: //...just as the name implies - the handle thickness!
		return defGuiHeight;

	case PM_SliderTickmarkOffset:
		return 0;
	}
	return QProxyStyle::pixelMetric(pMetric, opt, wgt);
}

QRect SliderProxyStyle::subControlRect(ComplexControl ctrl, const QStyleOptionComplex *opt, SubControl sub, const QWidget *wgt) const
{
	QRect rect = QProxyStyle::subControlRect(ctrl, opt, sub, wgt);
	if (ctrl == CC_Slider && sub == SC_SliderHandle)
	{
		const QStyleOptionSlider *o = static_cast<const QStyleOptionSlider *>(opt);
		qreal val = o->sliderValue;
		qreal min = o->minimum;
		qreal max = o->maximum;

		qreal t = pixelMetric(PM_SliderControlThickness, opt, wgt);
		bool  h = o->orientation == Qt::Horizontal;
		qreal s = h ? wgt->width() : wgt->height();
		int   v = qRound(Map(val,min,max,t/2,s-t/2));

		rect= QRectF(-1,-1,t+1,t+1).toRect(); // ... avoid minimal paint offset @ end positions due to rounding by painting the handle 1px bigger in any direction!
		h	? rect.moveCenter(QPoint(v,o->rect.center().y()))
			: rect.moveCenter(QPoint(o->rect.center().x(),v));
	}
	return rect;
}

void SliderProxyStyle::drawComplexControl(ComplexControl ctrl, const QStyleOptionComplex *opt, QPainter *p, const QWidget *wgt) const
{
	if (ctrl != CC_Slider)
		return;

	QPalette pl = opt->palette;
	QBrush high = pl.highlight();
	QBrush bgnd = pl.background();
	bool enable = wgt->isEnabled();
	if (!enable)
	{
		high = QBrush("#969696");
		bgnd = QBrush("#565656");
	}

	const QtSlider *qsl = qobject_cast<const QtSlider *>(wgt);
	QRectF h = subControlRect(CC_Slider, opt, SC_SliderHandle, wgt);
	QRect  r = opt->rect;
	int  off = r.width()/20;
	int  val = qsl->value();
	bool lft = val*2 > (qsl->maximum()-qsl->minimum());

	p->setRenderHint(QPainter::Antialiasing);
	p->fillRect(r, bgnd);

	QTextOption o;
	if (lft) o.setAlignment(Qt::AlignLeft |Qt::AlignVCenter);
	else	 o.setAlignment(Qt::AlignRight|Qt::AlignVCenter);

	p->setPen(Qt::black);
	p->drawText(r.adjusted(off,0,-off,0), wgt->accessibleName(), o);
	p->fillRect(h, high);

	QFont f = p->font();
	f.setPointSize(14);
	p->setFont(f);

	p->setPen(Qt::white);
	o.setAlignment(Qt::AlignHCenter|Qt::AlignVCenter);
	p->drawText(h, QString::number(val), o);
}
