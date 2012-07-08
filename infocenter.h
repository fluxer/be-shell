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

#ifndef INFOCENTER_H
#define INFOCENTER_H

#include "be.plugged.h"
#include <QDialog>
#include <QHash>
#include <QLabel>
#include <QDBusObjectPath>
#include <QPointer>
#include <QProgressBar>
#include <QVariantMap>

class QProgressBar;
class QTextBrowser;
class QToolBox;
class QToolButton;

namespace BE {

class InfoDialog : public QDialog
{
    Q_OBJECT
public:
    InfoDialog(QWidget *parent);
    QDBusObjectPath insertJob(const QString &icon, QString title, int capabilities);
    uint insertNote(uint uid, const QString &icon, QString title, const QString &body);
    virtual int openNotes() const;
    virtual QString requestedLabel() const { return " (i) "; };
public slots:
    void setVisible(bool);
protected:
    QToolBox *notes;
signals:
    void closed();
    void notificationClosed( uint id, uint reason );
    void labelRequest( const QString& );
private slots:
    void closeCurrent();
    void updateJobSummary();
private:
    QHash<uint, QLabel*> noteDict;
    QTextBrowser *log;
    static uint jobId, noteId;
};


class Job : public QWidget
{
    Q_OBJECT
public:
    Job(uint id, const QString &title, int capabilities, QWidget *parent);

signals:
    void cancelRequested();
    void log(const QString &string);
    void resumeRequested();
    void suspendRequested();

private:
    void updateToolTip();

private:
    friend class JobAdaptor;

    void clearDescriptionField(uint number);
    QString logPrefix() const;
    bool setDescriptionField(uint number, const QString &name, const QString &value);

    void setInfoMessage(const QString &infoMessage);

    void setProcessedAmount(qlonglong amount, const QString &unit);

    void setPercent(uint percent);

    void setSpeed(qlonglong bytesPerSecond);

    void setSuspended(bool suspended);

    void setTotalAmount(qlonglong amount, const QString &unit);

    void terminate(const QString &errorMessage);

private:
    qlonglong amounts[2];
    typedef QMap<uint, QPair<QString,QString> > Descriptions;
    Descriptions descriptions;
    QLabel *processInfo;
    QString info, mySpeed, processed, title, total;
    QToolButton *playPause, *cancel;
    int caps;
private:
    friend class InfoDialog;
    QDBusObjectPath myPath;
    QProgressBar *progress;
    int myId;
};

class InfoCenter : public QLabel, public Plugged
{
  Q_OBJECT
public:
    InfoCenter(QWidget *parent = 0);
    void configure( KConfigGroup *grp );
    void saveSettings( KConfigGroup *grp );
    QStringList summaries() const;
signals:
    void notificationClosed( uint id, uint reason );
    void actionInvoked( uint id, const QString& actionKey );
private slots:
    friend class InfoAdaptor;
    uint notify( const QString &appName, uint replacesId, const QString &eventId,
                 const QString &appIcon, const QString &summary, const QString &body,
                 const QStringList &actions, const QVariantMap &hints, int timeout );

    void closeNotification( uint id );
    void removeSummery(uint id);
private:
    friend class JobServerAdaptor;
    QDBusObjectPath requestView(const QString &appName, const QString &appIconName, int capabilities);
protected:
    void mousePressEvent( QMouseEvent *me );
    void paintEvent( QPaintEvent *pe );
    void timerEvent( QTimerEvent *te );
private slots:
    void conditionalHide();
    void dialogClosed();
    void startIconNotification();
    void stopIconNotification();
    void updateUnreadMailCount();
private:
    static uint myActiveNotes;
    static QPointer<InfoDialog> myInfoDialog;
    int myBlinkTimer, myBlinkLevel;
    uint unreadMail, unreadMailId;
    QHash<uint, QString> mySummaries;
};

}
#endif // INFOCENTER_H
