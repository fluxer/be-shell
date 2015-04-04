/**************************************************************************
*   Copyright (C) 2014 by Thomas Lübking                                  *
*   thomas.luebking@gmail.com                                             *
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
#include "wmctrl.h"
#include "flowlayout.h"
#include <QApplication>
#include <QCheckBox>
#include <QDBusConnection>
#include <QDBusInterface>
#include <QDialog>
#include <QLabel>
#include <QLineEdit>
#include <QMouseEvent>
#include <QToolButton>
#include <QVBoxLayout>
#include <QX11Info>
#include <KConfigGroup>
#include <KWindowInfo>
#include <KWindowSystem>
#include <netwm.h>

#include <QtDebug>

namespace BE {
class WmButton : public QToolButton
{
public:
    WmButton(QWidget *parent, char t, QString icon) : QToolButton(parent), type(t), iconString(icon)
    {
        setIcon(Plugged::themeIcon(iconString, parent));
    }
    char type;
    QString iconString;
};
}

BE::WmCtrl::WmCtrl(QWidget *parent) : QFrame(parent), BE::Plugged(parent), iOnlyManageMaximized(false), iHideDisabled(false)
{
    setObjectName("WmCtrl");
    FlowLayout *l = new FlowLayout(this);
    l->setContentsMargins(0, 0, 0, 0);
    l->setSpacing(0);
    connect(KWindowSystem::self(), SIGNAL(activeWindowChanged(WId)), SLOT(setManagedWindow(WId)));
    connect(KWindowSystem::self(), SIGNAL(windowChanged(WId, const unsigned long*)), SLOT(windowChanged(WId, const unsigned long*)));
    setManagedWindow(KWindowSystem::activeWindow());
}

static inline void toggleState(WId win, NET::State singleState)
{
    if (KWindowInfo(win, NET::WMState).state() & singleState)
        KWindowSystem::clearState(win, singleState);
    else
        KWindowSystem::setState(win, singleState);
}

void
BE::WmCtrl::buttonClicked()
{
    char type = 0;
    if (BE::WmButton *btn = dynamic_cast<BE::WmButton*>(sender()))
        type = btn->type;
    switch (type) {
    case 'S': // Sticky
        KWindowSystem::setOnAllDesktops(myManagedWindow, !KWindowInfo(myManagedWindow, NET::WMDesktop).onAllDesktops()); break;
    case 'F': // Keep Above
        toggleState(myManagedWindow, NET::KeepAbove); break;
    case 'B': // Keep Below
        toggleState(myManagedWindow, NET::KeepBelow); break;
    case 'L': // Shade button
        toggleState(myManagedWindow, NET::Shaded); break;
    case 'I': // Minimize
        KWindowSystem::minimizeWindow(myManagedWindow);  break;
    case 'A': // Maximize
        toggleState(myManagedWindow, NET::Max); break;
    case 'X': // Close button
        NETRootInfo(QX11Info::display(), NET::CloseWindow).closeWindowRequest(myManagedWindow); break;
    case '+': // moveResize
        setMouseTracking(true);
        grabMouse(); break;
    case 'E': // presentWindows
        QDBusInterface("org.kde.kglobalaccel", "/component/kwin", "org.kde.kglobalaccel.Component",
                       QDBusConnection::sessionBus()).call(QDBus::NoBlock, "invokeShortcut", "ExposeAll"); break;
    default:
        qDebug() << name() << "invalid button" << type;
    }
}

static QString defaultIcon(char c)
{
    switch (c) {
    case 'S': return "favorites"; // Sticky
    case 'F': return "arrow-up"; // Keep Above
    case 'B': return "arrow-down"; // Keep Below
    case 'L': return "go-top"; // Shade button
    case 'I': return "list-remove"; // Minimize
    case 'A': return "list-add"; // Maximize
    case 'X': return "window-close"; // Close button
    case '+': return "transform-move"; // moveResize
    case 'E': return "view-multiple-objects"; // presentWindows
    default:
        return QString();
    }
}

void
BE::WmCtrl::configure(KConfigGroup *grp)
{
    const bool didHide = iHideDisabled;
    iHideDisabled = grp->readEntry("HideDisabled", false);
    const bool didOnlyMaximized = iOnlyManageMaximized;
    iOnlyManageMaximized = grp->readEntry("OnlyMaximized", false);
    if (iOnlyManageMaximized != didOnlyMaximized || didHide != iHideDisabled)
        setManagedWindow(myManagedWindow);

    QString oldButtons = myButtons;
    myButtons = grp->readEntry("Buttons", "X_IA").toUpper();
    if (oldButtons != myButtons) {
        QLayoutItem *child;
        while ((child = layout()->takeAt(0))) {
            delete child->widget();
            delete child;
        }
        for (int i = 0; i < myButtons.count(); ++i) {
            const char c = myButtons.at(i).toLatin1();
            switch (c) {
            case '_':
                layout()->addItem(new QSpacerItem(4,4,QSizePolicy::Fixed,QSizePolicy::Fixed));
                break;
            case 'S': // Sticky
            case 'F': // Keep Above
            case 'B': // Keep Below
            case 'L': // Shade button
            case 'I': // Minimize
            case 'A': // Maximize
            case 'X': // Close button
            case '+': // moveResize
            case 'E': { // presentWindows
                WmButton *btn = new WmButton(this, c, grp->readEntry(QString(c), defaultIcon(c)));
                connect(btn, SIGNAL(clicked()), SLOT(buttonClicked()));
                layout()->addWidget(btn);
                break;
            }
            default:
                qDebug() << name() << ":" << c << "is not a valid WmCtrl layout item";
            }
        }
    }
}


NET::Direction currentDirection(QPoint p, WId &window)
{
    QList<WId> stack = KWindowSystem::stackingOrder();
    window = 0;
    QRect r;
    for (int i = stack.count() - 1; i > -1; --i) {
        KWindowInfo info(stack.at(i), NET::WMWindowType|NET::WMFrameExtents);
        NET::WindowType type = info.windowType(NET::AllTypesMask);
        if (!(type == NET::Normal || type == NET::Dialog || type == NET::Unknown || type == NET::Utility))
            continue;
        r = info.frameGeometry();
        if (r.contains(p)) {
            window = stack.at(i);
            break;
        }
    }
    if (!window)
        return NET::MoveResizeCancel;
    p -= r.topLeft();
    NET::Direction d = NET::Move;
    if (p.x() < r.width()/3) {
        d = NET::Left;
    } else if (p.x() > 2*r.width()/3) {
        d = NET::Right;
    }
    if (p.y() < r.height()/3) {
        d = (d == NET::Left) ? NET::TopLeft : ((d == NET::Right) ? NET::TopRight : NET::Top);
    } else if (p.y() > 2*r.height()/3) {
        d = (d == NET::Left) ? NET::BottomLeft : ((d == NET::Right) ? NET::BottomRight : NET::Bottom);
    }
    return d;
}

void
BE::WmCtrl::enableButtons(bool e)
{
    QList<BE::WmButton*> btns = findChildren<BE::WmButton*>();
    foreach (BE::WmButton *btn, btns) {
        if (!(btn->type == 'E' || btn->type == '+')) {
            btn->setEnabled(e);
            if (iHideDisabled)
                btn->setVisible(e);
        }
    }
}

void
BE::WmCtrl::mouseMoveEvent(QMouseEvent *ev)
{
    if (mouseGrabber() == this) {
        WId win;
        NET::Direction dir = currentDirection(static_cast<QMouseEvent*>(ev)->globalPos(), win);
        if (dir != NET::MoveResizeCancel) {
            Qt::CursorShape shape = Qt::ForbiddenCursor;
            switch (dir) {
            case NET::TopLeft:
            case NET::BottomRight:
                shape = Qt::SizeFDiagCursor; break;
            case NET::Top:
            case NET::Bottom:
                shape = Qt::SizeVerCursor; break;
            case NET::TopRight:
            case NET::BottomLeft:
                shape = Qt::SizeBDiagCursor; break;
            case NET::Right:
            case NET::Left:
                shape = Qt::SizeHorCursor; break;
            default:
            case NET::Move:
                shape = Qt::SizeAllCursor; break;
            }
            if (!QApplication::overrideCursor() || shape != QApplication::overrideCursor()->shape()) {
                if (QApplication::overrideCursor())
                    QApplication::restoreOverrideCursor();
                QApplication::setOverrideCursor(shape);
            }
        }
        ev->accept();
    } else
        QFrame::mouseMoveEvent(ev);
}

void
BE::WmCtrl::mousePressEvent(QMouseEvent *ev)
{
    if (mouseGrabber() == this) {
        setMouseTracking(false);
        releaseMouse();
        QApplication::restoreOverrideCursor();
        if( ev->button() == Qt::LeftButton ) {
            WId win;
            QPoint pos = static_cast<QMouseEvent*>(ev)->globalPos();
            NET::Direction dir = currentDirection(pos, win);
            NETRootInfo(QX11Info::display(), NET::WMMoveResize ).moveResizeRequest(win, pos.x(), pos.y(), dir);
        }
        ev->accept();
    }
    else if( ev->button() == Qt::RightButton ) {
        QDialog dlg;
        dlg.setLayout(new QVBoxLayout(&dlg));
        dlg.layout()->addWidget(new QLabel(tr("Each char in the string represents a button:\n"
                                              "I: Minimize, A: Maximize, X: Close\n"
                                              "S: Sticky, F: Keep Above, B: Keep Below, L: Shade\n"
                                              "+: Move & Resize, E: Expose, _: Spacer\n"
                                              "Other chars are ignored"), &dlg));
        QLineEdit *le = new QLineEdit(myButtons, &dlg);
        dlg.layout()->addWidget(le);
        QCheckBox *cb = new QCheckBox("Only for maximized windows", &dlg);
        cb->setChecked(iOnlyManageMaximized);
        dlg.layout()->addWidget(cb);
        dlg.adjustSize();
        dlg.move(popupPosition(dlg.size()));
        dlg.exec();
        QString newButtons = le->text().toUpper();
        if (myButtons != newButtons || iOnlyManageMaximized != cb->isChecked()) {
            QString oldButtons(myButtons);
            myButtons = newButtons;
            bool didOnlyMaximized(iOnlyManageMaximized);
            iOnlyManageMaximized = cb->isChecked();
            Plugged::saveSettings();
            myButtons = oldButtons;
            iOnlyManageMaximized = didOnlyMaximized;
            Plugged::configure();
        }
        ev->accept();
    }
    else
        QFrame::mousePressEvent(ev);
}

void
BE::WmCtrl::saveSettings(KConfigGroup *grp)
{
    grp->writeEntry("OnlyMaximized", iOnlyManageMaximized);
    grp->writeEntry("Buttons", myButtons);
}

void
BE::WmCtrl::setManagedWindow(WId win)
{
    myManagedWindow = win;
    bool enabled = !iOnlyManageMaximized || KWindowInfo(win, NET::WMState).state() & NET::Max/*Vert*/;
    if (enabled) {
        NET::WindowType type = KWindowInfo(win, NET::WMWindowType).windowType(NET::AllTypesMask);
        enabled = type == NET::Normal || type == NET::Dialog || type == NET::Unknown; // assume unknown as normal
    }
    enableButtons(enabled);
}

void
BE::WmCtrl::themeChanged()
{
    QList<BE::WmButton*> btns = findChildren<BE::WmButton*>();
    foreach (BE::WmButton *btn, btns)
        btn->setIcon(themeIcon(btn->iconString, this));
}

void
BE::WmCtrl::windowChanged(WId win, const unsigned long *props)
{
    if (win != myManagedWindow)
        return; // we don't care
    if (props[0] & NET::WMState)
        enableButtons(!iOnlyManageMaximized || KWindowInfo(win, NET::WMState).state() & NET::Max/*Vert*/);
}
