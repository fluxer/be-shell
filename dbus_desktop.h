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

#ifndef DESKTOP_ADAPTOR_H
#define DESKTOP_ADAPTOR_H

#include <QtDBus/QDBusAbstractAdaptor>
#include "desktop.h"

namespace BE
{
class DeskAdaptor : public QDBusAbstractAdaptor
{
   Q_OBJECT
   Q_CLASSINFO("D-Bus Interface", "org.kde.be.shell")

private:
    BE::Desk *myDesk;

public:
    DeskAdaptor(BE::Desk *desk) : QDBusAbstractAdaptor(desk), myDesk(desk) { }

public slots:
    void setWallpaper( const QString &file, int mode ) { myDesk->setWallpaper(file, mode); }
    void setWallpaper( const QString &file ) { myDesk->setWallpaper(file, -1); }
    void toggleDesktopShown() { myDesk->toggleDesktopShown(); }
    void toggleDesktopHighlighted() { myDesk->toggleDesktopHighlighted(); }
    void reArrangeIcons() { myDesk->reArrangeIcons(); }
    void refresh() { myDesk->update(); }
    void tint( const QString &color ) { myDesk->tint(QColor(color)); }
    void merryXmas() { myDesk->merryXmas(); }
    void merryXmas(uint flakes, uint fps) { myDesk->merryXmas(flakes, fps); }
    int winId() { return myDesk->winId(); }
};
}
#endif
