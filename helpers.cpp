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
#include "helpers.h"
#include <QDebug>

#include <QApplication>
#include <QDesktopWidget>
#include <QDir>

const QRect centerRect(int percentOfScreen, int screenNbr)
{
	QRect r(100, 100, 100, 100);
	QDesktopWidget *dwg;

	if (!(dwg = QApplication::desktop()))
		return r;

	if (screenNbr == -1)
		screenNbr  = dwg->screenNumber(QCursor::pos());

	r = dwg->screenGeometry(screenNbr);
	qreal  f = abs(percentOfScreen) / 100.0;

	QSizeF s = r.size() * (0.5-f);
	int absX = qRound(s.width ());
	int absY = qRound(s.height());
	return r.adjusted(absX, absY, -absX, -absY);
}

#ifdef _WIN32
	#include "windows.h"
#endif

bool getDiskSpace(const QString &anyPath, uint &totalMb, uint &freeMb)
{
	#ifdef _WIN32
	ULARGE_INTEGER free,total;
	if (!GetDiskFreeSpaceExA(
		 QDir::toNativeSeparators(anyPath).toUtf8().constData(), &free, &total, 0))
		return false;

	quint64 MB = quint64(1024*1024);
	freeMb	= (uint)(( free.QuadPart) / MB);
	totalMb = (uint)((total.QuadPart) / MB);

	return true;
	#endif
	return false;
}

const QString getValueFrom(const QString &string, const QString &inTag, const QString &outTag)
{
	if (string.contains(inTag) && string.contains(outTag))
	return string.split(inTag, QString::SkipEmptyParts).last().split(outTag, QString::SkipEmptyParts).first().simplified();
	return QString();
}
