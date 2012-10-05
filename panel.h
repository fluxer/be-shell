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

#ifndef BE_PANEL_H
#define BE_PANEL_H

#include <QFrame>
#include "be.plugged.h"

class QAction;
class QMenu;

namespace BE {

class StrutManager;

class Panel : public QFrame, public Plugged
{
    Q_OBJECT
public:
    enum Position { Top = 0, Bottom, Left, Right };
    Panel( QWidget *parent = 0 );
    ~Panel();
    BE::Plugged *addStretch(int s);
    void configure( KConfigGroup *grp );
    inline QPixmap *effectBgPix() { return myBgPix; }
    inline int layer() const { return myLayer; }
    Qt::Orientation orientation() const;
    inline Position position() { return myPosition; }
    void saveSettings( KConfigGroup *grp );
    inline int shadowPadding() { return myShadowPadding; }
    inline int shadowRadius() { return myShadowRadius; }
signals:
    void orientationChanged( Qt::Orientation o );
protected:
    void childEvent(QChildEvent*);
    bool eventFilter(QObject *o, QEvent *e);
    void hideEvent( QHideEvent * event );
    void leaveEvent(QEvent *e);
    void showEvent(QShowEvent *e);
    void mouseMoveEvent(QMouseEvent*);
    void mousePressEvent(QMouseEvent*);
    void mouseReleaseEvent(QMouseEvent*);
    void paintEvent(QPaintEvent*);
    void wheelEvent(QWheelEvent*);
private:
    void registerStrut();
    StrutManager *strut() const;
    virtual void themeChanged();
    void unregisterStrut();
    void updateName();
private slots:
    void bindScreenMenu();
    void conditionalHide();
    void desktopResized();
    void enable() { setEnabled(true); }
    void startMoveResize();
    void setAndSaveVisible( bool on );
    void setOnScreen(QAction*);
    void themeUpdated();
    void updateEffectBg();
    void updateSlideHint();
private:
    Position myPosition;
    int mySize, myLength, myOffset, myBlurRadius, myLayer, myAutoHideDelay, myProxyThickness, myScreen;
    QPixmap *myBgPix;
    QMenu *myConfigMenuEntry;
    QAction *myVisibility;
    Qt::CursorShape myMoveResizeMode;
    bool iStrut, iAmNested;
    QWidget *myProxy;
    QString myForcedId;
    int myShadowRadius, myShadowPadding;
private:
    friend class BE::Shell;
    QList<BE::Plugged*> myPlugs;
    void updatePlugOrder();
};

}

#endif // BE_PANEL_H
