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

#include <KDE/KAction>
#include <KDE/KConfigGroup>
#include <KDE/KDesktopFile>
#include <KDE/KDirWatch>
#include <KDE/KFileDialog>
#include <KDE/KGlobal>
#include <KDE/KIconLoader>
#include <KDE/KLocale>
#include <KDE/KRun>
#include <KDE/KStandardDirs>
#include <KDE/KMimeType>
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
#include <QFutureWatcher>
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
#include <QtConcurrentRun>
#include <QtDBus>
#include <QToolButton>
#include <QToolTip>
#include <QVBoxLayout>
#include <QX11Info>

#include <kdebug.h>

QSize BE::DeskIcon::ourSize(64,64);
bool BE::DeskIcon::showLabels = true;
static bool isClick = false;
static QPoint dragStart;

namespace BE {
class Snow : public QWidget {
public:
    Snow(QWidget *parent, uint flakes, uint fps) : QWidget(parent), numFlakes(flakes) {
//         setAttribute(Qt::WA_NoSystemBackground);
        setAttribute(Qt::WA_OpaquePaintEvent);
        setGeometry(0,0,parent->width(),parent->height());
        snowflakes = new Snowflake[numFlakes];
        srandom(QDateTime::currentDateTime().toTime_t()/*20131224*/);
        for (uint i = 0; i < numFlakes; ++i) {
            snowflakes[i].y = - (random() % height());
            snowflakes[i].x = random() % width();
            snowflakes[i].type = random() % 3;
            snowflakes[i].speed = (random() % 8) + 1;
            snowflakes[i].speed |= (random() % 9) << 4;
        }
        QPainter p;
        QFont fnt;
        fnt.setPixelSize(10);
        int sz = 8;
        QString flakeGlyph[3] = { QString::fromUtf8("❄"), QString::fromUtf8("❅"), QString::fromUtf8("❆") };
        for (int i = 0; i < 9; ++i) {
            if (i == 3) {
                fnt.setPixelSize(18);
                sz = 16;
            } else if (i == 6) {
                fnt.setPixelSize(26);
                sz = 24;
            }
            flake[i] = QPixmap(sz,sz);
            flake[i].fill(Qt::transparent);
            p.begin(&flake[i]);
            const int a = 255 - (sz-8)*3;
            p.setBrush(QColor(255,255,255,a));
            p.setPen(QColor(255,255,255,a));
            p.setFont(fnt);
            p.drawText(flake[i].rect(), Qt::AlignCenter, flakeGlyph[i%3]);
            p.end();
        }
        buffer = QPixmap(size());
        parent->render(&buffer, QPoint(), QRegion(), QWidget::DrawWindowBackground);
        myTimer = startTimer(1000/fps);
    }
    ~Snow() {
        killTimer(myTimer);
        myTimer = 0;
        delete [] snowflakes;
        snowflakes = 0;
    }
protected:
    void timerEvent(QTimerEvent *e) {
        if (e->timerId() != myTimer) {
            QWidget::timerEvent(e);
            return;
        }
        static int xspeed[9] = {-1, 0, 1, -1, 0 , 1, -1, 0, 1 };
        static int counter = 0;
        static QSize sz(24,24);
        if (++counter > 10) {
            for (int i = 0; i < 9; ++i)
                xspeed[i] = qMax(-4, qMin(4, xspeed[i] + int((random() % 3) - 1)));
            counter = 0;
        }
        int speed = random() % 8;
        lastFlakeRects = flakeRects;
        flakeRects = QRegion();
        for (uint i = 0; i < numFlakes; ++i) {
            if ((snowflakes[i].speed & 15) > speed) {
                snowflakes[i].y += 1;
            }
            if (snowflakes[i].y > height())
                snowflakes[i].y = 0;
            snowflakes[i].x += xspeed[((snowflakes[i].speed >> 4) & 15)];
            if (snowflakes[i].x > width())
                snowflakes[i].x -= width();
            else if (snowflakes[i].x < 0)
                snowflakes[i].x += width();
            flakeRects |= QRect(QPoint(snowflakes[i].x, snowflakes[i].y), sz);
        }
        update(lastFlakeRects|flakeRects);
    }
    void paintEvent(QPaintEvent *pe) {
        if (snowflakes) {
            QPainter p(this);
            p.setClipRegion(pe->region());
            p.drawPixmap(0,0,buffer);
            uint n = numFlakes/3;
            uint i;
            for (i = 0; i < n; ++i)
                p.drawPixmap(snowflakes[i].x, snowflakes[i].y, flake[snowflakes[i].type]);
            n *= 2;
            for (; i < n; ++i)
                p.drawPixmap(snowflakes[i].x, snowflakes[i].y, flake[snowflakes[i].type + 3]);
            for (; i < numFlakes; ++i)
                p.drawPixmap(snowflakes[i].x, snowflakes[i].y, flake[snowflakes[i].type + 6]);
            p.end();
        }
    }
private:
    struct Snowflake {
        int x, y;
        uchar type;
        char speed;
    };
    QRegion lastFlakeRects;
    QRegion flakeRects;
    Snowflake *snowflakes;
    QPixmap buffer;
    QPixmap flake[9];
    int myTimer;
    uint numFlakes;
};
} //namespace

BE::DeskIcon::DeskIcon( const QString &path, BE::Desk *parent ) : QFrame(parent), myUrl(path)
{
    QVBoxLayout *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(myButton = new QToolButton(this), 0, Qt::AlignCenter);
    myButton->setToolButtonStyle(Qt::ToolButtonIconOnly);
    myButton->setAttribute(Qt::WA_TransparentForMouseEvents, true);

    layout->addWidget(myLabel = new QLabel(this));
    myLabel->setAlignment(Qt::AlignCenter);
    myLabel->setTextFormat(Qt::PlainText);
    myLabel->setWordWrap(true);
    setLabelVisible(showLabels);

    setCursor(Qt::PointingHandCursor);
    myButton->setIconSize(ourSize);
    updateIcon();
    adjustSize();
}

void BE::DeskIcon::updateIcon()
{
    if( KDesktopFile::isDesktopFile(myUrl) ) {
        KDesktopFile ds(myUrl);
        myButton->setIcon(DesktopIcon(ds.readIcon(), 512));
        myLabel->setText(ds.readName());
        myName = ds.readName();
    } else {
//         QImageReader imgRdr(myUrl);
//         if (imgRdr.canRead()) {
//             QSize sz = imgRdr.size();
//             if (sz.isValid()) {
//                 int s;
//                 if (sz.width() < sz.height()) {
//                     s = qMin(512, sz.width());
//                     sz = QSize(s, sz.height()*s/sz.width());
//                 } else {
//                     s = qMin(512, sz.height());
//                     sz = QSize(sz.width()*s/sz.height(), s);
//                 }
//                 imgRdr.setScaledClipRect(QRect((sz.width()-s)/2,(sz.height()-s)/2,s,s));
//                 imgRdr.setScaledSize(sz);
//             }
//             myButton->setIcon(QPixmap::fromImage(imgRdr.read()));
//         } else {
            QString iconName = KMimeType::iconNameForUrl(myUrl);
            myButton->setIcon(DesktopIcon(iconName, 512));
//         }
        QFileInfo fileInfo(myUrl);
        myLabel->setText(fileInfo.fileName());
        myName = fileInfo.fileName();
    }
    update();
}

void BE::DeskIcon::enterEvent(QEvent *ev)
{
    qApp->sendEvent(myButton, ev);
}

void BE::DeskIcon::leaveEvent(QEvent *ev)
{
    qApp->sendEvent(myButton, ev);
}

void BE::DeskIcon::mousePressEvent(QMouseEvent *ev)
{
    QFrame::mousePressEvent(ev);
    if (ev->button() == Qt::RightButton) {
       // KonqPopupMenu menu();
       // menu.exec();
    }

    if (ev->button() == Qt::LeftButton) {
        isClick = true;
        dragStart = ev->pos();
        QMouseEvent me(QEvent::MouseButtonPress, QPoint(0,0), ev->button(), ev->buttons(), ev->modifiers());
        qApp->sendEvent(myButton, &me);
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
    QFrame::mouseReleaseEvent(ev);
    if (testAttribute(Qt::WA_UnderMouse)) {
        if (isClick)
            new KRun( KUrl(myUrl), this);
        else {
            if (ev->modifiers() & Qt::ControlModifier) {
                snapToGrid();
            } else {
                static_cast<Desk*>(parent())->alignIcon(this);
            }
            static_cast<Desk*>(parent())->scheduleSave();
        }
    }
    isClick = false;
    QMouseEvent me(QEvent::MouseButtonRelease, QPoint(0,0), ev->button(), ev->buttons(), ev->modifiers());
    qApp->sendEvent(myButton, &me);
}

void
BE::DeskIcon::setIconSize(const QSize &size)
{
    myButton->setIconSize(size);
}

void
BE::DeskIcon::setLabelVisible(bool b)
{
    myLabel->setVisible(b);
    setToolTip(b ? QString() : myLabel->text());
    adjustSize();
}

void
BE::DeskIcon::setSize(int s)
{
    ourSize = QSize(s,s);
}

void
BE::DeskIcon::snapToGrid()
{
    if (showLabels)
        return;
    const QRect &container = static_cast<Desk*>(parent())->iconRect();
    const int dx = width() + static_cast<Desk*>(parent())->iconMargin();
    const int dy = height() + static_cast<Desk*>(parent())->iconMargin();
    const int lx = qMin(container.right() - dx, x());
    const int ly = qMin(container.bottom() - dy, y());

    int tx = container.x() + (lx/dx)*dx;
    if (lx - tx > dx/2)
        tx += dx;
    int ty = container.y() + (ly/dy)*dy;
    if (ly - ty > dy/2)
        ty += dy;
    move(tx, ty);
}

void
BE::Desk::alignIcon(BE::DeskIcon *icon)
{
    QPoint target(-1,-1);
    const int d = iconMargin();
    QRect geo = icon->geometry();
    BE::DeskIcon *touching = 0;
    for (IconList::iterator it = myIcons.list.begin(),
                           end = myIcons.list.end(); it != end; ++it) {
        if (it->first && it->first != icon) {
            const QRect geo2 = it->first->geometry();
            if (target.x() < 0 &&
                geo.x() - d < geo2.right() && geo.x() + d > geo2.right() &&
                (geo.y() + geo.height()/2 > geo2.y() && geo.y() + geo.height()/2 < geo2.bottom())) {
                target.rx() = geo2.right() + d;
                if (!touching)
                    touching = it->first;
            }
            if (target.y() < 0 &&
                geo.y() - d < geo2.bottom() && geo.y() + d > geo2.bottom() &&
                (geo.x() + geo.width()/2 > geo2.x() && geo.x() + geo.width()/2 < geo2.right())) {
                target.ry() = geo2.bottom() + d;
                if (!touching)
                    touching = it->first;
            }
        }
        if (target.x() > -1 && target.y() > -1)
            break;
    }

    if (target.x() < 0 || target.y() < 0) {
        for (IconList::iterator it = myIcons.list.begin(),
                               end = myIcons.list.end(); it != end; ++it) {
            if (it->first && it->first != icon) {
                const QRect geo2 = it->first->geometry();
                if (target.x() < 0 &&
                    geo.right() + d > geo2.x() && geo.right() - d < geo2.x() &&
                    (geo.y() + geo.height()/2 > geo2.y() && geo.y() + geo.height()/2 < geo2.bottom())) {
                    target.rx() = geo2.x() - (d + geo.width());
                    if (!touching)
                        touching = it->first;
                }
                if (target.y() < 0 &&
                    geo.bottom() + d > geo2.y() && geo.bottom() - d < geo2.y() &&
                    (geo.x() + geo.width()/2 > geo2.x() && geo.x() + geo.width()/2 < geo2.right())) {
                    target.ry() = geo2.y() - (d + geo.height());
                    if (!touching)
                        touching = it->first;
                }
            }
            if (target.x() > -1 && target.y() > -1)
                break;
        }
    }

    if (touching) {
        const QRect geo2 = touching->geometry();
        if (target.x() < 0) {
            if (qAbs(geo.x() - geo2.x()) < d)
                target.rx() = geo2.x();
            else if (qAbs(geo.right() - geo2.right()) < d)
                target.rx() = geo2.right() - geo.width();
        } else if (target.y() < 0) {
            if (qAbs(geo.y() - geo2.y()) < d)
                target.ry() = geo2.y();
            else if (qAbs(geo.bottom() - geo2.bottom()) < d)
                target.ry() = geo2.bottom() - geo.height();
        }
    }

    if (target.x() < 0 || target.y() < 0) {
        const QPoint tl = iconRect().topLeft();
        QPoint p1 = icon->pos() - tl;
        QPoint p2((p1.x()/d)*d, (p1.y()/d)*d);
        p2 = p1 - p2;
        p1 = icon->pos() - p2;
        if (p2.x() > d/2)
            p1.rx() += d;
        if (p2.y() > d/2)
            p1.ry() += d;
        if (target.x() < 0)
            target.rx() = p1.x();
        if (target.y() < 0)
            target.ry() = p1.y();
    }

    icon->move(target);
}

void
BE::Desk::fileCreated(const QString &path)
{
    if (!myIcons.areShown)
        return;

    QString url = QFileInfo(path).absoluteFilePath();
    quint64 hash = qHash(url);
    IconList::iterator it = myIcons.list.find(hash);
    if (it != myIcons.list.end()) {
        if (it->first) {
            it->first->updateIcon();
        } else {
            it->first = new DeskIcon(url, this);
        }
    } else {
        it = myIcons.list.insert(hash, QPair<DeskIcon*,QPoint>(new DeskIcon(url, this), nextIconSlot()));
    }
    it->first->move(it->second);
    it->first->show();
}

void
BE::Desk::fileDeleted( const QString &path )
{
    if (!myIcons.areShown)
        return;

    IconList::iterator it = myIcons.list.find(qHash(QFileInfo(path).absoluteFilePath()));
    if (it != myIcons.list.end()) {
        delete it->first;
        it->first = NULL;
    }
}

void
BE::Desk::fileChanged(const QString &path)
{
    if (!myIcons.areShown)
        return;

    IconList::iterator it = myIcons.list.find(qHash(QFileInfo(path).absoluteFilePath()));
    if (it != myIcons.list.end() && it->first)
        it->first->updateIcon();
}

void
BE::Desk::populate( const QString &path )
{
    if (myIcons.areShown) {
        QDir desktopDir(path);
        QFileInfoList files = desktopDir.entryInfoList( QDir::AllEntries | QDir::NoDotAndDotDot );
        foreach(const QFileInfo &file, files)
            fileCreated(file.absoluteFilePath());
        // get rid of orphans
        IconList::iterator it = myIcons.list.begin();
        while (it != myIcons.list.end()) {
            if (it->first)
                ++it;
            else
                it = myIcons.list.erase(it);
        }
    } else {
        for (IconList::iterator it = myIcons.list.begin(),
                               end = myIcons.list.end(); it != end; ++it) {
            if (it->first) {
                it->second = it->first->pos();
                delete it->first;
                it->first = NULL;
            }
        }
    }
}


QPoint
BE::Desk::nextIconSlot(QPoint lastSlot, int *overlap)
{
    if (lastSlot.x() < 0 || lastSlot.y() < 0)
        lastSlot = myIcons.rect.topLeft();

    QSize iconSize;
    for (IconList::const_iterator it = myIcons.list.constBegin(),
                                    end = myIcons.list.constEnd(); it != end; ++it) {
        if (it->first && it->first->isVisibleTo(this)) {
            iconSize = it->first->geometry().size();
            if (!iconSize.isNull())
                break;
        }
    }

    if (iconSize.isNull()) {
        if (overlap)
            *overlap = __INT_MAX__;
        return lastSlot;
    }

    QPoint bestSlot = lastSlot;
    int bestSlotOverlap = iconSize.width()*iconSize.height();
    QRect geo(lastSlot, iconSize);
    while (true) {
        int maxOverlap = 0;
        for (IconList::const_iterator it = myIcons.list.constBegin(),
                                    end = myIcons.list.constEnd(); it != end; ++it) {
            if (!(it->first && it->first->isVisibleTo(this)))
                continue;
            const QRect r = geo & it->first->geometry();
            const int a = r.width()*r.height();
            if (a > maxOverlap)
                maxOverlap = a;
        }
        if (maxOverlap < bestSlotOverlap) {
            bestSlotOverlap = maxOverlap;
            bestSlot = geo.topLeft();
            if (!bestSlotOverlap)
                break;
        }
        if (geo.y() + 2*iconSize.height() + myIcons.margin > myIcons.rect.bottom()) {
            if (geo.x() + 2*iconSize.width() + myIcons.margin > myIcons.rect.right()) {
                break;
            } else {
                geo.translate(iconSize.width() + myIcons.margin,0);
            }
            geo.moveTop(myIcons.rect.top());
        } else {
            geo.translate(0,iconSize.height() + myIcons.margin);
        }
    }
    if (overlap)
        *overlap = bestSlotOverlap;
    return bestSlot;
}

void
BE::Desk::reArrangeIcons()
{
    if (!myIcons.areShown)
        return;

    int overlap;
    QPoint lastSlot = myIcons.rect.topLeft();

    setUpdatesEnabled(false);
    for (IconList::iterator it = myIcons.list.begin(),
                           end = myIcons.list.end(); it != end; ++it) {
        if (!(it->first && it->first->isVisibleTo(this)))
            continue;
        if (myIcons.rect.contains(it->first->geometry()))
            continue;
        it->first->hide();
        lastSlot = nextIconSlot(lastSlot, &overlap);
        it->first->move(lastSlot);
        it->first->show();
        if (overlap) // no ideal match, start from the beginning every time
            lastSlot = myIcons.rect.topLeft();
    }
    setUpdatesEnabled(true);
}


void
BE::Desk::snapIconsToGrid()
{
    setUpdatesEnabled(false);
    for (IconList::iterator it = myIcons.list.begin(),
                           end = myIcons.list.end(); it != end; ++it) {
        if (it->first) {
            it->first->snapToGrid();
            for (IconList::const_iterator it2 = myIcons.list.constBegin(),
                           end = myIcons.list.constEnd(); it2 != end; ++it2) {
                if (it2->first && it2->first != it->first && it2->first->geometry() == it->first->geometry()) {
                    it->first->move(nextIconSlot(it->first->geometry().topLeft()));
                    break;
                }
            }
        }
    }
    setUpdatesEnabled(true);
    BE::Plugged::saveSettings();
}


void
BE::Desk::setIconSize(QAction *act)
{
    const int size = act->data().toInt();
    if (DeskIcon::size().height() == size)
        return;
    DeskIcon::setSize(size);
    setUpdatesEnabled(false);
    QSize qsz(size,size);
    for (IconList::iterator it = myIcons.list.begin(),
                           end = myIcons.list.end(); it != end; ++it) {
        if (it->first) {
            it->first->setIconSize(qsz);
            it->first->adjustSize();
        }
    }
    setUpdatesEnabled(true);
    BE::Plugged::saveSettings();
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

BE::WallpaperDesktopDialog::WallpaperDesktopDialog( QWidget *parent ) : QDialog(parent)
{
    setWindowTitle(i18n("Select Desktops"));
    new QVBoxLayout(this);


    layout()->addWidget(filename = new QLabel(i18n("Wallpaper %1 on").arg("wallpaper"), this));
    QFont fnt = filename->font();
    fnt.setPointSize(fnt.pointSize()*1.618);
    fnt.setBold(true);
    filename->setFont(fnt);

    all = new QRadioButton(i18n("All Desktops (Ctrl)"), this);
    all->setAutoExclusive(false);
    connect(all, SIGNAL(clicked(bool)), SLOT(toggleSelection(bool)));
    layout()->addWidget(all);

    current = new QRadioButton(i18n("Current Desktop (Alt)"), this);
    current->setAutoExclusive(false);
    connect(current, SIGNAL(clicked(bool)), SLOT(toggleSelection(bool)));
    layout()->addWidget(current);

    list = new QGroupBox(i18n("Individual Desktops"), this);
    new QVBoxLayout(list);
    list->setCheckable(true);
    connect(list, SIGNAL(clicked(bool)), SLOT(toggleSelection(bool)));
    layout()->addWidget(list);

    QLabel *hint = new QLabel(i18n("Hint: press Shift to use Scale & Crop™"), this);
    QPalette pal = hint->palette();
    QColor c = pal.color(hint->foregroundRole()); c.setAlpha(2*c.alpha()/3);
    pal.setColor(hint->foregroundRole(), c);
    hint->setPalette(pal);
    layout()->addWidget(hint);

    QDialogButtonBox *buttonBox = new QDialogButtonBox(QDialogButtonBox::Ok|QDialogButtonBox::Cancel, Qt::Horizontal, this);
    connect (buttonBox, SIGNAL(accepted()), this, SLOT(accept()));
    connect (buttonBox, SIGNAL(rejected()), this, SLOT(reject()));
    layout()->addWidget(buttonBox);

    adjustSize();
}

void BE::WallpaperDesktopDialog::setWallpaper(const QString &name)
{
    filename->setText(i18n("Wallpaper %1 on").arg(name));
}

void BE::WallpaperDesktopDialog::toggleSelection(bool b)
{
    if (!b) { // enforce radio-a-like behavior (no self toggle)
        if (sender() == list)
            list->setChecked(true);
        else if (sender() == all)
            all->setChecked(true);
        else if (sender() == current)
            current->setChecked(true);
        return;
    }

    if (sender() != all)
        all->setChecked(false);
    if (sender() != current)
        current->setChecked(false);
    if (sender() != list)
        list->setChecked(false);
}

QList<int> BE::WallpaperDesktopDialog::ask()
{
    QList<int> desks;
    const int numberOfDesktops = KWindowSystem::numberOfDesktops();
    while (desk.count() > numberOfDesktops)
        delete desk.takeLast();
    for (int i = desk.count(); i < numberOfDesktops; ++i) {
        desk << new QCheckBox(i18n("Desktop %1").arg(i+1), list);
        list->layout()->addWidget(desk.last());
    }
    for (int i = 0; i < desk.count(); ++i)
        desk.at(i)->setChecked(false);
    list->setChecked(false);
    current->setChecked(false);
    all->setChecked(true);

    QPointer<WallpaperDesktopDialog> that = this;
    exec();
    if (!that)
        return desks;

    if (result() == Accepted) {
        if (list->isChecked()) {
            for (int i = 0; i < KWindowSystem::numberOfDesktops(); ++i)
                if (desk[i]->isChecked())
                    desks << i+1;
                if (desks.isEmpty())
                    desks << AllDesktops;
        } else if (current->isChecked()) {
            desks << KWindowSystem::currentDesktop();
        } else {
            desks << AllDesktops;
        }
    }
    return desks;
}

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
         ((e->type() == QEvent::ChildAdded || e->type() == QEvent::ShowToParent) && o == parentWidget()) )
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
, myFadingWallpaper(NULL)
, myFadingWallpaperTimer(NULL)
, mySaveSchedule(NULL)
, myFadingWallpaperStep(0)
, myFadingWallpaperSteps(8)
, iAmRedirected(false)
, myWpDialog(NULL)
{
    for (int i = 0; i < Qt::MiddleButton; ++i)
        myMouseActionMenu[i] = NULL;
    setObjectName("Desktop");
    setFocusPolicy( Qt::ClickFocus );
    setAcceptDrops( true );
    // to get background-image interpreted
    setAttribute(Qt::WA_StyledBackground, false);
    setAttribute(Qt::WA_NoSystemBackground);
    setAttribute(Qt::WA_OpaquePaintEvent);
    setAttribute(Qt::WA_X11NetWmWindowTypeDesktop);
//     setAttribute(Qt::WA_PaintOnScreen);
    connect( QApplication::desktop(), SIGNAL(resized(int)), this, SLOT(desktopResized(int)));
    connect( this, SIGNAL(wallpaperChanged()), this, SLOT(updateOnWallpaperChange()));
    connect( shell(), SIGNAL(styleSheetChanged()), this, SIGNAL(wallpaperChanged()));
//     KWindowSystem::setType( winId(), NET::Desktop );
    KWindowSystem::setOnAllDesktops( winId(), true );

    connect (configMenu(), SIGNAL(aboutToShow()), SLOT(bindScreenMenu()));

    myWallpaper.align = (Qt::Alignment)-1;
    myWallpaper.aspect = -1.0;
    myWallpaper.mode = (WallpaperMode)-1;
    myShadowOpacity = -1;
    myShadowCache.setMaxCost(8); // TODO depend this on the (shell private) amount of panels
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
    wpSettings.mode->addAction( i18n("Scale && Crop™") )->setData(700);
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
    menu->addSeparator();
    menu->addAction( i18n("None"), this, SLOT(unsetWallpaper()) );

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

    menu = configMenu()->addMenu(i18n("Icons"));
    myIcons.menuItem = menu->addAction(i18n("Visible"), this, SLOT(toggleIconsVisible(bool)));
    myIcons.menuItem->setCheckable(true);
    menu->addSeparator();
    myIcons.alignTrigger = menu->addAction(i18n("Align\t(Ctrl+Drag)"), this, SLOT(snapIconsToGrid()));

    menu = menu->addMenu(i18n("Size"));
    const int IconSizes[10] = {16, 22, 32, 48, 64, 72, 96, 128, 256, 512};
    for (int i = 0; i < 10; ++i) {
        menu->addAction(QString::number(IconSizes[i]))->setData(IconSizes[i]);
    }
    connect(menu, SIGNAL(triggered(QAction*)), SLOT(setIconSize(QAction*)));

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

    KAction *action = new KAction(this);
    action->setObjectName("BE::Desk::highlight");
    action->setGlobalShortcut(KShortcut());
    connect(action, SIGNAL(triggered(Qt::MouseButtons, Qt::KeyboardModifiers)), SLOT(toggleDesktopHighlighted()));

    myCurrentDesktop = KWindowSystem::currentDesktop();

    new BE::DeskAdaptor(this);

    XWMHints *hints = XGetWMHints ( QX11Info::display(), window()->winId() );
    hints->input = false;
    XSetWMHints ( QX11Info::display(), window()->winId(), hints );

}

void
BE::Desk::configure( KConfigGroup *grp )
{
    for (int i = 0; i < Qt::MiddleButton; ++i) {
        delete myMouseActionMenu[i];
        myMouseActionMenu[i] = NULL;
    }

    ignoreSaveRequest = true;
    bool b; int i; QString s; float f;
    bool changeWallpaper = false;
    bool needUpdate = false;

    iWheelOnClickOnly = grp->readEntry("WheelOnLMB", false);
    myMouseAction[Qt::LeftButton-1] = grp->readEntry("LMBAction", QString());
    myMouseAction[Qt::RightButton-1] = grp->readEntry("RMBAction", "menu:BE::Config");
    myMouseAction[Qt::MiddleButton-1] = grp->readEntry("MMBAction", "menu:windowlist");

    myIcons.margin = grp->readEntry("IconMargin", 8);
    QAction act(this);
    act.setData(grp->readEntry("IconSize", 64));
    setIconSize(&act);
    QStringList l = grp->readEntry("IconAreaPaddings", QStringList());
    bool needIconShift = false;
    if (l.count() < 4)
        myIconPaddings[0] = myIconPaddings[1] = myIconPaddings[2] = myIconPaddings[3] = 32;
    else {
        for (int i = 0; i < 4; ++i) {
            int p = l.at(i).toInt();
            needIconShift = needIconShift || myIconPaddings[i] != p;
            myIconPaddings[i] = p;
        }
    }

    const QStringList iconPos = grp->readEntry("IconPos", QString()).split(',', QString::SkipEmptyParts);
    foreach (const QString &pos, iconPos) {
        const QStringList fields = pos.split(':', QString::KeepEmptyParts);
        quint64 hash;
        int x,y;
        bool ok = fields.count() == 3;
        if (ok) hash = fields.at(0).toULongLong(&ok, 16);
        if (ok) x = fields.at(1).toInt(&ok);
        if (ok) y = fields.at(2).toInt(&ok);
        if (!ok) {
            qDebug() << "invalid icon position restored" << pos;
            continue;
        }
        IconList::iterator it = myIcons.list.find(hash);
        if (it == myIcons.list.end()) {
            myIcons.list.insert(hash, QPair<DeskIcon*,QPoint>(NULL, QPoint(x,y)));
        } else {
            it->second = QPoint(x,y);
            if (it->first)
                it->first->move(it->second);
        }
    }

    BE::DeskIcon::showLabels = grp->readEntry("IconLabels", QVariant(true)).toBool();
    myIcons.alignTrigger->setEnabled(!BE::DeskIcon::showLabels);
    for (IconList::const_iterator it = myIcons.list.constBegin(),
                                 end = myIcons.list.constEnd(); it != end; ++it) {
        if (BE::DeskIcon *icon = const_cast<BE::DeskIcon*>(it->first))
            icon->setLabelVisible(BE::DeskIcon::showLabels);
    }

    b = myIcons.areShown;
    // read before eventually resizing the desktop to avoid pointless re-arrangements
    myIcons.areShown = grp->readEntry("ShowIcons", QVariant(true) ).toBool();

    int oldScreen = myScreen;
    myScreen = grp->readEntry("Screen", QApplication::desktop()->primaryScreen() );
    if (myScreen >= QApplication::desktop()->screenCount())
        myScreen = QApplication::desktop()->primaryScreen();
    if (needIconShift || (myScreen != oldScreen && (oldScreen < 0 || myScreen < 0 ||
                          oldScreen >= QApplication::desktop()->screenCount() ||
                          QApplication::desktop()->screenGeometry(myScreen) != geometry())) ) {
        desktopResized( myScreen );
    }

    if (b != myIcons.areShown)
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

    myFadingWallpaperSteps = grp->readEntry("WallpaperFadeSteps", 8);

    changeWallpaper = false;

    QColor oldTint = myTint;
    myTint = grp->readEntry("Tint", QColor());
    if (oldTint != myTint)
        changeWallpaper = true;

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

    QColor c = myHaloColor;
    myHaloColor = grp->readEntry("HaloColor", QColor() );
    if (c != myHaloColor) {
        myShadowCache.clear(); // wipe cache
        needUpdate = true;
    }

    i = myShadowOpacity;
    myShadowOpacity = grp->readEntry("ShadowOpacity", 25 );
    if (i != myShadowOpacity) {
        myShadowCache.clear(); // wipe cache
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
BE::Desk::themeChanged()
{
    for (IconList::const_iterator it = myIcons.list.constBegin(),
                                 end = myIcons.list.constEnd(); it != end; ++it) {
        if (BE::DeskIcon *icon = const_cast<BE::DeskIcon*>(it->first))
            icon->adjustSize();
    }
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

static int alignToInt(Qt::Alignment a)
{
    return ((a & Qt::AlignTop) ? 1 : ((a & Qt::AlignBottom) ? 2 : 0)) +
            10 * ((a & Qt::AlignLeft) ? 1 : ((a & Qt::AlignRight) ? 2 : 3));
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
    Wallpapers::iterator i = myWallpapers.find(myCurrentDesktop/*KWindowSystem::currentDesktop()*/);
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
//     grp->deleteGroup(); // required to clean myWallpaperXYZ entries
    foreach (QString key, grp->keyList()) {
        if (key.startsWith("Wallpaper_") || key.startsWith("WallpaperAlign_") ||
            key.startsWith("WallpaperAspect_") || key.startsWith("WallpaperMode_"))
            grp->deleteEntry(key);
    }
    grp->writeEntry( "Corners", myCorners );
    if (myScreen != QApplication::desktop()->primaryScreen())
        grp->writeEntry( "Screen", myScreen );
    grp->writeEntry("ShowIcons", myIcons.areShown);
    grp->writeEntry("IconSize", DeskIcon::size().height());
    QString iconPos;
    for (IconList::const_iterator it = myIcons.list.constBegin(),
                                 end = myIcons.list.constEnd(); it != end; ++it) {
        const QPoint pos = it->first ? it->first->pos() : it->second;
        iconPos += QString::number(it.key(), 16) + ':' + QString::number(pos.x())
                                                 + ':' + QString::number(pos.y()) + ',';
    }
    grp->writeEntry("IconPos", iconPos);
    grp->writeEntry( "Tint", myTint );
    grp->writeEntry( "Wallpaper", myWallpaper.file );
    grp->writeEntry( "WallpaperAlign", (int)myWallpaper.align );
    grp->writeEntry( "WallpaperAspect", (float)myWallpaper.aspect );
    grp->writeEntry( "WallpaperMode", (int)myWallpaper.mode );
    Wallpapers::const_iterator i;
    for (i = myWallpapers.constBegin(); i != myWallpapers.constEnd(); ++i) {
        grp->writeEntry( QString("Wallpaper_%1").arg(i.key()), i.value().file );
        grp->writeEntry( QString("WallpaperAlign_%1").arg(i.key()), (int)i.value().align );
        grp->writeEntry( QString("WallpaperAspect_%1").arg(i.key()), (float)i.value().aspect );
        grp->writeEntry( QString("WallpaperMode_%1").arg(i.key()), (int)i.value().mode );
    }
    if (myTrash.can) {
        grp->writeEntry( "TrashSize", myTrash.can->width());
        grp->writeEntry( "TrashX", myTrash.can->geometry().x());
        grp->writeEntry( "TrashY", myTrash.can->geometry().y());
    }
}

void
BE::Desk::scheduleSave()
{
    if (!mySaveSchedule) {
        mySaveSchedule = new QTimer(this);
        mySaveSchedule->setInterval(30*1000); // 30 seconds? a minute? more?
        mySaveSchedule->setSingleShot(true);
        connect(mySaveSchedule, SIGNAL(timeout()), SLOT(callSaveSettings()));
    }
    mySaveSchedule->start();
}

void
BE::Desk::callSaveSettings()
{
    BE::Plugged::saveSettings();
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

BE::Desk::ImageToWallpaper BE::Desk::loadImage(QString file, int mode, QList<int> desks, Wallpaper *wp, QSize sz)
{
    if (mode >= Composed * 100 && mode < (Composed + 1) * 100) // Composed
        mode = Composed;
    ImageToWallpaper ret;
    ret.desks = desks;
    ret.targetSize = sz;
    ret.wp = 0;
    QImage tile;
    QImage center;

    QString iFile = file;
    if (iFile.endsWith(".bwp", Qt::CaseInsensitive)) {
        iFile = KStandardDirs::locateLocal("tmp", "be.shell/base.jpg") + ':' +
                KStandardDirs::locateLocal("tmp", "be.shell/tile.png") + ':' +
                KStandardDirs::locateLocal("tmp", "be.shell/center.png");
    }

    if (mode == Composed || (mode < 0 && iFile.contains(':'))) {
        mode = Composed;
        tile = QImage(iFile.section(':', 1, 1, QString::SectionSkipEmpty));
        if ( tile.isNull() )
            return ret;
        tile = tile.convertToFormat(QImage::Format_ARGB32);
        center = QImage(iFile.section(':', 2, 2, QString::SectionSkipEmpty));
        center = center.convertToFormat(QImage::Format_ARGB32);
        iFile = iFile.section(':', 0, 0, QString::SectionSkipEmpty);
    }

    QImage img( iFile );
    if ( img.isNull() )
        return ret;

    char addOverlay = 0;
    if (mode == Heuristic && !wp->pix.isNull() && img.hasAlphaChannel()) {
        if (img.width() + img.height() < 129)
            addOverlay = 1;
        else if (img.width() + img.height() < 1025)
            addOverlay = 2;
    }
    bool tint = true;
    if (addOverlay == 1 && (wp->mode == Tiled || wp->mode == ScaleV || wp->mode ==  ScaleH))
        addOverlay = 0; // nope - we replace the current tile instead
    if (addOverlay) {
        QString oldFile = wp->file;
        QString centerPath, tilePath;
        if (addOverlay == 1) {
            tile = img;
            tilePath = file;
        } else {
            center = img;
            centerPath = file;
        }
        if (wp->mode == Composed) {
            oldFile = wp->file.section(':', 0, 0, QString::SectionSkipEmpty);
            img = QImage(oldFile);
            BE::Shell::monochromatize(img, myTint);
            tint = false;

            if (addOverlay == 1) {
                centerPath = wp->file.section(':', 2, 2, QString::SectionSkipEmpty);
                center = QImage(centerPath);
                center = center.convertToFormat(QImage::Format_ARGB32);
            } else {
                tilePath = wp->file.section(':', 1, 1, QString::SectionSkipEmpty);
                tile = QImage(tilePath);
                tile = tile.convertToFormat(QImage::Format_ARGB32);
            }
        } else {
            img = wp->pix.toImage();
        }
        file = oldFile + ':' + tilePath + ':' + centerPath;
        mode = Composed;
    }

    if (tint) { // check whether better tint now or later (scaling up or down)
        const int imgPx = img.width()*img.height();
        if (imgPx < width()*height()) {
            BE::Shell::monochromatize(img, myTint);
            tint = false;
        } else if (mode == ScaleAndCrop || mode < 0) { // assume "worst" case, being ScaleAndCrop
            int tgtPx(0);
            if (float(height())/img.height() > float(width())/img.width()) // "flat
                tgtPx = height()*(img.width()*height()/img.height());
            else // tall
                tgtPx = width()*(img.height()*width()/img.width());
            if (imgPx < tgtPx) {
                BE::Shell::monochromatize(img, myTint);
                tint = false;
            }
        }
    }

    if (wp->file != file) {
        wp->file = file;
        wp->aspect = -1.0;
    }

    if (mode < 0)
    {   // heuristics, we want central, scale'n'crop if big enough, tiled if very small (and near square)
        wp->align = Qt::AlignCenter;
        if ( img.width() > sz.width()/2 && img.height() > sz.height()/2 )
        {
            wp->mode = myWallpaperDefaultMode;
            wp->align = myWallpaperDefaultAlign;
        }
        else if (( img.width() < sz.width()/6 && img.height() < sz.height()/6 ) ||
                ( img.width() == img.height() && img.width() < sz.width()/4 && img.height() < sz.height()/4 )) {
            wp->mode = Tiled;
        }
        else if ( img.width() < sz.width()/10 && img.height() > 3*sz.height()/4 )
            wp->mode = ScaleV;
        else if ( img.height() < sz.height()/10 && img.width() > 3*sz.width()/4 )
            wp->mode = ScaleH;
        else
            wp->mode = Plain;
    }

    else if (mode)
    {
        while ( mode < 100)
            mode *= 10;
        wp->mode = (WallpaperMode)( mode / 100 );
        const int a = mode - 100*wp->mode;
        wp->align = alignFromInt(a) | alignFromInt(a%10);
    }

    if (wp->aspect > 0.0)
    {
        const float asp = (float)sz.width()/(float)sz.height();
        if (wp->aspect > asp)
            img = img.scaled( sz.width(), sz.width()/wp->aspect, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
        else
            img = img.scaled( sz.height()*wp->aspect, sz.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
    }
    else
    {
        int wp_mode = wp->mode;
        if (wp_mode == Composed) // for composing big wallpapers w/overlay, we preserve the aspect
            wp_mode = (img.width() < sz.width() || img.height() < sz.height()) ? Scale : ScaleAndCrop;
        switch (wp_mode)
        {
        case Tiled: {
            // make tile at least 32x32, painting performance thing.
            QSize dst(img.size());
            if (img.width() < 32)
                dst.setWidth(qCeil(32.0f/img.width())*img.width());
            if (img.height() < 32)
                dst.setHeight(qCeil(32.0f/img.height())*img.height());
            if (img.size() != dst) {
                const bool hasAlpha = img.hasAlphaChannel();
                if (img.format() == QImage::Format_Indexed8) // not supported
                    img = img.convertToFormat(hasAlpha ? QImage::Format_ARGB32 : QImage::Format_RGB32);
                QImage nImg(dst, img.format());
                if (hasAlpha)
                    nImg.fill(Qt::transparent);
                QPainter p(&nImg);
                p.fillRect(nImg.rect(), QBrush(img));
                p.end();
                img = nImg;
            }
            break;
        }
        case ScaleV:
            img = img.scaled( img.width(), sz.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation ); break;
        case ScaleH:
            img = img.scaled( sz.width(), img.height(), Qt::IgnoreAspectRatio, Qt::SmoothTransformation ); break;
        case Scale:
            img = img.scaled( sz, Qt::IgnoreAspectRatio, Qt::SmoothTransformation );
            break;
        case Maximal:
            img = img.scaled( sz, Qt::KeepAspectRatio, Qt::SmoothTransformation ); break;
        case ScaleAndCrop:
            img = img.scaled( sz, Qt::KeepAspectRatioByExpanding, Qt::SmoothTransformation );
        default:
            break;
        }
    }
    if (tint) {
        BE::Shell::monochromatize(img, myTint);
        tint = false;
    }

    if (wp->mode == Composed) {
        if (img.format() == QImage::Format_Indexed8) // not supported
            img = img.convertToFormat(img.hasAlphaChannel() ? QImage::Format_ARGB32 : QImage::Format_RGB32);
        QPainter p(&img);
        p.fillRect(img.rect(), QBrush(tile));
        if (!center.isNull()) {
            QSize d = (img.size() - center.size())/2;
            p.drawImage(d.width(), d.height(), center);
        }
        p.end();
    }
    ret.wp = wp;
    ret.img = img;
    return ret;
}

void
BE::Desk::unsetWallpaper()
{
    setWallpaper("none");
}

void
BE::Desk::setWallpaper(QString file, int mode, int desktop)
{
    if (file.isEmpty())
        return; // "none" is ok - [empty] is not but might result from skipped dialog

    KUrl url(file);
    if (file.compare("none", Qt::CaseInsensitive)) {
        if (!url.isLocalFile())
        {
            file = KGlobal::dirs()->locateLocal("data","be.shell/myWallpaper");
            if ( !KIO::NetAccess::download(url, file, this) )
                return; // failed download
        }
    }

    QList<int> desks;
    if (KWindowSystem::numberOfDesktops() == 1)
        desks << AllDesktops;
    else if (desktop == AskForDesktops)
    {
        if (!myWpDialog)
        {
            myWpDialog = new WallpaperDesktopDialog(this);
            myWpDialog->setWallpaper(url.fileName());
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

    if (!file.compare("none", Qt::CaseInsensitive)) {
        wp.pix = QPixmap();
        wp.file = file;
        wp.aspect = -1.0;
    } else {
        if (file.endsWith(".bwp", Qt::CaseInsensitive)) {
            QUrl url(file + "/base.jpg");
            url.setScheme("tar");
            QString base(KStandardDirs::locateLocal("tmp", "be.shell/base.jpg"));
            if ( !KIO::NetAccess::download(url, base, this) )
                return; // failed download
            url = QUrl(file + "/tile.png");
            url.setScheme("tar");
            QString tile(KStandardDirs::locateLocal("tmp", "be.shell/tile.png"));
            if ( !KIO::NetAccess::download(url, tile, this) )
                return; // failed download
            url = QUrl(file + "/center.png");
            url.setScheme("tar");
            QString center(KStandardDirs::locateLocal("tmp", "be.shell/center.png"));
            if (!KIO::NetAccess::download(url, center, this))
                QFile::remove(center); // remove old junk
            mode = Composed;
        }

        // first check whether we already have this file loaded somewhere...
        bool canCopy = false;
        if (!mode)
            mode = myWallpaper.mode;
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
        if (canCopy)
            finishSetWallpaper();
        else
        {
            if (mode > 0) {
                while ( mode < 100)
                    mode *= 10;
                mode += alignToInt(wp.align);
            }
            QFutureWatcher<ImageToWallpaper>* watcher = new QFutureWatcher<ImageToWallpaper>;
            connect( watcher, SIGNAL( finished() ), SLOT( finishSetWallpaper() ), Qt::QueuedConnection );
            if (changed)
                connect( watcher, SIGNAL( finished() ), SIGNAL( wallpaperChanged() ), Qt::QueuedConnection );
            watcher->setFuture(QtConcurrent::run(this, &BE::Desk::loadImage, file, mode, desks, &wp, sz));
            return;
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
BE::Desk::fadeOutWallpaper()
{
    if (!myFadingWallpaperTimer) {
        // cache old background
        delete myFadingWallpaper;
        myFadingWallpaper = new QPixmap(size());
        myFadingWallpaperStep = 0;
        myFadingWallpaper->fill(Qt::transparent);
        render(myFadingWallpaper, QPoint(), rect(), DrawWindowBackground);
        myFadingWallpaperStep = myFadingWallpaperSteps + 2;
        delete myFadingWallpaperTimer;
        myFadingWallpaperTimer = new QTimer(this);
        connect (myFadingWallpaperTimer, SIGNAL(timeout()), SLOT(fadeOutWallpaper()));
        myFadingWallpaperTimer->start(50);
    }
    if (--myFadingWallpaperStep < 2) {
        myFadingWallpaperStep = 0;
        delete myFadingWallpaperTimer;
        myFadingWallpaperTimer = NULL;
        delete myFadingWallpaper;
        myFadingWallpaper = NULL;
        update();
        return;
    }
    if (!myFadingWallpaper)
        return;
    QPainter p(myFadingWallpaper);
    p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
    p.fillRect(rect(), QColor(255,255,255,qRound(255.0/myFadingWallpaperStep)));
    update();
}

void
BE::Desk::finishSetWallpaper()
{
    fadeOutWallpaper();
    ImageToWallpaper result;
    if (QFutureWatcher<ImageToWallpaper>* watcher =
                dynamic_cast< QFutureWatcher<ImageToWallpaper>* >(sender()))
    {
        result = watcher->result();
        watcher->deleteLater(); // has done it's job
    }

    if (!result.wp) // failed to load
        return;
    if (!result.img.isNull())
        result.wp->pix = QPixmap::fromImage(result.img);

    result.wp->calculateOffset(result.targetSize, wpSettings.area.topLeft());

    // apply to other demanded desktops - iff any
    if (!result.desks.isEmpty())
    {
        for (int i = 0; i < result.desks.count(); ++i)
            myWallpapers[result.desks.at(i)] = *(result.wp);
    }

    Plugged::saveSettings();
}

BE::Desk::Shadow *
BE::Desk::shadow(int r)
{
    Shadow *s = myShadowCache.object(r);
    if (s)
        return s;

    s = new Shadow;

    int alpha = myHaloColor.isValid() ? 10*sqrt(myShadowOpacity) : myShadowOpacity;
    alpha = alpha*255/100;
    Q_ASSERT(alpha>0);

    QPainter p;

    int size = 2*r+19;
    QColor c(myHaloColor);
    c.setAlpha(alpha);
    QPixmap shadowBlob(size,size);
    shadowBlob.fill(Qt::transparent);

    float d = size/2.0f;
    QRadialGradient rg(d, d, d);
    p.begin(&shadowBlob);
    p.setPen(Qt::NoPen);
    const float focus = float(r)/(r+9.0f);
    int dy[2];
    if (myHaloColor.isValid()) {
        dy[0] = dy[1] = 9;
        rg.setFocalPoint(d, d);
        rg.setColorAt( focus, QColor(255,255,255,alpha/2) );
        rg.setColorAt( focus + (1.0f - focus)/2.0f, QColor(255,255,255,0) );
        p.setBrush(rg);
        p.drawRect(shadowBlob.rect());
        rg.setStops(QGradientStops());
        rg.setColorAt( focus, c );
        c.setAlpha(0);
        rg.setColorAt( 1, c );
    } else {
        dy[0] = 6;
        dy[1] = 12;
        rg.setFocalPoint(d, r+7);
        rg.setColorAt( focus, c );
        c.setAlpha(0);
        rg.setColorAt( 1, c );
    }

    p.setBrush(rg);
    p.drawRect(shadowBlob.rect());
    if (r) {
        p.setRenderHint(QPainter::Antialiasing);
        p.setCompositionMode(QPainter::CompositionMode_DestinationOut);
        p.setBrush(Qt::white);
        p.drawEllipse(shadowBlob.rect().adjusted(10,dy[0]+1,-10,-(dy[1]+1)));
    }

    p.end();

    s->topLeft = shadowBlob.copy(0,0,r+9,r+dy[0]);
    s->topRight = shadowBlob.copy(r+10,0,r+9,r+dy[0]);
    s->bottomLeft = shadowBlob.copy(0,r+dy[0]+1,r+9,r+dy[1]);
    s->bottomRight = shadowBlob.copy(r+10,r+dy[0]+1,r+9,r+dy[1]);

#define DUMP_SHADOW(_T_, _W_, _H_)\
s->_T_ = QPixmap(_W_,_H_);\
s->_T_.fill(Qt::transparent);\
p.begin(&s->_T_);\
p.drawTiledPixmap(s->_T_.rect(), buffer);\
p.end()

    QPixmap buffer = shadowBlob.copy(r+10,0,1,r+dy[0]);
    DUMP_SHADOW(top, 32, r+dy[0]);
    buffer = shadowBlob.copy(r+10,r+dy[0]+1,1,r+dy[1]);
    DUMP_SHADOW(bottom, 32, r+dy[1]);
    buffer = shadowBlob.copy(0,r+dy[0]+1,r+9,1);
    DUMP_SHADOW(left, r+9, 32);
    buffer = shadowBlob.copy(r+10,r+dy[0]+1,r+9,1);
    DUMP_SHADOW(right, r+9, 32);
//     buffer = shadowBlob.copy(r+9,r+9,1,1);
//     DUMP_SHADOW(center, 32, 32); // ? usage?

#undef DUMP_SHADOW

    myShadowCache.insert(r, s, 1/*costs*/);
    return s;
}

void
BE::Desk::storeTrashPosition()
{
    myTrash.geometry = myTrash.can->geometry();
    Plugged::saveSettings();
}

void
BE::Desk::tint(QColor color)
{
    if (myTint == color)
        return;
    myTint = color;
    Plugged::saveSettings();
    bool common; Wallpaper &wp = currentWallpaper(&common);
    setWallpaper( wp.file, 0, common ? -3 : KWindowSystem::currentDesktop() );
}

void
BE::Desk::toggleDesktopShown()
{
    const long unsigned int props[2] = {0, NET::WM2ShowingDesktop};
    NETRootInfo net(QX11Info::display(), props, 2);
    net.setShowingDesktop(!net.showingDesktop());
}

void
BE::Desk::toggleDesktopHighlighted()
{
    static bool highlighted = false;
    highlighted = ! highlighted;
    if (highlighted)
        BE::Shell::highlightWindows(window()->winId(), QList<WId>() << window()->winId());
    else
        BE::Shell::highlightWindows(window()->winId(), QList<WId>());
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
    } else {
        XSetWindowBackground(QX11Info::display(), QX11Info::appRootWindow(), 0);
    }
    XClearWindow(QX11Info::display(), QX11Info::appRootWindow());
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

void
BE::Desk::triggerMouseAction(QMouseEvent *me)
{
    if (me->button() > Qt::MiddleButton || me->modifiers())
        return;
    const QString &mouseAction = myMouseAction[me->button()-1];
    if (mouseAction.isEmpty())
        return;
    if (mouseAction.startsWith("menu:")) {
        const QString menu = mouseAction.mid(5);
        if ( menu == "windowlist")
            BE::Shell::windowList()->popup(me->globalPos());
        else if (menu == "BE::Config")
            configMenu()->popup(me->globalPos());
        else if (menu == "BE::Run") {
            QPoint pos = me->globalPos();
            QDBusInterface runner( "org.kde.be.shell", "/Runner", "org.kde.be.shell", QDBusConnection::sessionBus() );
            runner.call("togglePopup", pos.x(), pos.y());

        } else {
            if (!myMouseActionMenu[me->button()-1]) {
                myMouseActionMenu[me->button()-1] = new QMenu(this);
                BE::Shell::buildMenu(menu, myMouseActionMenu[me->button()-1], "");
            }
            myMouseActionMenu[me->button()-1]->popup(me->globalPos());
        }
    } else if (mouseAction.startsWith("exec:")) {
        BE::Shell::run(mouseAction.mid(5));
    } else if (mouseAction.startsWith("dbus:")) {
        BE::Shell::call(mouseAction.mid(5));
    } else
        qDebug() << "Invalid mouse action, must be '[menu|exec|dbus]:'" << mouseAction;
}

static QElapsedTimer mouseDownTimer;
void
BE::Desk::mousePressEvent(QMouseEvent *me)
{
    if (me->button() == Qt::LeftButton) {
        if (BE::Shell::touchMode()) {
            mouseDownTimer.start();
            return;
        }
    }
    triggerMouseAction(me);
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
        if (showMenu) {
            QMouseEvent me2(QEvent::MouseButtonRelease, me->pos(), Qt::RightButton, Qt::RightButton, me->modifiers());
            triggerMouseAction(&me2);
        }
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
            if ( url.path().endsWith(".bwp", Qt::CaseInsensitive) || QImageReader(url.path()).canRead() )
                de->accept();
        }
        else
        {
            QString file = url.fileName();
            if (file.endsWith(".jpg", Qt::CaseInsensitive) || file.endsWith(".jpeg", Qt::CaseInsensitive) ||
                file.endsWith(".png", Qt::CaseInsensitive) || file.endsWith(".bmp", Qt::CaseInsensitive) ||
                file.endsWith(".bwp", Qt::CaseInsensitive))
                de->accept();
        }
    }
        if (de->mimeData()->hasColor())
            de->accept();
    }
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

        setWallpaper( url.pathOrUrl(), mode, desktop );
    } else if (de->mimeData()->hasColor())
        tint(qvariant_cast<QColor>(de->mimeData()->colorData()));
    }
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

void BE::Desk::merryXmas(uint flakes, uint fps)
{
    if (QDate::currentDate().month() != 12)
        return;
    static BE::Snow *snow = 0;
    if (snow) {
        delete snow;
        snow = 0;
    } else {
        snow = new BE::Snow(this, qMax(1u, flakes), qMin(qMax(1u,fps),30u));
        foreach (QStyle *style, qApp->findChildren<QStyle*>()) {
            if (!style->inherits("QStyleSheetStyle")) {
                snow->setStyle(style);
                break;
            }
        }
        desktopResized(myScreen);
        snow->lower();
        snow->show();
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

    foreach (const BE::Panel *panel, myPanels)
    {
        if (panel && panel->isVisible())
        {
            int x(0),y(0),w(0),h(0);
            QRect prect = panel->geometry();
            BE::Shell::getContentsMargins(panel, &x, &y, &w, &h);
//             panel->getContentsMargins(&x,&y,&w,&h);
            const int v = panel->shadowPadding();
            int d[4] = { (v&0xff) - 128, ((v >> 8) & 0xff) - 128,
                         ((v >> 16) & 0xff) - 128, ((v >> 24) & 0xff) - 128 };
            if (d[1] == d[2] && d[2] == d[3] && d[3] == 0xff)
                d[1] = d[2] = d[3] = d[0];
            prect.adjust(x-d[3],y-d[0],d[1]-w,d[2]-h);
            prect.getRect(&x,&y,&w,&h);
            if (panel->effectBgPix() && panel->updatesEnabled())
                p.drawPixmap(qMax(x,0), qMax(y,0), *panel->effectBgPix());

            if (!(myShadowOpacity && panel->castsShadow()) ||
                ((panel->layer() & 1) && BE::Shell::compositingActive()))
                continue;

            const int radius = panel->shadowRadius();

            if (radius < 0)
                continue;

            int flags = 0;
            if (y > rect().top()) flags |= Top;
            if (y+h < rect().bottom()) flags |= Bottom;
            if (x > rect().left()) flags |= Left;
            if (x+w < rect().right()) flags |= Right;

            Shadow *s = shadow(radius);
            x += radius;
            y += radius;
            w = qMax(w-2*radius, 0);
            h = qMax(h-2*radius, 0);
// //                 p.drawTiledPixmap(panel->geometry(), s->center);
            if (w && (flags & Top))
                p.drawTiledPixmap( x, y - s->top.height(), w, s->top.height(), s->top );
            if (w && (flags & Bottom))
                p.drawTiledPixmap( x, y+h, w, s->bottom.height(), s->bottom );
            if (h && (flags & Left))
                p.drawTiledPixmap( x-s->left.width(), y, s->left.width(), h, s->left );
            if (h && (flags & Right))
                p.drawTiledPixmap( x+w, y, s->right.width(), h, s->right );
            if (flags & (Top | Left))
                p.drawPixmap( x-s->topLeft.width(), y-s->topLeft.height(), s->topLeft );
            if (flags & (Top | Right))
                p.drawPixmap( x+w, y-s->topRight.height(), s->topRight );
            if (flags & (Bottom | Left))
                p.drawPixmap( x-s->bottomLeft.width(), y+h, s->bottomLeft );
            if (flags & (Bottom | Right))
                p.drawPixmap( x+w, y+h, s->bottomRight );

            QPen borderPen = panel->shadowBorder();
            if (borderPen.width())
            {
                p.setPen(borderPen);
                p.setBrush(Qt::NoBrush);
                p.setRenderHint(QPainter::Antialiasing);
                const uint &penWidth = borderPen.width();
                uint needsTrans = (penWidth & 1);
                p.save();
                if ( needsTrans )
                    p.translate(0.5, 0.5);
                int d = penWidth/2 + bool(radius);
                prect.adjust(d, d, -(d+needsTrans), -(d+needsTrans));
                p.drawRoundedRect(prect, radius, radius);
                p.restore();
            }
        }
    }

    if (myFadingWallpaper && myFadingWallpaperStep && !iAmRedirected) {
        p.drawPixmap(0, 0, *myFadingWallpaper);
    }

    p.end();
}

void
BE::Desk::wheelEvent(QWheelEvent *we)
{
    if (iWheelOnClickOnly && !(we->buttons() & Qt::LeftButton))
        return;
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
    if (prev != next) {
        qint64 pId = (prev == myWallpapers.end() ? myWallpaper.pix.cacheKey() : prev.value().pix.cacheKey());
        qint64 nId = (next == myWallpapers.end() ? myWallpaper.pix.cacheKey() : next.value().pix.cacheKey());
        if (pId != nId) {
            fadeOutWallpaper();
            myCurrentDesktop = desk;
            emit wallpaperChanged();
        }
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
    myPanelFreeRegion = rect();
    PanelList::iterator i = myPanels.begin();
    while (i != myPanels.end())
    {
        BE::Panel *p = *i;
        if (!p) {
            i = myPanels.erase(i);
            continue;
        }

        myPanelFreeRegion -= p->geometry();

        QPoint pt = mapFromGlobal(p->mapToGlobal(QPoint(0,0)));
        switch (p->position()) {
        case Panel::Top:
            if (pt.y() + p->height() > myIcons.rect.top())
                myIcons.rect.setTop(pt.y() + p->height());
            break;
        case Panel::Bottom:
            if (pt.y() < myIcons.rect.bottom())
                myIcons.rect.setBottom(pt.y());
            break;
        case Panel::Left:
            if (pt.x() + p->width() > myIcons.rect.left())
                myIcons.rect.setLeft(pt.x() + p->width());
            break;
        case Panel::Right:
            if (pt.x() < myIcons.rect.right())
                myIcons.rect.setRight(pt.x());
            break;
        }
        ++i;
    }
    myIcons.rect.adjust(myIconPaddings[0],myIconPaddings[1],-myIconPaddings[2],-myIconPaddings[3]);
    reArrangeIcons();
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
