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

static const int infocenterCacheVersion = 1;

#include "infocenter.h"
#include "dbus_info.h"

#include "be.shell.h"

#include <QApplication>
#include <QDateTime>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDBusReply>
// #include <QDBusPendingCall>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QHBoxLayout>
#include <QMouseEvent>
#include <QPainter>
#include <QScrollArea>
#include <QStyle>
#include <QTextBrowser>
#include <QTime>
#include <QTimer>
#include <QTimerEvent>
#include <QToolBox>
#include <QToolButton>
#include <QVBoxLayout>
#include <KDE/KConfigGroup>
#include <KDE/KGlobal>
#include <KDE/KIcon>
#include <KDE/KJob>
#include <KDE/KLocale>

#include <QtDebug>

class BE::Note : public QLabel
{
public:
    Note(const QString &text, QWidget *parent = 0) : QLabel(text, parent), hash(0) {}
    uint hash;
};

//     The reason the notification was closed.
//     1 - The notification expired.
//     2 - The notification was dismissed by the user.
//     3 - The notification was closed by a call to CloseNotification
//     4 - Undefined/reserved reasons.

uint BE::InfoDialog::noteId = 0;
uint BE::InfoDialog::jobId = 0;

BE::InfoDialog::InfoDialog(QWidget *parent) : QDialog(parent, Qt::Dialog|Qt::FramelessWindowHint)
{
    setObjectName("InfoDialog");
    setProperty("KStyleFeatureRequest", property("KStyleFeatureRequest").toUInt() | 1); // "Shadowed"
    setWindowTitle(i18nc("Title of the messenger dialog", "Hermes"));
    setSizeGripEnabled(true);
    QHBoxLayout *hbl = new QHBoxLayout;
    QVBoxLayout *vbl = new QVBoxLayout(this);

    /*hbl->addWidget(new QLabel("Notifications", this)); */
    QToolButton *cb = new QToolButton(this); cb->setText("x"); cb->setToolTip(i18n("Close current message"));
    hbl->addStretch(); hbl->addWidget(cb);
    connect(cb, SIGNAL(clicked(bool)), this, SLOT(closeCurrent()));

    QScrollArea *sa = new QScrollArea(this); sa->setWidgetResizable(true);
    sa->setFrameStyle(QFrame::NoFrame);
    notes = new QToolBox;
    notes->addItem( log = new QTextBrowser( notes ), i18n("Job Log") );
    sa->setWidget(notes);

    vbl->addWidget(sa);
    vbl->addLayout(hbl);

    QFile cache(QDesktopServices::storageLocation(QDesktopServices::CacheLocation) + "/infocenter");
    if (cache.open(QIODevice::ReadOnly)) {
        int cacheVersion;
        QDataStream stream(&cache);
        stream >> cacheVersion;
        if (cacheVersion == infocenterCacheVersion) {
            QString buffer1, buffer2, buffer3;
            uint uid, hash;
            stream >> buffer1;
            log->setHtml(buffer1);
            while (!stream.atEnd()) {
                stream >> uid >> buffer1 >> buffer2 >> buffer3 >> hash;
                insertNote(uid, buffer1, buffer2, buffer3, hash);
            }
            QMetaObject::invokeMethod(this, "labelRequest", Qt::QueuedConnection,
                                      Q_ARG(QString, notes->count() == 1 && notes->widget(0) == log ? " [l] " : " [i] "));
        } else {
            qDebug() << "Mismatching InfoCenter cache version. Not restoring";
        }
        cache.close();
    }

    connect(qApp, SIGNAL(aboutToQuit()), SLOT(storeMessages()));
}

void
BE::InfoDialog::closeCurrent()
{
    if ( notes->currentWidget() == log )
    {
        log->clear();
        if ( notes->count() > 1 )
            notes->setCurrentIndex( 1 );
        else  // only log is left
            close();
    }
    else if ( qobject_cast<BE::Job*>(notes->currentWidget()) )
    {
        const int next = notes->currentIndex() + 1;
        if ( notes->count() > next )
            notes->setCurrentIndex( next );
    }
    else
    {
        QHash<uint, Note*>::iterator i = noteDict.begin();
        while (i != noteDict.end())
        {
            if (i.value() == notes->currentWidget())
                break;
            ++i;
        }
        // cache for deletion
        QWidget *w = notes->currentWidget();
        // skip fwd (so we won't always fall back to the log)
        const int next = notes->currentIndex() + 1;
        if ( notes->count() > next )
            notes->setCurrentIndex( next );
        // delete "current"
        delete w;
        // erase from cache if it's there
        if (i != noteDict.end())
        {
            // only log -> (l) TODO: mails?!?!?
            emit labelRequest( notes->count() == 1 && notes->widget(0) == log ? " [l] " : " [i] " );
            emit notificationClosed( i.key(), 2 );
            noteDict.erase(i);
        }
    }
}

void
BE::InfoDialog::storeMessages()
{
    const QString path = QDesktopServices::storageLocation(QDesktopServices::CacheLocation);
    QDir().mkpath(path);
    QFile cache(path + "/infocenter");
//     cache.setPermissions(QFileDevice::ReadOwner|QFileDevice::WriteOwner); // Qt5
    if (cache.open(QIODevice::WriteOnly)) {
        QDataStream stream(&cache);
        stream << infocenterCacheVersion;
        stream << log->toHtml(); // log
        for (int i = 0; i < notes->count(); ++i) {
            if (Note *note = dynamic_cast<Note*>(notes->widget(i)))
                stream << noteDict.key(note, 0) <<
                          notes->itemIcon(i).name() << notes->itemText(i) << note->text() <<
                          note->hash;
        }
        cache.close();
    }
}

void
BE::InfoDialog::setVisible(bool show)
{
    if (show) {
        if (notes->currentWidget() == log && (!log->document() || log->document()->isEmpty()) && notes->count() > 1)
            notes->setCurrentIndex(1);
    }
    QDialog::setVisible(show);
    if (!show)
        emit closed();
}

QDBusObjectPath
BE::InfoDialog::insertJob(const QString &icon, QString title, int capabilities)
{
    if (!++jobId)
        jobId = 1;
    BE::Job *job = new BE::Job(jobId, title, capabilities, notes);
    job->setObjectName(i18n("Job %1").arg(jobId));
    notes->addItem( job, KIcon(icon), title );
    QString path = QString("/JobViewServer/JobView_%1").arg(jobId);
    job->myPath = QDBusObjectPath(path);

    connect ( job, SIGNAL( destroyed(QObject*) ), this, SLOT( updateJobSummary() ) );
    connect ( job, SIGNAL(log(const QString&)), log, SLOT(append(const QString&)) );
    connect ( job->progress, SIGNAL( valueChanged(int) ), this, SLOT( updateJobSummary() ) );

    new JobAdaptor(job);
    QDBusConnection::sessionBus().registerObject(path, job);

    return job->myPath;
}

uint
BE::InfoDialog::insertNote(uint uid, const QString &icon, QString title, const QString &body, uint hash)
{
    Note *note = 0;
    if (uid)
        note = noteDict.value(uid, 0L);
    if (!note) {
        for (QHash<uint, Note*>::const_iterator it = noteDict.constBegin(),  end = noteDict.constEnd(); it != end; ++it) {
            if ((*it)->hash == hash) {
                note = const_cast<Note*>(*it);
                break;
            }
        }
    }
    if (!note) {
        note = new Note(body, this);
        note->hash = hash;
        note->setMargin(6);
        note->setWordWrap(true);
        note->setOpenExternalLinks(true);
        note->setFrameStyle( QFrame::StyledPanel|QFrame::Sunken );
        if (!++noteId)
            noteId = 1;
        uid = noteId;
        noteDict.insert(uid, note);
        notes->addItem( note, KIcon(icon), title );

    } else {
        int id = notes->indexOf(note);
        notes->setItemText(id, title);
        notes->setItemIcon(id, KIcon(icon));
        note->setText(body);
    }
//     notes->adjustSize();
    return uid;
}

int
BE::InfoDialog::openNotes() const
{
    return notes->count() - 1 + int(log->document() && !log->document()->isEmpty());
}

void
BE::InfoDialog::updateJobSummary()
{
    const int n = notes->count();
    int jobs = 0, percent = 0;
    BE::Job *job;
    for (int i = 0; i < n; ++i)
    {
        if ((job = qobject_cast<BE::Job*>(notes->widget(i))))
        if (job->progress->value() < 100)
        {
            percent += job->progress->value();
            ++jobs;
        }
    }
    QString label;
    if ( jobs )
        label = QString("%1% (%2)").arg(percent/jobs).arg(jobs);
    else if ( notes->count() == 1 && log->document() && !log->document()->isEmpty() )
        label = " [l] ";
    else
        label = " [i] ";
    emit labelRequest( label );
}

BE::Job::Job(uint id, const QString &title, int capabilities, QWidget *parent) : QWidget(parent), playPause(0), cancel(0), myId(id)
{
    this->title = title;
    amounts[0] = amounts[1] = 0;
    QVBoxLayout *vl = new QVBoxLayout(this);
    QHBoxLayout *hl;
    vl->addLayout(hl = new QHBoxLayout);

    if (capabilities & KJob::Suspendable)
    {
        playPause = new QToolButton(this);
        hl->addWidget(playPause);
        setSuspended(false);
    }

    hl->addWidget(progress = new QProgressBar(this));
    progress->setRange(0,100); progress->setValue(0);
    vl->addLayout(hl = new QHBoxLayout);
    hl->addWidget(processInfo = new QLabel(this));

    if (capabilities & KJob::Killable)
    {
        cancel = new QToolButton(this);
        connect (cancel, SIGNAL(clicked(bool)), this, SIGNAL(cancelRequested()));
        cancel->setIcon(Plugged::themeIcon(/*"media-playback-stop"*/"dialog-cancel", window()));
        hl->addWidget(cancel);
    }
}

void
BE::Job::clearDescriptionField(uint number)
{
    descriptions.remove(number);
    updateToolTip();
}

QString
BE::Job::logPrefix() const
{
    return "<i>" + QTime::currentTime().toString() + "</i> | <b>" + title + "</b> (" + QString::number(myId) + ") >> ";
}

bool
BE::Job::setDescriptionField(uint number, const QString &name, const QString &value)
{
    bool update = true;
    Descriptions::const_iterator it = descriptions.find( number );
    if ( it != descriptions.constEnd() )
        update = it->first != name || it->second != value;
    if ( update )
    {
        emit log( logPrefix() + name + ": " + value + "<br/>" );
        descriptions.insert(number, QPair<QString,QString>(name, value));
        updateToolTip();
    }
    return true;
}

void
BE::Job::setInfoMessage(const QString &infoMessage)
{
    if ( !(infoMessage == info || infoMessage.isEmpty()) )
        emit log( logPrefix() + "<i>" + infoMessage + "</i><br/>");
    info = infoMessage;
    if (QToolBox *tb = qobject_cast<QToolBox*>(parentWidget()->parentWidget()->parentWidget()))
    {
        QString s = title;
        if (!info.isEmpty())
            s = QString(" - %1").arg(info);
        s += QString("  %1%").arg(progress->value());

        tb->setItemText(tb->indexOf(this), s);
    }
}

void
BE::Job::setPercent(uint percent)
{
    progress->setValue(percent);
    setInfoMessage(info);
}

static void setAmount(QString &which, qlonglong amount, const QString &unit)
{
    if (amount)
    {
        if (!unit.compare("bytes", Qt::CaseInsensitive ))
            which += KGlobal::locale()->formatByteSize(amount);
        else if (!unit.compare("files", Qt::CaseInsensitive ))
            which += i18np("%1 file", "%1 files", amount);
        else if (!unit.compare("dirs", Qt::CaseInsensitive ))
            which += i18np("%1 folder", "%1 folders", amount);
        else
            which += QString::number(amount) + " " + unit;
    }
    else
        which = QString();
}

void
BE::Job::setProcessedAmount(qlonglong amount, const QString &unit)
{
    amounts[0] = amount;
    if (amounts[1])
        setPercent(100*amount/amounts[1]);
    processed = QString();
    setAmount(processed, amount, unit);
    processInfo->setText(processed + total + mySpeed);
}

void
BE::Job::setTotalAmount(qlonglong amount, const QString &unit)
{
    amounts[1] = amount;
    if (amount)
        setPercent(100*amounts[0]/amount);
    total = " of ";
    setAmount(total, amount, unit);
    processInfo->setText(processed + total + mySpeed);
}

void
BE::Job::setSpeed(qlonglong bytesPerSecond)
{
    if (bytesPerSecond)
        mySpeed = " @ " + KGlobal::locale()->formatByteSize(bytesPerSecond);
    else
        mySpeed = QString();
    processInfo->setText(processed + total + mySpeed);
}

void
BE::Job::setSuspended(bool suspended)
{
    if (suspended)
    {
        playPause->disconnect(SIGNAL(clicked(bool)));
        playPause->setIcon(Plugged::themeIcon("media-playback-start", window()));
        connect (playPause, SIGNAL(clicked(bool)), this, SIGNAL(resumeRequested()));
    }
    else
    {
        playPause->disconnect(SIGNAL(clicked(bool)));
        playPause->setIcon(Plugged::themeIcon("media-playback-pause", window()));
        connect (playPause, SIGNAL(clicked(bool)), this, SIGNAL(suspendRequested()));
    }
}

void
BE::Job::terminate(const QString &errorMessage)
{
    QString entry(logPrefix() + " <i>done.");
    if ( errorMessage.isEmpty() )
        entry += "</i><br/>";
    else
        entry += " ( " + errorMessage + " )</i><br/>";
    emit log( entry );
    deleteLater();
}

void
BE::Job::updateToolTip()
{
    QString text = "<html>";
    Descriptions::const_iterator i;
    text += "<h3>" + info + "</h3>";
    for (i = descriptions.constBegin(); i != descriptions.constEnd(); ++i)
        text += "<dt>" + i.value().first + "</dt><dd>" + i.value().second + "</dd>";
    text += "</html>";
    setToolTip(text);
    processInfo->setText( text );
}


uint BE::InfoCenter::myActiveNotes = 0;
QPointer<BE::InfoDialog> BE::InfoCenter::myInfoDialog = 0;

BE::InfoCenter::InfoCenter( QWidget *parent ) : QLabel(" [i] ", parent), BE::Plugged(parent),
myBlinkTimer(0), myBlinkLevel(0), unreadMail(0), unreadMailId(0), iAmDirty(false)
{
    if (!myInfoDialog) {
        myInfoDialog = new InfoDialog(window());
    }

    myInfoDialog->disconnect(this);
    connect (myInfoDialog, SIGNAL(notificationClosed(uint,uint)), SIGNAL(notificationClosed(uint,uint)));
    connect (myInfoDialog, SIGNAL(notificationClosed(uint,uint)), SLOT(conditionalHide()));
    connect (myInfoDialog, SIGNAL(labelRequest(const QString&)), SLOT(setText(const QString&)));
    connect (myInfoDialog, SIGNAL(labelRequest(const QString&)), SLOT(conditionalHide()));
    connect (myInfoDialog, SIGNAL(closed()), SLOT(dialogClosed()));

    connect (this, SIGNAL(notificationClosed(uint,uint)), SLOT(removeSummery(uint)));

    setObjectName("InfoCenter");
    QFont fnt = font(); fnt.setBold(true); fnt.setStretch(QFont::Expanded); setFont(fnt);

    new BE::InfoAdaptor(this);
    QDBusConnection::sessionBus().registerService(QLatin1String("org.kde.VisualNotifications"));
    QDBusConnection::sessionBus().registerObject(QLatin1String("/VisualNotifications"), this);

    QDBusConnection::sessionBus().registerService(QLatin1String("org.freedesktop.Notifications"));
    QDBusConnection::sessionBus().registerObject(QLatin1String("/org/freedesktop/Notifications"), this);

    new BE::JobServerAdaptor(this);
    QDBusConnection::sessionBus().registerService(QLatin1String("org.kde.JobViewServer"));
    QDBusConnection::sessionBus().registerService(QLatin1String("org.kde.kuiserver"));
//     QDBusConnection::sessionBus().registerObject(QLatin1String("/JobView/Watcher"), this);
    QDBusConnection::sessionBus().registerObject(QLatin1String("/JobViewServer"), this);
//     QDBusInterface("org.kde.kuiserver", "/JobViewServer").call(QDBus::NoBlock, QLatin1String("registerService"), QDBusConnection::sessionBus().baseService(), "/JobView/Watcher");
//     QDBusConnection::sessionBus().connect( "org.kde.kmail", "/KMail", "org.kde.kmail.kmail", "unreadCountChanged", this, SLOT(updateUnreadMailCount()) );
    hide();
}

void
BE::InfoCenter::closeNotification( uint id )
{
//     qDebug() << "Shall close" << id;
    // we emit the signal to calm the caller, but i do not like taking the info away from the user...
    emit notificationClosed( id, 3 );
}

void
BE::InfoCenter::conditionalHide()
{
    int n = myInfoDialog->openNotes() + unreadMail;
    setVisible(n);
    if (!myInfoDialog->openNotes())
        setToolTip(QString());
}

void
BE::InfoCenter::configure(KConfigGroup *grp)
{
    QRect geo = grp->readEntry( "Geometry", QRect());
    if (geo.isValid())
        myInfoDialog->setGeometry(geo);
}

void
BE::InfoCenter::dialogClosed()
{
    conditionalHide();
    Plugged::saveSettings();
}

void
BE::InfoCenter::mousePressEvent(QMouseEvent *me)
{
    if (me->button() == Qt::LeftButton) {
        if (!myInfoDialog->isVisible()) {
            setProperty("unseenInfo", false);
            repolish();
        }
        myInfoDialog->setVisible(!myInfoDialog->isVisible());
    }
}
//
// emit  actionInvoked( uint id, const QString& actionKey );

uint
BE::InfoCenter::notify( const QString &appName, uint replacesId, const QString &eventId,
                        const QString &appIcon, const QString &summary, const QString &body,
                        const QStringList &actions, const QVariantMap &hints, int timeout )
{
    Q_UNUSED(eventId);
    Q_UNUSED(actions);
    Q_UNUSED(hints);
    Q_UNUSED(timeout);
//     qDebug() << "Got notified" << appName << replacesId << eventId << appIcon << summary << body << actions  << hints << timeout;
    uint hash = qHash(appName + appIcon + summary + body);
    QString hBody = body;
    hBody.replace('\n', "<br>");
    uint id = myInfoDialog->insertNote(replacesId, appIcon, i18n("Message from %1").arg(appName) +
                                      " ( " + QDateTime::currentDateTime().toString(Qt::TextDate) + " )",
                                        "<html>" + hBody + "</html>", hash);
    mySummaries[id] = summary.isEmpty() ? body.left(80).append("...") : summary;
    setText( " [i] " /*+ QTime::currentTime().toString("h:mm")*/ );
    setProperty("unseenInfo", true);
    repolish();
    show();
    setToolTip(i18n("<html>Last message:<p>%1</p><p align=\"right\">from %2</p></html>").arg(hBody).arg(appName));
    startIconNotification();
    QTimer::singleShot(6000, this, SLOT(stopIconNotification()));

    return id;
}

void
BE::InfoCenter::paintEvent( QPaintEvent *pe )
{
    const int alpha = palette().color(foregroundRole()).alpha();
    if ( alpha < 255 && ((QObject*)style())->inherits( "QStyleSheetStyle" ) )
    {
        QPixmap buffer(size());
        buffer.fill(Qt::transparent);

        QPainter::setRedirected( this, &buffer );
        QLabel::paintEvent( pe );
        QPainter::restoreRedirected( this );


        QPainter p( &buffer );
        p.setCompositionMode( QPainter::CompositionMode_DestinationIn );
        p.fillRect( rect(), QColor(255,255,255, alpha) );
        p.end();

        p.begin( this );
        p.drawPixmap( 0,0, buffer );
        p.end();

    }
    else
        QLabel::paintEvent( pe );
}

void
BE::InfoCenter::removeSummery(uint id)
{
    mySummaries.remove(id);
}

void
BE::InfoCenter::repolish()
{
    if (!iAmDirty)
        QMetaObject::invokeMethod(this, "_repolish", Qt::QueuedConnection);
    iAmDirty = true;
}

void
BE::InfoCenter::_repolish()
{
    iAmDirty = false;
    style()->unpolish(this);
    style()->polish(this);
}

QDBusObjectPath
BE::InfoCenter::requestView(const QString &appName, const QString &appIconName, int capabilities)
{
    show();
    setProperty("unseenInfo", true);
    repolish();
    return myInfoDialog->insertJob(appIconName, appName, capabilities);
}

void
BE::InfoCenter::saveSettings( KConfigGroup *grp )
{
    grp->writeEntry( "Geometry", myInfoDialog->geometry());
}

void
BE::InfoCenter::startIconNotification()
{
    ++myActiveNotes;

    if (BE::Shell::hasFullscreenAction())
        return;

    if (!myBlinkTimer)
        myBlinkTimer = startTimer(50);
}

void
BE::InfoCenter::stopIconNotification()
{
    if (myActiveNotes) // for security issues...
        --myActiveNotes;
    if (!myActiveNotes)
    {
        unreadMail = false;
        conditionalHide();
    }
}

QStringList
BE::InfoCenter::summaries() const
{
    return mySummaries.values();
}

void
BE::InfoCenter::timerEvent( QTimerEvent *te )
{
    if (te->timerId() != myBlinkTimer)
        return;

    if (myBlinkLevel > 9)
        myBlinkLevel = -10;
    else
        ++myBlinkLevel;

    QPalette pal = palette(); QColor c = pal.color(foregroundRole());
    c.setAlpha(255-25*qAbs(myBlinkLevel));
    pal.setColor(foregroundRole(), c); setPalette(pal);
    repaint();

    if (!myActiveNotes && !myBlinkLevel)
        { killTimer(myBlinkTimer); myBlinkTimer = 0; }
}

void
BE::InfoCenter::updateUnreadMailCount()
{
    ++unreadMail;
    unreadMailId = notify( "KMail", unreadMailId, QString(), "kmail", i18n("You've got Mail!"),
                           i18n("KMail holds %1 unread mails for you").arg(unreadMail), QStringList(), QVariantMap(), 15 );
}

