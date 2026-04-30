#ifndef WAVECHART_H
#define WAVECHART_H

#include <QWidget>
#include "src/chart/chartwidget.h"
#include "src/Common/StructDefine.h"
namespace Ui {
class wavechart;
}

class wavechart : public QWidget
{
    Q_OBJECT

public:
    explicit wavechart(QWidget *parent = nullptr);
    ~wavechart();
    void setParam(const STParamInfo& param);
private:
    Ui::wavechart *ui;
    STParamInfo m_param;
};

#endif // WAVECHART_H
