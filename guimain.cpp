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

#include <QCheckBox>
//
// note: most of the checkbox stuff can be considered display dummies;
// a proper configuration system should...
//
// 1. call the selected "configure.exe", and parse all available options
// 2. use a configuration object class which encapsulates all selections
// (the current string lists are made for easy joining to cmd line args)
//
const QString qtBuilderHeaderStyle("%1 { background: %2; color: white;} %1:disabled { background: #363636; color: #969696; border-right: 1px solid #565656; }");
const QString qtBuilderDefault("!");

QStringList qtBuilderMatchLists(const QStringList &full, const QStringList &partial, bool forceOff = false)
{
	QString on, off, dont;
	QStringList result;
	FOR_CONST_IT(full)
	{
		on = off = dont = *IT;
		off	.prepend("-no"	);
		dont.prepend("-dont");

		if (partial.contains(off))
			result.append(off);
		else if (partial.contains(on))
			result.append(on);
		else if (partial.contains(dont))
			result.append(dont);
		else if (forceOff)
			result.append(off);
		else
			result.append(qtBuilderDefault+on);
	}
	return	result;
}

QStringList qtBuilderGlobalOpts()
{
	QStringList global;
	if (globals.contains		(_dbAndRl))
	{
		global.append			(_debuglb);
		global.append			(_release);
	}
	else if (globals.contains	(_debuglb))
	{
		global.append			(_debuglb);
		global.append(QString	(_release).replace("-","-no\""));
	}
	else if (globals.contains	(_release))
	{
		global.append(QString	(_debuglb).replace("-","-no\""));
		global.append			(_release);
	}
	QStringList  g;
	FOR_CONST_IT(globalOptions)
			g.append(QString(*IT).prepend("-no"));
	return	global+qtBuilderMatchLists(globalOptions, globals, true);
}

void QtBuilder::createUi()
{
	QWidget *wgt = new QWidget(this);
	setCentralWidget(wgt);

	QVBoxLayout *vlt = new QVBoxLayout(wgt);
	vlt->setContentsMargins(0, 0, 0, 0);
	vlt->setSpacing(0);

	QBoxLayout *lyt = vlt;
	createAddons(lyt);

	m_log = new QtAppLog(wgt);
	m_bld = new BuildLog(wgt);
	m_cpy = new CopyProgress(wgt);
	m_tmp = new DiskSpaceBar("Build ", Process, wgt);
	m_tgt = new DiskSpaceBar("Target", Elevated, wgt);

	connect(this, SIGNAL(log(const QString &, const QString&,int)),	m_log, SLOT(add(const QString &, const QString&,int)),	Qt::BlockingQueuedConnection);
	connect(this, SIGNAL(log(const QString &, int)),				m_log, SLOT(add(const QString &, int)),					Qt::BlockingQueuedConnection);
	connect(this, SIGNAL(progress(int, const QString &, qreal)),	m_cpy, SLOT(progress(int, const QString &, qreal)),		Qt::BlockingQueuedConnection);
	connect(this, SIGNAL(progress(int, const QString &, qreal)),	m_tmp, SLOT(refresh()),									Qt::QueuedConnection);
	connect(this, SIGNAL(progress(int, const QString &, qreal)),	m_tgt, SLOT(refresh()),									Qt::QueuedConnection);

	lyt->addWidget(m_log);
	vlt->addWidget(m_bld);
	vlt->addWidget(m_tmp);
	vlt->addWidget(m_cpy);
	vlt->addWidget(m_tgt);

	vlt->setStretch(0, 2);
	vlt->setStretch(1, 3);
	vlt->setStretch(3, 0);
	vlt->setStretch(4, 0);
	vlt->setStretch(5, 0);
}

void QtBuilder::createAddons(QBoxLayout *&lyt)
{
	QBoxLayout *hlt = new QHBoxLayout();
	hlt->setContentsMargins(0, 0, 0, 0);
	hlt->setSpacing(0);
	lyt->addLayout(hlt);

	createCfgOpt(hlt);
	createBldOpt(hlt);
	createAppOpt(hlt);

	lyt = new QVBoxLayout();
	lyt->setContentsMargins(0, 0, 0, 0);
	lyt->setSpacing(0);
	hlt->addLayout(lyt);

	QFont f(font());
	f.setPointSize(11);

	QLabel *lbl = new QLabel("BUILD PROGRESS LOG", lyt->parentWidget());
	lbl->setStyleSheet(qtBuilderHeaderStyle.arg(CLASS_NAME(QLabel), colors.at(AppInfo)));
	lbl->setAlignment(Qt::AlignHCenter|Qt::AlignVCenter);
	lbl->setFixedHeight(defGuiHeight);
	lbl->setFont(f);
	lyt->addWidget(lbl);
}

void QtBuilder::createCfgOpt(QBoxLayout *lyt)
{
	Selections *s = new Selections("CONFIGURATION", Process, centralWidget());
	s->setDisabled(true);

	s->append(qtBuilderMatchLists(targetSwitches, exclude));
	s->append(plugins);

	s->create();
	lyt->addWidget(s);
}

void QtBuilder::createBldOpt(QBoxLayout *lyt)
{
	Selections *s = new Selections("BUILD OPTIONS", Elevated, centralWidget());
	s->setDisabled(true);
	s->setSorting(false);

	s->append(qtBuilderGlobalOpts());
	s->append(qtBuilderMatchLists(compileSwitches, switches));
	s->append(qtBuilderMatchLists(featureSwitches, features));

	s->create();
	lyt->addWidget(s);
}

void QtBuilder::createAppOpt(QBoxLayout *lyt)
{
	m_sel = new Selections("BUILD REQUESTS", Warning, centralWidget());
	m_sel->setAutoSplit(true);
	m_sel->setTriState(false);

	m_sel->addModes(m_types);
	m_sel->addModes(m_archs);
	m_sel->addModes(m_msvcs);

	m_sel->create();
	lyt->addWidget(m_sel);

	connect(m_sel,	SIGNAL(selected(int)),			 this, SLOT(setup(int)));
	connect(this,	SIGNAL(current(const Modes &)), m_sel, SLOT(activate(const Modes &)), Qt::QueuedConnection);

	m_go = new QtButton("GO", "STOP", m_sel, true);
	m_sel->layout()->addWidget(m_go);

	connect(m_go, SIGNAL(isOn(bool)), this, SLOT(process(bool)), Qt::QueuedConnection);
}



QtHeader::QtHeader(const QString &text, int color, QWidget *parent) : QLabel(text, parent)
{
	setStyleSheet(qtBuilderHeaderStyle.arg(CLASS_NAME(QLabel), colors.at(color)));
	setAlignment(Qt::AlignHCenter|Qt::AlignVCenter);
	setFixedHeight(defGuiHeight);
	setFont(parent->font());
}

QtButton::QtButton(const QString &off, const QString &on, QWidget *parent, bool resetExternal) : QtHeader(off, Critical, parent),
	m_isOn(false), m_resetExtern(resetExternal)
{
	m_text.append(off);
	m_text.append(on);

	setCursor(Qt::PointingHandCursor);
	setFocusPolicy(Qt::NoFocus);

	QFont f(font());
	f.setPointSize(12);
	f.setBold(true);
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



Selections::Selections(const QString &label, int color, QWidget *parent)
	: QWidget(parent), m_disabled(false), m_sorting(true), m_autoSplit(false), m_triState(true)
{
	setStyleSheet("QWidget:disabled { border-right: 1px solid #D8D8D8; }");
	setMinimumWidth(180);

	QFont f(font());
	f.setPointSize(11);
	setFont(f);

	QVBoxLayout *lyt = new QVBoxLayout(this);
	lyt->setContentsMargins(0, 0, 0, 0);
	lyt->setSpacing(0);

	QtHeader *hdr = new QtHeader(label, color, this);
	lyt->addWidget(hdr);

	m_main = new QVBoxLayout();
	m_main->setContentsMargins(9, 6, 9, 9);
	m_main->setSpacing(3);
	lyt->addLayout(m_main);

	m_mapper = new QSignalMapper(this);
	connect(m_mapper, SIGNAL(mapped(const QString &)), this, SLOT(toggled(const QString &)));
}

void Selections::addSpacer()
{
	QFrame *lne = new QFrame(this);
	lne->setStyleSheet("QFrame { border-top: 1px solid #A2A2A2; } QFrame:disabled { border-top: 1px solid #CDCDCD;}");
	lne->setFrameShape(QFrame::NoFrame);
	lne->setFixedHeight(1);

	m_main->addSpacing(9);
	m_main->addWidget(lne);
	m_main->addSpacing(9);
}

void Selections::addModes(const Modes &modes)
{
	QStringList list;
	QString m;
	FOR_CONST_IT(modes)
	{
		m = modeLabels.at(IT.key());
		if (!IT.value())
			m.prepend("-no-");
		list.append(m);
	}
	append(list);
}

void Selections::create()
{
	QChar c, p;
	FOR_CONST_IT((*this))
	{
		QMap<QString, QCheckBox *> map;
		QCheckBox *chb;
		QString label;

		QStringList list = *IT;
		FOR_JT(list)
		{
			Qt::CheckState state;
			if (!parseOption(*JT, label, state))
				continue;

			chb = new QCheckBox(label, this);
			chb->setStyleSheet("QWidget { border: none; }");
			chb->setFocusPolicy(Qt::NoFocus);
			chb->setFont(font());

			chb->setDisabled(m_disabled);
			chb->setTristate(m_triState);
			chb->setCheckState(state);


			map.insert(label, chb);
			*JT = label;

			m_mapper->setMapping(chb, label); // just quick and (very) dirty.
			connect(chb, SIGNAL(toggled(bool)), m_mapper, SLOT(map()));
		}

		if (!m_autoSplit && IT != constBegin())
			addSpacer();

		if (m_sorting) FOR_CONST_JT(map)
		{
			if (m_autoSplit)
			{
				c = JT.key()[0];
				if (p != c)
				{	p  = c;

					if (m_main->count())
						addSpacer();
				}
			}
			m_main->addWidget(JT.value());
		}
		else FOR_CONST_JT(list)
		{
			m_main->addWidget(map.value(*JT));
		}
	}

	m_main->addItem(new QSpacerItem(0, 0, QSizePolicy::Fixed, QSizePolicy::Expanding));
}

void Selections::toggled(const QString &name)
{
	emit selected(modeLabels.indexOf(name));
}

void Selections::activate(const Modes &modes)
{
	QFont f(font());
	QString label;
	FOR_CONST_IT(modes)
	{
		f.setBold(IT.value());
		label = modeLabels.at(IT.key());
		qobject_cast<QCheckBox *>(m_mapper->mapping(label))->setFont(f);
	}
}

bool Selections::nextCharUppper(const QChar &c) const
{
	return c.isDigit()||c.isSpace()||c.isSymbol();//||c.isPunct();
}

bool Selections::parseOption(const QString &opt, QString &label, Qt::CheckState &state)
{
	label = opt;
	state = Qt::Checked;

	if (label.startsWith("-nomake"))
	{
		state = Qt::Unchecked;
		label.remove("-nomake");
	}
	else if (label.startsWith("-no"))
	{
		state = Qt::Unchecked;
		label.remove("-no");
	}
	else if (label.startsWith("-dont"))
	{
		state = Qt::Unchecked;
		label.remove("-dont");
	}
	else if (label.startsWith(qtBuilderDefault))
	{
		state = Qt::PartiallyChecked;
		label.remove(qtBuilderDefault);
	}
	else if (label.isEmpty())
		return false;

	label = label.replace("-"," ").trimmed();
//	name.prepend(" ");int spc = 0;
//	while((spc = name.indexOf(" ", spc)) != -1)
//		name[++spc] = name[spc].toUpper();

	if (label == "dbus")
	{	label = "D-Bus";
		return true;
	}
	if (label == "3dnow")
	{	label  = "3DNow";
		return true;
	}
//
//	sloppy upper-case parsing ahead...
//
	QChar c; QString l;
	int count = label.length()-1;
	for(int i = 0; i < count; i++)
	{
		c = label[i];
		if (nextCharUppper(c))
			label[i+1] = label[i+1].toUpper();
		else l+=c;
	}
	if ( l.length()<4 )
		 label = label.toUpper();
	else label[0] = label[0].toUpper();
	return true;
}
