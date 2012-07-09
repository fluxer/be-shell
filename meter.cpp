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

#include "meter.h"
#include "be.shell.h"

// #include <QDBusConnection>
// #include <QProcess>
#include <QPainter>
#include <QTimerEvent>

#include <KDE/KConfigGroup>

BE::Meter::Meter( QWidget *parent ) : QFrame(parent), BE::Plugged(parent), myTimer(0), myFullScreenCheckTimer(0), iAmActive(true)
{
    setMinimumHeight(2*QFontMetrics(font()).height());
    setSizePolicy(QSizePolicy::Fixed, QSizePolicy::Fixed);
    setRanges(0,0,0,0);
    setValues(0,0);
}

void
BE::Meter::configure( KConfigGroup *grp )
{
    setPollInterval(grp->readEntry("PollInterval", 1000));
    myLabel = grp->readEntry("Label", "");
}

void
BE::Meter::changeEvent(QEvent *e)
{
    if (e->type() == QEvent::FontChange)
        setMinimumHeight(2*QFontMetrics(font()).height());
    QFrame::changeEvent(e);
}

// 3.46410161513775458705 == 2*tan(60°)
#define RATIO 3.46410161513775458705

void
BE::Meter::paintEvent(QPaintEvent *pe)
{
    QFrame::paintEvent(pe);

    const QRect cr = contentsRect();
    const int s = qMin(int(RATIO*cr.height()), cr.width());
    QRect r( cr.x() + qAbs(cr.width()-s)/2, cr.y() + qAbs(cr.height()-int(s/RATIO))/2, s, s );
    int t = r.height()/10;
    int t_2 = (t+1)/2;
    const float pct0 = percent(0);

    QColor c(palette().color(foregroundRole()));
    const int a = c.alpha();
    c.setAlpha(2*c.alpha()/3);
    QColor c2(palette().color(backgroundRole()));

    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(c, t, Qt::SolidLine, Qt::RoundCap));

    r.adjust(t_2,t_2,-t_2,-t_2);
//     p.drawArc(r, 150<<4, -(120<<4)*pct0);
    p.drawArc(r, (150<<4) - (115<<4)*pct0, -5<<4);

    t += t_2;
    r.adjust(t,t,-t,-t);
    t = r.height()/6;
    c2.setRgb( (c.red()+c2.red())/2, (c.green()+c2.green())/2, (c.blue()+c2.blue())/2, c.alpha() );
    p.setPen(QPen(c2, t, Qt::SolidLine, Qt::RoundCap));
    p.drawArc(r, 150<<4, -(120<<4)*percent(1));

    c.setAlpha(a);
    p.setPen(c);
    p.drawText(cr, Qt::AlignHCenter|Qt::AlignBottom, myLabel );

    p.end();
}

float
BE::Meter::percent(int v) const
{
    if (myMaximum[v] > myMinimum[v])
        return (myValue[v] - myMinimum[v]) / float(myMaximum[v] - myMinimum[v]);
    qWarning("Meter: invalid range!");
    return 0.0;
}

void
BE::Meter::setLabel(QString label)
{
    if (myLabel == label)
        return;
    myLabel = label;
    update();
}

void
BE::Meter::setPollInterval(int ms)
{
    myPollInterval = ms;
    if (myTimer)
        killTimer(myTimer);
    myTimer = 0;
    if (ms > 0)
        myTimer = startTimer(ms);
    if (!myFullScreenCheckTimer)
        myFullScreenCheckTimer = startTimer(10000);
}

void
BE::Meter::setRanges(int l1, int u1, int l2, int u2)
{
    const bool dirty = myMinimum[0] != l1 || myMinimum[1] != l2 || myMaximum[0] != u1 || myMaximum[1] != u2;
    myMinimum[0] = l1;
    myMinimum[1] = l2;
    myMaximum[0] = u1;
    myMaximum[1] = u2;
    if (dirty)
        update();
}

void
BE::Meter::setValues(int n1, int n2)
{
    const bool dirty = myValue[0] != n1 || myValue[1] != n2;
    myValue[0] = n1;
    myValue[1] = n2;
    if (dirty)
        update();
}

QSize
BE::Meter::sizeHint() const
{
    return QSize(RATIO*height(), height());
}

void
BE::Meter::timerEvent(QTimerEvent *te)
{
    if (te->timerId() == myTimer)
        { if (iAmActive) poll(); }
    else if (te->timerId() == myFullScreenCheckTimer)
        iAmActive = !BE::Shell::hasFullscreenAction();
    else
        QWidget::timerEvent(te);
}

BE::CpuMeter::CpuMeter(QWidget *parent) : Meter(parent)
{
    myCpuTime[0] = myCpuTime[1] = myIdleTime[0] = myIdleTime[1] = 0;
    setRanges(0,1000,0,1000);
    myFile.setFileName("/proc/stat");
    myFile.open(QIODevice::ReadOnly);
    poll();
}

void
BE::CpuMeter::configure( KConfigGroup *grp )
{
    myCpu = grp->readEntry("Cpu", "cpu");
    Meter::configure(grp);
}

void
BE::CpuMeter::poll()
{
    if (!myFile.isOpen())
        return;

    myFile.reset();
    for (int i = 0; i<8;++i)
    {
        QString line = myFile.readLine();
        if (line.startsWith(myCpu))
        {
            QStringList list = line.split(' ', QString::SkipEmptyParts);
            if (list.count() > 7) // probably valid line ;-)
            {
                if (list.at(0) != myCpu)
                    continue;
                myIdleTime[0] = myIdleTime[1];
                myCpuTime[0]  = myCpuTime[1];
                myCpuTime[1] = 0;
                for (int i = 1; i < 8; ++i)
                {
                    if (i == 4)
                        myCpuTime[1] += (myIdleTime[1] = list[i].toInt());
                    else
                        myCpuTime[1] += list[i].toInt();
                }
                const int v = 1000-1000*(myIdleTime[1]-myIdleTime[0])/(myCpuTime[1]-myCpuTime[0]);
                setValues( (value(1)+2*v)/3, (v+3*value(1))/4 );
                setLabel(QString::number( 100.0*percent(0), 'f', 1 ));
            }
            break;
        }
    }
}

BE::RamMeter::RamMeter(QWidget *parent) : Meter(parent)
{
    myFile.setFileName("/proc/meminfo");
    myFile.open(QIODevice::ReadOnly);
    poll();
}

void
BE::RamMeter::configure( KConfigGroup *grp )
{
    myMode = Used;
    QString mode = grp->readEntry("Mode", "Used");
    if (mode == "NonInactive")
        myMode = NonInactive;
    else if (mode == "Active")
        myMode = Active;
    Meter::configure(grp);
    poll();
}

void
BE::RamMeter::poll()
{
    if (!myFile.isOpen())
        return;

    myFile.reset();
    int found = 0;
    int values[6] = {0,0,0,0,0}; // MemTotal, MemFree, Active, Inactive, SwapTotal, SwapFree
    while (found < 6)
    {
        QString line = myFile.readLine();
        if (line.isEmpty())
            break; // this should simply not happen...
        if (line.startsWith("MemTotal:"))
            values[0] = line.section( ' ', 1, 1, QString::SectionSkipEmpty ).toInt();
        else if (line.startsWith("MemFree:"))
            values[1] = line.section( ' ', 1, 1, QString::SectionSkipEmpty ).toInt();
        else if (line.startsWith("Active:"))
            values[2] = line.section( ' ', 1, 1, QString::SectionSkipEmpty ).toInt();
        else if (line.startsWith("Inactive:"))
            values[3] = line.section( ' ', 1, 1, QString::SectionSkipEmpty ).toInt();
        else if (line.startsWith("SwapTotal:"))
            values[4] = line.section( ' ', 1, 1, QString::SectionSkipEmpty ).toInt();
        else if (line.startsWith("SwapFree:"))
            values[5] = line.section( ' ', 1, 1, QString::SectionSkipEmpty ).toInt();
        else
            continue;
        ++found;
    }
    if (found == 6) // ...
    {
        setRanges( 0, values[0], 0, values[4] );
        if ( myMode == Active )
            setValues( values[2], values[4] - values[5] );
        else if ( myMode == NonInactive )
            setValues( values[0] - (values[1] + values[3]), values[4] - values[5] );
        else // Used
            setValues( values[0] - values[1], values[4] - values[5] );
        setLabel(QString::number( 100.0*percent(0), 'f', 1 ));
    }
}

BE::NetMeter::NetMeter( QWidget *parent ) : BE::Meter(parent)
{
    myLastValues[0] = myLastValues[1] = 0;
}

void
BE::NetMeter::configure( KConfigGroup *grp )
{
    myDevice = "/sys/class/net/" + grp->readEntry("Device", "eth0") + "/statistics/";
    Meter::configure(grp);
    setRanges(0, grp->readEntry("MaxDown", 0), 0, grp->readEntry("MaxUp", 0));
    poll();
}

void
BE::NetMeter::poll()
{
    uint read = 0, written = 0;
    QFile f(myDevice + "rx_bytes");
    if (f.open(QIODevice::ReadOnly))
    {
        read = QString(f.readLine()).toUInt();
        f.close();
    }
    QFile f2(myDevice + "tx_bytes");
    if (f2.open(QIODevice::ReadOnly))
    {
        written = QString(f2.readLine()).toUInt();
        f2.close();
    }

    setValues( 8*(read - myLastValues[0])/myPollInterval, 8*(written - myLastValues[1])/myPollInterval );
    if (myLastValues[0] != 0 && myLastValues[1] != 0)
        setRanges(0, qMax(value(0),maximum(0)), 0, qMax(value(1),maximum(1)));

    const bool up = maximum(1)/(value(1)+1) < maximum(0)/(value(0)+1);
    QFont fnt = font();
    fnt.setOverline(up);
    fnt.setUnderline(!up);
    setFont(fnt);
    setLabel( speed(up) );

    myLastValues[0] = read;
    myLastValues[1] = written;
}

QString
BE::NetMeter::speed( int i )
{
    int v = value(i); // 1000*bits/s
    if ( v > 999999 )
        return QString::number( v/1000000.0, 'f', 1 ) + "G"; // maybe one day.... ;-)
    if ( v > 999 )
        return QString::number( v/1000.0, 'f', 1 ) + "M"; // today....
    return QString::number(v) + "k"; // yesterday....
}

BE::HddMeter::HddMeter( QWidget *parent ) : BE::Meter(parent)
{
    myLastValues[0] = myLastValues[1] = 0;
}

void
BE::HddMeter::configure( KConfigGroup *grp )
{
    myFile.close();
    myFile.setFileName("/sys/block/" + grp->readEntry("Device", "sda") + "/stat");
    Meter::configure(grp);
    setRanges(0, grp->readEntry("MaxRead", 0), 0, grp->readEntry("MaxWrite", 0));
    myFile.open(QIODevice::ReadOnly);
    poll();
}

void
BE::HddMeter::poll()
{
    if (!myFile.isOpen())
        return;
    
    myFile.reset();
    QStringList list = QString(myFile.readLine()).split(' ', QString::SkipEmptyParts);
    if (list.count() <  11)
        return; // WRONG! - should have 11 elements...

    const uint read     = 512*list.at(2).toUInt();
    const uint written  = 512*list.at(6).toUInt();
    setValues( (read - myLastValues[0])*1000/myPollInterval, (written - myLastValues[1])*1000/myPollInterval );
    if (myLastValues[0] != 0 && myLastValues[1] != 0)
        setRanges(0, qMax(value(0),maximum(0)), 0, qMax(value(1),maximum(1)));
//     setRanges(0, qMax(value(0), maximum(0))read*1000/(list.at(3).toUInt()+1), 0, written*1000/(list.at(7).toUInt()+1));

    const bool write = maximum(1)/(value(1)+1) < maximum(0)/(value(0)+1);
    QFont fnt = font();
    fnt.setOverline(!write);
    fnt.setUnderline(write);
    setFont(fnt);
    setLabel( speed(write) );

    myLastValues[0] = read;
    myLastValues[1] = written;
}

QString
BE::HddMeter::speed( int i )
{
    int v = value(i); // bytes/s
    if ( v > (1<<30) )
        return QString::number( v/1073741824.0, 'f', 1 ) + "G"; // maybe one day.... ;-)
    if ( v > (1<<20) )
        return QString::number( v/1048576.0, 'f', 1 ) + "M"; // today....
    if ( v > (1<<10) )
        return QString::number( v/1024.0, 'f', 1 ) + "k"; // today.... not much to do ;-)
    return QString::number(v) + "B"; // lazy day.... ;-)
}

