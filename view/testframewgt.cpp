#include "testframewgt.h"
#include "ui_testframewgt.h"

testframewgt::testframewgt(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::testframewgt)
{
    ui->setupUi(this);
}

testframewgt::~testframewgt()
{
    delete ui;
}
