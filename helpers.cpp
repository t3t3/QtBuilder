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
	SetErrorMode(SEM_FAILCRITICALERRORS);
	ULARGE_INTEGER free,total;
	if (GetDiskFreeSpaceExA(
		QString(anyPath.left(2)+"\\").toUtf8().constData(), &free, &total, 0))
	{
		quint64 MB = quint64(1024*1024);
		freeMb	= (uint)(( free.QuadPart) / MB);
		totalMb = (uint)((total.QuadPart) / MB);

		SetErrorMode(0);
		return true;
	}
#endif
	return false;
}

bool unmountFolder(const QString &path, QString &error)
{
	QString dir = QDir::toNativeSeparators(QDir::cleanPath(path));
#ifdef _WIN32
	LPCWSTR lpszVolumeMountPoint = (LPCWSTR)dir.utf16();

	if (DeleteVolumeMountPoint(lpszVolumeMountPoint) == NO_ERROR)
		return true;
#endif
	error = getLastWinError();
	return false;
}

bool mountFolder(const QString &srcDrive, const QString &tgtPath, QString &error)
{
	QString drv = QDir::cleanPath(srcDrive)+"\\";
	QString nat = QDir::toNativeSeparators(tgtPath);
#ifdef _WIN32
	LPCWSTR lpszVolumeMountPoint = (LPCWSTR)drv.utf16();
	LPWSTR  lpszVolumeName = '\0';

	if (GetVolumeNameForVolumeMountPoint(lpszVolumeMountPoint, lpszVolumeName, 100) == NO_ERROR)
	{
		lpszVolumeMountPoint = (LPCWSTR)nat.utf16();
		if (SetVolumeMountPoint(lpszVolumeMountPoint, lpszVolumeName) == NO_ERROR)
			return true;
	}
#endif
	error = getLastWinError();
	return false;
}

bool createSymlink(const QString &source, const QString &target, QString &error)
{
	QString src = QDir::toNativeSeparators(QDir::cleanPath(source));
	QString tgt = QDir::toNativeSeparators(QDir::cleanPath(target));
#ifdef _WIN32
	LPCWSTR lpSymlinkFileName = (LPCWSTR)tgt.utf16();
	LPCWSTR lpTargetFileName  = (LPCWSTR)src.utf16();

	if (CreateSymbolicLink(lpSymlinkFileName, lpTargetFileName, SYMBOLIC_LINK_FLAG_DIRECTORY))
		return true;
#endif
	error = getLastWinError();
	return false;
}

bool removeSymlink(const QString &target)
{
	QFileInfo fi(target);
	if (!fi.exists())
		return true;

	if (fi.symLinkTarget().isEmpty())
		return false;

	if (fi.isDir())
		return QDir().rmdir(target);

	return QFile(target).remove();
}

const QString getLastWinError()
{
#ifdef _WIN32
	DWORD msgId;
	if (!(msgId = GetLastError()))
		return QString();

	qDebug()<<"WINDOWS ERROR:"<<msgId;

	wchar_t buf[BUFSIZ];
	 size_t size = FormatMessageW(FORMAT_MESSAGE_FROM_SYSTEM, NULL, GetLastError(),
		MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), buf, BUFSIZ, 0);

	QString message = QString::fromWCharArray(buf, (int)size);
	LocalFree(buf);
	return	message;
#endif
	return QString();
}

const QString getValueFrom(const QString &string, const QString &inTag, const QString &outTag)
{
	if (string.contains(inTag) && string.contains(outTag))
	return string.split(inTag, QString::SkipEmptyParts).last().split(outTag, QString::SkipEmptyParts).first().simplified();
	return QString();
}
