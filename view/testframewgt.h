#ifndef TESTFRAMEWGT_H
#define TESTFRAMEWGT_H

#include <QWidget>
#include "styledledlabel.h"
namespace Ui {
class testframewgt;
}

class testframewgt : public QWidget
{
    Q_OBJECT

public:
    explicit testframewgt(QWidget *parent = nullptr);
    ~testframewgt();

private:
    Ui::testframewgt *ui;
};

#endif // TESTFRAMEWGT_H
