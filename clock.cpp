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
#include "be.shell.h"
#include <KDE/KConfigGroup>
#include <KDE/KPopupFrame>
#include <KDE/KCMultiDialog>
#include <KDE/KSystemTimeZones>

#include <QAbstractItemView>
#include <QDateTime>
#include <QInputDialog>
#include <QLocale>
#include <QMenu>
#include <QMouseEvent>
#include <QTimerEvent>
#include <QToolButton>

static BE::CalendarWidget *gs_calendar = 0;

BE::CalendarWidget::CalendarWidget(QWidget *parent) : QCalendarWidget(parent)
{
    connect(this, SIGNAL(activated(const QDate&)), SLOT(runActionOn(const QDate&)));
    configure();
    setFirstDayOfWeek(QLocale::system().firstDayOfWeek());
    if (QAbstractItemView *view = findChild<QAbstractItemView*>("qt_calendar_calendarview"))
        view->viewport()->installEventFilter(this);
    else
        qWarning("could not event handle view, activation on single click!");
    if (QToolButton *btn = findChild<QToolButton*>("qt_calendar_prevmonth")) {
        btn->setIcon(QIcon());
        btn->setText("<");
    }
    if (QToolButton *btn = findChild<QToolButton*>("qt_calendar_nextmonth")) {
        btn->setIcon(QIcon());
        btn->setText(">");
    }
}

void BE::CalendarWidget::configure()
{
    KConfigGroup grp = KSharedConfig::openConfig("be.shell")->group("BE::Calendar");
    myCommand = grp.readEntry("Command", QString());
    setGridVisible(grp.readEntry("ShowGrid", false));
    QFont fnt(font());
    fnt.setPointSize(grp.readEntry("FontSize", 17));
    setFont(fnt);
    adjustSize();
    resize(size()*grp.readEntry("SizeFactor", 1.618));
    iNeedToReconfigure = false;
}

void BE::CalendarWidget::runActionOn(const QDate &date)
{
    if (myLastClick.isValid() && myLastClick.elapsed() < 30)
        return;
    if (iNeedToReconfigure)
        configure();
    const QString cmd = date.toString(myCommand);
//     qDebug() << cmd;
    if (!cmd.isEmpty())
        BE::Shell::run(date.toString(myCommand));
}

bool BE::CalendarWidget::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::MouseButtonRelease) {
        myLastClick.start();
        return false;
    }
    if (e->type() == QEvent::MouseButtonDblClick) {
        myLastClick.invalidate();
        runActionOn(selectedDate());
    }
    return false;
}

void BE::CalendarWidget::showEvent(QShowEvent *e)
{
    myLastClick.invalidate();
    if (iNeedToReconfigure)
        configure();
    QDate currentDate = QDate::currentDate();
    if (currentDate != myLastCurrentDate) {
        for (int i = 0; i < 2; ++i) {
            QTextCharFormat fmt = dateTextFormat(myLastCurrentDate);
            fmt.setFontWeight(i ? QFont::Black : QFont::Normal);
            setDateTextFormat(myLastCurrentDate, fmt);
            myLastCurrentDate = currentDate;
        }
    }
    QCalendarWidget::showEvent(e);
}

static QString timeToCountDown(const QString &s)
{
    QStringList l = s.split(QChar('\''));
    for (int i = 0; i < l.count(); i += 2) {
        QString *ls = const_cast<QString*>(&l.at(i));
        ls->replace('s', '0');
        ls->replace('m', 's');
        ls->replace('h', 'm');
        ls->replace('H', 'm');
    }
    return l.join(QChar('\''));
}

BE::Clock::Clock(QWidget *parent, const QString &pattern ) : QLabel(parent)
, BE::Plugged(parent)
, myPattern(pattern)
, myTimer(0)
, myCountDown(-1)
, myPrecision(Unknown)
, myTzSecOffset(0)
{
    setObjectName("Clock");
    setAlignment( Qt::AlignCenter );

    myConfigMenu = new QMenu(this);
    myConfigMenu->setSeparatorsCollapsible(false);
    myConfigMenu->addSeparator()->setText(i18n("Configure Clock"));
    myConfigMenu->addAction(i18n("Change pattern..."), this, SLOT(configPattern()));
    myConfigMenu->addAction(i18n("Set Time..."), this, SLOT(configTime()));
    myConfigMenu->addSeparator();
    myConfigMenu->addAction(i18n("CountDown..."), this, SLOT(startCountDown()));
//     myConfigMenu->addSeparator();
//     myConfigMenu->addAction("Reminders...", this, SLOT(configReminder()));

//     updateTime();
}

void
BE::Clock::configure( KConfigGroup *grp )
{
    if (gs_calendar)
        gs_calendar->reconfigure();
    const QDateTime cdt = QDateTime::currentDateTime();

    const QString s = myPattern;
    myPattern = grp->readEntry("Pattern", "hh:mm\nddd, MMM d");
    myCountDownPattern = grp->readEntry("CountDownPattern", timeToCountDown(myPattern));
    myTakeOff = grp->readEntry("TakeOff", QString());

    if (myTimer)
        killTimer(myTimer);
    if (s != myPattern) {
        static QRegExp comments("'[^']*'");
        QString timeString(myPattern);
        timeString.remove(comments);
        if (timeString.contains('s'))
            myPrecision = Seconds;
        else if (timeString.contains('m'))
            myPrecision = Minutes;
        else if (timeString.contains('h'))
            myPrecision = Hours;
        else
            myPrecision = Days;
    }

    myTimer = startTimer(1000); // initial second percision to find the tact.

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

    static const QString helpText = i18n(
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
    "</dl></html>");

    QString text = QInputDialog::getText(this, i18n("Setup Clock pattern"), helpText, QLineEdit::Normal, myPattern, &ok);
    if (ok && !text.isEmpty()) {
        myPattern = text;
        myCountDownPattern = timeToCountDown(myPattern);
        updateTime();
    }
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
            const QString oldText(text());
            if (myCountDown > -1)
                --myCountDown;
            updateTime();
            if (myCountDown < 0 && myPrecision < Seconds) { // changed
                killTimer(myTimer);
                if (oldText == text()) {
                    myTimer = startTimer(1000); // wait for the tact in second precision
                } else {
                switch (myPrecision) {
                    case Minutes:
                        myTimer = startTimer(55000); break;
                    case Hours:
                        myTimer = startTimer(3540000); break;
                    case Days:
                        myTimer = startTimer(86100000); break;
                    default:
                        myTimer = startTimer(1000);
                    }
                }
            }
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
        static KPopupFrame *calenderPopup = 0;
        if (!calenderPopup) {
            calenderPopup = new KPopupFrame;
            calenderPopup->setProperty("KStyleFeatureRequest", property("KStyleFeatureRequest").toUInt() | 1); // "Shadowed"
            calenderPopup->setObjectName("Calendar");
            gs_calendar = new CalendarWidget(calenderPopup);
            calenderPopup->setMainWidget(gs_calendar);
            calenderPopup->adjustSize();
        }
        calenderPopup->exec( popupPosition(calenderPopup->size()) );
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
        killTimer(myTimer);
        myTimer = startTimer(1000);
        updateTime();
    }
}

void
BE::Clock::updateTime() {
    if (myCountDown > -1) {
        QDateTime dt(QDateTime::currentDateTime());
        const int h = myCountDown/3600;
        const int m = (myCountDown - 3600*h)/60;
        const int s = myCountDown - (3600*h + 60*m);
        dt.setTime(QTime(h, m, s));
        setText( dt.toString(myCountDownPattern) );
        if (!myCountDown && !myTakeOff.isEmpty())
            BE::Shell::run(myTakeOff);
    }
    else
        setText( QDateTime::currentDateTime().addSecs(myTzSecOffset).toString(myPattern) );
}