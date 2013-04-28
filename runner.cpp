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

#include "runner.h"
#include "dbus_runner.h"
#include "be.shell.h"

#include <cmath>

#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <QX11Info>
#include "fixx11h.h"

#include <QAbstractItemDelegate>
#include <QApplication>
#include <QCompleter>
#include <QCoreApplication>
#include <QDBusConnection>
#include <QDesktopWidget>
#include <QDir>
#include <QKeyEvent>
#include <QLinearGradient>
#include <QLineEdit>
#include <QPainter>
#include <QtConcurrentRun>
#include <QScrollBar>
#include <QTextBrowser>
#include <QTreeWidget>
#include <QTreeWidgetItem>
#include <QVBoxLayout>

#include <KDE/KAction>
#include <KDE/KConfigGroup>
#include <KDE/KIcon>
#include <KDE/KMessageBox>
#include <KDE/KProcess>
#include <KDE/KRun>
#include <KDE/KSycoca>
#include <KDE/KToolInvocation>

#include <kurifilter.h>
#include <kwindowsystem.h>

#include <kdebug.h>

static const int TreeLevel = 33, Executable = 34, EntryPath = 35, ExecPath = 36, GenericName = 37,
                 Keywords = 38, Triggered = 39, Favor = 40;


namespace BE {
class RunDelegate : public QAbstractItemDelegate
{
public:
    RunDelegate( QObject *parent = 0 ) : QAbstractItemDelegate(parent), flat(false){}

    void paint( QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index ) const
    {
        if (!option.rect.isValid())
            return;
        if (option.state & QStyle::State_MouseOver)
            { QFont fnt = painter->font(); fnt.setBold(true); painter->setFont(fnt); }
        const QPalette &pal = option.palette;
        const QRect &rect = option.rect;
        const bool executable = index.data(Executable).toBool();
        int iconSize = option.decorationSize.width();
        if ( !executable )
        {
            iconSize = 22;
            if ( option.state & QStyle::State_Selected )
            {
                painter->fillRect( rect, pal.color(QPalette::Highlight) );
                painter->setPen( pal.color(QPalette::HighlightedText) );
            }
            else
            {
                const int level = index.data(TreeLevel).toInt();
                const float f = sqrt(level);
                const QColor &bg = pal.color(QPalette::WindowText);
                const QColor &fg = pal.color(QPalette::Window);
#define CHAN(_chan_) (fg._chan_()*f + bg._chan_()*(5-f))/5
                QColor hd_bg(CHAN(red), CHAN(green), CHAN(blue));
#undef CHAN
                painter->fillRect( rect, hd_bg );
                painter->setPen( fg );
            }
        }
//         else if ( index.data(Triggered).toBool() ) // creates a "flash"
//         {
//             painter->fillRect( rect, pal.color(QPalette::Window) );
//             painter->setPen( pal.color(QPalette::WindowText) );
//         }
        else if ( (option.state & QStyle::State_HasFocus) && !index.data(Triggered).toBool() )
        {
            painter->fillRect( rect, pal.color(QPalette::Highlight) );
            painter->setPen( pal.color(QPalette::HighlightedText) );
        }
        else
            painter->setPen( pal.color(QPalette::WindowText) );


        const int textFlags = Qt::AlignVCenter | Qt::TextSingleLine | Qt::TextShowMnemonic;
        QRect textRect(rect);
        QPixmap iconPix(index.data(Qt::DecorationRole).value<QIcon>().pixmap( iconSize, iconSize ));

        const QPoint center = rect.center();
        QRect iconRect(iconPix.rect());
        iconRect.moveCenter(center);

        if (executable)
        {
            textRect.setLeft(iconRect.right() + 8);
            painter->drawText(textRect, textFlags | Qt::AlignLeft, index.data().toString());
            const QString genName = index.data(GenericName).toString();
            if (!genName.isEmpty())
            {
                textRect.setLeft(rect.left() + 4);
                textRect.setRight(iconRect.left() - 8);
                painter->drawText(textRect, textFlags | Qt::AlignRight, genName);
            }
        }
        else
        {
            if (!iconPix.isNull())
                textRect.setLeft(textRect.left() + (iconRect.width()/2 + 8));
            painter->drawText(textRect, textFlags | Qt::AlignHCenter, index.data().toString(), &textRect);
            iconRect.moveRight(textRect.left() - 8);
        }

        if (!iconPix.isNull())
            painter->drawPixmap( iconRect.x(), iconRect.y(), iconPix);

        if ( option.state & QStyle::State_MouseOver )
            { QFont fnt = painter->font(); fnt.setBold(false); painter->setFont(fnt);
        }
    }

    QSize sizeHint( const QStyleOptionViewItem &option, const QModelIndex &index ) const
    {
        if (flat && !index.data(Executable).toBool())
            return QSize(128, 0);

        QFontMetrics fm(option.font);
        QSize sz(128, fm.height()); // the actual width is irrelevant, we have only ONE visible column and get whatever the widget has...
        // in case i'd ever figure to be wrong ;-)
//         size ( Qt::TextSingleLine | Qt::TextShowMnemonic, index.data().toString() );
//         sz.rwidth() += 12 + option.decorationSize.width();

        int iconSize = index.data(Executable).toBool() ? option.decorationSize.height() : 22;

        if ( iconSize > sz.height() )
            sz.setHeight( iconSize );

        sz.rheight() += 8;

        return sz;
    }
    bool flat;

};

class RunPopupDelegate : public RunDelegate
{
public:
    RunPopupDelegate( QObject *parent = 0 ) : RunDelegate(parent) {}

    void paint( QPainter *painter, const QStyleOptionViewItem &option, const QModelIndex &index ) const
    {
        if (!option.rect.isValid())
            return;
        if (option.state & QStyle::State_MouseOver)
            { QFont fnt = painter->font(); fnt.setBold(true); painter->setFont(fnt); }
        const QPalette &pal = option.palette;
        const QRect &rect = option.rect;
        const bool executable = index.data(Executable).toBool();
        int iconSize = option.decorationSize.width();
        if ( !executable )
        {
            iconSize = 22;
            if ( option.state & QStyle::State_Selected )
            {
                painter->fillRect( rect, pal.color(QPalette::Highlight) );
                painter->setPen( pal.color(QPalette::HighlightedText) );
            }
            else
            {
                const int level = index.data(TreeLevel).toInt();
                const float f = sqrt(level);
                const QColor &bg = pal.color(QPalette::WindowText);
                const QColor &fg = pal.color(QPalette::Window);
#define CHAN(_chan_) (fg._chan_()*f + bg._chan_()*(5-f))/5
                QColor hd_bg(CHAN(red), CHAN(green), CHAN(blue));
#undef CHAN
                painter->fillRect( rect, hd_bg );
                painter->setPen( fg );
            }
        }
        else if ( (option.state & QStyle::State_HasFocus) && !index.data(Triggered).toBool() )
        {
            painter->fillRect( rect, pal.color(QPalette::Highlight) );
            painter->setPen( pal.color(QPalette::HighlightedText) );
        }
        else
            painter->setPen( pal.color(QPalette::WindowText) );


        const int textFlags = Qt::AlignVCenter | Qt::TextSingleLine | Qt::TextShowMnemonic;
        QRect textRect(rect);
        QPixmap iconPix(index.data(Qt::DecorationRole).value<QIcon>().pixmap( iconSize, iconSize ));

        QRect iconRect(iconPix.rect());
        iconRect.moveCenter(rect.center());
        iconRect.moveLeft(rect.left());
        textRect.setLeft(iconRect.right() + 8);

        if (executable)
        {
            painter->drawText(textRect, textFlags | Qt::AlignLeft, index.data().toString(), &textRect);
            const QString genName = index.data(GenericName).toString();
            if (!genName.isEmpty())
            {
                textRect.setLeft(textRect.right() + 4);
                textRect.setRight(rect.right() - 8);
                QColor c = painter->pen().color();
                c.setAlpha(c.alpha()/3+16);
                painter->setPen( c );
                if ( option.state & QStyle::State_MouseOver )
                    { QFont fnt = painter->font(); fnt.setBold(false); painter->setFont(fnt); }
                painter->drawText(textRect, textFlags | Qt::AlignRight, genName);
            }
        }
        else
        {
            painter->drawText(textRect, textFlags | Qt::AlignLeft, index.data().toString(), &textRect);
            iconRect.moveRight(textRect.left() - 8);
        }

        if (!iconPix.isNull())
            painter->drawPixmap( iconRect.x(), iconRect.y(), iconPix);

        if ( option.state & QStyle::State_MouseOver )
            { QFont fnt = painter->font(); fnt.setBold(false); painter->setFont(fnt); }
    }

};
} // namespace

void
BE::Run::installPathCompleter()
{
    QStringList binaries;
    const char *binpath = getenv("PATH");
    if (binpath)
    {
        QStringList paths = QString(binpath).split(':');
        paths.removeDuplicates();
        foreach (QString path, paths)
        {
            QDir dir(path);
            if (dir.exists())
                binaries.append(dir.entryList( QDir::Executable | QDir::Files | QDir::NoDotAndDotDot, QDir::Name ));
        }
        binaries.removeDuplicates();
    }
    m_binCompleter = new QCompleter(binaries, 0);
    m_binCompleter->setCompletionMode(QCompleter::InlineCompletion);
    m_binCompleter->moveToThread(QApplication::instance()->thread());
    m_binCompleter->setParent(m_shell);
    m_shell->setCompleter(m_binCompleter);
}

BE::Run::Run( QObject *) : QDialog(0, Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint),
m_currentHistoryEntry(-1), m_visibleIcons(10000), m_flat(false), iScheduledResort(false), mySettingsDirty(false), m_triggeredItem(0L), myOutput(0L)
{
    m_hideTimer = new QTimer(this);
    connect ( m_hideTimer, SIGNAL(timeout()), this, SLOT(hideDueToInactivity()) );

    setObjectName("BE::Run");
    setWindowTitle("BE::Run");
    setProperty("KStyleFeatureRequest", property("KStyleFeatureRequest").toUInt() | 1); // "Shadowed"
//     setAttribute(Qt::WA_MacBrushedMetal);
    setSizeGripEnabled(true);

    KAction *action = new KAction(this);
    action->setObjectName("BE::Run::showAsDialog");
    action->setGlobalShortcut(KShortcut(Qt::AltModifier + Qt::Key_Space));
    connect ( action, SIGNAL(triggered(Qt::MouseButtons, Qt::KeyboardModifiers)), this, SLOT(showAsDialog()) );

    QDBusConnection::sessionBus().registerObject("/Runner", this);
    new BE::RunnerAdaptor(this);

    m_shell = new QLineEdit(this);
    m_shell->installEventFilter(this);
    m_shell->setAlignment(Qt::AlignCenter);

    m_binCompleter = 0;
    QtConcurrent::run(this, &BE::Run::installPathCompleter);

    m_tree = new QTreeWidget(this);
    m_tree->setFocusProxy(m_shell);
    // UI adjustments
//     m_tree->setAllColumnsShowFocus(true);
    m_tree->setCursor(Qt::PointingHandCursor);
    m_tree->setExpandsOnDoubleClick(false); // use single click
    m_tree->setRootIsDecorated(false);
    m_tree->setIconSize( QSize(22, 22) );
    m_tree->setHeaderHidden(true);
    m_tree->setIndentation(0);
    m_tree->setVerticalScrollMode( QAbstractItemView::ScrollPerPixel );
    m_tree->setAnimated( true );

    QWidget *vp = m_tree->viewport();
    vp->setAttribute(Qt::WA_Hover);
    // make "transparent"
    m_tree->setFrameStyle( QFrame::NoFrame );
    vp->setAutoFillBackground(false);
    m_tree->setBackgroundRole(QPalette::Window);
    vp->setBackgroundRole(QPalette::Window);
    m_tree->setForegroundRole(QPalette::WindowText);
    vp->setForegroundRole(QPalette::WindowText);

    delegate[0] = new BE::RunDelegate(this);
    delegate[1] = new BE::RunPopupDelegate(this);
    m_tree->setItemDelegate(delegate[0]);

    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->addWidget(m_shell);
    layout->addWidget(m_tree);

    connect ( m_tree, SIGNAL(itemClicked(QTreeWidgetItem*, int)),
             this, SLOT(slotItemActivated(QTreeWidgetItem*, int)) );
//     connect ( m_tree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)),
//              this, SLOT(slotItemActivated(QTreeWidgetItem*, int)) );
    connect ( m_tree, SIGNAL(itemDoubleClicked(QTreeWidgetItem*, int)), this, SLOT(hide()) );

    connect ( m_tree, SIGNAL(currentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)),
              this, SLOT(slotCurrentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*)) );

    connect ( KSycoca::self(), SIGNAL(databaseChanged()), this, SLOT(slotRepopulate()));
    connect ( KSycoca::self(), SIGNAL(databaseChanged(const QStringList&)), this, SLOT(slotRepopulate()));

    connect ( m_shell, SIGNAL(textEdited(const QString&)), this, SLOT(filter(const QString&)) );
}

void
BE::Run::hide()
{
    if (m_isPopup)
        m_tree->setItemDelegate(delegate[0]);
    m_isPopup = false;
    QDialog::hide();
    if ( iScheduledResort )
        updateFavorites();
    if ( mySettingsDirty || iScheduledResort )
        Plugged::saveSettings();

    iScheduledResort = false;
    mySettingsDirty = false;
}

void
BE::Run::configure( KConfigGroup *grp )
{
    if (grp->readEntry("ARGB", false))
    {
        setAttribute(Qt::WA_TranslucentBackground, true);
        setWindowOpacity(1.0);
    }
    else
    {
        setAttribute(Qt::WA_TranslucentBackground, false);
        setWindowOpacity(0.9);
    }

    static const uint zero = 0;
    static Atom BlurAtom = XInternAtom(QX11Info::display(), "_KDE_NET_WM_BLUR_BEHIND_REGION", False);
    XChangeProperty(QX11Info::display(), winId(), BlurAtom, XA_CARDINAL, 32, PropModeReplace, (uchar*)&zero, 1 );

    myVisibilityTimeout = grp->readEntry("VisibilityTimeout", 2000);

    myAliases.clear();
    QStringList stringList = grp->readEntry("Aliases", QStringList());
    int i;
    foreach (QString string, stringList)
    {
        i = string.indexOf(':');
        myAliases.insert(string.left(i), string.right(string.size()-(i+1)));
    }
    // qDebug() << myAliases;

    KConfigGroup grp2 = KSharedConfig::openConfig("be.shell.history")->group(name());
    m_history = grp2.readEntry("History", QStringList());
    stringList = grp2.readEntry("Favors", QStringList());
    foreach (QString string, stringList)
    {
        i = string.lastIndexOf(':');
        favorHash[string.left(i)] += string.right(string.size()-(i+1)).toFloat();
    }
    slotRepopulate();
}

void BE::Run::rSaveFavor(QTreeWidgetItem *item)
{
    QTreeWidgetItem *kid;
    double d;
    const int cnt = item->childCount();
    for (int i = 0; i < cnt; ++i)
    {
        kid = item->child(i);
        if ( kid->childCount() && kid != favorites ) //this is a group
            rSaveFavor(kid);
        else if ((d = kid->data(0, Favor).toDouble()) > 1.0)
        {
            float &f = favorHash[kid->data(0, ExecPath).toString()];
            if (d > f) f = d;
        }
    }
}

void
BE::Run::saveSettings( KConfigGroup *grp )
{
    KConfigGroup grp2 = KSharedConfig::openConfig("be.shell.history")->group(name());
    grp2.writeEntry( "History", m_history );
    favorHash.clear();
    rSaveFavor(m_tree->invisibleRootItem());
    QStringList list;
    QHash<QString, float>::const_iterator i;
    for (i = favorHash.constBegin(); i != favorHash.constEnd(); ++i)
        list.append(i.key() + ':' + QString::number(i.value(), 'f', 4));
    grp2.writeEntry( "Favors", list );
}

void
BE::Run::changeEvent( QEvent *ev )
{
    QDialog::changeEvent(ev);
    if (ev->type() == QEvent::ActivationChange && (m_isPopup || BE::Shell::touchMode()) && !isActiveWindow())
    {
        focusOut.start();
        hide();
    }
}

void
BE::Run::paintEvent(QPaintEvent *pe)
{
    if (testAttribute(Qt::WA_TranslucentBackground))
    {
        QColor c = palette().color(foregroundRole());
        int v = qMin(qMin(c.red(), c.green()), c.blue());
        if (v > 160)
            c = Qt::black;
        else
            c = Qt::white;
        QLinearGradient lg(0,0,0,height());
        if (BE::Shell::compositingActive())
        {
            c.setAlpha(qAbs(c.red()-85));
            lg.setColorAt(0.1, c);
            c.setAlpha(255-c.alpha());
        }
        else
        {
            v = qMin(c.red()+64,255);
            c.setRgb(v,v,v);
            lg.setColorAt(0.1, c);
            v -= 32;
            c.setRgb(v,v,v);
        }
        lg.setColorAt(0.9, c);
        QPainter p(this);
        p.setClipRegion(pe->region());
        p.setPen(Qt::NoPen);
        p.setBrush(lg);
        p.drawRect(rect());
        p.end();
    }
    else
        QDialog::paintEvent(pe);
}

void
BE::Run::timerEvent( QTimerEvent *te)
{
    if (te->timerId() == m_slideTimer)
    {
        const int v = m_tree->verticalScrollBar()->value();
        bool stop = false;
        if (v == m_slideTarget)
            stop = true;
        else
        {
            const bool growing = v < m_slideTarget;
            int diff = (m_slideTarget-v)/5;
            if (!diff)
                diff = growing ? 1 : -1;
            m_tree->verticalScrollBar()->setValue(v + diff);
            stop =  (growing && m_tree->verticalScrollBar()->value() >= m_slideTarget) ||
                    (!growing && m_tree->verticalScrollBar()->value() <= m_slideTarget);
        }

        if (stop)
        {
            killTimer(m_slideTimer);
            m_slideTimer = 0;
        }
    }
    else
        QDialog::timerEvent(te);
}

void
BE::Run::showAsDialog()
{
    if (m_isPopup)
        m_tree->setItemDelegate(delegate[0]);
    QRect r = QApplication::desktop()->availableGeometry();
    const int w = qMax(r.width()/3, 480);
    const int h = qMax(r.height()/4, 360);
    resize(w,h);
    move( r.x() + (r.width() - w)/2, r.y() + (r.height() - h)/2 );

    KWindowSystem::setOnDesktop(winId(), KWindowSystem::currentDesktop());
    setWindowFlags(Qt::Dialog | Qt::FramelessWindowHint | Qt::WindowStaysOnTopHint);
    KWindowSystem::setState(winId(), NET::Sticky | NET::SkipTaskbar | NET::SkipPager | NET::StaysOnTop );
    show();
    m_shell->setFocus(Qt::OtherFocusReason);
    activateWindow();
    raise();
    m_isPopup = false;
}

void BE::Run::togglePopup(int x, int y)
{
    if (focusOut.isValid() && focusOut.elapsed() < 250) // this probably means some runner button was clicked
        return;

    if (isVisible() /*&& isOnCurrentDesktop*/)
        { hide(); return; }

    setWindowFlags(Qt::Popup);
    m_tree->setItemDelegate(delegate[1]);
    QRect r = QApplication::desktop()->availableGeometry();
    const int h = qMax(r.height()/2, 400);
    const int w = h/2;
    resize(w,h);
    if (y > r.height()/2) y -= h;
    if (y + h > r.bottom()) y = r.bottom() - h;
    if (y < r.y()) y = r.y();

    if (x > r.width()/2) x -= w;
    if (x + w > r.right()) x = r.right() - w;
    if (x < r.x()) x = r.x();

    move( x, y );

    KWindowSystem::setOnDesktop(winId(), KWindowSystem::currentDesktop());
    show();
    m_shell->setFocus(Qt::OtherFocusReason);
    activateWindow();
    raise();
    m_isPopup = true;
}

void BE::Run::resetShellPalette()
{
    m_shell->setPalette(m_originalPal);
}

void
BE::Run::flash( const QColor &c )
{
    if (m_shell->testAttribute(Qt::WA_SetPalette))
        m_originalPal = m_shell->palette();
    else
        m_originalPal = QPalette();
    QPalette pal = m_shell->palette();
    QColor bg = pal.color(m_shell->backgroundRole());
#define CHAN(_chan_) (bg._chan_()*2 + c._chan_())/3
    bg.setRgb( CHAN(red), CHAN(green), CHAN(blue), CHAN(alpha) );
#undef CHAN
    pal.setColor(m_shell->backgroundRole(), bg);
    m_shell->setPalette(pal);
    QTimer::singleShot( 200, this, SLOT(resetShellPalette()) );
}

void BE::Run::execute( const QString &exec/*Line*/ )
{
    if (!myIOProcs.isEmpty())
    {
        myIOProcs.last()->write(exec.toLocal8Bit());
        return;
    }
    m_currentHistoryEntry = -1;
#if 0
    // search and set environment vars ================
    QStringList elements = execLine.trimmed().split(' ', QString::SkipEmptyParts);
    if (elements.isEmpty())
        return;

    QStringList environment = QProcess::systemEnvironment();
    QStringList::iterator element = elements.begin();
    while (element != elements.end())
    {
        if (element->count('=') == 1)
        {
//             qDebug() << "FOUND ENVIRONMET" << *element;
            environment << *element;
            element = elements.erase(element);
        }
        else
            break; // environment has to be set at the beginning and must not be separated by a command
    }

    QString exec = elements.join(" ");
#endif
#if 0
    if ( exec.toLower() == "logout" )
    {
        hide();
        KWorkSpace::requestShutDown( KWorkSpace::ShutdownConfirmDefault, KWorkSpace::ShutdownTypeNone);
        return;
    }
#endif
    bool bc_abuse = exec.startsWith('=');
    if (bc_abuse || exec.startsWith(':')) // bc request || IO command
    {
        if (!myOutput) {
            layout()->addWidget(myOutput = new QTextBrowser(this));
            myOutput->setFocusPolicy(Qt::NoFocus);
            myOutput->setObjectName("TextShell");
        }
        myOutput->document()->clear();
        m_tree->hide();
        myOutput->show();

        QStringList cmds;
        if (bc_abuse) {
            cmds << "bc -l";
            if (myOutput->objectName() != "Calculator") {
                myOutput->setObjectName("Calculator");
                style()->unpolish(myOutput);
                style()->polish(myOutput);
            }
        } else {
            cmds = exec.mid(1).split('|', QString::SkipEmptyParts);
            if (myOutput->objectName() != "TextShell") {
                myOutput->setObjectName("TextShell");
                style()->unpolish(myOutput);
                style()->polish(myOutput);
            }
        }

        for (int i = 0; i < cmds.count(); ++i)
        {
            QProcess *proc = new QProcess(this);
            connect (proc, SIGNAL(finished(int, QProcess::ExitStatus)), SLOT(exitIOMode()));
            connect (proc, SIGNAL(error(QProcess::ProcessError)), SLOT(exitIOMode()));
            if (i == cmds.count() - 1) // last in pipe chain
                connect (proc, SIGNAL(readyReadStandardOutput()), SLOT(pipeIOProc()));
            if (i > 0)
                myIOProcs.last()->setStandardOutputProcess(proc);

            myIOProcs << proc;
        }


        for (int i = 0; i < myIOProcs.count(); ++i) {
//             qDebug() << myAliases.value(cmds.at(i), cmds.at(i));
            myIOProcs.at(i)->start(myAliases.value(cmds.at(i), cmds.at(i)));
        }
        if (bc_abuse)
        {
            myIOProcs.last()->write(QString("scale=8;" + exec.mid(1) + '\n').toLocal8Bit());
            myIOProcs.last()->closeWriteChannel();
            myIOProcs.last()->readAll();
        }
        // NO FURTHER INPUT PROCESSING!
        return;
    }
    QStringList list = exec.split(' ', QString::SkipEmptyParts);
    for (Aliases::const_iterator it = myAliases.constBegin(), end = myAliases.constEnd(); it != end; ++it)
    {
        for (int i = 0; i < list.count(); ++i)
            if (list.at(i) == it.key())
                list[i] = it.value();
    }
    KUriFilterData execLineData( list.join(" ") );
    KUriFilter::self()->filterUri( execLineData, QStringList() << "kurisearchfilter" << "kshorturifilter" );
    QString command = ( execLineData.uri().isLocalFile() ? execLineData.uri().path() : execLineData.uri().url() );

    if ( !command.isEmpty() )
    switch( execLineData.uriType() )
    {
        case KUriFilterData::LocalFile:
        case KUriFilterData::LocalDir:
        case KUriFilterData::NetProtocol:
        case KUriFilterData::Help:
        {
            new KRun( execLineData.uri(), this );
            filter( QString() );
            m_shell->clear(); flash( Qt::green );
            m_history.removeOne(exec); m_history.append(exec);
            mySettingsDirty = true;
            m_inactive = true;
            m_hideTimer->start( myVisibilityTimeout );
            break;
        }
#if 0
        case KUriFilterData::Executable:
        case KUriFilterData::Shell:
        {
            KProcess *proc = new KProcess;
            proc->setEnvironment(environment);
            proc->setShellCommand(exec);
            proc->start();
            if (proc->waitForStarted(3000))
            {
                qDebug() << "running" << exec;
                connect (proc, SIGNAL(finished(int, QProcess::ExitStatus)), proc, SLOT(deleteLater()));
                filter( QString() );
                m_shell->clear(); flash( Qt::green );
                m_history.removeOne(exec); m_history.append(exec);
                mySettingsDirty = true;
                m_inactive = true;
                m_hideTimer->start( myVisibilityTimeout );
            }
            else
            {
                qDebug() << "failed" << exec;
                delete proc;
                flash( Qt::red );
            }
            break;
        }
#else
        case KUriFilterData::Executable:
        case KUriFilterData::Shell:
        {
            QString exe = command;
            if( execLineData.hasArgsAndOptions() )
                command += execLineData.argsAndOptions();
            if (!KRun::runCommand( command, exe, "", this ))
                goto def;
            else
            {
                filter( QString() );
                m_shell->clear(); flash( Qt::green );
                m_history.removeOne(exec); m_history.append(exec);
                mySettingsDirty = true;
                m_inactive = true;
                m_hideTimer->start( myVisibilityTimeout );
            }

            break;
        }
#endif
        case KUriFilterData::Unknown:
        case KUriFilterData::Error:
        default:
def:
            if (m_visibleIcons == 1)
            {
                QKeyEvent ke(QEvent::KeyPress, Qt::Key_Down, Qt::NoModifier);
                QCoreApplication::sendEvent(m_tree, &ke);
                slotItemActivated(m_tree->currentItem(), 0);
            }
            else
                flash( Qt::red );
            break;
    }
}

void BE::Run::exitIOMode()
{
    myIOProcs.removeAll(qobject_cast<QProcess*>(sender()));
    sender()->deleteLater(); // instant deletion can cause segfaults on process errors
}

void BE::Run::pipeIOProc()
{
    QTextCursor cursor(myOutput->textCursor());
    cursor.movePosition(QTextCursor::End);
    cursor.movePosition(QTextCursor::NextBlock);
    cursor.insertText(QString::fromLocal8Bit(myIOProcs.last()->readAllStandardOutput()));
//     QScrollBar *bar = messageList->verticalScrollBar();
//     bar->setValue(bar->maximum());
//     myOutput->document()->setPlainText( myOutput->document()->toPlainText() + QString::fromLocal8Bit(myIOProc->readAllStandardOutput()) );
}

void BE::Run::slotCurrentItemChanged(QTreeWidgetItem *current, QTreeWidgetItem *previous)
{
    if ( m_flat && current && !current->data( 0, Executable).toBool() )
    {
        QTreeWidgetItem *above = m_tree->itemAbove(current);
        QTreeWidgetItem *below = m_tree->itemBelow(current);
        if ( previous == above && below )
            m_tree->setCurrentItem( below, 0 );
        else if ( previous == below )
            m_tree->setCurrentItem( above, 0 );
    }
}

bool BE::Run::eventFilter( QObject *o, QEvent *ev )
{
    if (o != m_shell || ev->type() != QEvent::KeyPress)
        return false;

    m_inactive = false;

    QKeyEvent *ke = static_cast<QKeyEvent*>(ev);
    QTreeWidgetItem *item = m_tree->currentItem();

    switch (ke->key())
    {
    case Qt::Key_Enter:
    case Qt::Key_Return:
        if ( !item || item->isHidden() )
            { execute( m_shell->text() ); return false; }
        else
            { slotItemActivated(item, 0); return true; }
    case Qt::Key_PageUp:
    case Qt::Key_Up:
        if ( !item )
        {
            if (!m_history.isEmpty())
            {
                if ( m_currentHistoryEntry == 0 )
                    return true;
                if ( m_currentHistoryEntry < 0 )
                    m_currentHistoryEntry = m_history.count();
                --m_currentHistoryEntry;
                m_shell->setText(m_history.at(m_currentHistoryEntry));
            }
            return true;
        }
        else
        {
            m_currentHistoryEntry = -1; // sanitize!
            while ((item = m_tree->itemAbove(item)))
            {
                if (item->isHidden() || (m_flat && !item->data(0, Executable).toBool()))
                    continue;
                break;
            }
            if ( !item )
            {
                m_tree->setCurrentItem(0,0);
                m_shell->selectAll();
//                 flash(m_shell->palette().color(QPalette::Highlight));
                return true;
            }
            // fall through - we send the event
        }
    case Qt::Key_PageDown:
    case Qt::Key_Down:
        if ( !m_history.isEmpty() &&
             m_currentHistoryEntry > -1 && m_currentHistoryEntry < m_history.count()-1 )
        {
            ++m_currentHistoryEntry;
            m_shell->setText(m_history.at(m_currentHistoryEntry));
            return true;
        }
        m_shell->deselect();
        QCoreApplication::sendEvent(m_tree, ke);
        return true;
    case Qt::Key_Left:
    case Qt::Key_Right:
    {
        if (ke->modifiers() & Qt::ControlModifier)
        {
            QCoreApplication::sendEvent(m_tree, ke);
            return true;
        }
        else
            return false;
    }
    case Qt::Key_Escape:
        if (myOutput && myOutput->isVisible())
        {
            if (!myIOProcs.isEmpty())
            {
                for (int i = 0; i < myIOProcs.count(); ++i ) {
                    myIOProcs.at(i)->terminate();
                }
                myIOProcs.clear();
            }
            else
            {
                filter( QString() );
                m_shell->clear();
                myOutput->hide();
                m_tree->show();
            }
        }
        else
            hide();
        return true;

    default:
        return false;
    }
    return false;
}

void
BE::Run::slideTo( const QTreeWidgetItem *item, QAbstractItemView::ScrollHint hint )
{
    const QRect r(m_tree->visualItemRect(item));
    const QRect vr(m_tree->viewport()->rect());
    int dy = 0;
    switch (hint)
    {
        case QAbstractItemView::EnsureVisible:
            if (r.intersects(vr))
                break;
            // else slide to center...
        case QAbstractItemView::PositionAtCenter:
            dy = r.center().y() - vr.center().y();
            break;
        case QAbstractItemView::PositionAtTop:
            dy = r.y() - vr.y();
            break;
        case QAbstractItemView::PositionAtBottom:
            dy = r.bottom() - vr.bottom();
            break;
    }
    if (dy)
    {
        const int v = m_tree->verticalScrollBar()->value();
        const int   min = m_tree->verticalScrollBar()->minimum(),
                    max = m_tree->verticalScrollBar()->maximum();
        m_slideTarget = qMax(qMin(v + dy, max), min);

        if (m_slideTimer)
            killTimer(m_slideTimer);
        m_slideTimer = startTimer(40);
    }
}

float BE::Run::favor( const QString &exec )
{
    QHash<QString,float>::const_iterator i = favorHash.constFind(exec);
    if (i == favorHash.constEnd())
        return 1.0;
    return i.value();
}

static int rFilter( const QStringList &strings, QTreeWidgetItem *item, bool incremental, QTreeWidgetItem *favorites, bool forceForDad = false )
{
    int kidCnt, count = 0;
    QTreeWidgetItem *kid;
    float groupProbability = 0.0;

    const int cnt = item->childCount();
    for (int i = 0; i < cnt; ++i)
    {
        kid = item->child(i);
        kidCnt = 0;
        const bool exec = !kid->childCount() && kid->data(0, Executable).toBool();
        bool visible = !(incremental && kid->isHidden());
        if (visible) // not ruled out before
        {   // let's try to figure out how much the user wants /this/ entry
            float probability = 0.0;
            foreach (QString string, strings)
            {
                int hit = 0;
                int index = kid->text(0).indexOf( string, 0, Qt::CaseInsensitive );
                // index == 0 == startsWith - that's better then some mid term hit
                if ( index == 0 )
                    { hit = 2; probability += 0.8; }
                else if ( index != -1 ) // appname still containes this string
                    { hit = 1; probability += 0.4; }

                // check on generic name, bin exec and keywords as well
                if (exec)
                {
                    if ( hit != 2 ) // we could do better
                    {   // the exec name is worth as much as the name (for the pros ;-)
                        index = kid->data(0, ExecPath).toString().indexOf( string, 0, Qt::CaseInsensitive );
                        if ( index == 0 ) { hit = 2; probability += 0.8; }
                        else if ( index != -1 ) { hit = 1; probability += 0.4; }
                    }
                    if ( hit != 2 )
                    {   // the generic name isn't worth as much as the name... ("Text Editor" vs. "KWrite")
                        index = kid->data(0, GenericName).toString().indexOf( string, 0, Qt::CaseInsensitive );
                        if ( index == 0 ) { hit = 2; probability += 0.6; }
                        else if ( index != -1 ) { hit = 1; probability += 0.3; }
                    }
                    if (!hit)
                    {   // the keys have few value - the user never sees them and there can be many, i.e. easy to hit
                        QStringList keys = kid->data(0, Keywords).toStringList();
                        foreach (QString key, keys)
                        {
                            index = key.indexOf( string, 0, Qt::CaseInsensitive );
                            if ( index != -1 )
                                { hit = true; probability += ( index == 0 ) ? 0.1 : 0.05; }
                        }
                    }
                    probability *= kid->data(0, Favor).toDouble();
                }

                if (!hit) // some token isn't contained nowhere - this is a showstopper!
                    // (the item will be shown for it's parent or kids at best, but deserves no probability)
                    { probability = 0.0; visible = false; break; }
            }

            if ( kid->childCount() && kid != favorites )
            {   //this is a group, filter children as well
                kidCnt = rFilter( strings, kid, incremental, favorites, visible );
                visible = visible || kidCnt; // ...but maybe due to visible children
                // the recursion determines the odds of this group, set it as probability
                probability = kid->data(1, Qt::DisplayRole).toDouble();
            }

            // now set the probability for this kid - whether group or exec is catched above
            kid->setData(1, Qt::DisplayRole, probability);

            // maybe visible because of a parenting group hit - no probability points, though
            if (!visible)
                visible = forceForDad;

            kid->setHidden(!visible);

            // update this groups (item refers a group entry...) probability
            groupProbability = qMax(groupProbability, probability);
        }

        // the return value counts visible children
        count += kidCnt + (kidCnt ? 0 : visible);
    }

    // set the group probability to this group (item)
    item->setData(1, Qt::DisplayRole, groupProbability);
    return count;
}

static float leastFavor = 1.0;

static void rCollectFavorites(QTreeWidgetItem *item, QList<QTreeWidgetItem*> *list )
{
    QTreeWidgetItem *kid;
    const int cnt = item->childCount();
    for (int i = 0; i < cnt; ++i)
    {
        kid = item->child(i);

        if ( kid->childCount() ) //this is a group
            rCollectFavorites(kid, list);
        else if (kid->data(0, Favor).toDouble() <= 1.0)
            continue;
        else
        {
            bool alreadyIn = false;
            foreach (QTreeWidgetItem *l, *list)
            {
                if (l->data(0, ExecPath) == kid->data(0, ExecPath))
                    { alreadyIn = true; break; }
            }
            if (alreadyIn)
                continue;

            if (list->count() < 8)
                list->append(kid);
            else if (kid->data(0, Favor).toDouble() > leastFavor)
            {
                double d,w = 999.999; // just to convince gcc...
                QList<QTreeWidgetItem*>::iterator kick = list->end();
                QList<QTreeWidgetItem*>::iterator it;
                for (it = list->begin(); it != list->end(); ++it )
                {
                    d = (*it)->data(0, Favor).toDouble();
                    if ( kick == list->end() )
                        { kick = it; w = d; leastFavor = d; }
                    else if ( d < w )
                        { kick = it; leastFavor = w; w = d; }
                }
                if (kick != list->end())
                    list->erase(kick);
                list->append(kid);
            }
        }
    }
}

void BE::Run::updateFavorites()
{
    // get rid of old
    QList<QTreeWidgetItem*> list = favorites->takeChildren();
    qDeleteAll(list.begin(), list.end());
    list.clear();

    // and add new
    leastFavor = 1.0;
    rCollectFavorites(m_tree->invisibleRootItem(), &list);
    leastFavor = 1.0;

    QTreeWidgetItem *kid;
    foreach (QTreeWidgetItem *item, list)
    {
        kid = new QTreeWidgetItem(*item);
        kid->setData(1, Qt::DisplayRole, kid->data(0, Favor).toDouble());
        favorites->addChild(kid);
    }
    favorites->sortChildren(1, Qt::DescendingOrder);
}

void BE::Run::filter( const QString &string )
{
    if (string.startsWith(':') || string.startsWith('='))
        return; // IO command - no filtering. Doesn't make sense
    m_currentHistoryEntry = -1;
    bool inc = !(string.isEmpty() || m_lastFilter.isEmpty()) && string.contains(m_lastFilter, Qt::CaseInsensitive);

    if ( inc && m_visibleIcons < 2 ) {
        return;
    }

    m_visibleIcons = rFilter(string.split( ' ', QString::SkipEmptyParts, Qt::CaseInsensitive ),
                              m_tree->invisibleRootItem(), inc, favorites);

    const bool wasFlat = m_flat;
    delegate[0]->flat = delegate[1]->flat = m_flat = (m_visibleIcons < 30);

    if ( m_flat )
    {
        if ( !wasFlat )
        {
            m_tree->sortItems ( 1, Qt::DescendingOrder );
            m_tree->expandAll();
        }
    }
    else
    {
        m_tree->collapseAll();
        if (wasFlat)
            m_tree->sortItems ( 0, Qt::AscendingOrder );
    }

    m_tree->insertTopLevelItem( 0, m_tree->takeTopLevelItem( m_tree->indexOfTopLevelItem(favorites) ) );
    bool showFavorites = string.isEmpty();
    favorites->setHidden(!showFavorites);
    if (showFavorites)
    {
        favorites->sortChildren(1, Qt::DescendingOrder);
        favorites->setExpanded(string.isEmpty());
    }

    const int iconSize = m_visibleIcons > 29 ? 22 : (m_visibleIcons < 8 ? 48 : 32);
    m_tree->setIconSize( QSize(iconSize, iconSize) );

    m_lastFilter = string;
}

bool BE::Run::repopulate( KSharedPtr<KServiceGroup> group, QTreeWidgetItem *parent )
{
    if (!group || !group->isValid())
        return 0;

    bool ret = false;
    group->setShowEmptyMenu(false);
    // Iterate over all entries in the group
    const int level = parent->data(0,TreeLevel).toInt() + 1;
    foreach( const KSharedPtr<KSycocaEntry> p, group->entries(true, true, false, true))
    {
        if (p->isType(KST_KService))
        {
            KSharedPtr<KService> a = KSharedPtr<KService>::staticCast(p);
            if( a->isApplication() )
            {
                QTreeWidgetItem *item = new QTreeWidgetItem( parent );
                item->setIcon( 0, KIcon(a->icon()) );
                item->setText( 0, a->name() );
                item->setData( 0, GenericName, a->genericName() );
                item->setData( 0, Keywords, a->keywords() );
                item->setData( 0, Executable, true );
                item->setData( 0, ExecPath, a->exec() );
                item->setData( 0, EntryPath, p->entryPath() );
                item->setData( 0, Favor, favor(a->exec()) );
                ret = true;
            }
            else
                kDebug() << "Dunno here" << p->entryPath();
        }
        else if (p->isType(KST_KServiceGroup))
        {
            KSharedPtr<KServiceGroup> g = KSharedPtr<KServiceGroup>::staticCast(p);
            g->setShowEmptyMenu(false);
            if( g->entries(true,true).count() != 0 )
            {
//                 kDebug() << "Menu" << g->caption() << g->entries(true,true).count();
                QTreeWidgetItem *item = new QTreeWidgetItem( parent );
                item->setIcon( 0, KIcon(g->icon()) ); item->setText( 0, g->caption() );
                item->setData( 0, Executable, false ); item->setData( 0, TreeLevel, level );
                if (repopulate(g, item))
                    ret = true;
                else
                    delete item;
            }
        }
        else
            kDebug() << "Dunno" << p->entryPath();

    }
    return ret;
}

void BE::Run::focusInput()
{
    m_currentHistoryEntry = -1;
    m_tree->setCurrentItem(0,0);
    m_shell->selectAll();
}

void BE::Run::hideDueToInactivity()
{
    m_hideTimer->stop();
    if (m_inactive)
    {
        hide();
        focusInput();
    }
    m_inactive = false;
}

void BE::Run::releaseTrigger()
{
    if (m_triggeredItem)
    {
        m_triggeredItem->setData(0, Triggered, false);
        m_tree->viewport()->repaint( m_tree->visualItemRect(m_triggeredItem) );
    }
    m_triggeredItem = 0L;
}

static void rFadeFavor(QTreeWidgetItem *item)
{
    QTreeWidgetItem *kid;
    const int cnt = item->childCount();
    for (int i = 0; i < cnt; ++i)
    {
        kid = item->child(i);
        if ( kid->childCount() ) //this is a group
            rFadeFavor(kid);
        else if ( kid->data(0, Executable).toBool() )
            kid->setData(0, Favor, powf(kid->data(0, Favor).toDouble(), 0.9));
    }
}

static bool rMergeFavor(QTreeWidgetItem *item, QTreeWidgetItem *root, QTreeWidgetItem *favorites)
{
    QTreeWidgetItem *kid;
    const int cnt = root->childCount();
    for (int i = 0; i < cnt; ++i)
    {
        kid = root->child(i);
        if ( kid->childCount() && kid != favorites ) //this is a group
        {
            if (rMergeFavor(item, kid, favorites))
                return true;
        }
        else if ( item->data(0, ExecPath) == kid->data(0, ExecPath) )
        {
            kid->setData(0, Favor, item->data(0, Favor).toDouble());
            return true;
        }
    }
    return false;
}

void BE::Run::slotItemActivated(QTreeWidgetItem *item, int)
{
//     kDebug() << item->data().toString() << "clicked";
    if (!item)
        return;
    if (item->childCount())
    {
        m_slideRange = m_tree->verticalScrollBar()->maximum() - m_tree->verticalScrollBar()->minimum();
        bool expanded = !item->isExpanded();
        item->setExpanded(expanded);
        if (expanded)
            { m_slideItem = item; m_Slidehint = QAbstractItemView::PositionAtTop; }
        else if ( QTreeWidgetItem *dad = item->parent() )
            { m_slideItem = dad; m_Slidehint = QAbstractItemView::PositionAtTop; }
        else
            { m_slideItem = item; m_Slidehint = QAbstractItemView::PositionAtCenter; }
        QTimer::singleShot(100, this, SLOT(startSlide()));
    }
    else if ( item->data(0, Executable).toBool() )
    {
        QString errorString;
        if( KToolInvocation::startServiceByDesktopPath( item->data(0, EntryPath).toString(), QStringList(), &errorString ) > 0)
            kDebug() << "Error:"  << errorString;
        else
        {
#if 0
            if (m_triggeredItem)
                m_triggeredItem->setData(0, Triggered, false);
            m_triggeredItem = item;
            m_triggeredItem->setData(0, Triggered, true);
            QTimer::singleShot( 150, this, SLOT(releaseTrigger()) );
            m_tree->viewport()->repaint( m_tree->visualItemRect(item) );
#endif // doesn't work at all :-(
            if (!m_shell->text().isEmpty())
                focusInput();
            flash(m_shell->palette().color(QPalette::Highlight));
            m_inactive = true;
            rFadeFavor(m_tree->invisibleRootItem());
            item->setData(0, Favor, item->data(0, Favor).toDouble() + 1.0);
            if (item->parent() == favorites)
                rMergeFavor(item, m_tree->invisibleRootItem(), favorites);
            iScheduledResort = true;
            m_hideTimer->start( myVisibilityTimeout );
        }
    }
}

void BE::Run::slotRepopulate()
{
    m_tree->clear();
    m_tree->setColumnCount( 2 );
    m_tree->hideColumn( 1 );
    m_tree->setCurrentItem ( 0, 0 );
    m_tree->invisibleRootItem()->setData( 0, TreeLevel, -1 );

    favorites = new QTreeWidgetItem( m_tree->invisibleRootItem() );
    /*favorites->setIcon( 0, KIcon(g->icon()) );*/ favorites->setText( 0, i18n("Favorites") );
    favorites->setData( 0, Executable, false ); favorites->setData( 0, TreeLevel, 0 );

    repopulate( KServiceGroup::root(), m_tree->invisibleRootItem() );
    updateFavorites();
    favorites->setHidden(false);
    favorites->setExpanded(true);
}

static int sanityCounter;
void BE::Run::startSlide()
{
    if (!m_slideItem)
        return;
    if (sanityCounter > 5 || m_slideRange != m_tree->verticalScrollBar()->maximum() - m_tree->verticalScrollBar()->minimum())
    {
        slideTo(m_slideItem, m_Slidehint);
        sanityCounter = 0;
        m_slideItem = 0;
    }
    else
    {
        ++sanityCounter;
        QTimer::singleShot(100, this, SLOT(startSlide()));
    }
}
