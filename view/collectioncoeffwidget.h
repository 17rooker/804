#ifndef COLLECTIONCOEFFWIDGET_H
#define COLLECTIONCOEFFWIDGET_H

#include <QWidget>
#include <QVector>

#define COEFF_ROWS 7
#define COEFF_COLS 4

// 定义输入和输出的数据结构
struct InputData {
    double force;   // 拉力
    double temp;    // 温度
    double angle;   // 角度
};

struct OutputData {
    double tension1; // 拉力1
    double tension2; // 拉力2
    double angleVal; // 角度值
    double pressure1;// 压力1
    double pressure2;// 压力2
    double temp1;    // 温度1
    double temp2;    // 温度2
};

class QLineEdit;
class CollectionCoeffWidget : public QWidget {
    Q_OBJECT
public:
    explicit CollectionCoeffWidget(QWidget *parent = nullptr);
    ~CollectionCoeffWidget();

    // 原有接口：传入机构1-4的原始数据，返回计算后的结果
    OutputData calculateValues(const InputData data[COEFF_COLS]);

    // 新增接口1：获取指定行(0-6)、列(0-3)的公式文本
    QString getFormula(int row, int col);

    // 新增接口2：计算指定行(0-6)、列(0-3)的公式，输入x值返回计算结果
    double calculateSingleFormula(int row, int col, double x);

    // 新增接口3：批量计算所有28个公式结果（7行4列）
    // xArray：4个机构对应的x值数组（列0-3对应机构1-4）
    QVector<QVector<double>> calculateAllFormulas(const double xArray[COEFF_COLS]);

private:
    void setupUI();
    QLineEdit* m_edits[COEFF_ROWS][COEFF_COLS];

    // 新增私有函数：核心公式解析逻辑（复用）
    double parseFormula(const QString &formula, double x);
};

#endif // COLLECTIONCOEFFWIDGET_H
