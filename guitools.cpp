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

#include <QMouseEvent>
#include <QFileDialog>
#include <QPainter>

const QString qtBuilderRegistryIns("InstallDir");
const QString qtBuilderRegistryDig("HKEY_CURRENT_USER\\Software\\Digia\\Versions");
const QString qtBuilderRegistryTrl("HKEY_CURRENT_USER\\Software\\Trolltech\\Versions");

const QString qtBuilderHeaderStyle("%1 { background: %2; color: white; } %1:disabled { background: #969696; color: #EFEFEF; border-right: 1px solid #B2B2B2; }");
const QString qtBuilderQtMenuStyle("QMenu { color: black; background: white; border: none; } QMenu::item:selected { color: white; background: %1; }");
const QString qtBuilderButtonStyle("QPushButton { background: %1; color: black; border: none; text-align: left; padding: 0px 9px; }"
								   "QPushButton:disabled { background: #969696; color: #EFEFEF;  border-right: 1px solid #B2B2B2; }"
								   "QPushButton:hover { color: white; background: %2; } QPushButton::menu-indicator { image:none; }");

QtHeader::QtHeader(const QString &text, int color, QWidget *parent) : QLabel(text, parent)
{
	setStyleSheet(qtBuilderHeaderStyle.arg(CLASS_NAME(QLabel), colors.at(color)));
	setAlignment(Qt::AlignHCenter|Qt::AlignVCenter);
	setFixedHeight(defGuiHeight);

	QFont f(font());
	f.setPointSize(11);
	setFont(f);
}



QtButton::QtButton(const QString &off, const QString &on, QWidget *parent, bool resetExternal) : QtHeader(off, Critical, parent),
	m_isOn(false), m_resetExtern(resetExternal)
{
	m_text.append(off);
	m_text.append(on);

	setStyleSheet(styleSheet()+QString("QLabel:hover { background: %1; }").arg(colors.at(Warning)));
	setCursor(Qt::PointingHandCursor);
	setFocusPolicy(Qt::NoFocus);

	QFont f(font());
	f.setPointSize(14);
	setFont(f);
}

void QtButton::mouseReleaseEvent(QMouseEvent *event)
{
	Q_UNUSED(event);
	if (!m_isOn || !m_resetExtern) setOff(m_isOn);
	isOn(m_isOn);
}

void QtButton::setOff(bool off)
{
	m_isOn = !off;
	setText(m_text.at(m_isOn));

	QString c = colors.at(m_isOn ? AppInfo : Critical);
	setStyleSheet(qtBuilderHeaderStyle.arg(CLASS_NAME(QLabel), c));
}



DirSelect::DirSelect(const QString &path, int dir, int color, QWidget *parent) : QPushButton(parent),
	m_dir(dir), m_isElided(false)
{
	setStyleSheet(qtBuilderButtonStyle.arg(colors.at(color), colors.at(Critical)));
	setToolTip(QDir::toNativeSeparators(path));
	setCursor(Qt::PointingHandCursor);
	setContentsMargins(9, 0, 0, 0);
	setFixedHeight(defGuiHeight);
	setFocusPolicy(Qt::NoFocus);

	QFont f(font());
	f.setPointSizeF(8.75);
	f.setStrikeOut(!QDir(path).exists());
	setFont(f);


	DirMenu *m = new DirMenu(dir, Critical, this);
	if (dir != QtBuilder::Source || m->isEmpty())
		connect(this, SIGNAL(released()), m, SLOT(browseDisk()));
	else
		setMenu(m);
	{	connect(m, SIGNAL(dirSelected(const QString &, const QString &)),
			 this, SIGNAL(dirSelected(const QString &, const QString &)));
		connect(m, SIGNAL(dirSelected(const QString &, const QString &)),
			 this,	 SLOT(	updateDir(const QString &, const QString &)));
	}
	installEventFilter(this);
}

void DirSelect::updateDir(const QString &path, const QString &ver)
{
	Q_UNUSED(ver);
	QString dir = QDir::toNativeSeparators(path);
	setToolTip(dir);

	QFont f(font());
	QFontMetrics m(f);
	QString t = dir.toUpper();
	t = m.elidedText(t, Qt::ElideMiddle, width()-20);
	f.setStrikeOut(!QDir(path).exists());

	setFont(f);
	setText(t);
	m_isElided = t != toolTip();
}

bool DirSelect::eventFilter(QObject *object, QEvent *event)
{
	if (event->type() == QEvent::ToolTip && !m_isElided)
	{
		event->accept();
		return true;
	}
	return QPushButton::eventFilter(object, event);
}

void DirSelect::resizeEvent(QResizeEvent *event)
{
	QPushButton::resizeEvent(event);
	updateDir(toolTip());
}

void DirSelect::paintEvent(QPaintEvent *event)
{
	QPushButton::paintEvent(event);
	QPainter painter(this);

	QRect	r = event->rect();
	QPoint tr = r.topRight();
	int		h = r.height()*2/3;
	QPainterPath p;

	switch(m_dir)
	{
	case QtBuilder::Source:
	//	tr-=QPoint(h/5*2,-h);
	//	p.moveTo(tr);
	//	p.lineTo(tr-QPoint(h/3,0));
	//	p.lineTo(tr-QPoint(h/3,-h*2/3));
	//	p.lineTo(tr+QPoint(0,	h*2/3));
	//	p.closeSubpath();
	//	painter.fillPath(p, Qt::white);
		break;

	case QtBuilder::Target:
		p.moveTo(tr);
		p.lineTo(tr-QPoint(h,0));
		p.lineTo(tr-QPoint(h/2,-h*2/3));
		p.closeSubpath();
		painter.fillPath(p, Qt::white);
		break;
	}
}



DirMenu::DirMenu(int dir, int color, QWidget *parent) : QMenu(parent),
	m_dir(dir)
{
	setStyleSheet(qtBuilderQtMenuStyle.arg(colors.at(color)));

	QFont f(font());
	f.setPointSize(10);
	setFont(f);

	if (predefined())
	{
		addSeparator();
		addAction("Browse disk ...")->setData(dir);

		connect(this, SIGNAL(triggered(QAction *)), this, SLOT(action(QAction *)));
	}
}

void DirMenu::action(QAction *act)
{
	QString path, ver;
	bool ok;
	int val;
	if ((val = act->data().toInt(&ok))&&ok&&
		 val == QtBuilder::Source)
	{
		browseDisk();
		return;
	}
	else
	{
		QString key = act->data().toString();
		QSettings s	( key, QSettings::NativeFormat );
		path = QDir(s.value(qtBuilderRegistryIns).toString()).absolutePath();
		emit dirSelected(path, QDir(key).dirName());
	}
}

void DirMenu::browseDisk()
{
	QString  dir;
	switch(m_dir)
	{
	case QtBuilder::Source:
		dir = QFileDialog::getOpenFileName(0,"Select Qt sources path","C:/Qt",qtConfigure);
		dir = QFileInfo(dir).absolutePath();
		break;

	case QtBuilder::Target:
		dir = QFileDialog::getExistingDirectory(0,"Select build target path","C:/Qt");
		break;

	default:
		return;
	}

	if (dir.isEmpty())
		return;

	QStringList n;
	bool ok=false;  int v;
	foreach(const QChar &c, dir)
		if (c.isDigit()&&(ok||(ok=(v=c.digitValue())>3||v<6)))
			n.append(c);

	emit dirSelected(dir, n.join("."));
}

bool DirMenu::predefined()
{
	QStringList reg;
	reg += qtBuilderRegistryDig;
	reg += qtBuilderRegistryTrl;
	QMap<QString, QString> map, ver;

	FOR_CONST_IT(reg)
	{
		QSettings s(*IT, QSettings::NativeFormat);
		QStringList  keys = s.childGroups();
		FOR_CONST_JT(keys)
		{
			QString key = (*IT)+"\\"+*JT;
			map.insert(*JT, key);
		}
	}
	FOR_CONST_IT(map)
	{
		QString key = IT.value();
		QSettings s(key, QSettings::NativeFormat);
		QString dir = s.value(qtBuilderRegistryIns).toString().trimmed();

		if (!s.contains(qtBuilderRegistryIns) && !dir.isEmpty())
			continue;

		QDir  d = QDir(dir);
		dir = d.absolutePath();

		if (!ver.contains(key) && d.exists() &&
			QFileInfo(dir+SLASH+qtConfigure).exists())
			ver.insert(IT.key(), dir);
	}
	FOR_CONST_IT(ver)
		addAction(IT.key()+": "+QDir::toNativeSeparators(IT.value()))->setData(map.value(IT.key()));
	return ver.count();
}
