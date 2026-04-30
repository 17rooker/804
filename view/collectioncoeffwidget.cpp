#include "collectioncoeffwidget.h"
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QVBoxLayout>
#include <QDebug>
#include <QRegularExpression>
#include <QCoreApplication>
#include <QDir>
#include <QSettings>

CollectionCoeffWidget::CollectionCoeffWidget(QWidget *parent) : QWidget(parent) {
    setupUI();
}

CollectionCoeffWidget::~CollectionCoeffWidget() {
}

void CollectionCoeffWidget::setupUI() {
    // --- 1. 主水平布局 ---
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(15, 15, 15, 15);
    mainLayout->setSpacing(15); // 左右两部分的间距

    // ==========================================
    // 左侧部分：标题 + 输入框矩阵
    // ==========================================
    QWidget *leftWidget = new QWidget(this);
    QGridLayout *gridLayout = new QGridLayout(leftWidget);
    gridLayout->setHorizontalSpacing(5); // 减小列间距
    gridLayout->setVerticalSpacing(4);   // 减小行间距
    gridLayout->setContentsMargins(0, 0, 0, 0);

    // 1.1 顶部标题
    QStringList headers = {"机构I", "机构II", "机构III", "机构IV"};
    QFont headerFont;
    headerFont.setBold(true);
    for (int col = 0; col < 4; ++col) {
        QLabel *headerLabel = new QLabel(headers[col]);
        headerLabel->setFont(headerFont);
        headerLabel->setAlignment(Qt::AlignCenter);
        headerLabel->setFixedHeight(20);
        gridLayout->addWidget(headerLabel, 0, col);
    }

    // 1.2 定义每一行每一列的准确数据 (严格按照截图)
    // 行索引 0-6 对应 7行数据
    QString data[7][4] = {
        // 第1行 (拉力1)
        {"x*6.12875", "x/4095*10*5.44778", "x/4095*10*5.44778", "x/4095*10*5.44778"},
        // 第2行 (拉力2)
        {"x*5.44778", "x/4095*10*5.44778", "x/4095*10*5.44778", "x/4095*10*5.44778"},
        // 第3行 (角度值)
        {"x/32767*360-269.9", "x/32767*360-122.02", "x/32767*360-244.703", "x/32767*360-257.4"},
        // 第4行 (压力1)
        {"x*0.4-0.4-0.014", "x/4095*5*0.4-0.4", "x/4095*5*0.4-0.4", "x/4095*5*0.4-0.4"},
        // 第5行 (压力2)
        {"x*0.4-0.4-0.01", "x/4095*5*0.4-0.4", "x/4095*5*0.4-0.4", "x/4095*5*0.4-0.4"},
        // 第6行 (温度1)
        {"(x/4095*5000-1447.5)/17.293", "(x/4095*5000-1443.6)/17.337", "(x/4095*5000-1443.5)/17.307", "(x/4095*5000-1446.4)/17.278"},
        // 第7行 (温度2)
        {"(x/4095*5000-1433.1)/17.359", "(x/4095*5000-1441.5)/17.307", "(x/4095*5000-1442.8)/17.311", "(x/4095*5000-1446)/17.285"}
    };

    // 1.3 生成输入框矩阵
    for (int row = 0; row < 7; ++row) {
        for (int col = 0; col < 4; ++col) {
            QLineEdit *edit = new QLineEdit();
            edit->setText(data[row][col]);
            // 样式设置
            edit->setStyleSheet("QLineEdit { border: 1px solid gray; padding: 2px; background-color: white; }");
            edit->setFixedHeight(45);
            edit->setFixedWidth(200);
            edit->setAlignment(Qt::AlignCenter);
            // 输入框放在标题下面，所以行号是 row + 1
            gridLayout->addWidget(edit, row + 1, col);
            m_edits[row][col] = edit; // 保存指针供计算函数使用
        }
    }

    // ==========================================
    // 右侧部分：单位标签垂直排列
    // ==========================================
    QWidget *rightWidget = new QWidget(this);
    QVBoxLayout *rightLayout = new QVBoxLayout(rightWidget);
    rightLayout->setContentsMargins(10, 15, 0, 0);
    rightLayout->setSpacing(4);

    QStringList labels = {
        "拉力1(kN)", "拉力2(kN)", "角度值(°)",
        "压力1(MPa)", "压力2(MPa)", "温度1(°C)", "温度2(°C)"
    };

    for (const QString &text : labels) {
        QLabel *label = new QLabel(text);
        label->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        label->setFixedHeight(45);
        rightLayout->addWidget(label);
    }
    rightLayout->addStretch();

    // ==========================================
    // 将左右两部分加入主布局
    // ==========================================
    mainLayout->addWidget(leftWidget, 0, Qt::AlignTop);
    mainLayout->addWidget(rightWidget, 0, Qt::AlignTop);
    // 加载配置文件中的公式
    QString appPath = QCoreApplication::applicationDirPath();
    QDir configDir(appPath + QDir::separator() + "config");
    QString coeffConfigPath = configDir.absoluteFilePath("coeff_config.ini");

    if (QFile::exists(coeffConfigPath)) {
        QSettings coeffSettings(coeffConfigPath, QSettings::IniFormat);
        for (int row = 0; row < COEFF_ROWS; ++row) {
            for (int col = 0; col < COEFF_COLS; ++col) {
                QString key = QString("CollectionCoeff/Row%1_Col%2").arg(row).arg(col);
                QString formula = coeffSettings.value(key).toString();
                if (!formula.isEmpty()) {
                    m_edits[row][col]->setText(formula);
                }
            }
        }
        qInfo() << "[CollectionCoeffWidget] 从配置文件加载公式成功：" << coeffConfigPath;
    } else {
        qInfo() << "[CollectionCoeffWidget] 配置文件不存在，使用默认公式：" << coeffConfigPath;
    }
}

// 核心私有函数：解析单个公式并计算结果
double CollectionCoeffWidget::parseFormula(const QString &formula, double x) {
    QString trimmedFormula = formula.trimmed();
    double finalVal = 0;

    // 情况 A: x * a  或  x*a
    QRegularExpression re1(R"((?:^|\s)x\s*\*\s*([-+]?\d*\.?\d+))");
    QRegularExpressionMatch match1 = re1.match(trimmedFormula);
    if (match1.hasMatch()) {
        double a = match1.captured(1).toDouble();
        finalVal = x * a;
        return finalVal;
    }

    // 情况 B: x / a
    QRegularExpression re2(R"((?:^|\s)x\s*/\s*([-+]?\d*\.?\d+))");
    QRegularExpressionMatch match2 = re2.match(trimmedFormula);
    if (match2.hasMatch()) {
        double a = match2.captured(1).toDouble();
        if (a == 0) { // 避免除零
            qWarning() << "Formula division by zero: " << trimmedFormula;
            return 0;
        }
        finalVal = x / a;
        return finalVal;
    }

    // 情况 C: (x / 4095*5000 - offset) / factor  (温度公式)
    if (trimmedFormula.contains("/4095*5000")) {
        // 提取 offset (减数)
        QRegularExpression reOffset(R"(\-\s*([-+]?\d*\.?\d+)[\)\s])");
        double offset = 0;
        auto matchOffset = reOffset.match(trimmedFormula);
        if (matchOffset.hasMatch()) {
            offset = matchOffset.captured(1).toDouble();
        }
        // 提取最后的除数
        QRegularExpression reFactor(R"(/\s*([-+]?\d*\.?\d+)$)");
        double factor = 1;
        auto matchFactor = reFactor.match(trimmedFormula);
        if (matchFactor.hasMatch()) {
            factor = matchFactor.captured(1).toDouble();
            if (factor == 0) { // 避免除零
                qWarning() << "Formula division by zero: " << trimmedFormula;
                return 0;
            }
        }
        finalVal = ((x / 4095 * 5000) - offset) / factor;
        return finalVal;
    }

    // 情况 D: x / a * b - c (角度/部分压力公式)
    if (trimmedFormula.contains("/")) {
        // 提取分母 a
        QRegularExpression reA(R"(x\s*/\s*(\d+))");
        // 提取乘数 b
        QRegularExpression reB(R"(/\s*\d+\s*\*\s*(\d+\.?\d*))");
        // 提取减数 c
        QRegularExpression reC(R"(\-\s*(\d+\.?\d*))");

        double a = 1;
        auto matchA = reA.match(trimmedFormula);
        if (matchA.hasMatch()) {
            a = matchA.captured(1).toDouble();
            if (a == 0) {
                qWarning() << "Formula division by zero: " << trimmedFormula;
                return 0;
            }
        }

        double b = 1;
        auto matchB = reB.match(trimmedFormula);
        if (matchB.hasMatch()) {
            b = matchB.captured(1).toDouble();
        }

        double c = 0;
        auto matchC = reC.match(trimmedFormula);
        if (matchC.hasMatch()) {
            c = matchC.captured(1).toDouble();
        }

        finalVal = (x / a * b) - c;
        return finalVal;
    }

    // 情况 E: x*a - b - c (压力公式)
    if (trimmedFormula.contains("*") && trimmedFormula.contains("-")) {
        QRegularExpression reA(R"(x\s*\*\s*([-+]?\d*\.?\d+))");
        QRegularExpression reB(R"(\*\s*[-+]?\d*\.?\d+\s*\-\s*([-+]?\d*\.?\d+))"); // 第一个减数
        QRegularExpression reC(R"(\-\s*([-+]?\d*\.?\d+)\s*\-\s*([-+]?\d*\.?\d+))"); // 第二个减数

        double a = 0;
        auto matchA = reA.match(trimmedFormula);
        if (matchA.hasMatch()) {
            a = matchA.captured(1).toDouble();
        }

        double b = 0;
        auto matchB = reB.match(trimmedFormula);
        if (matchB.hasMatch()) {
            b = matchB.captured(1).toDouble();
        }

        double c = 0;
        auto matchC = reC.match(trimmedFormula);
        if (matchC.hasMatch()) {
            // 取最后一个减数
            c = matchC.captured(matchC.lastCapturedIndex()).toDouble();
        }

        finalVal = x * a - b - c;
        return finalVal;
    }

    // 情况 F: 无匹配规则，直接返回x
    finalVal = x;
    return finalVal;
}

// 新增接口实现：获取指定行/列的公式文本
QString CollectionCoeffWidget::getFormula(int row, int col) {
    // 参数越界校验
    if (row < 0 || row >= COEFF_ROWS || col < 0 || col >= COEFF_COLS) {
        qWarning() << "Invalid row/col: row=" << row << ", col=" << col;
        return QString();
    }
    return m_edits[row][col]->text().trimmed();
}


// 新增接口实现：计算单个公式结果
double CollectionCoeffWidget::calculateSingleFormula(int row, int col, double x) {
    // 参数越界校验
    if (row < 0 || row >= COEFF_ROWS || col < 0 || col >= COEFF_COLS) {
        qWarning() << "Invalid row/col: row=" << row << ", col=" << col;
        return 0.0;
    }
    QString formula = getFormula(row, col);
    return parseFormula(formula, x);
}

// 新增接口实现：批量计算所有28个公式结果
QVector<QVector<double>> CollectionCoeffWidget::calculateAllFormulas(const double xArray[COEFF_COLS]) {
    QVector<QVector<double>> result(COEFF_ROWS, QVector<double>(COEFF_COLS, 0.0));
    for (int row = 0; row < COEFF_ROWS; ++row) {
        for (int col = 0; col < COEFF_COLS; ++col) {
            result[row][col] = calculateSingleFormula(row, col, xArray[col]);
        }
    }
    return result;
}

// 重构原有接口：复用parseFormula函数
OutputData CollectionCoeffWidget::calculateValues(const InputData data[COEFF_COLS]) {
    OutputData result = {0};

    // 遍历每一行
    for (int row = 0; row < COEFF_ROWS; ++row) {
        for (int col = 0; col < COEFF_COLS; ++col) {
            QString formula = m_edits[row][col]->text().trimmed();
            double xValue = 0;

            // 1. 确定 X 的来源 (根据行号)
            switch (row) {
                case 0: case 1: xValue = data[col].force; break; // 拉力行用 force
                case 2: xValue = data[col].angle; break;         // 角度行用 angle
                default: xValue = data[col].temp; break;         // 其他行(压力/温度)用 temp
            }

            // 复用核心解析函数
            double finalVal = parseFormula(formula, xValue);

            // 2. 存储结果 (临时方案：只取机构1(col==0)的计算结果)
            if (col == 0) {
                switch (row) {
                    case 0: result.tension1 = finalVal; break;
                    case 1: result.tension2 = finalVal; break;
                    case 2: result.angleVal = finalVal; break;
                    case 3: result.pressure1 = finalVal; break;
                    case 4: result.pressure2 = finalVal; break;
                    case 5: result.temp1 = finalVal; break;
                    case 6: result.temp2 = finalVal; break;
                }
            }
        }
    }
    return result;
}
