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

#include "trash.h"
#include "be.shell.h"


#include <KDE/KConfigGroup>
#include <KDE/KDialog>
#include <KDE/KDirLister>
#include <KDE/KLocale>
#include <KDE/KMessageBox>
#include <KDE/KNotification>
#include <KDE/KRun>
#include <KDE/KStandardDirs>
#include <KDE/KUrl>

#include <kio/copyjob.h>
#include <kio/jobclasses.h>
#include <kio/job.h>
#include <kio/jobuidelegate.h>


#include <QCursor>
#include <QDragEnterEvent>
#include <QDropEvent>
#include <QMenu>
#include <QProcess>
#include <QTimer>

KDirLister *BE::Trash::ourTrashcan = 0;
QMenu *BE::Trash::ourTaskMenu = 0;


BE::Trash::Trash( QWidget *parent ) : QLabel(parent), BE::Plugged(parent)
{
    
    setAcceptDrops( true );
    setContentsMargins(0, 0, 0, 0);
    setCursor(Qt::PointingHandCursor);
    setObjectName(name());
    setScaledContents(true);
//     if (parent && parent->inherits("BE::Desk"))
//         move(parent->width() - 160, parent->height() - 160);

    if (!ourTaskMenu)
    {
        ourTaskMenu = new QMenu;
        ourTaskMenu->setSeparatorsCollapsible(false);
        ourTaskMenu->addSeparator()->setText(i18n("Trashcan"));
        ourTaskMenu->addAction( i18n("Open"), this, SLOT(open()) );
        ourTaskMenu->addAction( i18n("Empty..."), this, SLOT(tryEmptyTrash()) );
    }

    if (!ourTrashcan)
    {
        ourTrashcan = new KDirLister;
        ourTrashcan->openUrl(KUrl("trash:/"));
    }
    connect( ourTrashcan, SIGNAL( clear() ), this, SLOT( updateStatus() ) );
    connect( ourTrashcan, SIGNAL( completed() ), this, SLOT( updateStatus() ) );
    connect( ourTrashcan, SIGNAL( deleteItem( const KFileItem & ) ), this, SLOT( updateStatus() ) );
    
    updateStatus();
    show();
}

void
BE::Trash::emptyTrash()
{
    QByteArray args;
    QDataStream stream( &args, QIODevice::WriteOnly ); stream << (int)1;
    KIO::Job* job = KIO::special( KUrl("trash:/"), args );
    QTimer::singleShot(150, this, SLOT(updateStatus()));
    connect (job, SIGNAL(finished(KJob*)), this, SLOT(updateStatus()));
    job->ui()->setWindow(0);
    ourTrashcan->updateDirectory(ourTrashcan->url());
//     KNotification::event("Trash: emptied", QString() , QPixmap() , 0l, KNotification::DefaultEvent );
//     job->ui()->setAutoErrorHandlingEnabled(true);
}

void
BE::Trash::dragEnterEvent(QDragEnterEvent *dee)
{
    if ( dee && dee->mimeData() && dee->mimeData()->hasUrls() )
        dee->accept();
}

void
BE::Trash::dropEvent(QDropEvent *de)
{
    if ( !(de && de->mimeData() && de->mimeData()->hasUrls()) )
        return;
    
    if (de && KUrl::List::canDecode(de->mimeData()))
    {
        const KUrl::List urls = KUrl::List::fromMimeData(de->mimeData());
        if (urls.count())
        {
            de->accept();
            KIO::Job* job = KIO::trash(urls);
            job->ui()->setWindow(0);
            job->ui()->setAutoErrorHandlingEnabled(true);
        }
    }
}

void
BE::Trash::tryEmptyTrash()
{
    const QString text(i18nc("@info", "Do you really want to empty the trash? All items will be deleted."));

    KDialog *dlg = new KDialog(this);
    connect( dlg, SIGNAL(okClicked()), this, SLOT(emptyTrash()) );
    KMessageBox::createKMessageBox( dlg, KIcon("user-trash"), text, QStringList(), QString(), 0,
                                    KMessageBox::NoExec, QString(), QMessageBox::Warning );
    dlg->show();
}

bool
BE::Trash::isEmpty()
{
    return !(ourTrashcan && ourTrashcan->items(KDirLister::AllItems).count());
}

static bool isClick = false;
static QPoint dragStart;

void
BE::Trash::mousePressEvent(QMouseEvent *ev)
{
    if (ev->button() == Qt::RightButton)
        isClick = true;
    else if (ev->button() == Qt::LeftButton)
    {
        isClick = true;
        dragStart = ev->pos();
    }
}

void
BE::Trash::mouseMoveEvent(QMouseEvent * ev)
{
    if ( !(parentWidget() && parentWidget()->inherits("BE::Desk")) )
        return;
    isClick = false;
    QPoint p = ev->globalPos() - dragStart;
    move(p.x(), p.y());
}

void
BE::Trash::mouseReleaseEvent(QMouseEvent *ev)
{
    if (testAttribute(Qt::WA_UnderMouse))
    {
        if (isClick && !isEmpty())
            ourTaskMenu->popup(QCursor::pos());
        else
            emit moved();
    }
    isClick = false;
}

void
BE::Trash::open()
{
    KRun::runUrl(KUrl("trash:/"), "inode/directory", 0);
}

void
BE::Trash::resizeEvent( QResizeEvent *re )
{
    if ( width() == height() )
        QLabel::resizeEvent(re);
    else if (width() > height())
        setMaximumWidth(height());
    else
        setMaximumHeight(width());
}

void
BE::Trash::themeChanged()
{
    updateStatus();
}

void
BE::Trash::updateStatus()
{
    int n = ourTrashcan->items(KDirLister::AllItems).count();
    if (n > 0)
    {
        setPixmap(themeIcon("user-trash-full").pixmap(128));
        setToolTip(i18n("%1 trashed items").arg(n));
    }
    else
    {
        setPixmap(themeIcon("user-trash").pixmap(128));
        setToolTip(i18n("Trash is empty"));
    }
}

