/**************************************************************************
*   Copyright (C) 2011 by Thomas Luebking                                 *
*   thomas.luebking@web.de                                                *
*                                                                         *
*   This program is free software; you can redistribute it and/or modify  *
*   it under the terms of the GNU General Public License as published by  *
*   the Free Software Foundation; either version 2 of the License, or     *
*   (at your option) any later version.                                   *
*                                                                         *
*   This program is distributed in the hope that it will be useful,       *
*   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
*   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
*   GNU General Public License for more details.                          *
*                                                                         *
*   You should have received a copy of the GNU General Public License     *
*   along with this program; if not, write to the                         *
*   Free Software Foundation, Inc.,                                       *
*   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
***************************************************************************/

#include "battery.h"

#include <KDE/KConfigGroup>

#include <QEvent>
#include <QPainter>
#include <QPainterPath>
#include <QStyle>
#include <QStyleOptionToolButton>
#include <solid/device.h>
#include <solid/devicenotifier.h>

#include <QtDebug>

static QPainterPath gs_icon;

BE::Battery::Battery( QWidget *parent ) : Button(parent)
, myACisPlugged(false)
, iAmCharging(false)
, iAmDirty(false)
, myFillIsCharge(true)
{
    if (gs_icon.isEmpty())
    {
        gs_icon.moveTo(0,0);
        gs_icon.lineTo(90,0);
        gs_icon.lineTo(90,25);
        gs_icon.lineTo(100,25);
        gs_icon.lineTo(100,75);
        gs_icon.lineTo(90,75);
        gs_icon.lineTo(90,100);
        gs_icon.lineTo(0,100);
        gs_icon.closeSubpath();
    }

    setProperty("charging", false);
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceAdded(const QString&)), this, SLOT(addDevice(const QString&)));
    connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceRemoved(const QString&)), this, SLOT(removeDevice(const QString&)));
    QMetaObject::invokeMethod(this, "collectDevices", Qt::QueuedConnection);
}

void
BE::Battery::configure(KConfigGroup *grp)
{
    Button::configure(grp);
    myFillIsCharge = grp->readEntry("FillIsCharge", true);
}

void
BE::Battery::addDevice( const QString &udi )
{
    Solid::Device dev(udi);
    if (Solid::AcAdapter *ac = dev.as<Solid::AcAdapter>())
    {
        connect(ac, SIGNAL(plugStateChanged(bool, const QString&)), SLOT(setAcPlugged(bool, const QString&)));
        myACs.insert(udi, ac->isPlugged());
        myACisPlugged |= ac->isPlugged();
    }
    else if (Solid::Battery *battery = dev.as<Solid::Battery>())
    {
//         connect(battery, SIGNAL(plugStateChanged(bool, const QString&)), SLOT(rPlugged(bool, const QString&)));
        connect(battery, SIGNAL(chargePercentChanged(int, const QString&)), SLOT(setCharge(int, const QString&)));
        connect(battery, SIGNAL(chargeStateChanged(int, const QString&)), SLOT(setState(int, const QString&)));
        const bool cin = battery->chargeState() == Solid::Battery::Charging;
        myBatteries.insert(udi, cin ? battery->chargePercent() : -battery->chargePercent());
        countCharge();
    }
    update();
}


bool BE::Battery::event(QEvent *e)
{
    if (e->type() == QEvent::Polish || e->type() == QEvent::StyleChange) {
        QStyleOptionToolButton opt;
        initStyleOption(&opt);
        myPadding = style()->sizeFromContents(QStyle::CT_ToolButton, &opt, size(), this) - size();
    }
    return Button::event(e);
}

void BE::Battery::collectDevices()
{
    foreach( Solid::Device device, Solid::Device::listFromType(Solid::DeviceInterface::AcAdapter))
        addDevice(device.udi());
    foreach( Solid::Device device, Solid::Device::listFromType(Solid::DeviceInterface::Battery))
        addDevice(device.udi());
    update();
}

void
BE::Battery::repolish()
{
    if (!iAmDirty)
        QMetaObject::invokeMethod(this, "_repolish", Qt::QueuedConnection);
    iAmDirty = true;
}

void
BE::Battery::_repolish()
{
    iAmDirty = false;
    style()->unpolish(this);
    style()->polish(this);
}

void
BE::Battery::countCharge()
{
    const int lastCharge = myCharge;
    const bool wasCharging = iAmCharging;
    myCharge = 0;
    iAmCharging = false;
    for (QMap<QString, int>::const_iterator it = myBatteries.constBegin(),
                                            end = myBatteries.constEnd(); it != end; ++it)
    {
        if (*it > 0)
        {
            iAmCharging = true;
            myCharge += *it;
        }
        else
            myCharge -= *it;
    }
    if (myBatteries.count())
        myCharge /= myBatteries.count();

    if (wasCharging != iAmCharging) {
        setProperty("charging", iAmCharging);
        repolish();
    }

    if (myCharge >= 67) {
        if (lastCharge < 67) {
            setProperty("charge", "high");
            repolish();
        }
    } else if (myCharge >= 34) {
        if (lastCharge < 34 || lastCharge >= 67) {
            setProperty("charge", "mid");
            repolish();
        }
    } else if (lastCharge > 33) {
        setProperty("charge", "low");
        repolish();
    }
    setToolTip(QString::number(myCharge) + '%');
}

void
BE::Battery::paintEvent(QPaintEvent *pe)
{
    Button::paintEvent(pe);

    // contentsRect() seems broken with QStyleSheetStyle :-(
    QRect r(rect().adjusted(myPadding.width(), myPadding.height(), 0, 0));
    const int demH = 22*r.height()/10;
    if (demH > r.width())
        r.setHeight(10*r.width()/22);
    else
        r.setWidth(demH);
    r.adjust((width() - r.width()) % 2, (height() - r.height()) % 2, 0, 0);
    r.moveCenter(rect().center());
    const int w = r.width(), h = r.height();

    QPainter p(this);
    p.setPen(Qt::NoPen);
    p.setRenderHint(QPainter::Antialiasing);
    p.translate(r.topLeft());
    if (!iAmCharging) {
        p.translate(w,0);
        p.scale(-1,1);
    }
    p.scale(w/100.0, h/100.0);

    p.setBrush(palette().brush(foregroundRole()));
    p.drawPath(gs_icon);

    p.resetTransform();

    p.translate(r.topLeft());
    if (myFillIsCharge)
        p.setClipRect(0, 0, w*myCharge/100, height());
    else if (iAmCharging)
        p.setClipRect(w*myCharge/100, 0, width(), height());
    else
        p.setClipRect(0, 0, w*(100-myCharge)/100, height());

    if (!iAmCharging) {
        p.translate(w,0);
        p.scale(-1, 1);
    }
    p.scale(w/120.0, h/120.0);
    p.translate(10,10);

    p.setBrush(palette().brush(QPalette::Highlight));
    p.drawPath(gs_icon);

    p.end();
}

void
BE::Battery::removeDevice( const QString &udi )
{
    if (myACs.remove( udi ))
    {
        myACisPlugged = false;
        for (QMap<QString, bool>::const_iterator it = myACs.constBegin(), end = myACs.constEnd(); it != end; ++it)
            myACisPlugged |= *it;
    }
    else if (myBatteries.remove( udi ))
    {
        countCharge();
    }
    update();
}

void
BE::Battery::setAcPlugged(bool nowPlugged, const QString &udi)
{
    QMap<QString, bool>::iterator wasPlugged = myACs.find(udi);
    if (wasPlugged == myACs.end())
        return; // should not happen
    if (nowPlugged != *wasPlugged)
    {
        *wasPlugged = nowPlugged;
        myACisPlugged = false;
        for (QMap<QString, bool>::const_iterator it = myACs.constBegin(), end = myACs.constEnd(); it != end; ++it)
            myACisPlugged |= *it;
        update();
    }
}

void
BE::Battery::setBatteryPlugged(bool nowPlugged, const QString &udi)
{

}

void
BE::Battery::setCharge(int value, const QString &udi)
{
    QMap<QString, int>::iterator charge = myBatteries.find(udi);
    if (charge == myBatteries.end())
        return; // should not happen
    *charge = *charge > 0 ? value : -value;
    countCharge();
    update();
}

void
BE::Battery::setState(int state, const QString &udi)
{
    QMap<QString, int>::iterator charge = myBatteries.find(udi);
    if (charge == myBatteries.end())
        return; // should not happen
    if (((state == Solid::Battery::Charging) && *charge < 0) ||
        ((state != Solid::Battery::Charging) && *charge > -1))
    {
        *charge = -(*charge);
        const bool wasCharging = iAmCharging;
        iAmCharging = false;
        for (QMap<QString, int>::const_iterator it = myBatteries.constBegin(), end = myBatteries.constEnd(); it != end; ++it)
        {
            if (*it > 0)
            {
                iAmCharging = true;
                break;
            }
        }
        if (wasCharging != iAmCharging) {
            setProperty("charging", iAmCharging);
            repolish();
        }
        update();
    }
}

QSize
BE::Battery::sizeHint() const
{
    if (orientation() == Qt::Horizontal) {
        const int h = height() - myPadding.height();
        return QSize(11*h/5, h) + myPadding;
    }
    const int w = width() - myPadding.width();
    return QSize(w, 5*w/11) + myPadding;
}