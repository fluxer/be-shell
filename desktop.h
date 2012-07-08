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

#ifndef DESKTOPWIDGET_H
#define DESKTOPWIDGET_H

#include <QToolButton>
#include <QHash>
#include <QPixmap>
#include <QPoint>
#include <QPointer>
#include <QDateTime>

#include "be.plugged.h"

class TopBar;
class BottomBar;
class Config;
class QGridLayout;
class KDirWatch;
class KConfigGroup;

namespace BE {

enum WallpaperMode { Heuristic = -1, Invalid = 0, Plain = 1, Tiled, ScaleV, ScaleH, Scale, Maximal, ScaleAndCrop };
const int AllDesktops = -1;
const int AskForDesktops = -2;

class DeskIcon : public QToolButton
{
    Q_OBJECT
public:
    DeskIcon( const QString &path, QWidget *parent );
    void updateIcon();
    static void setSize(int size);
    inline static QSize size() { return ourSize; };

protected:
    void enterEvent(QEvent *ev);
    void leaveEvent(QEvent *ev);
    void mousePressEvent(QMouseEvent *);
    void mouseMoveEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
//     void paintEvent(QPaintEvent *);
private:
    QString myUrl, myName;
    int myNameWidth;
    static QSize ourSize;
};

class Panel;
class Trash;

class Wallpaper
{
public:
    Wallpaper() { aspect = -1.0; }
    Wallpaper(const Wallpaper &other)
    {
        align = other.align; file = other.file;
        mode = other.mode; pix = other.pix;
        offset = other.offset; timestamp = other.timestamp;
        aspect = other.aspect;
    }
    bool fits( const QString &file, int mode ) const;
    void calculateOffset(const QSize &screenSize, const QPoint &screenOffset);
    Qt::Alignment align;
    QString file;
    WallpaperMode mode;
    QPoint offset;
    QPixmap pix;
    QDateTime timestamp;
    float aspect;
};

class Corner;
class CornerDialog;
class WallpaperDesktopDialog;

class Desk : public QWidget, public Plugged
{
    Q_OBJECT
public:
    Desk( QWidget *parent = 0 );
    void registrate( BE::Panel * );
    void configure( KConfigGroup *grp );
    void saveSettings( KConfigGroup *grp );
    inline int screen() { return myScreen; }
public slots:
    void configure() { Plugged::configure(); }

signals:
    void resized();
    void wallpaperChanged();

protected:
    bool eventFilter(QObject *o, QEvent *e);
    void focusInEvent(QFocusEvent *) {};
    void focusOutEvent(QFocusEvent *) {};
    void mousePressEvent( QMouseEvent* event );
    void mouseReleaseEvent(QMouseEvent *);
    void dragEnterEvent( QDragEnterEvent* );
    void dropEvent ( QDropEvent* );
    void keyPressEvent( QKeyEvent * );
    void mouseDoubleClickEvent(QMouseEvent*);
    void paintEvent(QPaintEvent*);
    void wheelEvent(QWheelEvent*);

private:
    Wallpaper &currentWallpaper(bool *common = 0L);
    void populate( const QString &path );

private:
    friend class DeskAdaptor;
    void setWallpaper( const QString &file, int mode = 700, int desktop = -1 );
    void toggleDesktopShown();

private slots:
    void bindScreenMenu();
    void changeWallpaperAlignment( QAction *action );
    void changeWallpaperAspect( QAction *action );
    void changeWallpaperMode( QAction *action );
    void desktopChanged ( int desk );
    void desktopResized ( int screen );
    void fileCreated( const QString &path );
    void fileDeleted( const QString &path );
    void fileChanged( const QString &path );
    void kcmshell4(QAction *);
    void selectWallpaper();
    void setOnScreen( QAction *action );
    void setRoundCorners();
    void storeTrashPosition();
    void toggleIconsVisible( bool );
    void toggleTrashcan( bool );
    void updateOnWallpaperChange();

private:

    typedef QHash<QString,DeskIcon*> IconList;

    struct Icons {
        bool areShown;
        QPoint lastPos;
        IconList list;
        QRect rect;
        QAction *menuItem;
    } myIcons;

    int myScreen;

    struct Shadow {
        QPixmap topLeft, top, topRight, left, center, right, bottomLeft, bottom, bottomRight;
        int opacity;
    } myShadow;

    struct WpSettings {
        QMenu *mode, *align, *aspect;
        QRect area;
    } wpSettings;

    bool ignoreSaveRequest, iRootTheWallpaper;
    uint myCorners;
    int myCurrentDesktop;

    Wallpaper myWallpaper;
    typedef QHash<int,Wallpaper> Wallpapers;
    Wallpapers myWallpapers;
    Qt::Alignment myWallpaperDefaultAlign;
    WallpaperMode myWallpaperDefaultMode;


    typedef QList< QPointer<Panel> > PanelList;
    PanelList myPanels;

    int myRootBlurRadius;
    struct MyTrash {
        Trash *can;
        QRect geometry;
        QAction *menuItem;
    } myTrash;

    KDirWatch *myDesktopWatcher;
    WallpaperDesktopDialog *myWpDialog;
};
}

#endif // DESKTOPWIDGET_H
