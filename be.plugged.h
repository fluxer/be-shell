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

#ifndef BE_PLUGGED_H
#define BE_PLUGGED_H

class KConfigGroup;
class QMenu;
class QDomElement;
class QWidget;

#include <QIcon>
#include <QString>

namespace BE {

class Shell;

class Plugged
{
    friend class Shell;
    friend void themeChanged();
public:
    Plugged( QWidget *parent = 0, const QString &name = QString() ){ myParent = parent; myName = name; }
    virtual ~Plugged() {}
    void configure();
    virtual void configure( KConfigGroup *grp ) { (void)grp; }
    QWidget *desktop() const;
    inline const QString &name() const { return myName; }
    Qt::Orientation orientation() const;
    inline QWidget *parent() const { return myParent; }
    static QString theme();
    static QIcon themeIcon(const QString &icon, const QWidget *w, bool tryKDE = true);
    QIcon themeIcon(const QString &icon, bool tryKDE = true);
    void saveSettings();
    virtual void saveSettings( KConfigGroup *grp ) { (void)grp; }
protected:
    static QMenu *configMenu();
    QPoint popupPosition(const QSize &popupSize) const;
    static const Shell *shell();
    virtual void themeChanged() {};
private:
    QString myName;
    QWidget *myParent;
    static void setShell(const Shell *shell);
};
}

#endif // BE_PLUGGED_H
