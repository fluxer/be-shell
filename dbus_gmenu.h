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

#ifndef GMENU_ADAPTOR_H
#define GMENU_ADAPTOR_H

#include <QDBusAbstractAdaptor>
#include "globalmenu.h"

namespace BE
{
class GMenuAdaptor : public QDBusAbstractAdaptor
{
    Q_OBJECT
    Q_CLASSINFO("D-Bus Interface", "org.kde.XBar")

private:
    BE::GMenu *myGlobalMenu;

public:
    GMenuAdaptor(BE::GMenu *menu) : QDBusAbstractAdaptor(menu), myGlobalMenu(menu) { }

public slots:
    void registerMenu(const QString &service, qlonglong key, const QString &title, const QStringList &entries)
    { myGlobalMenu->registerMenu(service, key, title, entries); }
    void unregisterMenu(qlonglong key) { myGlobalMenu->unregisterMenu(key); }

    void setOpenPopup(int idx) { myGlobalMenu->setOpenPopup(idx); }
    void requestFocus(qlonglong key) { myGlobalMenu->requestFocus(key); }
    void releaseFocus(qlonglong key) { myGlobalMenu->releaseFocus(key); }
    void reparent(qlonglong oldKey, qlonglong newKey)
    { myGlobalMenu->reparent(oldKey, newKey); }
    void addEntry(qlonglong key, int idx, const QString &entry)
    { myGlobalMenu->changeEntry(key, idx, entry, true); }
    void changeEntry(qlonglong key, int idx, const QString &entry)
    { myGlobalMenu->changeEntry(key, idx, entry, false); }
    void removeEntry(qlonglong key, int idx) { myGlobalMenu->changeEntry(key, idx); }

};
}
#endif //GMENU_ADAPTOR_H
