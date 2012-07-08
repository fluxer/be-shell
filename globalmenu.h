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

#ifndef GMENU_H
#define GMENU_H

class QMenu;
class QMenuBar;
class QFileSystemWatcher;

#include "be.plugged.h"
#include <QMap>
#include <QPointer>
#include <QWidget>

namespace BE {
class HMenuBar;
class VMenuBar;
class GMenu : public QWidget, public Plugged
{
    Q_OBJECT
public:
    GMenu(QWidget *parent);
    ~GMenu();
    void registerMenu(const QString &service, qlonglong key, const QString &title, const QStringList &entries);
    void unregisterMenu(qlonglong key);
    void reparent(qlonglong oldKey, qlonglong newKey);
    void changeEntry(qlonglong key, int idx, const QString &entry = QString(), bool add = false);
    void setOpenPopup(int idx);
    static void showMainMenu();
    void requestFocus(qlonglong key);
    void releaseFocus(qlonglong key);
protected:
    void focusOutEvent(QFocusEvent *ev);
    void mousePressEvent( QMouseEvent *ev );
    void wheelEvent( QWheelEvent *ev );
private:
    void blockUpdates(bool);
    HMenuBar *currentHBar() const;
    VMenuBar *currentVBar() const;
    void hide(QWidget *item);
    void show(QWidget *item);
    bool dbusAction(const QObject *o, int idx, const QString &cmd);
    static const QString &service(QWidget*);
    static QString &title(QWidget*);

    static bool globalX11EventFilter( void *msg );
    QWidget *ggmCreate( WId id );
    void ggmUpdate( WId id );
private slots:
    void byeMenus();
    void callMenus();
    void cleanBodies();
    void hover(int);
    void orientationChanged( Qt::Orientation o );
    void raiseCurrentWindow();
    void repopulateMainMenu();
    void trigger(int);
    void unlockUpdates();
    void unregisterCurrentMenu();

    void ggmWindowActivated( WId id );
    void ggmWindowAdded( WId id );
    void ggmWindowRemoved( WId id );
    void runGgmAction();
private:
    typedef QMap<qlonglong, QPointer<QWidget> > MenuMap;
    MenuMap myMenus;
    typedef QList<WId> GGMList;
    GGMList ggmMenus;
    WId ggmLastId;
    
    QPointer<QWidget> myCurrentBar;
    Qt::Orientation myOrientation;
    QWidget *myMainMenu;
    QFileSystemWatcher *myMainMenuDefWatcher;
    static QTimer ourBodyCleaner;
    
};
}
#endif // GMENU_H
