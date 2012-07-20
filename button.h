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

#ifndef BUTTON_H
#define BUTTON_H

#include "be.plugged.h"

#include <QToolButton>

namespace BE {
class ButtonAdaptor;
class Button : public QToolButton, public Plugged
{
  Q_OBJECT
public:
    Button(QWidget *parent = 0, const QString &name = QString());
    void configure( KConfigGroup *grp );
    inline const QString &exe() const { return myExe; }
    void requestAttention(int count = 12);
protected:
    bool eventFilter(QObject *o, QEvent *e);
    inline bool isSyntheticCrossing() const { return imNotReallyCrossed; }
    void enterEvent(QEvent *e);
    void leaveEvent(QEvent *e);
    void mousePressEvent(QMouseEvent *me);
    void mouseReleaseEvent(QMouseEvent *me);
    void paintEvent(QPaintEvent *pe);
    void resizeEvent(QResizeEvent *re);
    void setCommand( QString cmd );
    void themeChanged();
    void timerEvent(QTimerEvent *te);
    void wheelEvent(QWheelEvent *ev);
private slots:
    void dbusCall();
    void pulse();
    void runCommand();
    void startService();
    void updateMenu();
private:
    void createBuffer();
private:
    QString myIcon, myCommand, myExe, myWheel[2];
    QMenu *myMenu;
    int myPulseIteration, myPulseLimit, myUpdaterTimeout, myAnimationTimer, myAnimationStep;
    ButtonAdaptor *myDBus;
    bool imNotReallyCrossed;
    QPixmap *myBuffer[2], *myRenderTarget;
};

}
#endif // BUTTON_H
