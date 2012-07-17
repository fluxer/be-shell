/**************************************************************************
*   Copyright (C) 2009 by Thomas Luebking                                  *
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

#include "session.h"

#include <KDE/KConfigGroup>
#include <KDE/KDialog>
#include <KDE/KLocale>
#include <KDE/KUser>
// #include <KDE/KIcon>
// #include <KDE/KLocale>

#include <QDBusConnection>
#include <QDBusInterface>
#include <QImage>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QProcess>
#include <QTimerEvent>
#include <QtDebug>

#define BE_SHELL_USE_HAL 0

namespace BE {

class RescueDialog : public KDialog {
public:
    RescueDialog(const QString &rescueFrom, uint timeout, QWidget *parent = 0) : KDialog(parent)
    {
        setWindowFlags(windowFlags()|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint);
        countDown = timeout;
        setModal(countDown);
        if (!countDown) {
            const int minutes = getMinutes();
            if (minutes < 1) {
                deleteLater();
                return;
            }
            setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
            countDown = 60*minutes;
        }
        setButtons(Ok|Cancel|Reset);
        setButtonText(Reset, i18n("Delay %1", rescueFrom));
        this->rescueFrom = rescueFrom;
        label = new QLabel(this);
        updateLabel();
        QFont fnt = label->font(); fnt.setPointSize(2*fnt.pointSize()); label->setFont(fnt);
        setMainWidget(label);
        countDownTimer = startTimer(1000);
    }
protected:
    void slotButtonClicked(int button) {
        if (button == Reset) {
            killTimer(countDownTimer);
            const int minutes = getMinutes();
            if (minutes > 0) {
                const bool wasModal = isModal();
                if (wasModal) {
                    setModal(false);
                    setWindowModality(Qt::NonModal);
                    hide();
                }
                setWindowFlags(windowFlags() & ~Qt::WindowStaysOnTopHint);
                countDown = 60*minutes;
                updateLabel();
                if (wasModal)
                    show();
            }
            countDownTimer = startTimer(1000);
        }
        KDialog::slotButtonClicked(button);
    }
    void timerEvent(QTimerEvent *te)
    {
        if (te->timerId() != countDownTimer)
            return;
        if (!countDown)
        {
            killTimer(countDownTimer);
            accept();
        }
        if (countDown < 60 || !(countDown%60))
            updateLabel();
        --countDown;
    }
private:
    int getMinutes() {
        bool ok;
        const int m = QInputDialog::getInt( this, i18n("Delay %1", rescueFrom), i18n("How many minutes to wait for the %1?", rescueFrom), 5, 1, 2147483647, 1, &ok, Qt::Dialog|Qt::FramelessWindowHint|Qt::WindowStaysOnTopHint );
        return ok ? m : -1;
    }
    void updateLabel() {
        label->setText(QString("%1 in %2 %3").arg(rescueFrom).arg(countDown>60?countDown/60:countDown).arg(countDown>60?"min":"sec"));
    }
    int countDownTimer;
    QLabel *label;
    QString rescueFrom;
    uint countDown;
};
}

#define ksmserver QDBusInterface("org.kde.ksmserver", "/KSMServer", "org.kde.KSMServerInterface", QDBusConnection::sessionBus())

BE::Session::Session( QWidget *parent ) : Button(parent)
{
    setObjectName("SessionButton");
    window()->setAttribute(Qt::WA_AlwaysShowToolTips);

    setIcon(QPixmap::fromImage(QImage(KUser().faceIconPath())));

    setPopupMode( QToolButton::InstantPopup );
    QMenu *menu = new QMenu(this);
    mySessionMenu = new QMenu(menu);
    menu->addAction(i18n("Lock Screen"), this, SLOT(lockscreen()));
    menu->addAction(i18n("Fall asleep"), this, SLOT(suspend()));
    menu->addSeparator();
    menu->addAction(i18n("Logout"), this, SLOT(logout()));
    mySessionAction = menu->addAction(i18n("New session ..."), this, SLOT(login()));
    QList<QVariant> answer = ksmserver.call("canShutdown").arguments();
    if (!answer.isEmpty() && answer.at(0).toBool())
    {
        menu->addSeparator();
        menu->addAction(i18n("Reboot"), this, SLOT(reboot()));
        menu->addAction(i18n("Power Off"), this, SLOT(shutdown()));
    }
    setMenu(menu);
    connect (menu, SIGNAL(aboutToShow()), SLOT(updateSessions()));

    myConfigMenu = new QMenu(this);
    myConfigMenu->setSeparatorsCollapsible(false);
    myConfigMenu->addSeparator()->setText("Session Manager");
    showIcon = myConfigMenu->addAction("Show Icon", this, SLOT(updateSettings()));
    showIcon->setCheckable(true);
    useFullName = myConfigMenu->addAction("Show full name", this, SLOT(updateSettings()));
    useFullName->setCheckable(true);
    setShortcut(QKeySequence());
}

void
BE::Session::configure( KConfigGroup *grp )
{
    bool b = grp->readEntry("UserIcon", false);
    showIcon->setChecked(b);
    if (b)
        setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    else
        setToolButtonStyle(Qt::ToolButtonTextOnly);

    KUser user;
    b = grp->readEntry("FullName", false);
    useFullName->setChecked(b);
    if (b)
        setText(user.property(KUser::FullName).toString());
    else
        setText(user.loginName());
    QString tooltip("<html><b>%1<b><hr/>"
                    "<table><tr><td>%2</td><td align=\"right\">(%3)</td></tr>"
                           "<tr><td>Home:</td><td align=\"right\">%4</td></tr>"
                           "<tr><td>Shell:</td><td align=\"right\">%5</td></tr>"
                           "</table></html>");
    //     QStringList   groupNames () const
    //     bool   isSuperUser () const
    //     QVariant   property (UserProperty which) const
    //     RoomNumber, WorkPhone, HomePhone
    setToolTip( tooltip.arg(user.property(KUser::FullName).toString()).arg(user.loginName()).
                        arg(user.uid()).arg(user.homeDir()).arg(user.shell()) );
}

void
BE::Session::activateSession()
{
    QAction *act = qobject_cast<QAction*>(sender());
    if (act)
        QProcess::startDetached("kdmctl", QStringList() << "activate" << act->data().toString());
}

void
BE::Session::lockscreen()
{
    QDBusInterface("org.freedesktop.ScreenSaver", "/ScreenSaver",
                    "org.freedesktop.ScreenSaver", QDBusConnection::sessionBus()).call(QLatin1String("Lock"));
}

void
BE::Session::mousePressEvent(QMouseEvent *ev)
{
    if( ev->button() == Qt::RightButton )
    {
        myConfigMenu->exec(QCursor::pos());
        ev->accept();
    }
    else
        Button::mousePressEvent(ev);
}

void BE::Session::login()
{
    lockscreen();
    QProcess::startDetached("kdmctl", QStringList() << "reserve");
}

/**
First parameter:        confirm
    Obey the user's confirmation setting:   -1
    Don't confirm, shutdown without asking: 0
    Always confirm, ask even if the user turned it off: 1
Second parameter:       type
    Select previous action or the default if it's the first time: -1
    Only log out: 0
    Log out and reboot the machine: 1
    Log out and halt the machine: 2
Third parameter:        mode
    Select previous mode or the default if it's the first time: -1
    Schedule a shutdown (halt or reboot) for the time all active sessions have exited: 0
    Shut down, if no sessions are active. Otherwise do nothing: 1
    Force shutdown. Kill any possibly active sessions: 2
    Pop up a dialog asking the user what to do if sessions are still active: 3
*/
void BE::Session::logout()
{
    RescueDialog *dlg = new RescueDialog(i18n("Logout"), 10, desktop());
    dlg->setProperty("DelayedAction", Logout);
    connect (dlg, SIGNAL(finished(int)), SLOT(rescueDialogFinished(int)));
    dlg->show();
}

void BE::Session::reboot()
{
    RescueDialog *dlg = new RescueDialog(i18n("Reboot"), 10, desktop());
    dlg->setProperty("DelayedAction", Reboot);
    connect (dlg, SIGNAL(finished(int)), SLOT(rescueDialogFinished(int)));
    dlg->show();
}

void
BE::Session::saveSettings( KConfigGroup *grp )
{
    grp->writeEntry( "UserIcon", showIcon->isChecked());
    grp->writeEntry( "FullName", useFullName->isChecked());
}

void BE::Session::suspend()
{
    RescueDialog *dlg = new RescueDialog(i18n("Fall asleep"), 10, desktop());
    dlg->setProperty("DelayedAction", Suspend);
    connect (dlg, SIGNAL(finished(int)), SLOT(rescueDialogFinished(int)));
    dlg->show();
}

void BE::Session::shutdown()
{
    RescueDialog *dlg = new RescueDialog(i18n("Power Off"), 10, desktop());
    dlg->setProperty("DelayedAction", PowerOff);
    connect (dlg, SIGNAL(finished(int)), SLOT(rescueDialogFinished(int)));
    dlg->show();
}

void BE::Session::rescueDialogFinished(int result)
{
    if (!sender())
        return;
    if (result == QDialog::Accepted) {
        int type = sender()->property("DelayedAction").toInt();
        switch (type) {
            case PowerOff:
                ksmserver.call(QLatin1String("logout"), 0, 2, 2); break;
            case Reboot:
                ksmserver.call(QLatin1String("logout"), 0, 1, 2); break;
            case Logout:
                ksmserver.call(QLatin1String("logout"), 0, 0, 2); break;
            case Suspend: {
#if BE_SHELL_USE_HAL
                QDBusInterface("org.freedesktop.Hal", "/org/freedesktop/Hal/devices/computer",
                            "org.freedesktop.Hal.Device.SystemPowerManagement", QDBusConnection::systemBus()).
                            call(QLatin1String("Suspend"), 0);
#else
            //  org.freedesktop.UPower.Suspend
            //  org.freedesktop.UPower.CanSuspend
                QDBusInterface("org.freedesktop.UPower", "/org/freedesktop/UPower",
                            "org.freedesktop.UPower", QDBusConnection::systemBus()).call(QLatin1String("Suspend"));
#endif
                break;
            }
        }
    }
    sender()->deleteLater();
}

void BE::Session::updateSessions()
{
    QProcess proc;
    proc.start("kdmctl", QStringList() << "list", QIODevice::ReadOnly);
    proc.waitForFinished(2000);
    mySessionMenu->clear();
    mySessionMenu->addAction(i18n("New session ..."), this, SLOT(login()));
    mySessionMenu->addSeparator();
    QStringList sessions = QString(proc.readAllStandardOutput()).split('\t', QString::SkipEmptyParts);
    int ns = 0;
    foreach (const QString &session, sessions)
    {
        if (!session[1].isNumber())
            continue;
        QStringList sinfo = session.split(',', QString::KeepEmptyParts); // ID, VT, USER, TYPE, [* = isActive]
        if (sinfo.count() < 4)
            continue; // some invalid junk
        ++ns;
        const QString title = i18n("%1 @ %2 on %3", sinfo.at(2), sinfo.at(3), sinfo.at(1));
        QAction *a = mySessionMenu->addAction(title, this, SLOT(activateSession()));
        a->setData(sinfo.at(0));
        a->setEnabled(sinfo.count() < 5 || !sinfo.at(4).startsWith("*") );
    }
    if (ns > 1) // user has more than one session
    {
        mySessionAction->disconnect();
        mySessionAction->setText(i18n("Sessions"));
        mySessionAction->setMenu(mySessionMenu);
    }
    else
    {
        mySessionAction->setMenu(0);
        mySessionAction->setText(i18n("New session ..."));
        connect(mySessionAction, SIGNAL(triggered(bool)), SLOT(login()));
    }
}

void
BE::Session::updateSettings()
{
    if (showIcon->isChecked())
        setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    else
        setToolButtonStyle(Qt::ToolButtonTextOnly);

    KUser user;
    setText(useFullName->isChecked() ? user.property(KUser::FullName).toString() : user.loginName());
    Plugged::saveSettings();
}
