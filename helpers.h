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
#ifndef HELPERS_H
#define HELPERS_H

#include <QStringList>
#include <QRect>

typedef const char *const StringLiteral;

const char	FMT_F = 'f'  ;
const QChar FILLSPC(' ' );
const QChar FILLNUL('0' );
const QString SLASH("/" );
const QString PLCHD("%1");
const QString ___LF("\n");
const QString __TAB("\t");
const QString _CRLF("\r\n");

const QString Q_SETTINGS_GEOMETRY("WindowGeometry");

#define FOR_CONST_IT(OBJECT)											\
	for (auto IT = OBJECT.constBegin(); IT != OBJECT.constEnd(); ++IT)	\

#define FOR_CONST_JT(OBJECT)											\
	for (auto JT = OBJECT.constBegin(); JT != OBJECT.constEnd(); ++JT)	\

#define FOR_CONST_KT(OBJECT)											\
	for (auto KT = OBJECT.constBegin(); KT != OBJECT.constEnd(); ++KT)	\

#define FOR_CONST_LT(OBJECT)											\
	for (auto LT = OBJECT.constBegin(); LT != OBJECT.constEnd(); ++LT)	\

#define FOR_IT(OBJECT)													\
	for (auto IT = OBJECT.begin(); IT != OBJECT.end(); ++IT)			\

#define FOR_JT(OBJECT)													\
	for (auto JT = OBJECT.begin(); JT != OBJECT.end(); ++JT)			\

#define FOR_KT(OBJECT)													\
	for (auto KT = OBJECT.begin(); KT != OBJECT.end(); ++KT)			\

#define FOR_LT(OBJECT)													\
	for (auto LT = OBJECT.begin(); LT != OBJECT.end(); ++LT)			\

#define ____EXPAND(X) X // for MSVC2010 compatibility
#define _PP_SEQ_N() 7, 6, 5, 4, 3, 2, 1, 0
#define _PP_ARG_N(_1, _2, _3, _4, _5, _6, _7, NAME, ...) NAME
#define _PP_NARG_(...) ____EXPAND(_PP_ARG_N(__VA_ARGS__))
#define _PP_N_ARG(...) ____EXPAND(_PP_NARG_(__VA_ARGS__,_PP_SEQ_N()))

#define _CALL_QUEUED_2(_1, _2)						QMetaObject::invokeMethod(_1, #_2, Qt::QueuedConnection)
#define _CALL_QUEUED_3(_1, _2, _3)					QMetaObject::invokeMethod(_1, #_2, Qt::QueuedConnection, Q_ARG##_3)
#define _CALL_QUEUED_4(_1, _2, _3, _4)				QMetaObject::invokeMethod(_1, #_2, Qt::QueuedConnection, Q_ARG##_3, Q_ARG##_4)
#define _CALL_QUEUED_5(_1, _2, _3, _4, _5)			QMetaObject::invokeMethod(_1, #_2, Qt::QueuedConnection, Q_ARG##_3, Q_ARG##_4, Q_ARG##_5)
#define _CALL_QUEUED_6(_1, _2, _3, _4, _5, _6)		QMetaObject::invokeMethod(_1, #_2, Qt::QueuedConnection, Q_ARG##_3, Q_ARG##_4, Q_ARG##_5, Q_ARG##_6)
#define _CALL_QUEUED_7(_1, _2, _3, _4, _5, _6, _7)	QMetaObject::invokeMethod(_1, #_2, Qt::QueuedConnection, Q_ARG##_3, Q_ARG##_4, Q_ARG##_5, Q_ARG##_6, Q_ARG##_7)

#define _CALL_QUEBLK_2(_1, _2)						QMetaObject::invokeMethod(_1, #_2, Qt::BlockingQueuedConnection)
#define _CALL_QUEBLK_3(_1, _2, _3)					QMetaObject::invokeMethod(_1, #_2, Qt::BlockingQueuedConnection, Q_ARG##_3)
#define _CALL_QUEBLK_4(_1, _2, _3, _4)				QMetaObject::invokeMethod(_1, #_2, Qt::BlockingQueuedConnection, Q_ARG##_3, Q_ARG##_4)
#define _CALL_QUEBLK_5(_1, _2, _3, _4, _5)			QMetaObject::invokeMethod(_1, #_2, Qt::BlockingQueuedConnection, Q_ARG##_3, Q_ARG##_4, Q_ARG##_5)
#define _CALL_QUEBLK_6(_1, _2, _3, _4, _5, _6)		QMetaObject::invokeMethod(_1, #_2, Qt::BlockingQueuedConnection, Q_ARG##_3, Q_ARG##_4, Q_ARG##_5, Q_ARG##_6)
#define _CALL_QUEBLK_7(_1, _2, _3, _4, _5, _6, _7)	QMetaObject::invokeMethod(_1, #_2, Qt::BlockingQueuedConnection, Q_ARG##_3, Q_ARG##_4, Q_ARG##_5, Q_ARG##_6, Q_ARG##_7)

#define _CALL_QUEUED_(N)	 _CALL_QUEUED_##N
#define _CALL_QUEBLK_(N)	 _CALL_QUEBLK_##N
#define _CALL_QUEUED_EVAL(N) _CALL_QUEUED_(N)
#define _CALL_QUEBLK_EVAL(N) _CALL_QUEBLK_(N)

#define CALL_QUEUED(...) ____EXPAND(_CALL_QUEUED_EVAL(____EXPAND(_PP_N_ARG(__VA_ARGS__)))(__VA_ARGS__))
#define CALL_QUEBLK(...) ____EXPAND(_CALL_QUEBLK_EVAL(____EXPAND(_PP_N_ARG(__VA_ARGS__)))(__VA_ARGS__))

#define META_OBJECT(CLASS)	(##CLASS::staticMetaObject)
#define CLASS_NAME(CLASS)	 META_OBJECT(##CLASS).className()
#define MEMBER(NAME)		 #NAME

const QRect centerRect(int percentOfScreen, int screenNbr = -1);

bool getDiskSpace(const QString &anyPath, uint &totalMb, uint &freeMb);
bool createSymlink(const QString &source, const QString &target, QString &error = QString());
bool removeSymlink(const QString &target);
bool unmountFolder(const QString &path, QString &error = QString());
bool mountFolder(const QString &srcDrive, const QString &tgtPath, QString &error = QString());
const QString getValueFrom(const QString &string, const QString &inTag, const QString &outTag);
const QString getLastWinError();

const qreal MBYTE = 1024*1024.0;
#endif // HELPERS_H
