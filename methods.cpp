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

#include <QQueue>
#include <QDateTime>
#include <QDirIterator>
#include <QThreadPool>

const QString qtBuilderStaticDrive = "";//Y";	// ... use an existing drive letter; the ram disk part is skipped when set to anything else but ""; left-over build garbage will not get removed!

void QtCompile::clearPath(const QString &dirPath)
{
	QDir dir(dirPath);
	if (!dir.exists(dirPath))
		return;

	QFileInfoList infos = dir.entryInfoList(QDir::System|QDir::Hidden|QDir::Files);
	FOR_CONST_IT( infos )
		QFile((*IT).absoluteFilePath()).remove();
}

bool QtCompile::removeDir(const QString &dirPath, const QStringList &inc)
{
	bool result = true;
	QDir dir(dirPath);

	if (!dir.exists(dirPath))
		return result;

	bool i = !inc.isEmpty();

	QFileInfo info;
	QFileInfoList infos = dir.entryInfoList(QDir::NoDotAndDotDot|QDir::System|QDir::Hidden|QDir::AllDirs|QDir::Files, QDir::DirsFirst);
	FOR_CONST_IT( infos )
	{		 info = *IT;
		if ( info.isDir())
		{
			if (!i  ||  inc.contains(info.fileName()))
			{	 result &= removeDir(info.absoluteFilePath());
			}
			else continue;
		}
		else if (!i ||  inc.contains((QDir(info.absolutePath()).dirName())))
			 result &= QFile::remove(info.absoluteFilePath());
		if (!result)
			return result;
	}
	return result && dir.rmdir(dirPath);
}

int QtCompile::copyFolder(int fr, int to, bool synchronize, bool skipRootFiles)
{
	int count;
	QString source, target;
	if (!checkDir(to, target) ||
		!checkDir(fr, source, count, skipRootFiles))
		return 0;

	diskOp(to, true, count);
	count = 0;

	QQueue<QPair<QDir, QDir> > queue;
	queue.enqueue(qMakePair(QDir(source), QDir(target)));
	bool filter =!m_extFilter.isEmpty();

	QFileInfoList sinfo, dinfo;
	QPair<QDir, QDir>  pair;
	QString tgt, destd, nat, nme;
	QStringList compare;
	QDir srcDir, desDir;
	QFileInfo src, des;

	qreal  mb = 0;
	int level = 0;

	start();

	while(!queue.isEmpty())
	{
		if (cancelled())
			goto end;

		level--;
		pair = queue.dequeue();
		srcDir = pair.first;
		desDir = pair.second;

		if (filterDir(srcDir.absolutePath(), level >= 0))
		{
			if (synchronize && desDir.exists() &&
			   !removeDir(desDir.absolutePath()))
			{
				log("Synchronize faild; couldn't remove:",
					QDir::toNativeSeparators(desDir.absolutePath()), Elevated);
				goto error;
			}
			continue;
		}

		if(!desDir.exists() && !QDir().mkpath(desDir.absolutePath()))
			goto error;

		destd = desDir.absolutePath()+SLASH;
		sinfo = srcDir.entryInfoList(QDir::Files);

		if (skipRootFiles)
		{	skipRootFiles = false;
		}
		else FOR_CONST_IT(sinfo)
		{
			if (cancelled())
				goto end;

			src = *IT;
			nme = src.fileName();
			tgt = destd+nme;
			des = QFileInfo(tgt);
			nat = QDir::toNativeSeparators(tgt);

			if (filter && filterExt(src))
				continue;

			if (tgt.length() > 256)
			{
				log("Path length exceeded", nat, Elevated);
				goto error;
			}
			{	count++;
				mb +=src.size()/MBYTE;
				quint64 e = elapsed();

				if (e && !((e)%10))
					emit progress(count, nat, mb*1000/e);
			}

			bool exists;
			if ((exists  = des.exists()) &&
				des.size() == src.size() &&
				des.lastModified() == src.lastModified())
			{
				if (synchronize)
					compare.append(nme);
				continue;
			}
			else if (exists && !QFile(tgt).remove())
			{
				log("Couldn't replace old file:", nat, Elevated);
				goto error;
			}
			else if (!QFile::copy(src.absoluteFilePath(), tgt))
			{
				log("Couldn't copy new file:", nat, Elevated);
				goto error;
			}
			else if (synchronize)
			{
				compare.append(nme);
			}
		}

		if (synchronize)
		{
			dinfo = desDir.entryInfoList(QDir::Files);
			FOR_CONST_IT(dinfo)
			{
				if (cancelled())
					goto end;

				des = *IT;
				if (!compare.contains(des.fileName()) &&
					!QFile(des.absoluteFilePath()).remove())
				{
					log("Synchronize failed; couldn't remove:",
						QDir::toNativeSeparators(des.absoluteFilePath()), Elevated);
					goto error;
				}
			}
			compare.clear();
		}

		sinfo = srcDir.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot);
		destd = desDir.absolutePath()+SLASH;

		FOR_CONST_IT(sinfo)
		{
			if (cancelled())
				goto end;

			srcDir = (*IT).absoluteFilePath();
			nme = srcDir.dirName();
			queue.enqueue(qMakePair(srcDir, QDir(destd+nme)));

			if (synchronize)
				compare.append(nme);
		}
		if (level == -1)
			level  = sinfo.count();
		if (level ==  0)
			level  = -1;

		if (synchronize)
		{
			dinfo = desDir.entryInfoList(QDir::AllDirs | QDir::NoDotAndDotDot);
			FOR_CONST_IT(dinfo)
			{
				if (cancelled())
					goto end;

				des = *IT;
				if (!compare.contains(des.fileName()) &&
					!removeDir(des.absoluteFilePath()))
				{
					log("Synchronize failed; couldn't remove:",
						QDir::toNativeSeparators(des.absoluteFilePath()), Elevated);
					goto error;
				}
			}
		}
	}

	goto end;
	error:
	count=0;
	end:
	diskOp();
	return count;
}

bool QtCompile::filterDir(const QString &dirPath, bool isRoot)
{
	bool skip = false;
	FOR_CONST_IT(m_dirFilter)
	{
		if (!isRoot && (*IT).startsWith(SLASH))
		{
		//	qDebug()<<dirPath;
			continue;
		}

		if ((skip = dirPath.endsWith(*IT)))
			break;
	}
	return	skip;
}

bool QtCompile::filterExt(const QFileInfo &info)
{
	return m_extFilter.contains(info.suffix().toLower());
}

bool QtCompile::checkDir(int which)
{
	return checkDir(which, QString());
}

bool QtCompile::checkDir(int which, QString &path)
{
	int result = Critical;
	QString name;
	switch(which)
	{
	case Source:
		path = m_source;
		name = "Source directory";
		break;

	case Target:
		path = m_target;
		name = "Target directory";
		break;
	case Build:
		path = m_build;
		name = "Temporary drive";
		break;

	case Temp:
		path = m_btemp;
		name = "Temp directory";
		result = Warning;
		break;

	default:
		return false;
	}
	if (QDir(path).exists())
			result  = Informal;
	else	log(QString("%1 removed").arg(name), QDir::toNativeSeparators(path), result);
	return	result != Critical;
}

bool QtCompile::checkDir(int which, QString &path, int &count, bool skipRootFiles)
{
	count = 0;
	if (!checkDir(which, path))
		return false;

	QQueue<QDir> queue;
	queue.enqueue(QDir(path));
	bool filter =!m_extFilter.isEmpty();

	QFileInfoList files, dirs;
	int level = 0;
	QDir  dir;

	while(!queue.isEmpty())
	{
		level--;
		dir = queue.dequeue();
		if (filterDir(dir.absolutePath(), level >= 0))
			continue;

		files = dir.entryInfoList(QDir::Files);

		if (skipRootFiles)
			skipRootFiles = false;

		else FOR_CONST_IT(files)
			if (!filter || !filterExt(*IT))
				count++;

		dirs = dir.entryInfoList(QDir::AllDirs|QDir::NoDotAndDotDot);
		FOR_CONST_IT(dirs)
			queue.enqueue(QDir((*IT).absoluteFilePath()));

		if (level == -1)
			level  = dirs.count();
		if (level ==  0)
			level  = -1;
	}
	return count;
}

bool QtCompile::attachImdisk(QString &letter)
{
	if (!qtBuilderStaticDrive.isEmpty())
	{
		letter = qtBuilderStaticDrive;
		m_keepDisk = true;
		return true;
	}

	QString out;
	{	InlineProcess p(this, "imdisk.exe", "-l -n", true);
		out = p.stdOut();
	}
	if (out.contains(QString::number(m_imdiskUnit)))
	{
		InlineProcess p(this, "imdisk.exe", QString("-l -u %1").arg(m_imdiskUnit), true);
		QString inf = p.stdOut();
		if (inf.contains(imdiskDrive))
		{	//
			// TODO: in this case it would also be needed to extend the disk if needed
			//
			letter	 = getValueFrom(inf, imdiskDrive, ___LF);
			int size = getValueFrom(inf, imdiskSizeS, " ").toULongLong() /1024 /1024 /1024;
			log("Using existing RAM disk", QString("Drive letter %1, %2GB").arg(letter).arg(size));

			m_keepDisk = true;
			return true;
		}
		else while(out.contains(QString::number(m_imdiskUnit)))
		{
			m_imdiskUnit++;
		}
	}

	int size = m_bopts.value(RamDisk);
	log("Trying to attach RAM disk", QString("Drive letter %1, %2GB").arg(letter).arg(size));

	QString args = QString("-a -m %1: -u %2 -s %3G -o rem -p \"/fs:ntfs /q /y\"");
	args  = args.arg(letter).arg(m_imdiskUnit).arg(size);

	InlineProcess p(this, "imdisk.exe", args, false);
	return p.normalExit();
}

bool QtCompile::removeImdisk(bool silent, bool force)
{
	if (m_keepDisk)
		return true;

	if (!silent)
		log("Trying to remove RAM disk");

	QString args = QString("-%1 -u %2");
	args  = args.arg(force ? "D" : "d").arg(m_imdiskUnit);

	InlineProcess p(this, "imdisk.exe", args, silent);
	return p.normalExit();
}

const QString QtCompile::targetDir(int msvc, int arch, int type, QString &native, QStringList &t)
{	//
	//	USER_SET_LIB_PATH	...	something like				"D:\3d-Party\Qt"
	//	  \VERSION			... currently processed version	i.e. "4.8.7"
	//	    \TYPE			...	library type:				"shared" or "static"
	//	      \ARCHITECTURE	... MSVC $(Platform) compatible	"Win32" or "x64"
	//	        \MSVC_VER	...	MSVC $(PlatformToolset)		"v100" or "v110" or "v120" or "v130"
	//
	t = m_libPath.split(SLASH);
	t.append(m_version);
	t.append(bPaths[type]);
	t.append(bPaths[arch]);
	t.append(bPaths[msvc]);
	t.removeDuplicates();

	QString target = t.join(SLASH);
			native = QDir::toNativeSeparators(target);
	return	target;
}

const QString QtCompile::driveLetter()
{
	QFileInfoList drives = QDir::drives();
	QStringList aToZ;

	int a = (int)QChar('A').toLatin1();
	int z = (int)QChar('Z').toLatin1();
	for(int i = a; i <= z; i++)
		aToZ.append(QChar::fromLatin1(i));

	FOR_CONST_IT(drives)
		aToZ.removeOne((*IT).absolutePath().left(1).toUpper());
	if (aToZ.isEmpty())
		return QString();

	return aToZ.last();
}
