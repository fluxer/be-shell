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

#include "be.shell.h"
#include "desktop.h"
#include "dbus_desktop.h"
#include "panel.h"
#include "trash.h"

#include <KDE/KConfigGroup>
#include <KDE/KDesktopFile>
#include <KDE/KDirWatch>
#include <KDE/KFileDialog>
#include <KDE/KGlobal>
#include <KDE/KIconLoader>
#include <KDE/KLocale>
#include <KDE/KRun>
#include <KDE/KStandardDirs>
#include <KDE/KUrlPixmapProvider>
#include <KDE/KWindowSystem>
#include <kio/netaccess.h>
#include <netwm.h>

//#include <konq_popupmenu.h>

#include <QImageReader>
#include <QApplication>
#include <QCheckBox>
#include <QDBusInterface>
#include <QDesktopServices>
#include <QDesktopWidget>
#include <QDialogButtonBox>
#include <QDir>
#include <QElapsedTimer>
#include <QFile>
#include <QFileInfo>
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QPainter>
#include <QPalette>
#include <QRadioButton>
#include <QSpinBox>
#include <QStyle>
#include <QtDBus>
#include <QToolTip>
#include <QVBoxLayout>
#include <QX11Info>

#include <kdebug.h>

QSize BE::DeskIcon::ourSize(64,64);
static bool isClick = false;
static QPoint dragStart;

BE::DeskIcon::DeskIcon( const QString &path, QWidget *parent ) : QToolButton( parent ), myUrl(path)
{
//     setToolButtonStyle( Qt::ToolButtonTextUnderIcon );
    setCursor(Qt::PointingHandCursor);
    setIconSize(ourSize);
    updateIcon();
}

void BE::DeskIcon::updateIcon()
{
    if( KDesktopFile::isDesktopFile(myUrl) )
    {
        KDesktopFile ds(myUrl);
        setIcon(DesktopIcon( ds.readIcon(), ourSize.width() ));
//         setText(ds.readName());
        setToolTip(ds.readName());
        myName = ds.readName();
    }
    else
    {
        QFileInfo fileInfo(myUrl);
        KUrlPixmapProvider imgLoader;
        setIcon(imgLoader.pixmapFor( myUrl, ourSize.width() ));
//         setText(fileInfo.fileName());
        setToolTip(fileInfo.fileName());
        myName = fileInfo.fileName();
    }
    myNameWidth = QFontMetrics(font()).width(myName) + 2*style()->pixelMetric( QStyle::PM_ToolTipLabelFrameWidth );
//     setFixedSize(size);
    update();
}

void BE::DeskIcon::enterEvent(QEvent *ev)
{
    QToolButton::enterEvent(ev);
//     QPoint pt = mapToGlobal(rect().bottomLeft());
//     pt.setX( pt.x() + (width()-myNameWidth)/2 - 4 );
//     pt.setY( pt.y() - 16 );
//     QToolTip::showText( pt, myName, this );
}

void BE::DeskIcon::leaveEvent(QEvent *ev)
{
//     QToolTip::hideText();
    QToolButton::leaveEvent(ev);
}

void BE::DeskIcon::mousePressEvent(QMouseEvent *ev)
{
    QToolButton::mousePressEvent(ev);
    if (ev->button() == Qt::RightButton)
    {
       // KonqPopupMenu menu();
       // menu.exec();
    }

    if (ev->button() == Qt::LeftButton)
    {
        isClick = true;
        dragStart = ev->pos();
    }
}

void BE::DeskIcon::mouseMoveEvent(QMouseEvent * ev)
{
    isClick = false;
    QPoint p = ev->globalPos() - dragStart;
    move(p.x(), p.y());
}

void BE::DeskIcon::mouseReleaseEvent(QMouseEvent *ev)
{
    QToolButton::mouseReleaseEvent(ev);
    if (testAttribute(Qt::WA_UnderMouse))
    {
        if (isClick)
            new KRun( KUrl(myUrl), this);
//         else
//             update location in the config file...
    }
    isClick = false;
}

void BE::DeskIcon::setSize(int s)
{
    ourSize = QSize(s,s);
}

void
BE::Desk::fileCreated( const QString &path )
{
    if (!myIcons.areShown)
        return;

    const int d = 4*DeskIcon::size().height()/3;
    if ( myIcons.lastPos.y() > myIcons.rect.bottom() - d )
    {
        myIcons.lastPos.ry() = myIcons.rect.top();
        myIcons.lastPos.rx() += d;
    }

    QString url = QFileInfo(path).absoluteFilePath();
    IconList::iterator i = myIcons.list.find(url);
    if ( i == myIcons.list.end() )
    {
        i = myIcons.list.insert(url, new DeskIcon( url, this ));
        (*i)->move( myIcons.lastPos );
        (*i)->show();
    }
    else
        (*i)->updateIcon();

    myIcons.lastPos.ry() += d;
}

void
BE::Desk::fileDeleted( const QString &path )
{
    if (!myIcons.areShown)
        return;
    delete myIcons.list.take( QFileInfo(path).absoluteFilePath() );
}

void
BE::Desk::fileChanged( const QString &path )
{
    if ( !myIcons.areShown )
        return;

    IconList::iterator i = myIcons.list.find( QFileInfo(path).absoluteFilePath() );
    if (i != myIcons.list.end())
        (*i)->updateIcon();
}

void
BE::Desk::populate( const QString &path )
{
    myIcons.lastPos = myIcons.rect.topLeft();

    if (!myIcons.areShown)
    {
        IconList::iterator i = myIcons.list.begin();
        while (i != myIcons.list.end())
            { delete *i; *i = 0; i = myIcons.list.erase(i); }
        myIcons.list.clear(); // rebuild hash
        return;
    }

    QDir desktopDir( path );
    QFileInfoList files = desktopDir.entryInfoList( QDir::AllEntries | QDir::NoDotAndDotDot );
    foreach( QFileInfo file, files)
        fileCreated(file.absoluteFilePath());
}


class BE::CornerDialog : public QDialog
{
public:
    CornerDialog( uint value, QWidget *parent ) : QDialog(parent), originalValue( value )
    {
        setWindowTitle(i18n("Set Round Corners"));
        setStyleSheet(QString());
        QGridLayout *gl = new QGridLayout(this);
        gl->addWidget( corners[0] = new QCheckBox( this ), 0, 0, Qt::AlignTop|Qt::AlignLeft );
        gl->addWidget( corners[1] = new QCheckBox( this ), 0, 2, Qt::AlignTop|Qt::AlignRight );
        gl->addWidget( corners[3] = new QCheckBox( this ), 2, 0, Qt::AlignBottom|Qt::AlignLeft );
        gl->addWidget( corners[2] = new QCheckBox( this ), 2, 2, Qt::AlignBottom|Qt::AlignRight );
        gl->addWidget( radius     = new QSpinBox( this ),  1, 1, Qt::AlignCenter );

        for ( int i = 0; i < 4; ++i )
            corners[i]->setChecked( bool(value & (1<<i)) );
        radius->setValue( value / 16 );

        QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, Qt::Horizontal, this);
        connect (buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
        connect (buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
        gl->addWidget(buttonBox, 3, 0, 1, -1);

        adjustSize();
        const int s = qMax(width(), height());
        resize(4*s/3,s);
//         dlg.move(QCursor::pos());
    }

    uint ask()
    {
        exec();
        if ( result() != Accepted )
            return originalValue;
        return corners[0]->isChecked() + 2*corners[1]->isChecked() +
               4*corners[2]->isChecked() + 8 * corners[3]->isChecked() +
               16 * radius->value();
    }
private:
    QCheckBox *corners[4];
    QSpinBox *radius;
    int originalValue;
};

class BE::WallpaperDesktopDialog : public QDialog
{
public:
    WallpaperDesktopDialog( QWidget *parent ) : QDialog(parent)
    {
        setWindowTitle(i18n("Select Desktops"));
        new QVBoxLayout(this);

        QLabel *hint = new QLabel(i18n("(ctrl: All, alt: Current, shift: force Scale & Crop)"), this);
        QPalette pal = hint->palette();
        QColor c = pal.color(hint->foregroundRole()); c.setAlpha(2*c.alpha()/3);
        pal.setColor(hint->foregroundRole(), c);
        hint->setPalette(pal);
        layout()->addWidget(hint);

        layout()->addWidget(new QLabel(i18n("Set myWallpaper on:"), this));

        all = new QRadioButton(i18n("All Desktops"), this);
        layout()->addWidget(all);

        current = new QRadioButton(i18n("Current Desktop"), this);
        layout()->addWidget(current);

        list = new QGroupBox(i18n("Individual Desktops"), this);
        connect (list, SIGNAL(toggled(bool)), all, SLOT(setDisabled(bool)) );
        connect (list, SIGNAL(toggled(bool)), current, SLOT(setDisabled(bool)) );
        new QVBoxLayout(list);
        desk = new QCheckBox*[KWindowSystem::numberOfDesktops()];
        for (int i = 0; i < KWindowSystem::numberOfDesktops(); ++i)
            list->layout()->addWidget(desk[i] = new QCheckBox(i18n("Desktop %1").arg(i+1), list));
        list->setCheckable(true);
        layout()->addWidget(list);

        QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, Qt::Horizontal, this);
        connect (buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
        connect (buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
        layout()->addWidget(buttonBox);

        adjustSize();
        const int s = qMax(width(), height());
        resize(4*s/3,s);
//         dlg.move(QCursor::pos());
    }
    ~WallpaperDesktopDialog()
    {
        delete [] desk; // radios are deleted by Qt
    }

    QList<int> ask()
    {
        QList<int> desks;
        for (int i = 0; i < KWindowSystem::numberOfDesktops(); ++i)
            desk[i]->setChecked(false);
        list->setChecked(false);
        current->setChecked(false);
        all->setChecked(true);

        exec();
        if (result() == Accepted)
        {
            if (list->isChecked())
            {
                for (int i = 0; i < KWindowSystem::numberOfDesktops(); ++i)
                    if (desk[i]->isChecked())
                        desks << i+1;
                    if (desks.isEmpty())
                        desks << AllDesktops;
            }
            else if (current->isChecked())
                desks << KWindowSystem::currentDesktop();
            else
                desks << AllDesktops;
        }
        return desks;
    }
private:
    QCheckBox **desk;
    QGroupBox *list;
    QRadioButton *all, *current;
};

class BE::Corner : public QWidget
{
public:
    Corner( Qt::Corner corner, QWidget *parent = 0 );
    static void setRadius( uint r )
    {
        if ( r == ourRadius )
            return;
        delete ourPixmap;
        ourPixmap = 0L;
        ourRadius = r;
    }
protected:
    bool eventFilter( QObject *o, QEvent *e );
    void paintEvent( QPaintEvent *pe );
private:
    Qt::Corner myCorner;
    static QPixmap *ourPixmap;
    static bool weAreNotInRace;
    static uint ourRadius;
};

QPixmap *BE::Corner::ourPixmap = 0;
bool BE::Corner::weAreNotInRace = true;
uint BE::Corner::ourRadius = 12;

BE::Corner::Corner( Qt::Corner corner, QWidget *parent ) : QWidget( parent ), myCorner( corner )
{
    setFixedSize( ourRadius, ourRadius );

    if ( !ourPixmap )
    {
        ourPixmap = new QPixmap( 2*ourRadius, 2*ourRadius );
        ourPixmap->fill( Qt::transparent ); // make ARGB
        QPainter p( ourPixmap );
        p.fillRect( ourPixmap->rect(), Qt::black );
        p.setPen( Qt::NoPen );
        p.setBrush( Qt::white );
        p.setRenderHint( QPainter::Antialiasing );
        p.setCompositionMode( QPainter::CompositionMode_DestinationOut );
        p.drawEllipse( ourPixmap->rect() );
        p.end();
    }
    QEvent e( QEvent::Resize );
    eventFilter( parentWidget(), &e ); // move
    show();
    raise();
    installEventFilter( this );
    parentWidget()->installEventFilter( this );
}

bool
BE::Corner::eventFilter( QObject *o, QEvent *e )
{
    if ( (e->type() == QEvent::ZOrderChange && o == this) ||
         (e->type() == QEvent::ChildAdded && o == parentWidget()) )
    {
        if ( weAreNotInRace )
        {
            weAreNotInRace = false;
            raise();
            weAreNotInRace = true;
        }
        return false;
    }

    if ( e->type() == QEvent::Resize && o == parentWidget() )
    {
        switch ( myCorner )
        {
        case Qt::TopLeftCorner:     move( 0, 0); break;
        case Qt::BottomLeftCorner:  move( 0, parentWidget()->height() - height() ); break;
        case Qt::TopRightCorner:    move( parentWidget()->width() - width(), 0 ); break;
        case Qt::BottomRightCorner: move( parentWidget()->width() - width(), parentWidget()->height() - height() ); break;
        }
        return false;
    }
    return false;
}

void
BE::Corner::paintEvent( QPaintEvent * )
{
    int sx(0), sy(0);
    switch ( myCorner )
    {
    case Qt::TopLeftCorner:     break;
    case Qt::BottomLeftCorner:  sy = height(); break;
    case Qt::TopRightCorner:    sx = width(); break;
    case Qt::BottomRightCorner: sx = width(); sy = height(); break;
    }
    QPainter p(this);
    p.drawPixmap ( 0, 0, *ourPixmap, sx, sy, width(), height() );
    p.end();
}

void
BE::Desk::setRoundCorners()
{
    if ( sender() )
    {
        uint corners = CornerDialog( myCorners, this ).ask();
        if ( corners == myCorners )
            return;
        myCorners = corners;
        Plugged::saveSettings();
    }

    QList<BE::Corner*> corners;
    QObjectList kids = children();
    for ( int i = 0; i < kids.count(); ++i )
    {
        if ( BE::Corner *cnr = dynamic_cast<BE::Corner*>(kids.at(i)) )
            corners << cnr;
    }
    for ( int i = 0; i < corners.count(); ++i )
        delete corners.at(i);

    if ( !myCorners )
        return;

    BE::Corner::setRadius( myCorners >> 4 );

//     corner &= 0xf;
    if ( myCorners & 1 )
        new Corner( Qt::TopLeftCorner, this );
    if ( myCorners & 2 )
        new Corner( Qt::TopRightCorner, this );
    if ( myCorners & 4 )
        new Corner( Qt::BottomRightCorner, this );
    if ( myCorners & 8 )
        new Corner( Qt::BottomLeftCorner, this );
}

BE::Desk::Desk( QWidget *parent ) : QWidget(parent)
, BE::Plugged(parent)
, myScreen(QApplication::desktop()->primaryScreen())
, myCorners( 0 )
, myWpDialog(0)
{
    setObjectName("Desktop");
    setFocusPolicy( Qt::ClickFocus );
    setAcceptDrops( true );
    // to get background-image interpreted
//     setAttribute(Qt::WA_StyledBackground);
    setAttribute(Qt::WA_NoSystemBackground);
    connect( QApplication::desktop(), SIGNAL(resized(int)), this, SLOT(desktopResized(int)));
    connect( this, SIGNAL(wallpaperChanged()), this, SLOT(updateOnWallpaperChange()));
    KWindowSystem::setType( winId(), NET::Desktop );
    KWindowSystem::setOnAllDesktops( winId(), true );

    connect (configMenu(), SIGNAL(aboutToShow()), SLOT(bindScreenMenu()));

    myWallpaper.align = (Qt::Alignment)-1;
    myWallpaper.aspect = -1.0;
    myWallpaper.mode = (WallpaperMode)-1;
    myShadow.opacity = -1;
    myIcons.areShown = false;
    ignoreSaveRequest = false;
    iRootTheWallpaper = false;

    configMenu()->addSeparator()->setText(i18n("Desktop"));
    QMenu *menu = configMenu()->addMenu(i18n("Wallpaper"));
    menu->addAction( i18n("Select..."), this, SLOT(selectWallpaper()) );
    wpSettings.mode = menu->addMenu(i18n("Mode"));
    wpSettings.mode->addAction( i18n("Plain") )->setData(100);
    wpSettings.mode->addAction( i18n("Tile #") )->setData(200);
    wpSettings.mode->addAction( i18n("Tile || ") )->setData(300);
    wpSettings.mode->addAction( i18n("Tile = ") )->setData(400);
    wpSettings.mode->addAction( i18n("Stretch (drop aspect)") )->setData(500);
    wpSettings.mode->addAction( i18n("Maximal (keep aspect)") )->setData(600);
    wpSettings.mode->addAction( i18n("Scale && Crop (tm)") )->setData(700);
    foreach (QAction *act, wpSettings.mode->actions())
        act->setCheckable(true);
    connect(wpSettings.mode, SIGNAL(triggered(QAction*)), this, SLOT(changeWallpaperMode(QAction*)));

    wpSettings.align = menu->addMenu(i18n("Alignment"));
    wpSettings.align->addAction( i18n("Left") )->setData(10);
    wpSettings.align->addAction( i18n("Center") )->setData(30);
    wpSettings.align->addAction( i18n("Right") )->setData(20);
    wpSettings.align->addSeparator();
    wpSettings.align->addAction( i18n("Top") )->setData(1);
    wpSettings.align->addAction( i18n("Center") )->setData(0);
    wpSettings.align->addAction( i18n("Bottom") )->setData(2);
    foreach (QAction *act, wpSettings.align->actions())
        act->setCheckable(true);
    connect(wpSettings.align, SIGNAL(triggered(QAction*)), this, SLOT(changeWallpaperAlignment(QAction*)));

    wpSettings.aspect = menu->addMenu(i18n("Aspect"));
    wpSettings.aspect->addAction( i18n("Original") )->setData(-1.0);
    wpSettings.aspect->addAction( i18n("1:1") )->setData(1.0);
    wpSettings.aspect->addAction( i18n("5:4") )->setData(5.0/4.0);
    wpSettings.aspect->addAction( i18n("4:3") )->setData(4.0/3.0);
    wpSettings.aspect->addAction( i18n("Photo / VistaVision / \"3:2\"") )->setData(3.0/2.0);
    wpSettings.aspect->addAction( i18n("14:9") )->setData(14.0/9.0);
    wpSettings.aspect->addAction( i18n("16:10") )->setData(16.0/10.0);
    wpSettings.aspect->addAction( i18n("16:9") )->setData(16.0/9.0);
    wpSettings.aspect->addAction( i18n("Movie") )->setData(1.85);
    wpSettings.aspect->addAction( i18n("2:1") )->setData(2.0);
    wpSettings.aspect->addAction( i18n("Cinemascope") )->setData(2.35);
    wpSettings.aspect->addAction( i18n("Panavision") )->setData(2.39);
    wpSettings.aspect->addAction( i18n("Cinemascope 55") )->setData(2.55);
    foreach (QAction *act, wpSettings.aspect->actions())
        act->setCheckable(true);
    connect(wpSettings.aspect, SIGNAL(triggered(QAction*)), this, SLOT(changeWallpaperAspect(QAction*)));

    menu = configMenu()->addMenu(i18n("Settings"));
//     xserver              - X-Server information
//     xinerama             - Configure KDE for multiple monitors
//     desktoppath          - Change the location important files are stored
//     icons                - Customize KDE Icons
    menu->addAction( i18n("System settings"))->setData("systemsettings");
    menu->addSeparator();
    menu->addAction( i18n("Active screen edges"))->setData("kwinscreenedges");
    menu->addAction( i18n("Virtual Desktops"))->setData("desktop");
    menu->addAction( i18n("Window behaviour"))->setData("kwinoptions");
    menu->addAction( i18n("Window switching"))->setData("kwintabbox");
    menu->addSeparator();
    menu->addAction( i18n("Screen locking"))->setData("screensaver");
    menu->addAction( i18n("Desktop FX"))->setData("kwincompositing");
    menu->addAction( i18n("Monitor"))->setData("display");
    menu->addSeparator();
    menu->addAction( i18n("Trashcan"))->setData("kcmtrash");
    connect(menu, SIGNAL(triggered(QAction*)), this, SLOT(kcmshell4(QAction*)) );

    configMenu()->addAction( i18n("Rounded corners..."), this, SLOT(setRoundCorners()) );

    myIcons.menuItem = configMenu()->addAction( i18n("Icons"), this, SLOT(toggleIconsVisible(bool)) );
    myIcons.menuItem->setCheckable(true);

    myRootBlurRadius = 0;

    myTrash.can = 0;
    myTrash.menuItem = configMenu()->addAction( i18n("Trashcan"), this, SLOT(toggleTrashcan(bool)) );
    myTrash.menuItem->setCheckable(true);

    myDesktopWatcher = new KDirWatch(this);
    connect( myDesktopWatcher, SIGNAL( created(QString) ), this, SLOT( fileCreated(QString) ));
    connect( myDesktopWatcher, SIGNAL( deleted(QString) ), this, SLOT( fileDeleted(QString) ));
    connect( myDesktopWatcher, SIGNAL( dirty(QString) ), this, SLOT( fileChanged(QString) ));


    myDesktopWatcher->addDir(QDesktopServices::storageLocation(QDesktopServices::DesktopLocation), KDirWatch::WatchFiles );

    QDBusConnection::sessionBus().registerObject("/Desktop", this);

    setGeometry(0, 0, QApplication::desktop()->width(), QApplication::desktop()->height() );

    connect( KWindowSystem::self(), SIGNAL(currentDesktopChanged(int)), this, SLOT(desktopChanged(int)) );
    connect( KWindowSystem::self(), SIGNAL(numberOfDesktopsChanged (int)), this, SLOT(configure()) );

    myCurrentDesktop = KWindowSystem::currentDesktop();

    new BE::DeskAdaptor(this);

    XWMHints *hints = XGetWMHints ( QX11Info::display(), window()->winId() );
    hints->input = false;
    XSetWMHints ( QX11Info::display(), window()->winId(), hints );

}

void
BE::Desk::configure( KConfigGroup *grp )
{
    delete myWpDialog; myWpDialog = 0;

    ignoreSaveRequest = true;
    bool b; int i; QString s; float f;
    bool changeWallpaper = false;
    bool needUpdate = false;

    int oldScreen = myScreen;
    myScreen = grp->readEntry("Screen", QApplication::desktop()->primaryScreen() );
    if (myScreen >= QApplication::desktop()->screenCount())
        myScreen = QApplication::desktop()->primaryScreen();
    if (myScreen != oldScreen && (oldScreen < 0 || myScreen < 0 ||
                                  oldScreen >= QApplication::desktop()->screenCount() ||
                                  QApplication::desktop()->screenGeometry(myScreen) != geometry()) )
        desktopResized( myScreen );

    b = myIcons.areShown;
    myIcons.areShown = grp->readEntry("ShowIcons", QVariant(true) ).toBool();
    if (b != myIcons.areShown )
        populate(QDesktopServices::storageLocation(QDesktopServices::DesktopLocation));
    myIcons.menuItem->setChecked(myIcons.areShown);

    toggleTrashcan(grp->readEntry("TrashCan", QVariant(true) ).toBool());
    myTrash.menuItem->setChecked(myTrash.can);

    int sz = grp->readEntry("TrashSize", 64);
    myTrash.geometry.setRect(grp->readEntry("TrashX", 200), grp->readEntry("TrashY", 200), sz, sz);
    if (myTrash.can)
    {
        myTrash.can->setFixedSize(myTrash.geometry.size());
        myTrash.can->move(myTrash.geometry.topLeft());
    }

    uint corners = myCorners;
    myCorners = grp->readEntry("Corners", myCorners);
    if ( myCorners != corners )
        setRoundCorners();

    changeWallpaper = false;

    QRect oldWPArea = wpSettings.area;
    wpSettings.area = grp->readEntry("WallpaperArea", QRect());
    if (wpSettings.area != oldWPArea)
        changeWallpaper = true;

    s = myWallpaper.file;
    myWallpaper.file = grp->readEntry("Wallpaper", QString());
    if ( s != myWallpaper.file )
        changeWallpaper = true;
    i = (int)myWallpaper.align;
    myWallpaperDefaultAlign = (Qt::Alignment)grp->readEntry("WallpaperDefaultAlign", QVariant(Qt::AlignCenter) ).toInt();
    myWallpaper.align = (Qt::Alignment)grp->readEntry("WallpaperAlign", QVariant(myWallpaperDefaultAlign) ).toInt();
    if ( i != myWallpaper.align )
        needUpdate = true;

    f = myWallpaper.aspect;
    myWallpaper.aspect = grp->readEntry("WallpaperAspect", -1.0f );
    if ( f != myWallpaper.aspect )
        needUpdate = true;

    i = (int)myWallpaper.mode;
    myWallpaperDefaultMode = (WallpaperMode)grp->readEntry("WallpaperDefaultMode", int(ScaleAndCrop) );
    myWallpaper.mode = (WallpaperMode)grp->readEntry("WallpaperMode", int(myWallpaperDefaultMode) );
    if ( i != myWallpaper.mode)
        changeWallpaper = true;

    b = iRootTheWallpaper;
    iRootTheWallpaper = grp->readEntry("Rootpaper", true );
    if ( b != iRootTheWallpaper)
        changeWallpaper = true;

    if (changeWallpaper)
        setWallpaper( myWallpaper.file, 0 );

    if (KWindowSystem::numberOfDesktops() > 1)
    for (int d = 1; d <= KWindowSystem::numberOfDesktops(); ++d )
    {
        changeWallpaper = false;
        Wallpapers::iterator wp = myWallpapers.find(d);
        s = grp->readEntry(QString("Wallpaper_%1").arg(d), QString());
        if (s.isEmpty() || !s.compare("none", Qt::CaseInsensitive))
        {
            if (wp != myWallpapers.end())
                myWallpapers.erase(wp);
            continue;
        }

        if (wp == myWallpapers.end())
        {
            wp = myWallpapers.insert(d, Wallpaper());
            wp->align = (Qt::Alignment)-1;
            wp->aspect = -1.0;
            wp->mode = (WallpaperMode)-1;
            changeWallpaper = true;
        }

        if ( s != wp->file )
            changeWallpaper = true;
        wp->file = s;

        i = (int)wp->align;
        wp->align = (Qt::Alignment)grp->readEntry(QString("WallpaperAlign_%1").arg(d), int(Qt::AlignCenter) );
        if ( i != wp->align )
            needUpdate = true;

        f = (int)wp->aspect;
        wp->aspect = grp->readEntry(QString("WallpaperAspect_%1").arg(d), -1.0f );
        if ( f != wp->aspect )
            needUpdate = true;

        i = (int)wp->mode;
        wp->mode = (WallpaperMode)grp->readEntry(QString("WallpaperMode_%1").arg(d), int(ScaleAndCrop) );
        if ( i != wp->mode)
            changeWallpaper = true;

        if (changeWallpaper)
            setWallpaper( wp->file, 0, d );
    }

    i = myShadow.opacity;
    myShadow.opacity = grp->readEntry("ShadowOpacity", 25 );
    if (i != myShadow.opacity)
    {
        const int alpha = myShadow.opacity*255/100;
        if (alpha)
        {
            QPainter p;

            int size = 19;
            QPixmap shadowBlob(size,size);
            shadowBlob.fill(Qt::transparent);
            float d = size/2.0;
            QRadialGradient rg(d, d, d);
            rg.setFocalPoint(d, 7);
            rg.setColorAt( 0, QColor(0,0,0,alpha) );
            rg.setColorAt( 1, QColor(0,0,0,0) );

            p.begin(&shadowBlob);
            p.fillRect(shadowBlob.rect(), rg);
            p.end();

            myShadow.topLeft = shadowBlob.copy(0,0,9,6);
            myShadow.topRight = shadowBlob.copy(10,0,9,6);
            myShadow.bottomLeft = shadowBlob.copy(0,7,9,12);
            myShadow.bottomRight = shadowBlob.copy(10,7,9,12);

#define DUMP_SHADOW(_T_, _W_, _H_)\
myShadow._T_ = QPixmap(_W_,_H_);\
myShadow._T_.fill(Qt::transparent);\
p.begin(&myShadow._T_);\
p.drawTiledPixmap(myShadow._T_.rect(), buffer);\
p.end()

            QPixmap buffer = shadowBlob.copy(10,0,1,6);
            DUMP_SHADOW(top, 32, 6);
            buffer = shadowBlob.copy(10,7,1,12);
            DUMP_SHADOW(bottom, 32, 12);
            buffer = shadowBlob.copy(0,7,9,1);
            DUMP_SHADOW(left, 9, 32);
            buffer = shadowBlob.copy(10,7,9,1);
            DUMP_SHADOW(right, 9, 32);
            buffer = shadowBlob.copy(9,9,1,1);
            DUMP_SHADOW(center, 32, 32); // ? usage?
#undef DUMP_SHADOW
        }
        needUpdate = true;
    }

    i = myRootBlurRadius;
    myRootBlurRadius = grp->readEntry("BlurRadius", 0 );
    if (i != myRootBlurRadius)
        updateOnWallpaperChange();
    else if (needUpdate)
        update();
    ignoreSaveRequest = false;
}

void
BE::Desk::changeWallpaperAspect( QAction *action )
{
    bool common; Wallpaper &wp = currentWallpaper(&common);
    bool ok; wp.aspect = action->data().toFloat(&ok); if (!ok) wp.aspect = -1.0;
    setWallpaper( wp.file, 0, common ? -3 : KWindowSystem::currentDesktop() );
}

void
BE::Desk::changeWallpaperMode( QAction *action )
{
    bool common; const Wallpaper &wp = currentWallpaper(&common);
    setWallpaper( wp.file, action->data().toInt(), common ? -3 : KWindowSystem::currentDesktop() );
}

static Qt::Alignment alignFromInt(int i)
{
    if (i > 9)
    {
        i /= 10;
        return i == 1 ? Qt::AlignLeft : (i == 2 ? Qt::AlignRight : Qt::AlignHCenter);
    }
    else
        return i == 1 ? Qt::AlignTop : (i == 2 ? Qt::AlignBottom : Qt::AlignVCenter);
}

void
BE::Desk::changeWallpaperAlignment( QAction *action )
{
    Wallpaper &wp = currentWallpaper();
    int a = action->data().toInt();
    if (a > 9)
        wp.align = (wp.align & (Qt::AlignTop|Qt::AlignBottom|Qt::AlignVCenter)) | alignFromInt(a);
    else
        wp.align = (wp.align & (Qt::AlignLeft|Qt::AlignRight|Qt::AlignHCenter)) | alignFromInt(a);

    wp.calculateOffset(wpSettings.area.isValid() ? wpSettings.area.size() : size(), wpSettings.area.topLeft());

    emit wallpaperChanged();
    Plugged::saveSettings();
}

BE::Wallpaper &
BE::Desk::currentWallpaper(bool *common)
{
    Wallpapers::iterator i = myWallpapers.find(KWindowSystem::currentDesktop());
    if (i == myWallpapers.end())
    {
        if (common) *common = true;
        return myWallpaper;
    }
    if (common) *common = false;
    return i.value();
}

void
BE::Desk::kcmshell4(QAction *act)
{
    if (act)
    {
        QString module = act->data().toString();
        if (module == "systemsettings")
            BE::Shell::run(module);
        else if (!module.isEmpty())
            BE::Shell::run("kcmshell4 " + module);
    }
}

void
BE::Desk::saveSettings( KConfigGroup *grp )
{
    if (ignoreSaveRequest)
        return;
    grp->deleteGroup(); // required to clean myWallpaperXYZ entries
    grp->writeEntry( "BlurRadius", myRootBlurRadius );
    grp->writeEntry( "Corners", myCorners );
    if (myScreen != QApplication::desktop()->primaryScreen())
        grp->writeEntry( "Screen", myScreen );
    grp->writeEntry( "ShadowOpacity", myShadow.opacity );
    grp->writeEntry( "ShowIcons", myIcons.areShown );
    grp->writeEntry( "Wallpaper", myWallpaper.file );
    grp->writeEntry( "WallpaperDefaultAlign", (int)myWallpaperDefaultAlign );
    grp->writeEntry( "WallpaperDefaultMode", (int)myWallpaperDefaultMode );
    grp->writeEntry( "WallpaperAlign", (int)myWallpaper.align );
    if (wpSettings.area.isValid())
        grp->writeEntry( "WallpaperArea", wpSettings.area );
    grp->writeEntry( "WallpaperAspect", (float)myWallpaper.aspect );
    grp->writeEntry( "WallpaperMode", (int)myWallpaper.mode );
    Wallpapers::const_iterator i;
    for (i = myWallpapers.constBegin(); i != myWallpapers.constEnd(); ++i)
    {
        grp->writeEntry( QString("Wallpaper_%1").arg(i.key()), i.value().file );
        grp->writeEntry( QString("WallpaperAlign_%1").arg(i.key()), (int)i.value().align );
        grp->writeEntry( QString("WallpaperAspect_%1").arg(i.key()), (float)i.value().aspect );
        grp->writeEntry( QString("WallpaperMode_%1").arg(i.key()), (int)i.value().mode );
    }
    grp->writeEntry( "TrashCan", bool(myTrash.can) );
    if (myTrash.can)
    {
        grp->writeEntry( "TrashSize", myTrash.can->width());
        grp->writeEntry( "TrashX", myTrash.can->geometry().x());
        grp->writeEntry( "TrashY", myTrash.can->geometry().y());
    }
}

void
BE::Desk::selectWallpaper()
{
    setWallpaper(KFileDialog::getImageOpenUrl(KUrl("kfiledialog:///myWallpaper"), 0, "Try drag & drop an image onto the Desktop!").path(), -1, -2);
}

void
BE::Desk::bindScreenMenu()
{
    BE::Shell::screenMenu()->disconnect(SIGNAL(triggered(QAction*)));
    BE::Shell::screenMenu()->setProperty("CurrentScreen", myScreen);
    connect (BE::Shell::screenMenu(), SIGNAL(triggered(QAction*)), SLOT(setOnScreen(QAction*)));
}

void
BE::Desk::setOnScreen( QAction *action )
{
    BE::Shell::screenMenu()->disconnect(this);
    if (!action)
        return;
    bool ok; int screen = action->data().toInt(&ok);
    if (!ok)
        return;
    if (screen >= QApplication::desktop()->screenCount())
        return;
    if (((screen < 0) xor (myScreen < 0)) || QApplication::desktop()->screenGeometry(screen) != geometry())
        desktopResized( myScreen = screen );
}

void
BE::Desk::setWallpaper( const QString &file, int mode, int desktop )
{
    if (file.isEmpty())
        return; // "none" is ok - [empty] is not but might result from skipped dialog
    QList<int> desks;
    if (KWindowSystem::numberOfDesktops() == 1)
        desks << AllDesktops;
    else if (desktop == AskForDesktops)
    {
        if (!myWpDialog)
        {
            myWpDialog = new WallpaperDesktopDialog(this);
            myWpDialog->setStyleSheet(QString());
        }
        desks = myWpDialog->ask();
        if (myWpDialog->result() != QDialog::Accepted)
            return;
    }
    else
        desks << desktop;

    bool changed = desks.contains(KWindowSystem::currentDesktop()) ||
                   desks.contains(AllDesktops) ||
                   desktop == -3;

    desktop = desks.takeFirst();

    Wallpaper &wp = (desktop < 0) ? myWallpaper : myWallpapers[desktop];

    if ( file == "none" )
    {
        wp.pix = QPixmap();
        wp.file = file;
        wp.aspect = -1.0;
    }
    else
    {

        // first check whether we already have this file loaded somewhere...
        bool canCopy = false;
        if ( (canCopy = myWallpaper.fits(file, mode)) )
            wp = myWallpaper;
        else foreach (Wallpaper present, myWallpapers)
        {
            if ( (canCopy = present.fits(file, mode)) )
            {
                wp = present;
                break;
            }
        }

        if ( canCopy )
            canCopy = wp.aspect <= 0.0 || (wp.pix.height() * wp.aspect == wp.pix.width());

        // matches all, we copied a present one in case, get rid of individuals
        if (desktop == AllDesktops)
            myWallpapers.clear();

        QSize sz = wpSettings.area.isValid() ? wpSettings.area.size() : size();

        // we couldn't take one out of the list, load new from file
        if (!canCopy)
        {
            QImage img( file );
            if ( img.isNull() )
                return;

            if (wp.file != file)
            {
                wp.file = file;
                wp.aspect = -1.0;
            }

            if (mode < 0)
            {   // heuristics, we want central, scale'n'crop if big enough, tiled if very small (and near square)
                wp.align = Qt::AlignCenter;
                if ( img.width() > sz.width()/2 && img.height() > sz.height()/2 )
                {
                    wp.mode = myWallpaperDefaultMode;
                    wp.align = myWallpaperDefaultAlign;
                }
                else if (( img.width() < sz.width()/6 && img.height() < sz.height()/6 ) ||
                        ( img.width() == img.height() && img.width() < sz.width()/4 && img.height() < sz.height()/4 ))
                    wp.mode = Tiled;
                else if ( img.width() < sz.width()/10 && img.height() > 3*sz.height()/4 )
                    wp.mode = ScaleV;
                else if ( img.height() < sz.height()/10 && img.width() > 3*sz.width()/4 )
                    wp.mode = ScaleH;
                else
                    wp.mode = Plain;
            }

            else if (mode)
            {
                while ( mode < 100)
                    mode *= 10;
                wp.mode = (WallpaperMode)( mode / 100 );
                const int a = mode - 100*wp.mode;
                wp.align = alignFromInt(a) | alignFromInt(a - a/10);
            }

            if (wp.aspect > 0.0)
            {
                const float asp = (float)sz.width()/(float)sz.height();
                if (wp.aspect > asp)
                    img = img.scaled( sz.width(), sz.width()/wp.aspect, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
                else
                    img = img.scaled( sz.height()*wp.aspect, sz.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
            }
            else
            {
                switch (wp.mode)
                {
                case ScaleV:
                    img = img.scaled( img.width(), sz.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation ); break;
                case ScaleH:
                    img = img.scaled( sz.width(), img.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation ); break;
                case Scale:
                    img = img.scaled( sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation ); break;
                case Maximal:
                    img = img.scaled( sz, Qt::KeepAspectRatio, Qt::SmoothTransformation ); break;
                case ScaleAndCrop:
                    img = img.scaled( sz, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation );
                default:
                    break;
                }
            }

            wp.pix = QPixmap::fromImage(img);

        }
        wp.calculateOffset(sz, wpSettings.area.topLeft());

        // apply to other demanded desktops - iff any
        if (!desks.isEmpty())
        {
            foreach (desktop, desks)
                myWallpapers[desktop] = wp;
        }

    }

    if (changed)
        emit wallpaperChanged();
    Plugged::saveSettings();
}

void
BE::Desk::storeTrashPosition()
{
    myTrash.geometry = myTrash.can->geometry();
    Plugged::saveSettings();
}

void
BE::Desk::toggleDesktopShown()
{
    const long unsigned int props[2] = {0, NET::WM2ShowingDesktop};
    NETRootInfo net(QX11Info::display(), props, 2);
    net.setShowingDesktop(!net.showingDesktop());
}

void
BE::Desk::toggleIconsVisible( bool on )
{
    myIcons.areShown = on;
    populate(QDesktopServices::storageLocation(QDesktopServices::DesktopLocation));
    Plugged::saveSettings();
}

void
BE::Desk::toggleTrashcan( bool on )
{
    if (!on)
        {delete myTrash.can; myTrash.can = 0; }
    else if (!myTrash.can)
    {
        myTrash.can = new BE::Trash(this);
        connect ( myTrash.can, SIGNAL(moved()), this, SLOT(storeTrashPosition()) );
        myTrash.can->setFixedSize(myTrash.geometry.size());
        myTrash.can->move(myTrash.geometry.topLeft());
    }
    Plugged::saveSettings();
}

void
BE::Desk::updateOnWallpaperChange()
{
//     XSetWindowBackgroundPixmap(QX11Info::display(), winId(), currentWallpaper().pix.handle());
//     XClearWindow(QX11Info::display(), winId());
    update();

    const BE::Wallpaper &wp = currentWallpaper();
    if (iRootTheWallpaper)
    {
        QRect sr = geometry();
        sr.moveTo(-wp.offset);
        Pixmap xPix = XCreatePixmap(QX11Info::display(), QX11Info::appRootWindow(),
                                    width(), height(),
                                    DefaultDepth(QX11Info::display(), DefaultScreen(QX11Info::display())));
        QPixmap qPix = QPixmap::fromX11Pixmap(xPix, QPixmap::ExplicitlyShared);
        if (myRootBlurRadius)
        {
            QImage *img = new QImage(size(), QImage::Format_RGB32);
            render(img, QPoint(), rect(), DrawWindowBackground);
            BE::Shell::blur( *img, myRootBlurRadius );
            QPainter p(&qPix);
            p.drawImage(0,0,*img);
            p.end();
            delete img;
        }
        else
            render(&qPix, QPoint(), rect(), DrawWindowBackground);
        XSetWindowBackgroundPixmap(QX11Info::display(), QX11Info::appRootWindow(), xPix);
        XFreePixmap(QX11Info::display(), xPix);
        XClearWindow(QX11Info::display(), QX11Info::appRootWindow());
    }
    // update menus
    foreach (QAction *act, wpSettings.aspect->actions())
        act->setChecked( act->data() == wp.aspect );
    foreach (QAction *act, wpSettings.mode->actions())
        act->setChecked( act->data() == wp.mode*100 );
    foreach (QAction *act, wpSettings.align->actions())
        act->setChecked( wp.align & alignFromInt(act->data().toInt()) );

}

bool
BE::Desk::eventFilter(QObject *o, QEvent *e)
{
    if (e->type() == QEvent::Hide || e->type() == QEvent::Show)
    if (qobject_cast<BE::Panel*>(o))
        update();
    return false;
}


static QElapsedTimer mouseDownTimer;
void
BE::Desk::mousePressEvent( QMouseEvent *me )
{
    if (me->button() == Qt::LeftButton && BE::Shell::touchMode())
        mouseDownTimer.start();
    else if (me->button() == Qt::RightButton)
        configMenu()->popup( me->globalPos() );
    else if (me->button() == Qt::MidButton)
    {
        QPoint pos = me->globalPos();
        QDBusInterface shell( "org.kde.be.shell", "/Shell", "org.kde.be.shell", QDBusConnection::sessionBus() );
        shell.call("showWindowList", pos.x(), pos.y());
    }
}

void
BE::Desk::mouseReleaseEvent(QMouseEvent *me)
{
    if (me->button() == Qt::LeftButton)
    {
        const bool showMenu = BE::Shell::touchMode() && mouseDownTimer.elapsed() > 350;
        mouseDownTimer.invalidate();
        XWMHints *hints = XGetWMHints ( QX11Info::display(), window()->winId() );
        hints->input = true;
        XSetWMHints( QX11Info::display(), winId(), hints );
        XSync( QX11Info::display(), 0 );
//         activateWindow();
        KWindowSystem::forceActiveWindow(winId());
        XSync( QX11Info::display(), 0 );
        hints->input = false;
        XSetWMHints( QX11Info::display(), winId(), hints );
        XSync( QX11Info::display(), 0 );
        if (showMenu)
            configMenu()->popup( me->globalPos() );
    }
}

#define FIRST_URL ( de && de->mimeData() ) { \
    QList<QUrl> urls = de->mimeData()->urls();\
    if (!urls.isEmpty()) { \
        KUrl url = urls.first();

void
BE::Desk::dragEnterEvent( QDragEnterEvent *de )
{
    if FIRST_URL
        if (url.isLocalFile())
        {
            if ( QImageReader(url.path()).canRead() )
                de->accept();
        }
        else
        {
            QString file = url.fileName();
            if (file.endsWith(".jpg", Qt::CaseInsensitive) || file.endsWith(".jpeg", Qt::CaseInsensitive) ||
                file.endsWith(".png", Qt::CaseInsensitive) || file.endsWith(".bmp", Qt::CaseInsensitive))
                de->accept();
        }
    }}
}

void
BE::Desk::dropEvent ( QDropEvent *de )
{
    if FIRST_URL
        int mode = Heuristic, desktop = AskForDesktops;
        if (QApplication::keyboardModifiers() & Qt::ControlModifier)
            desktop = AllDesktops;
        else if (QApplication::keyboardModifiers() & Qt::AltModifier)
            desktop = KWindowSystem::currentDesktop();

        if (QApplication::keyboardModifiers() & Qt::ShiftModifier)
            mode = ScaleAndCrop;

        if (url.isLocalFile())
            setWallpaper( url.path(), mode, desktop );
        else
        {
            QString file = KGlobal::dirs()->locateLocal("data","be.shell/myWallpaper");
            if ( KIO::NetAccess::download(url, file, this) )
                setWallpaper( file, mode, desktop );
        }
    }}
}

#if 0 // doesn't work, areas don't reflect the pager layout... :-(
inline static bool
desktopIsConnected(int d, int dir)
{
    if (d == KWindowSystem::currentDesktop()) return false;
    switch (dir)
    {
        case Qt::Key_Left: return KWindowSystem::workArea().left() - 1 <= KWindowSystem::workArea(d).right();
        case Qt::Key_Right: return KWindowSystem::workArea().right() + 1 >= KWindowSystem::workArea(d).x();
        case Qt::Key_Up: return KWindowSystem::workArea().y() - 1 <= KWindowSystem::workArea(d).bottom();
        case Qt::Key_Down: return KWindowSystem::workArea().bottom() + 1 >= KWindowSystem::workArea(d).y();
        default: return false;
    }
}
#endif
void
BE::Desk::keyPressEvent( QKeyEvent *ke )
{
    switch (ke->key())
    {
    case Qt::Key_F5:
        update();
        break;
    case Qt::Key_Return:
    case Qt::Key_Enter:
        BE::Shell::call("session;org.kde.be.shell;/Runner;org.kde.be.shell;showAsDialog");
        break;
        /* find some?
    case Qt::Key_Insert
    case Qt::Key_Delete
    case Qt::Key_Pause
    case Qt::Key_Print
    case Qt::Key_SysReq
    case Qt::Key_Clear
    case Qt::Key_Home
    case Qt::Key_End
        */

//     case Qt::Key_Left:
//     case Qt::Key_Up:
//     case Qt::Key_Right:
//     case Qt::Key_Down:
//         if (KWindowSystem::numberOfDesktops() < 2)
//             break;
//         for (int d = 1; d < KWindowSystem::numberOfDesktops()+1; ++d)
//             if ( desktopIsConnected(d, ke->key()) )
//             {
//                 KWindowSystem::setCurrentDesktop(d);
//                 break;
//             }
//         break;
    default:
        break;
    }
}


void
BE::Desk::mouseDoubleClickEvent(QMouseEvent *me)
{
    if (me->button() == Qt::LeftButton)
    {
        const long unsigned int props[2] = {0, NET::WM2ShowingDesktop};
        NETRootInfo net(QX11Info::display(), props, 2);
        net.setShowingDesktop(!net.showingDesktop());
    }
}

enum PosFlag { Top = 1, Bottom = 2, Left = 4, Right = 8 };

void
BE::Desk::paintEvent(QPaintEvent *pe)
{
//     QWidget::paintEvent(pe);

    QPainter p(this);
    p.setClipRegion( pe->rect() );
    p.fillRect(rect(), palette().brush(backgroundRole()));
    const Wallpaper &wp = currentWallpaper();
    if ( !wp.pix.isNull() )
    {
        if ( wp.mode == Tiled || wp.mode == ScaleV || wp.mode == ScaleH )
            p.drawTiledPixmap( wpSettings.area.isValid() ? wpSettings.area : rect(), wp.pix, wp.offset );
        else
            p.drawPixmap( wp.offset, wp.pix );
    }

    foreach (BE::Panel *panel, myPanels)
    {
        if (panel && panel->isVisible())
        {
            int x,y,w,h; panel->geometry().getRect(&x,&y,&w,&h);
            if (panel->effectBgPix() && panel->updatesEnabled())
                p.drawPixmap(x,y, *panel->effectBgPix());

            if (!myShadow.opacity)
                continue;

            int flags = 0;
            if (panel->orientation() == Qt::Horizontal)
            {
                flags = panel->position() == BE::Panel::Top ? Bottom : Top;
                if (x > 0) flags |= Left;
                if (x+w < width()) flags |= Right;
            }
            else
            {
                flags = panel->position() == BE::Panel::Left ? Right : Left;
                if (y > 0) flags |= Top;
                if (y+h < height()) flags |= Bottom;
            }
//                 p.drawTiledPixmap(panel->geometry(), myShadow.center);
            if (flags & Top)
                p.drawTiledPixmap( x, y - myShadow.top.height(), w, myShadow.top.height(), myShadow.top );
            if (flags & Bottom)
                p.drawTiledPixmap( x, y+h, w, myShadow.bottom.height(), myShadow.bottom );
            if (flags & Left)
                p.drawTiledPixmap( x-myShadow.left.width(), y, myShadow.left.width(), h, myShadow.left );
            if (flags & Right)
                p.drawTiledPixmap( x+w, y, myShadow.right.width(), h, myShadow.right );
            if (flags & (Top | Left))
                p.drawPixmap( x-myShadow.topLeft.width(), y-myShadow.topLeft.height(), myShadow.topLeft );
            if (flags & (Top | Right))
                p.drawPixmap( x+w, y-myShadow.topRight.height(), myShadow.topRight );
            if (flags & (Bottom | Left))
                p.drawPixmap( x-myShadow.bottomLeft.width(), y+h, myShadow.bottomLeft );
            if (flags & (Bottom | Right))
                p.drawPixmap( x+w, y+h, myShadow.bottomRight );
        }
    }
    p.end();
}

void
BE::Desk::wheelEvent(QWheelEvent *we)
{
    if (KWindowSystem::numberOfDesktops() < 2)
        return;
    int next = KWindowSystem::currentDesktop();
    const int available = KWindowSystem::numberOfDesktops();
    if ( we->delta() < 0 )
        ++next;
    else
        --next;
    if ( next < 1 )
        next += available;
    if ( next > available )
        next -= available;
    KWindowSystem::setCurrentDesktop( next );
}

void
BE::Desk::desktopChanged ( int desk )
{
    Wallpapers::iterator prev = myWallpapers.find(myCurrentDesktop);
    Wallpapers::iterator next = myWallpapers.find(desk);
    if (prev != next)
    {
        qint64 pId = (prev == myWallpapers.end() ? myWallpaper.pix.cacheKey() : prev.value().pix.cacheKey());
        qint64 nId = (next == myWallpapers.end() ? myWallpaper.pix.cacheKey() : next.value().pix.cacheKey());
        if (pId != nId)
            emit wallpaperChanged();
    }
    myCurrentDesktop = desk;
}

void
BE::Desk::desktopResized ( int screen )
{
    if (screen != myScreen && myScreen > -1)
        return;

    QSize oldSize = size();
    QRect r;
    if (myScreen < 0)
    {
        QRegion reg;
        for (int i = 0; i < QApplication::desktop()->screenCount(); ++i)
            reg |= QApplication::desktop()->screenGeometry(i);
        r = reg.boundingRect();
    }
    else
        r = QApplication::desktop()->screenGeometry(myScreen);
    setGeometry(r);
    if (oldSize != size())
    {
        bool common; Wallpaper &wp = currentWallpaper(&common);
        setWallpaper( wp.file, 0, common ? -3 : KWindowSystem::currentDesktop() );
        emit resized();
    }
    myIcons.rect = rect();
    PanelList::iterator i = myPanels.begin();
    while (i != myPanels.end())
    {
        BE::Panel *p = *i;
        if (!p)
        {
            i = myPanels.erase(i);
            continue;
        }

        QPoint pt = mapFromGlobal(p->mapToGlobal(QPoint(0,0)));
        if (p->position() == Panel::Top)
        {
            if (pt.y() + p->height() > myIcons.rect.top())
                myIcons.rect.setTop(pt.y() + p->height());
        }
        else // if (i->position() == Panel::Bototm)
        {
            if (pt.y() < myIcons.rect.bottom())
                myIcons.rect.setBottom(pt.y());
        }
        ++i;
    }
    myIcons.rect.adjust(30,30,-30,-30);
}

void
BE::Desk::registrate(BE::Panel *panel)
{
    if (panel && !myPanels.contains(panel))
    {
        myPanels.append(panel);
        panel->installEventFilter(this);
        desktopResized( myScreen );
    }
}


bool
BE::Wallpaper::fits( const QString &f, int m ) const
{
    return file == f && (m < 0 || mode == m) && timestamp > QFileInfo(file).lastModified();
}

void
BE::Wallpaper::calculateOffset(const QSize &screenSize, const QPoint &off)
{
    int x = 0, y = 0;
    if ( !(align & Qt::AlignLeft) )
    {
        x = screenSize.width() - pix.width();
        if ( !(align & Qt::AlignRight) )
            x /= 2;
    }
    if ( !(align & Qt::AlignTop) )
    {
        y = screenSize.height() - pix.height();
        if ( !(align & Qt::AlignBottom) )
            y /= 2;
    }
    offset = QPoint(x,y) + off;
}
