/**************************************************************************
*   Copyright (C) 2009 by Thomas Luebking                                  *
*   thomas.luebking@web.de                                                *
*   Copyright (C) 2009 by Ian Reinhart Geiser                             *
*   geiseri@kde.org                                                       *
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

#include "clock.h"

#include <KDE/KConfigGroup>
#include <KDE/KPopupFrame>
#include <KDE/KDatePicker>
#include <KDE/KCMultiDialog>
#include <KDE/KSystemTimeZones>

#include <QDateTime>
#include <QInputDialog>
#include <QMenu>
#include <QMouseEvent>
#include <QTimerEvent>

BE::Clock::Clock(QWidget *parent, const QString &pattern ) : QLabel(parent), BE::Plugged(parent)
{
    setObjectName("Clock");
    setAlignment( Qt::AlignCenter );
    myPattern = pattern;
    myTzSecOffset = 0;
    myCountDown = -1;

    myConfigMenu = new QMenu(this);
    myConfigMenu->setSeparatorsCollapsible(false);
    myConfigMenu->addSeparator()->setText(i18n("Configure Clock"));
    myConfigMenu->addAction(i18n("Change pattern..."), this, SLOT(configPattern()));
    myConfigMenu->addAction(i18n("Set Time..."), this, SLOT(configTime()));
    myConfigMenu->addSeparator();
    myConfigMenu->addAction(i18n("CountDown..."), this, SLOT(startCountDown()));
//     myConfigMenu->addSeparator();
//     myConfigMenu->addAction("Reminders...", this, SLOT(configReminder()));

    myTimer = startTimer(1000); // check every second
    updateTime();
}

void
BE::Clock::configure( KConfigGroup *grp )
{
    const QString s = myPattern;
    myPattern = grp->readEntry("Pattern", "hh:mm\nddd, MMM d");

    const QDateTime cdt = QDateTime::currentDateTime();
    const QString homeZone = grp->readEntry( "TimeZone", KSystemTimeZones::local().name() );
    const QDateTime hcdt = KSystemTimeZones::local().convert(KSystemTimeZones::zone(homeZone), cdt);
    const qint64 offset = myTzSecOffset;
    myTzSecOffset = cdt.secsTo(hcdt);

    if ( s != myPattern || myTzSecOffset != offset)
        updateTime();
}

void
BE::Clock::configTime()
{
    KCMultiDialog timeConf(this);
    timeConf.addModule(QLatin1String("clock"));
    timeConf.exec();
}

void
BE::Clock::configPattern()
{
    bool ok;
    
    const char *helpText =
    "<html><b>Date:</b>"
    "<dl>"
    "<dt>d dd / M MM</dt><dd>day/month (e.g. 5 or 05)</dd>"
    "<dt>ddd dddd / MMM MMMM</dt><dd>short or long day/month name ('Dec' 'December')</dd>"
    "<dt>yy yyyy</dt><dd>year (09 2009)</dd>"
    "</dl><dl>"
    "<b>Time:</b>"
    "<dt>h hh / m mm / s ss</dt><dd>hour/minute/second (w/o w leading '0')</dd>"
    "<dt>AP/ap</dt><dd>'AM'|'PM' / 'am'|'pm'</dd>"
    "<dt>H HH</dt><dd>hour, 24h ignoring AM/PM</dd>"
    "</dl><dl>"
    "<b>Format strings:</b>"
    "<dt>\\n</dt><dd>Line break</dd>"
    "<dt>\\t</dt><dd>Tabulator</dd>"
    "</dl></html>";
    
    QString text = QInputDialog::getText(this, i18n("Setup Clock pattern"), helpText, QLineEdit::Normal, myPattern, &ok);
    if ( ok && !text.isEmpty() )
        { myPattern = text; updateTime(); }
    Plugged::saveSettings();
}

// void
// BE::Clock::configReminder()
// {
//     
// }

void
BE::Clock::saveSettings( KConfigGroup *grp )
{
    grp->writeEntry( "Pattern", myPattern );
}

bool
BE::Clock::event(QEvent *ev)
{
    if (ev->type() == QEvent::Timer)
    {
        if (static_cast<QTimerEvent*>(ev)->timerId() == myTimer) {
            if (myCountDown > -1)
                --myCountDown;
            updateTime();
            return true;
        }
    }
    else if (ev->type() == QEvent::ToolTip)
        setToolTip( "<html><h3>" + QDate::currentDate().toString(Qt::DefaultLocaleLongDate) + "</h3></html>" );
    return QLabel::event(ev);
}

void
BE::Clock::mousePressEvent(QMouseEvent *ev)
{
    if( ev->button() == Qt::LeftButton )
    {
        KPopupFrame calenderPopup(this);
        calenderPopup.setProperty("KStyleFeatureRequest", property("KStyleFeatureRequest").toUInt() | 1); // "Shadowed"
        calenderPopup.setObjectName("Calendar");
        KDatePicker *calender = new KDatePicker(&calenderPopup);
        calenderPopup.setMainWidget(calender);
        calender->adjustSize();
        calenderPopup.adjustSize();
        calenderPopup.exec( popupPosition(calenderPopup.size()) );
    }
    else if( ev->button() == Qt::RightButton )
        myConfigMenu->exec(QCursor::pos());
}

void
BE::Clock::startCountDown()
{
    bool ok;
    const double d = QInputDialog::getDouble(this, i18n("Start Count Down"), i18n("<h3>Fun fact:</h3>The countdown was invented by <b>Fritz Lang</b><br/>for the 1928 movie <b>Frau im Mond</b><br/>to raise suspense for the rocket launch scene<h3>Enter minutes:</h3>"), 5, 0, 60, 2, &ok, Qt::Dialog|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint );
    if (ok) {
        myCountDown = 60*(int(d) + (d - (int(d))));
        updateTime();
    }
}

void
BE::Clock::updateTime() {
    if (myCountDown > -1) {
        QDateTime dt(QDateTime::currentDateTime());
        const int h = myCountDown/60;
        dt.setTime(QTime(h, myCountDown - 60*h));
        setText( dt.toString(myPattern) );
    }
    else
        setText( QDateTime::currentDateTime().addSecs(myTzSecOffset).toString(myPattern) );
}