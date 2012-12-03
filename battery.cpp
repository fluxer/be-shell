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

#include <battery.h>
#include <QPainter>
#include <QPainterPath>
#include <solid/device.h>
#include <solid/devicenotifier.h>

#include <QtDebug>

static QPainterPath icon;

BE::Battery::Battery( QWidget *parent ) : QFrame(parent)
, BE::Plugged(parent)
, myACisPlugged(false)
, iAmCharging(false)
{
    if (icon.isEmpty())
    {
        icon.moveTo(0,0);
        icon.lineTo(90,0);
        icon.lineTo(90,25);
        icon.lineTo(100,25);
        icon.lineTo(100,75);
        icon.lineTo(90,75);
        icon.lineTo(90,100);
        icon.lineTo(0,100);
        icon.closeSubpath();
    }

    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceAdded(const QString&)), this, SLOT(addDevice(const QString&)));
    connect(Solid::DeviceNotifier::instance(), SIGNAL(deviceRemoved(const QString&)), this, SLOT(removeDevice(const QString&)));
    QMetaObject::invokeMethod(this, "collectDevices", Qt::QueuedConnection);
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
        iAmCharging |= cin;
        myCharge = 0;
        for (QMap<QString, int>::const_iterator it = myBatteries.constBegin(), end = myBatteries.constEnd(); it != end; ++it)
            myCharge += qAbs(*it);
        if (myBatteries.count())
            myCharge /= myBatteries.count();
    }
    update();
}


void BE::Battery::collectDevices()
{
    window()->setAttribute(Qt::WA_AlwaysShowToolTips);
    foreach( Solid::Device device, Solid::Device::listFromType(Solid::DeviceInterface::AcAdapter))
        addDevice(device.udi());
    foreach( Solid::Device device, Solid::Device::listFromType(Solid::DeviceInterface::Battery))
        addDevice(device.udi());
    update();
}

void
BE::Battery::paintEvent(QPaintEvent *pe)
{
    QColor c = palette().color(foregroundRole());
    QPainter p(this);
    p.setPen(Qt::NoPen);
    p.setRenderHint(QPainter::Antialiasing);

    QRect r(contentsRect());
    const int demH = 22*r.height()/10;
    if (demH > r.width())
        r.setHeight(10*r.width()/22);
    else
        r.setWidth(demH);
    r.moveCenter(contentsRect().center());
    const int w = r.width(), h = r.height();
    p.translate(r.topLeft());
    if (!iAmCharging)
    {
        p.translate(w/2,h/2);
        p.rotate(180);
        p.translate(-w/2,-h/2);
    }
    p.scale(w/100.0, h/100.0);

    c.setAlpha(96);
    p.setBrush(c);
    p.drawPath(icon);

    p.resetTransform();

    p.translate(r.topLeft());
    p.setClipRect(0, 0, w*myCharge/100, height());

    if (!iAmCharging)
    {
        p.translate(w/2,h/2);
        p.rotate(180);
        p.translate(-w/2,-h/2);
        p.scale(w/120.0, h/120.0);
        p.translate(10,10);
    }
    else
    {
        p.scale(w/120.0, h/120.0);
        p.translate(10,10);
    }

    c.setAlpha(192);
    p.setBrush(c);
    p.drawPath(icon);

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
        myCharge = 0;
        iAmCharging = false;
        for (QMap<QString, int>::const_iterator it = myBatteries.constBegin(), end = myBatteries.constEnd(); it != end; ++it)
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
    myCharge = 0;
    for (QMap<QString, int>::const_iterator it = myBatteries.constBegin(), end = myBatteries.constEnd(); it != end; ++it)
        myCharge += qAbs(*it);
    if (myBatteries.count())
        myCharge /= myBatteries.count();
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
        iAmCharging = false;
        for (QMap<QString, int>::const_iterator it = myBatteries.constBegin(), end = myBatteries.constEnd(); it != end; ++it)
        {
            if (*it > 0)
            {
                iAmCharging = true;
                break;
            }
        }
        update();
    }
}

QSize
BE::Battery::sizeHint() const
{
    int l,t,r,b;
    getContentsMargins(&l,&t,&r,&b);
    if (orientation() == Qt::Horizontal) {
        const int h = height() - (t+b);
        return QSize(11*h/5 + l+r, h + t+b);
    }
    const int w = width() - (l+r);
    return QSize(w + l+r, 5*w/11 + t+b);
}