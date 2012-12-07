/**************************************************************************
*   Copyright (C) 2009 by Thomas Lübking                                  *
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

#ifndef METER_H
#define METER_H

#include "be.plugged.h"

#include <QFile>
#include <QFrame>

namespace BE {

class Meter : public QFrame, public Plugged
{
    Q_OBJECT
public:
    Meter(QWidget *parent = 0);
    virtual void configure( KConfigGroup *grp );
    float percent(int v) const;
    void setLabel(QString label);
    void setPollInterval(int ms);
    void setRanges(int l1, int u1, int l2, int u2);
    void setValues(int n1, int n2);
    QSize sizeHint() const;
    inline int value(int i) { return myValue[i]; }
    inline int maximum(int i) { return myMaximum[i]; }
    inline int minimum(int i) { return myMinimum[i]; }
protected:
    void changeEvent(QEvent *e);
    void paintEvent(QPaintEvent *pe);
    virtual void poll() = 0;
    void timerEvent(QTimerEvent *te);
    int myPollInterval;
    QFile myFile;
private:
    int myMinimum[2], myMaximum[2], myValue[2];
    int myTimer, myFullScreenCheckTimer;
    bool iAmActive;
    QString myLabel;
};

class CpuMeter : public Meter
{
    Q_OBJECT
public:
    CpuMeter(QWidget *parent = 0);
    void configure( KConfigGroup *grp );
protected:
    virtual void poll();
private:
    int myIdleTime[2], myCpuTime[2];
    QString myCpu;
};

class RamMeter : public Meter
{
    Q_OBJECT
public:
    enum Mode { Used, NonInactive, Active };
    RamMeter(QWidget *parent = 0);
    void configure( KConfigGroup *grp );
protected:
    virtual void poll();
private:
    Mode myMode;
};

class NetMeter : public Meter
{
    Q_OBJECT
public:
    enum Mode { Dynamic, Up, Down };
    NetMeter( QWidget *parent = 0 );
    void configure( KConfigGroup *grp );
protected:
    virtual void poll();
private:
    QString speed(int i);
    QString myDevice;
    uint myLastValues[2];
    Mode myMode;
};

class HddMeter : public Meter
{
    Q_OBJECT
public:
    HddMeter( QWidget *parent = 0 );
    void configure( KConfigGroup *grp );
protected:
    virtual void poll();
private:
    QString speed(int i);
    QFile myFile;
    uint myLastValues[2];
};

}
#endif // METER_H
