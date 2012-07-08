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


// namespace BE {
//     
// class _CLASS_ : public _BASE_
// {
//     Q_OBJECT
// public:
//     _CLASS_ ( const QString &service = QString(), qlonglong key = 0, QWidget *parent = 0);

    QAction *action(int idx) const;
    void clear();
    void hide();
    int index(const QAction *action) const;
    inline qlonglong key() const { return myKey; }
    inline const QString& service() const { return myService; }
signals:
    void hovered(int);
    void triggered(int);
protected:
    void leaveEvent(QEvent *event);
    void timerEvent(QTimerEvent *event);
private:
friend class GMenu;
    void popDown();
    void setOpenPopup(int popup);
    QString title;
private slots:
    void hover(QAction*);
    void trigger(QAction*);
private:
    QString myService;
    qlonglong myKey;
// };

// }

