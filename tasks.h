/**************************************************************************
*   Copyright (C) 2011 by Thomas Lübking                                  *
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

#ifndef TASKS_H
#define TASKS_H

class QLabel;
#include "be.plugged.h"
#include "button.h"

#include <QMap>
// #include <QPointer>

namespace BE {

class Task : public Button
{
    Q_OBJECT
public:
    Task(QWidget *parent, WId id, bool sticky = false, const QString &name = QString());
    void add(WId id);
    inline void clear()  { myWindows.clear(); }
    void configure( KConfigGroup *grp );
    inline bool contains(WId id) { return myWindows.contains(id); }
    inline int count() { return myWindows.count(); }
    WId firstRelevant() const;
    inline const QString &group() { return myGroup; }
    inline bool isActive() { return isChecked(); }
    inline bool isEmpty() { return myWindows.isEmpty(); }
    inline bool isMinimized() { return iAmMinimized; }
    bool isOnCurrentDesktop() const;
    inline bool isRelevant() const { return bool(firstRelevant()); }
    inline bool isRunning() { return !myWindows.isEmpty(); }
    inline bool isSticky() { return iStick; }
    WId lastRelevant() const;
    bool remove(WId id);
    inline void setActive(bool active) { setChecked(active); }
    inline void setGroup(const QString &group) { myGroup = group; }
    void setSticky(bool stick);
    void setToolButtonStyle(Qt::ToolButtonStyle);
    QSize sizeHint() const;

    void showWindowList();
    void update(const unsigned long *properties);
protected:
    void enterEvent(QEvent *e);
    void leaveEvent(QEvent *e);
    void mousePressEvent(QMouseEvent *);
    void mouseReleaseEvent(QMouseEvent *);
    void moveEvent(QMoveEvent *);
    void resizeEvent(QResizeEvent *);
    void wheelEvent(QWheelEvent *ev);
private:
    void publishGeometry(const QRect &r);
    void repolish();
    QString resortedText(const QString &text);
    QString squeezedText(const QString &text);
    void toggleState(WId id);
private slots:
    void highlightAllOrNone();
    void highlightWindow(QAction *a);
    void _repolish();
    void toggleSticky();
private:
    bool iStick, iAmImportant, iAmMinimized, iAmDirty;
    QString myGroup, myLabel, myText;
    Qt::ToolButtonStyle myStyle;
    QList<WId> myWindows;
    static QLabel *ourToolTip;
    QSize mySizeHint;
    bool mySizeHintIsDirty;
};

class Tasks : public QFrame, public Plugged
{
    Q_OBJECT
public:
    Tasks(QWidget *parent);
    void configure( KConfigGroup *grp );
protected:
//     void mousePressEvent( QMouseEvent *ev );
    void leaveEvent(QEvent *e);
    void wheelEvent( QWheelEvent *ev );
private slots:
    Task *addWindow( WId id );
    void checkSanity();
    void orientationChanged( Qt::Orientation o );
    void removeWindow( WId id );
    void setCurrentDesktop(int desktop);
    void updateWindowProperties(WId id, const unsigned long *properties);
    void windowActivated( WId id );

    void setStyle();
    void updateVisibility(Task *t);

private:
    typedef QList<Task*> TaskList;
    TaskList myTasks;
    QList<WId> myWindows;
    Qt::Orientation myOrientation;
    bool iStack, iStackNow, iSeparateDesktops, iSeparateScreens, iIgnoreVisible, hasStickies;
    Qt::ToolButtonStyle myButtonMode;
};
}


#endif // TASKS_H