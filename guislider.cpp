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

#include <QStyleOptionSlider>
#include <QPainter>

QtSlider::QtSlider(int option, int bgnd, int color, QWidget *parent) : QSlider(parent),
	m_clickSet(false), m_sliding(false), m_option(option)
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
		emit optionChanged(m_option, value());
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
	{
		setValue(valueFromPosition(event->pos()));
		emit optionChanged(m_option, value());
	}

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
	QColor text = Qt::white;
	QBrush high = pl.highlight();
	QBrush bgnd = pl.background();
	bool enable = wgt->isEnabled();
	if (!enable)
	{
		high = QBrush("#C2C2C2");
		bgnd = QBrush("#969696");
	}
	else if ((opt->state & QStyle::State_MouseOver) &&
			 (opt->activeSubControls & SC_SliderHandle))
	{
		text = high.color();
		high = QBrush(Qt::white);
	}

	const QtSlider *qsl = qobject_cast<const QtSlider *>(wgt);
	QRectF h = subControlRect(CC_Slider, opt, SC_SliderHandle, wgt);
	QRect  r = opt->rect;
	int  off = r.width()/20;
	int  val = qsl->value();
	int  min = qsl->minimum();
	int  max = qsl->maximum();
	bool lft = val*2-min > (max-min);

	p->setRenderHint(QPainter::TextAntialiasing);
	p->fillRect(r, bgnd);

	QTextOption o;
	if (lft) o.setAlignment(Qt::AlignLeft |Qt::AlignVCenter);
	else	 o.setAlignment(Qt::AlignRight|Qt::AlignVCenter);

	p->setPen(Qt::black);
	p->drawText(r.adjusted(off,0,-off,0), wgt->objectName(), o);
	p->fillRect(h, high);

	QFont f = p->font();
	f.setPointSize(14);
	p->setFont(f);

	p->setPen(text);
	o.setAlignment(Qt::AlignHCenter|Qt::AlignVCenter);
	p->drawText(h, QString::number(val), o);
}
