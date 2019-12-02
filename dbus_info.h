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

#ifndef INFO_ADAPTOR_H
#define INFO_ADAPTOR_H

#include <QtDebug>
#include <QtDBus/QDBusAbstractAdaptor>
#include "infocenter.h"

// implements http://www.galago-project.org/specs/notification/0.9/x408.html

namespace BE
{
class InfoAdaptor : public QDBusAbstractAdaptor
{
   Q_OBJECT
//    Q_CLASSINFO("D-Bus Interface", "org.kde.VisualNotifications")
   Q_CLASSINFO("D-Bus Interface", "org.freedesktop.Notifications")

private:
    BE::InfoCenter *myInfoCenter;

public:
    InfoAdaptor(BE::InfoCenter *infoCenter) : QDBusAbstractAdaptor(infoCenter), myInfoCenter(infoCenter)
    {
        connect (myInfoCenter, SIGNAL(notificationClosed(uint,uint)), SIGNAL(NotificationClosed(uint,uint)));
        connect (myInfoCenter, SIGNAL(actionInvoked(uint,const QString&)), SIGNAL(ActionInvoked(uint,const QString&)));
    }

public slots:
    uint Notify( const QString &appName, uint replacesId, const QString &eventId,
                 const QString &appIcon, const QString &summary, const QString &body,
                 const QStringList &actions, const QVariantMap &hints, int timeout)
    {
        return myInfoCenter->notify(appName, replacesId, eventId, appIcon, summary, body, actions, hints, timeout);
    }
    uint Notify( const QString &appName, uint replacesId, const QString &appIcon,
                 const QString &summary, const QString &body, const QStringList &actions,
                 const QVariantMap &hints, int timeout )
    {
        return myInfoCenter->notify(appName, replacesId, QString(), appIcon, summary, body, actions, hints, timeout);
    }
    uint Notify( const QString &appName, uint replacesId, const QString &appIcon, const QString &summary, const QString &body )
    {
        return myInfoCenter->notify(appName, replacesId, QString(), appIcon, summary, body, QStringList(), QVariantMap(), 0);
    }
    QStringList summaries() const { return myInfoCenter->summaries(); }
    QStringList GetCapabilities()
    {
        return QStringList() << "body" << "body-hyperlinks" << "body-markup" << "icon-static" << "actions";
    }

    QString GetServerInformation(QString& vendor, QString& version, QString& specVersion)
    {
        vendor = "KDE";
        version = "1.0"; // FIXME
        specVersion = "0.10";
        return "BE::Shell";
    }

    void CloseNotification( uint id ) { myInfoCenter->closeNotification(id); }

signals:
    void NotificationClosed( uint id, uint reason );
    void ActionInvoked( uint id, const QString& actionKey );
};

class JobAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
//     Q_CLASSINFO("D-Bus Interface", "org.kde.JobView")
    Q_CLASSINFO("D-Bus Interface", "org.kde.JobViewV2")
    
private:
    BE::Job *myJob;

public:
    JobAdaptor(BE::Job *job) : QDBusAbstractAdaptor(job), myJob(job)
    {
        connect (myJob, SIGNAL(cancelRequested()), SIGNAL(cancelRequested()));
        connect (myJob, SIGNAL(resumeRequested()), SIGNAL(resumeRequested()));
        connect (myJob, SIGNAL(suspendRequested()), SIGNAL(suspendRequested()));
    }
public slots:
    
    
    void clearDescriptionField(uint number) { myJob->clearDescriptionField(number); }
    bool setDescriptionField(uint number, const QString &name, const QString &value)
    {
        return myJob->setDescriptionField(number, name, value);
    }
    
    void setInfoMessage(const QString &infoMessage) { myJob->setInfoMessage(infoMessage); }
    
    void setProcessedAmount(qlonglong amount, const QString &unit) { myJob->setProcessedAmount(amount, unit); }
    
    void setPercent(uint percent) { myJob->setPercent(percent); }
    
    void setSpeed(qlonglong bytesPerSecond) { myJob->setSpeed(bytesPerSecond); }
    
    void setSuspended(bool suspended) { myJob->setSuspended(suspended); }
    void setTotalAmount(qlonglong amount, const QString &unit) { myJob->setTotalAmount(amount, unit); }
    
    void terminate(const QString &errorMessage) { myJob->terminate(errorMessage); }

    void setDestUrl(const QDBusVariant& destUrl) {} // TODO
    
signals:
    void cancelRequested();
    void resumeRequested();
    void suspendRequested();
};



class JobServerAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.JobViewServer")
    
private:
    BE::InfoCenter *myInfoCenter;

public:
    JobServerAdaptor(BE::InfoCenter *infoCenter) : QDBusAbstractAdaptor(infoCenter), myInfoCenter(infoCenter) { }

public slots:
    QDBusObjectPath requestView(const QString &appName, const QString &appIconName, int capabilities)
    {
        return myInfoCenter->requestView(appName, appIconName, capabilities);
    }
};

}
#endif //INFO_ADAPTOR_H
