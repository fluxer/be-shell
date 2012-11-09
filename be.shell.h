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

#ifndef BE_SHELL_H
#define BE_SHELL_H

#include <QMap>
#include <QObject>
#include <QPoint>
#include <QPointer>
#include <QPen>
#include <qwindowdefs.h>

class KSharedConfig;
class QAction;
class QDomElement;
class QFileSystemWatcher;
class QImage;
class QMenu;

namespace BE {

class Config;
class Desk;
class Plugged;
class Panel;

typedef QList<BE::Plugged*> PlugList;

class Shell : public QObject
{
    Q_OBJECT
public:
    Shell(QObject *parent = 0);
    ~Shell();
    static BE::Plugged* addApplet(const QString &name, Panel *panel);
    static void blur( QImage &img, int radius );
    static void buildMenu(const QString &name, QWidget *widget, const QString &type);
    static void call(const QString &instruction);
    static bool callThemeChangeEventFor(BE::Plugged*);
    static bool compositingActive();
    static QRect desktopGeometry(int screen = -1);
    static QString executable(WId id);
    static bool hasFullscreenAction();
    static void populateWindowList(const QList<WId> &windows, QMenu *popup, bool allDesktops);
    static void run(const QString &command);
    static int screen();
    static QMenu *screenMenu();
    static int shadowPadding(const QString &string);
    static int shadowRadius(const QString &string);
    static QPen shadowBorder(const QString &string);
    static void showBeMenu();
    static void showWindowContextMenu(WId window, const QPoint &pos);
    static bool touchMode();
    static QMenu *windowList();
    inline QString theme() const { return myTheme; };
public slots:
    void updateStyleSheet( const QString &filename );
protected:
    bool eventFilter(QObject *o, QEvent *e);
signals:
    void desktopResized();
    void styleSheetChanged();

private slots:
    friend class ShellAdaptor;
    void configure();
    void contextWindowAction(QAction*);
    void contextWindowToDesktop(QAction*);
    void resetCompositing();
    void setTheme(const QString &);
private slots:
    friend class ScreenLockerAdaptor;
    void lockScreen();
    void configureScreenLocker();
signals:
    void ActiveChanged();
private:
    friend class Plugged;
    static void configure( Plugged *plug );
    void rBuildMenu(const QDomElement &node, QWidget *widget);
    static void saveSettings( Plugged *plug );
    void saveSettings();

private slots:
    void callFromAction();
    void runFromAction();
    void editConfig();
    void editThemeSheet();
    void launchRunner();
    void populateScreenMenu();
    void populateWindowList();
    void setActiveWindow();
    void setCurrentDesktop();
    void setTheme(QAction *);
    void updateThemeList();
private:
    BE::Desk *myDesk;
    QFileSystemWatcher *myStyleWatcher;
    QMenu *myWindowList, *myThemesMenu, *myContextMenu, *myScreenMenu;
    int myScreen;
    WId myContextWindow;
    PlugList myPlugs;
    QString myTheme;
    typedef QList<QString> QStringList;
    QStringList myPanels;
    bool iAmTouchy;
    QMap<QString, QVariant> myShadowRadius, myShadowPadding, myShadowBorder;
};
}

#endif // BE_SHELL_H
