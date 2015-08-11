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

#include <QMotifStyle>

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
