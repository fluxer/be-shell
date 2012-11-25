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

#include "be.plugged.h"
#include "be.shell.h"
#include "panel.h"

#include <QApplication>
#include <QDesktopWidget>
#include <QMenu>

#include <KDE/KGlobal>
#include <KDE/KIcon>
#include <KDE/KStandardDirs>

#include <QtDebug>

static const BE::Shell* _shell = 0;
static QMenu *menu = 0;

QMenu *
BE::Plugged::configMenu()
{
    if (!menu)
    {
        menu = new QMenu("BE::Shell");
        menu->setSeparatorsCollapsible(false);
    }
    return menu;
}

const BE::Shell *
BE::Plugged::shell()
{
    return _shell;
}

void
BE::Plugged::configure()
{
    if (!name().isEmpty())
    if (QObject *o = dynamic_cast<QObject*>(this))
        o->setObjectName(name());
    if (_shell)
        _shell->configure(this);
}

QWidget *
BE::Plugged::desktop() const
{
    QWidget *w = myParent;
    while (w && w->parentWidget())
        w = w->parentWidget();
    return w;
}

Qt::Orientation
BE::Plugged::orientation() const
{
    QWidget *w = myParent;
    while (w)
    {
        if (BE::Panel *p = dynamic_cast<BE::Panel*>(w))
            return p->orientation();
        w = w->parentWidget();
    }
    return Qt::Horizontal;
}

QPoint
BE::Plugged::popupPosition(const QSize &popupSize) const
{
    const QWidget *that = dynamic_cast<const QWidget*>(this);
    if (!that) {
        qWarning("Warning: Calling popupPosition() from a non widget plugin is pointless!");
        return QPoint(0,0);
    }
    const QRect screen = QApplication::desktop()->screenGeometry();
    const QPoint center = screen.center();
    QPoint pt = that->mapToGlobal(that->rect().center());
    if (orientation() == Qt::Horizontal)
        pt -= QPoint(popupSize.width()/2, (pt.y() > center.y()) ? that->height()/2 + popupSize.height() + 8 : -(that->height()/2 + 8));
    else
        pt -= QPoint((pt.x() > center.x()) ? that->width()/2 + popupSize.width() + 8 : -(that->width()/2 + 8), popupSize.height()/2 );
    QRect r(pt, popupSize); // bind to screen
    if (r.right() > screen.right())
        r.moveRight(screen.right());
    if (r.bottom() > screen.bottom())
        r.moveBottom(screen.bottom());
    if (r.left() < screen.left())
        r.moveLeft(screen.left());
    if (r.top() < screen.top())
        r.moveTop(screen.top());
    return r.topLeft();
}

void
BE::Plugged::saveSettings()
{
    if (_shell)
        _shell->saveSettings(this);
}

void
BE::Plugged::setShell(const BE::Shell *sh)
{
    _shell = sh;
}

QString
BE::Plugged::theme()
{
    if (_shell) return _shell->theme();
    return QString();
}

QIcon
BE::Plugged::themeIcon(const QString &icon, bool tryKDE)
{
    QString file = KGlobal::dirs()->locate("data","be.shell/Themes/" + theme() + "/" + icon + ".png");
    QIcon icn(file);
    if (tryKDE && icn.isNull())
        return KIcon(icon);
    return icn;
}
