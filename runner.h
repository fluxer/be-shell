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

#ifndef RUN_H
#define RUN_H

#include <QAbstractItemView>
#include <QDialog>
#include <QHash>
#include <QTime>
#include <KDE/KServiceGroup>

#include "be.plugged.h"

class QCompleter;
class QLineEdit;
class QProcess;
class QTimer;
class QTextBrowser;
class QTreeWidget;
class QTreeWidgetItem;

class KConfigGroup;

namespace BE {
class RunDelegate;
class Run : public QDialog, public Plugged
{
    Q_OBJECT
public:
    Run( QObject *parent = 0 );
    void configure( KConfigGroup* );
    void saveSettings( KConfigGroup* );

public slots:
    void hide();
    void showAsDialog();
    void togglePopup(int x, int y);

protected:
    void changeEvent( QEvent* );
    bool eventFilter( QObject*, QEvent* );
    void paintEvent( QPaintEvent* );
    void timerEvent( QTimerEvent *te);

private slots:
    void exitIOMode();
    void filter( const QString & );
    void hideDueToInactivity();
    void pipeIOProc();
    void releaseTrigger();
    void resetShellPalette();
    void slotItemActivated(QTreeWidgetItem*, int);
    void slotCurrentItemChanged(QTreeWidgetItem*, QTreeWidgetItem*);
    void slotRepopulate();
    void startSlide();

private:
    void execute( const QString &execLine );
    void installPathCompleter();
    float favor( const QString &exec );
    void flash( const QColor &c );
    void focusInput();
    void closeShellOutput(bool resetText = true);
    bool repopulate( KSharedPtr<KServiceGroup> group, QTreeWidgetItem *);
    void rSaveFavor(QTreeWidgetItem *item);
    void slideTo( const QTreeWidgetItem *item, QAbstractItemView::ScrollHint hint = QAbstractItemView::EnsureVisible );
    void updateFavorites();

    QCompleter *m_binCompleter;
    int m_currentHistoryEntry, m_visibleIcons;
    QHash<QString,float> favorHash;
    QTreeWidgetItem *favorites;
    QTimer *m_hideTimer;
    int myVisibilityTimeout;
    bool m_flat, m_inactive, m_isPopup, iScheduledResort, mySettingsDirty;
    QStringList m_history;
    QString m_lastFilter;
    QPalette m_originalPal;
    QLineEdit *m_shell;
    QTreeWidget *m_tree;
    int m_slideTarget, m_slideTimer, m_slideRange;
    QTreeWidgetItem *m_slideItem;
    QAbstractItemView::ScrollHint m_Slidehint;
    QTreeWidgetItem *m_triggeredItem;
    RunDelegate *delegate[2];
    QTime focusOut;
    QList<QProcess *>myIOProcs;
    QTextBrowser *myOutput;
    typedef QMap<QString, QString> Aliases;
    Aliases myAliases;
    QString myBcCmd;
};
} // namepsace
#endif // RUN_H
