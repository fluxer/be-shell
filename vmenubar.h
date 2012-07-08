#ifndef VMENUBAR_H
#define VMENUBAR_H

#include <QMenu>
namespace BE {
class VMenuBar : public QMenu
{
    Q_OBJECT
public:
    VMenuBar ( const QString &service = QString(), qlonglong key = 0, QWidget *parent = 0);
#include "menubar.h"
};
}

#endif
