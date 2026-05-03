#include "controllerpanel.h"
#include <QHBoxLayout>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QFrame>
#include <QTimer>
#include <QRandomGenerator>
#include <QtEndian>
#include <QScriptEngine>
#include <QDir>
#include <QCoreApplication>
#include <QSettings>
#include "src/CustomMessage/DataInteractionManager.h"
#include "src/Common/LoggerManager.h"
#include "src/StyleEventFilter.h"
#include "InitiativeMsgEvent.h"
double FrameWorker::calculateCoeff(double x, int row, int col)
{
    // 1. 构建配置文件路径：运行目录/config/coeff_config.ini
    QString configPath = QDir::cleanPath(
        QCoreApplication::applicationDirPath() + QDir::separator()
        + "config" + QDir::separator() + "coeff_config.ini"
    );

    // 检查配置文件是否存在
    QFile configFile(configPath);
    if (!configFile.exists()) {
        qCritical() << "Config file not found: " << configPath;
        return NAN;
    }

    // 2. 读取INI文件中的公式
    QSettings settings(configPath, QSettings::IniFormat);
    settings.beginGroup("CollectionCoeff"); // 对应INI中的[CollectionCoeff]节
    QString key = QString("Row%1_Col%2").arg(row).arg(col);
    QString formula = settings.value(key).toString();
    settings.endGroup();

    if (formula.isEmpty()) {
        qCritical() << "Formula key not found: " << key;
        return NAN;
    }

    // 3. 替换公式中的x为传入的数值
    QString expr = formula.replace("x", QString::number(x));

    // 4. 解析并计算表达式
    QScriptEngine engine;
    QScriptValue result = engine.evaluate(expr);
    if (engine.hasUncaughtException()) {
        qCritical() << "Formula calculate error: " << engine.uncaughtException().toString()
                    << " Formula: " << formula << " Expr: " << expr;
        return NAN;
    }

    if (!result.isNumber()) {
        qCritical() << "Formula result is not a number: " << formula << " Expr: " << expr;
        return NAN;
    }

    return result.toNumber();
}


bool FrameWorker::isParamOutOfRange(double paramValue, ParamType type)
{
    double lower = 0.0, upper = 0.0;

    // 读取配置文件中的上下限
    if (!readParamRange(type, lower, upper)) {
        qCritical() << "Read param range failed!";
        // 读取失败时默认判定为超出范围（可根据业务调整）
        return true;
    }

    // 判断是否超出范围：小于下限 或 大于上限 → 返回true
    if (paramValue < lower || paramValue > upper) {
        qDebug() << QString("Param out of range! Value: %1, Lower: %2, Upper: %3")
                    .arg(paramValue).arg(lower).arg(upper);
        return true;
    }

    // 在范围内 → 返回false
    return false;
}

bool FrameWorker::readParamRange(ParamType type, double &lower, double &upper)
{
    QString appDir = QCoreApplication::applicationDirPath();
    QString configFilePath = QString("%1/config/criteria_config.ini").arg(appDir);
    // 检查配置文件是否存在
    if (!QFile::exists(configFilePath)) {
        qCritical() << "Config file not exist! Path:" << configFilePath;
        return false;
    }

    // 初始化QSettings读取INI文件（IniFormat跨平台兼容Linux/Windows）
    QSettings settings(configFilePath, QSettings::IniFormat);
    // 设置INI文件的节名称（对应配置文件中的[CollectionCriteria]）
    settings.beginGroup("CollectionCriteria");

    // 根据参数类型读取对应的上下限键值
    switch (type) {
    case ParamType::Tension:
        lower = settings.value("Tension_Lower", -3.0).toDouble();  // 缺省值与配置文件一致
        upper = settings.value("Tension_Upper", 60.0).toDouble();
        break;
    case ParamType::Angle:
        lower = settings.value("Angle_Lower", 2.0).toDouble();
        upper = settings.value("Angle_Upper", 3.0).toDouble();
        break;
    case ParamType::Pressure:
        lower = settings.value("Pressure_Lower", -0.6).toDouble();
        upper = settings.value("Pressure_Upper", 1.3).toDouble();
        break;
    default:
        qCritical() << "Unsupported param type!";
        settings.endGroup();
        return false;
    }

    settings.endGroup();

    // 验证上下限逻辑（防止配置文件中下限>上限的错误）
    if (lower > upper) {
        qCritical() << "Invalid range! Lower > Upper. Type:" << static_cast<int>(type);
        return false;
    }

    return true;
}
void FrameWorker::processData(const QByteArray &data)
{
    QByteArray buf = data;
    const int MAX_FRAMES = 100;
    const int EMIT_INTERVAL = 5;  // 每处理5帧才emit一次，避免主线程卡死
    int framesProcessed = 0;

    QMap<QString, bool> lastLed;
    QMap<QString, QString> lastEdit;

    while (buf.size() >= 8 && framesProcessed < MAX_FRAMES) {
        // 1. 校验帧头
        if (buf.left(4) != QByteArray::fromHex("FDB18540")) {
            buf.remove(0, 1);
            continue;
        }

        // 2. 解析帧长（大端序）
        QByteArray lenBytes = buf.mid(4, 4);
        qint32 frameLen = (static_cast<quint8>(lenBytes[0]) << 24) |
                          (static_cast<quint8>(lenBytes[1]) << 16) |
                          (static_cast<quint8>(lenBytes[2]) << 8) |
                           static_cast<quint8>(lenBytes[3]);

        // 3. 校验帧长
        if (frameLen <= 85 || frameLen > 1024 * 1024) {
            buf.remove(0, 4);
            continue;
        }

        // 4. 检查是否包含完整帧
        if (buf.size() < frameLen)
            break;

        // 5. 提取完整帧
        QByteArray frameData = buf.left(frameLen);
        buf.remove(0, frameLen);

        // 6. 解析
        FrameDataAnalysis analy;
        STPackage pack;
        pack.channelId = "serial_E";
        pack.eDataType = (frameLen == 428)
            ? EDataType::E_ServerControl_BrakeRelease_A5
            : EDataType::E_ServerControl_BrakeRelease_A6;
        pack.channelType = EChannelType::Serial;
        pack.baDataRecv = frameData;

        STParamInfo param{};
        analy.parseData(pack, param);

        QMap<QString, bool> ledStates;
        QMap<QString, QString> editValues;
        if(frameLen==428)
            paramProcess(param, ledStates, editValues);
        else
            paramProcess_A6(param, ledStates, editValues);
        
        // CRC16/XMODEM 校验（bytes 5~18+N，存于帧尾前2字节，大端序）
        int totalLen = frameData.size();
        if (totalLen >= 10) {
            quint16 calcCrc = FrameDataAnalysis::crc16Xmodem(frameData, 4, totalLen - 10);
            quint16 storedCrc = (static_cast<quint8>(frameData[totalLen - 6]) << 8) |
                                 static_cast<quint8>(frameData[totalLen - 5]);
            editValues["校验结果"] = (calcCrc == storedCrc) ? QStringLiteral("校验正确") : QStringLiteral("校验错误");
        } else {
            editValues["校验结果"] = QStringLiteral("帧长不足");
        }
        // 保存当前帧，供循环结束时确保最后一帧被emit
        lastLed = ledStates;
        lastEdit = editValues;

        // 节拍：每EMIT_INTERVAL帧emit一次，减轻主线程压力
        if (framesProcessed % EMIT_INTERVAL == 0)
            emit dataProcessed(lastLed, lastEdit);

        ++framesProcessed;
    }

    // 确保最后一帧也被更新（若未被间隔emit覆盖）
    if (framesProcessed > 0 && framesProcessed % EMIT_INTERVAL != 0)
        emit dataProcessed(lastLed, lastEdit);
}

void FrameWorker::processData_sel( STParamInfo &param)
{
    QMap<QString, bool> ledStates;
    QMap<QString, QString> editValues;
    paramProcess(param,ledStates,editValues);
    emit dataProcessed(ledStates, editValues);
}

void FrameWorker::paramProcess(STParamInfo &m_param, QMap<QString, bool> &m_ledStates,  QMap<QString, QString> &m_editValues)
{
    for (auto mapIt = m_param.mapParams.cbegin(); mapIt != m_param.mapParams.cend(); ++mapIt)
    {
        const STParamItem& item = mapIt.value();

        // 预留参数更新逻辑
        if(mapIt.key()=="InitiatorProtectionStatus1")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }
            // 2. 解析位域：B7-B4为常值0，B3-B0为火保继电器K4~K1状态（0断开，1闭合）
            quint8 b7_b4 = (rawValue >> 4) & 0x0F; // 高4位（常值0）
            quint8 b3_b0 = rawValue & 0x0F;        // 低4位（K4~K1状态）

            // 校验高4位是否为0（可选，用于调试）
            if (b7_b4 != 0) {
                //                qWarning() << "[InitiatorProtectionStatus1] B7-B4 非预期值：" << QString::number(b7_b4, 16);
            }

            // 3. 拆分低4位到对应继电器状态（B0=K1, B1=K2, B2=K3, B3=K4）
            bool k1State = (b3_b0 & 0x01) != 0; // B0 → 火保K1（1=闭合，0=断开）
            bool k2State = (b3_b0 & 0x02) != 0; // B1 → 火保K2
            bool k3State = (b3_b0 & 0x04) != 0; // B2 → 火保K3
            bool k4State = (b3_b0 & 0x08) != 0; // B3 → 火保K4

            // 4. 存入m_ledStates（key与setupUI中创建的LED名称一致）
            m_ledStates["火保K1"] = k1State;
            m_ledStates["火保K2"] = k2State;
            m_ledStates["火保K3"] = k3State;
            m_ledStates["火保K4"] = k4State;
        }
        else if(mapIt.key()=="FrameType")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }

            QString frameType;
            switch (rawValue) {
            case 0x01:
                frameType = "终端";
                break;
            case 0x02:
                frameType = "串口服务器";
                break;
            default:
                frameType = "未知帧类型";
                break;
            }

            //  键名和setupUI完全匹配：数值+文字+校验
            m_editValues["帧类型"] = QString::number(rawValue, 16).toUpper(); // 十六进制数值
            m_editValues["帧类型_校验"] = frameType; // 校验列赋值（可根据你的逻辑修改）
            m_editValues["时序_帧类型"] = QString::number(rawValue); // 文字描述
        }
        else if(mapIt.key()=="FPGAID")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }

            QString fpgaId, num;
            switch (rawValue) {
            case 0xAA:
                fpgaId = "FPGA1";
                num = "0xAA"; //  修复：直接赋值字符串
                break;
            case 0xBB:
                fpgaId = "FPGA2";
                num = "0xBB";
                break;
            case 0xCC:
                fpgaId = "FPGA3";
                num = "0xCC";
                break;
            default:
                fpgaId = "未知FPGA";
                num = "0x"+QString::number(rawValue, 16).toUpper();
                break;
            }

            //  键名匹配
            m_editValues["FPGA_ID"] = num;       // 十六进制数值
            m_editValues["FPGA_ID_校验"] = fpgaId;// 校验列赋值
            m_editValues["FPGA_ID_文本"] = fpgaId;// 文字描述
        }
        else if(mapIt.key()=="WorkMode")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }

            QString workMode, NUM;
            switch (rawValue) {
            case 0xAA:
                workMode = "测试模式";
                NUM = "0xAA"; //
                break;
            case 0xBB:
                workMode = "手动模式";
                NUM = "0xBB";
                break;
            case 0xCC:
                workMode = "自动模式";
                NUM = "0xCC";
                break;
            default:
                workMode = "未知模式";
                NUM = "0x"+QString::number(rawValue, 16).toUpper();
                break;
            }

            //  键名匹配
            m_editValues["工作模式"] = NUM;         // 数值
            m_editValues["工作模式_校验"] = workMode;  // 校验列
            m_editValues["工作模式_文本"] = workMode;// 文字
        }
        else if(mapIt.key()=="FrameLength")
        {
            quint32 rawValue = 0;
            if (item.varParaValue.canConvert<quint32>()) {
                rawValue = item.varParaValue.value<quint32>();
            }

            // 存入帧长状态
            m_editValues["帧长"] =  QString::number(rawValue);
            m_editValues["时序_帧长"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="FrameCount")
        {
            qint32 rawValue = 0;
            if (item.varParaValue.canConvert<qint32>()) {
                rawValue = item.varParaValue.value<qint32>();
            }

            // 存入帧长状态
            m_editValues["帧计数"] =  QString::number(rawValue);
            m_editValues["帧计数1"] =  QString::number(rawValue);
            m_editValues["帧计数2"] =  QString::number(rawValue);
            m_editValues["帧计数3"] =  QString::number(rawValue);
            m_editValues["帧计数4"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="TimeFlag")
        {
            // 4字节时间标志，上电计数，单位1ms
            quint32 rawValue = 0;
            if (item.varParaValue.canConvert<quint32>()) {
                rawValue = item.varParaValue.value<quint32>();
            }

            // 存入状态字典
            m_editValues["时间标志"] = QString::number( rawValue);
        }
        else if(mapIt.key()=="InitiatorProtectionStatus2")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }
            // 解析位域：B7-B4为常值0，B3-B0为火工品保护状态4~1路
            quint8 b7_b4 = (rawValue >> 4) & 0x0F; // 高4位（常值0）
            quint8 b3_b0 = rawValue & 0x0F;        // 低4位（保护状态）

            // 校验高4位是否为0（可选调试）
            if (b7_b4 != 0) {
                //                qWarning() << "[InitiatorProtectionStatus2] B7-B4 非预期值：" << QString::number(b7_b4, 16);
            }

            // 拆分低4位：B0=第1路，B1=第2路，B2=第3路，B3=第4路（1=保护，0=未保护）
            bool protect1State = (b3_b0 & 0x01) != 0;  // B0 → 火工品保护第1路
            bool protect2State = (b3_b0 & 0x02) != 0;  // B1 → 火工品保护第2路
            bool protect3State = (b3_b0 & 0x04) != 0;  // B2 → 火工品保护第3路
            bool protect4State = (b3_b0 & 0x08) != 0;  // B3 → 火工品保护第4路

            // 存入状态字典，键名与UI控件对应
            m_ledStates["火保1"] = protect1State;
            m_ledStates["火保2"] = protect2State;
            m_ledStates["火保3"] = protect3State;
            m_ledStates["火保4"] = protect4State;
        }
        else if(mapIt.key()=="UncontrolStatus")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }

            // 1. 解析单个位域 + 分组位域
            quint8 b7 = (rawValue >> 7) & 0x01; // B7：常值0
            bool relayTestState = (rawValue >> 6) & 0x01; // B6：电爆电路测试继电器（0断开，1闭合）
            bool jkTState = (rawValue >> 5) & 0x01;       // B5：解控状态JKt（1解控，0非解控）
            //            if(!jkTState)
            m_ledStates["非解控JKt"] = jkTState;
            bool jkBackTState = (rawValue >> 4) & 0x01;   // B4：解控状态JKbackt（1解控，0非解控）
            //            if(!jkBackTState)
            m_ledStates["非解控JKb"] = jkBackTState;
            quint8 b3_b0 = rawValue & 0x0F;               // B3~B0：解控继电器J4~J1状态

            // 校验B7是否为0（可选调试）
            if (b7 != 0) {
                //                qWarning() << "[UncontrolStatus] B7 非预期值：" << QString::number(b7, 16);
            }

            // 2. 拆分解控继电器J4~J1（B3=J4, B2=J3, B1=J2, B0=J1 | 0断开，1闭合）
            bool j1State = (b3_b0 & 0x01) != 0; // B0 → 解控继电器J1
            bool j2State = (b3_b0 & 0x02) != 0; // B1 → 解控继电器J2
            bool j3State = (b3_b0 & 0x04) != 0; // B2 → 解控继电器J3
            bool j4State = (b3_b0 & 0x08) != 0; // B3 → 解控继电器J4

            m_ledStates["解控K1"] = j1State;
            m_ledStates["解控K2"] = j2State;
            m_ledStates["解控K3"] = j3State;
            m_ledStates["解控K4"] = j4State;
        }
        else if (mapIt.key()=="SolenoidValveStatus1")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }

            // 1. 逐位解析状态（严格匹配需求定义）
            bool signalForward1   = (rawValue >> 7) & 0x01;  // B7：转发发控允许释放信号1（0未转发，1已转发）
            bool xf01a1Status1    = (rawValue >> 6) & 0x01;  // B6：接插件XF01A1状态监测点1(0未连接，1连接好)
            quint8 b5 = (rawValue >> 5) & 0x01;              // B5：固定为0
            bool dcf1Y3_State     = (rawValue >> 4) & 0x01;  // B4：机构Ⅰ供气DCF1-Y3状态（1闭合，0断开）
            quint8 b3 = (rawValue >> 3) & 0x01;              // B3：固定为0
            bool dcf1Y2_State     = (rawValue >> 2) & 0x01;  // B2：机构Ⅰ锁定DCF1-Y2状态（1闭合，0断开）
            bool dcf1Y1_2_State   = (rawValue >> 1) & 0x01;  // B1：机构Ⅰ释放(备)DCF1-Y1-2状态（1闭合，0断开）
            bool dcf1Y1_1_State   = rawValue & 0x01;         // B0：机构Ⅰ释放(主)DCF1-Y1-1状态（1闭合，0断开）

            // 2. 校验固定位B5、B3是否为0（可选调试，异常时打印警告）
            if (b5 != 0 || b3 != 0) {
                //                qWarning() << "[SolenoidValveStatus1] B5/B3 非预期值，B5:" << b5 << "B3:" << b3;
            }

            // 3. 所有有效状态存入m_ledStates（键名与UI控件严格对应）
            m_ledStates["转发释放好1"] = signalForward1;
            m_ledStates["机构1-控制电缆连接情况"] = xf01a1Status1;
            m_ledStates["机构1供气"] = dcf1Y3_State;
            m_ledStates["机构1锁定"] = dcf1Y2_State;
            m_ledStates["机构1-锁定到位"] = dcf1Y2_State;
            m_ledStates["机构1释放备"] = dcf1Y1_2_State;
            m_ledStates["机构1释放主"] = dcf1Y1_1_State;
        }
        else if (mapIt.key()=="SolenoidValveStatus2")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }

            // 逐位解析状态（严格匹配需求定义）
            bool signalForward2   = (rawValue >> 7) & 0x01;  // B7：转发发控允许释放信号2（0未转发，1已转发）
            bool xf01a1Status2    = (rawValue >> 6) & 0x01;  // B6：接插件XF01A1状态监测点2(0未连接，1连接好)
            quint8 b5 = (rawValue >> 5) & 0x01;              // B5：固定为0
            bool dcf2Y3_State     = (rawValue >> 4) & 0x01;  // B4：机构Ⅱ供气DCF2-Y3状态（1闭合，0断开）
            quint8 b3 = (rawValue >> 3) & 0x01;              // B3：固定为0
            bool dcf2Y2_State     = (rawValue >> 2) & 0x01;  // B2：机构Ⅱ锁定DCF2-Y2状态（1闭合，0断开）
            bool dcf2Y1_2_State   = (rawValue >> 1) & 0x01;  // B1：机构Ⅱ释放(备)DCF2-Y1-2状态（1闭合，0断开）
            bool dcf2Y1_1_State   = rawValue & 0x01;         // B0：机构Ⅱ释放(主)DCF2-Y1-1状态（1闭合，0断开）

            // 校验固定位B5、B3是否为0（调试用）
            if (b5 != 0 || b3 != 0) {
                //        qWarning() << "[SolenoidValveStatus2] B5/B3 非预期值";
            }

            // 状态存入m_ledStates
            m_ledStates["转发释放好2"] = signalForward2;
            m_ledStates["机构2-控制电缆连接情况"] = xf01a1Status2;
            m_ledStates["机构2供气"] = dcf2Y3_State;
            m_ledStates["机构2锁定"] = dcf2Y2_State;
            m_ledStates["机构2-锁定到位"] = dcf2Y2_State;
            m_ledStates["机构2释放备"] = dcf2Y1_2_State;
            m_ledStates["机构2释放主"] = dcf2Y1_1_State;
        }
        else if (mapIt.key()=="SolenoidValveStatus3")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }

            // 逐位解析状态
            quint8 b7 = (rawValue >> 7) & 0x01;              // B7：常值0
            bool xf01a2Status1    = (rawValue >> 6) & 0x01;  // B6：接插件XF01A2状态监测点1(0未连接，1连接好)
            quint8 b5 = (rawValue >> 5) & 0x01;              // B5：固定为0
            bool dcf3Y3_State     = (rawValue >> 4) & 0x01;  // B4：机构Ⅲ供气DCF3-Y3状态（1闭合，0断开）
            quint8 b3 = (rawValue >> 3) & 0x01;              // B3：固定为0
            bool dcf3Y2_State     = (rawValue >> 2) & 0x01;  // B2：机构Ⅲ锁定DCF3-Y2状态（1闭合，0断开）
            bool dcf3Y1_2_State   = (rawValue >> 1) & 0x01;  // B1：机构Ⅲ释放(备)DCF3-Y1-2状态（1闭合，0断开）
            bool dcf3Y1_1_State   = rawValue & 0x01;         // B0：机构Ⅲ释放(主)DCF3-Y1-1状态（1闭合，0断开）

            // 校验固定位
            if (b7 != 0 || b5 != 0 || b3 != 0) {
                //        qWarning() << "[SolenoidValveStatus3] 固定位非预期值";
            }

            // 存入状态
            m_ledStates["机构3-控制电缆连接情况"] = xf01a2Status1;
            m_ledStates["机构3供气"] = dcf3Y3_State;
            m_ledStates["机构3锁定"] = dcf3Y2_State;
            m_ledStates["机构3-锁定到位"] = dcf3Y2_State;
            m_ledStates["机构3释放备"] = dcf3Y1_2_State;
            m_ledStates["机构3释放主"] = dcf3Y1_1_State;
        }
        else if(mapIt.key()=="SolenoidValveStatus4")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }

            // 逐位解析状态
            quint8 b7 = (rawValue >> 7) & 0x01;              // B7：常值0
            bool xf01a2Status2    = (rawValue >> 6) & 0x01;  // B6：接插件XF01A2状态监测点2(0未连接，1连接好)
            quint8 b5 = (rawValue >> 5) & 0x01;              // B5：固定为0
            bool dcf4Y3_State     = (rawValue >> 4) & 0x01;  // B4：机构Ⅳ供气DCF4-Y3状态（1闭合，0断开）
            quint8 b3 = (rawValue >> 3) & 0x01;              // B3：固定为0
            bool dcf4Y2_State     = (rawValue >> 2) & 0x01;  // B2：机构Ⅳ锁定DCF4-Y2状态（1闭合，0断开）
            bool dcf4Y1_2_State   = (rawValue >> 1) & 0x01;  // B1：机构Ⅳ释放(备)DCF4-Y1-2状态（1闭合，0断开）
            bool dcf4Y1_1_State   = rawValue & 0x01;         // B0：机构Ⅳ释放(主)DCF4-Y1-1状态（1闭合，0断开）

            // 校验固定位
            if (b7 != 0 || b5 != 0 || b3 != 0) {
                //        qWarning() << "[SolenoidValveStatus4] 固定位非预期值";
            }

            // 状态存入m_ledStates
            m_ledStates["机构4-控制电缆连接情况"] = xf01a2Status2;
            m_ledStates["机构4供气"] = dcf4Y3_State;
            m_ledStates["机构4锁定"] = dcf4Y2_State;
            m_ledStates["机构4-锁定到位"] = dcf4Y2_State;
            m_ledStates["机构4释放备"] = dcf4Y1_2_State;
            m_ledStates["机构4释放主"] = dcf4Y1_1_State;
        }
        else if(mapIt.key()=="InitiatorDetonateRelayStatus")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }

            // 解析位域：B7~B4 电爆电路状态4~1路，B3~B0 火工品引爆继电器状态4~1路
//            quint8 circuitStatus = (rawValue >> 4) & 0x0F;  // 高4位：电爆电路状态
            quint8 relayStatus = rawValue & 0x0F;           // 低4位：引爆继电器状态

            // 拆分电爆电路第4~1路（B7=第4路，B6=第3路，B5=第2路，B4=第1路）
//            bool circuit1 = (circuitStatus & 0x01) != 0;
//            bool circuit2 = (circuitStatus & 0x02) != 0;
//            bool circuit3 = (circuitStatus & 0x04) != 0;
//            bool circuit4 = (circuitStatus & 0x08) != 0;

            // 拆分解爆继电器第4~1路（B3=第4路，B2=第3路，B1=第2路，B0=第1路）
            bool detonateRelay1 = (relayStatus & 0x01) != 0;
            bool detonateRelay2 = (relayStatus & 0x02) != 0;
            bool detonateRelay3 = (relayStatus & 0x04) != 0;
            bool detonateRelay4 = (relayStatus & 0x08) != 0;

            // 存入状态字典
            //            m_ledStates["电爆电路第1路"] = circuit1;
            //            m_ledStates["电爆电路第2路"] = circuit2;
            //            m_ledStates["电爆电路第3路"] = circuit3;
            //            m_ledStates["电爆电路第4路"] = circuit4;
            m_ledStates["火引爆1"] = detonateRelay1;
            m_ledStates["火引爆2"] = detonateRelay2;
            m_ledStates["火引爆3"] = detonateRelay3;
            m_ledStates["火引爆4"] = detonateRelay4;
        }
        else if(mapIt.key()=="ReleasePermitAndPowerStatus")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }

            // 逐位解析状态
            bool switchLockStatus = (rawValue >> 7) & 0x01;        // B7：开关锁状态（0关锁，1开锁）
            bool holdReleaseReady = (rawValue >> 6) & 0x01;        // B6：牵制释放装备好（0未准备好，1准备好）
            bool vk5Status = (rawValue >> 5) & 0x01;               // B5：5VK状态指示(0关闭，1打开)
            quint8 b4 = (rawValue >> 4) & 0x01;                   // B4：固定为0
            bool xf01dConnectStatus = (rawValue >> 3) & 0x01;      // B3：XF01D电缆连接指示(0未连接,1连接好)
            bool xf01eConnectStatus = (rawValue >> 2) & 0x01;      // B2：XF01E电缆连接指示(0未连接,1连接好)
            bool testReleasePermit = (rawValue >> 1) & 0x01;       // B1：测发控允许释放状态(1允许，0禁止)
            bool engineReleasePermit = rawValue & 0x01;            // B0：发动机控制器允许释放状态(1允许，0禁止)

            // 校验固定位B4
            if (b4 != 0) {
                //        qWarning() << "[ReleasePermitAndPowerStatus] B4 非预期值";
            }

            // 状态存入m_ledStates
            m_ledStates["开锁"] = switchLockStatus;
            m_ledStates["牵制释放好"] = holdReleaseReady;
            m_ledStates["5VK打开"] = vk5Status;
            m_ledStates["转释好连接(XF01D)"] = xf01dConnectStatus;
            m_ledStates["允辉连接(XF01E)"] = xf01eConnectStatus;
            m_ledStates["地面允释"] = testReleasePermit;
            m_ledStates["箭上允释"] = engineReleasePermit;
        }
        else if(mapIt.key()=="DigitalPowerVoltage")
        {
            // 2字节采集值，高字节在前，低字节在后
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }

            // 终端处理：实际电压 = 原始值 * 0.75656V
            double digitalPowerVoltage = rawValue * 0.75656;

            // 将计算后的电压值存入状态字典
            m_editValues["数字供电"] =  QString::number(digitalPowerVoltage, 'f', 2);
        }
        else if(mapIt.key()=="DrivePowerVoltage1")
        {
            // 2字节采集值，高字节在前，低字节在后
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }

            // 终端处理：实际电压 = 原始值 * 0.75656V
            double drivePowerVoltage1 = rawValue * 0.75656;

            // 将计算后的电压值存入状态字典
            m_editValues["驱动供电1"] =  QString::number(drivePowerVoltage1, 'f', 2);
        }
        else if(mapIt.key()=="DrivePowerVoltage2")
        {
            // 2字节采集值，高字节在前，低字节在后
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }

            // 终端处理：实际电压 = 原始值 * 0.75656V
            double drivePowerVoltage1 = rawValue * 0.75656;

            // 将计算后的电压值存入状态字典
            m_editValues["驱动供电2"] =  QString::number(drivePowerVoltage1, 'f', 2);
        }
        else if(mapIt.key()=="Digital5V1")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }

            // 每bit为1则累加0.0293，计算最终电压值
            double digital5V1 = rawValue*0.0293;

            // 存入状态字典
            m_editValues["5V1"] = QString::number(digital5V1, 'f', 3);
        }
        else if(mapIt.key()=="Digital5V2")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }

            // 每bit为1则累加0.0293，计算最终电压值
            double digital5V1 = rawValue*0.0293;

            // 存入状态字典
            m_editValues["5V2"] = QString::number(digital5V1, 'f', 2);
        }
        else if(mapIt.key()=="Digital5V3")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }

            // 每bit为1则累加0.0293，计算最终电压值
            double digital5V1 = rawValue*0.0293;

            // 存入状态字典
            m_editValues["5V3"] = QString::number(digital5V1, 'f', 2);
        }
        else if(mapIt.key()=="DigitalBoardTemperature")
        {
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }
            // 存入状态字典
            m_editValues["数字板温度"] = QString::number(rawValue, 'f', 2);
        }
        else if(mapIt.key()=="InitiatorDetonateTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 终端处理：实际电压 = 原始值 * 0.75656V
            double drivePowerVoltage1 = rawValue ;

            // 将计算后的电压值存入状态字典
            m_editValues["火引爆装订时间"] =  QString::number(drivePowerVoltage1, 'f', 0);
        }
        else if(mapIt.key()=="InitiatorDetonateRelayCloseTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 终端处理：实际电压 = 原始值 * 0.75656V
            double drivePowerVoltage1 = rawValue ;

            // 将计算后的电压值存入状态字典
            m_editValues["继电器关闭时间"] =  QString::number(drivePowerVoltage1, 'f', 0);
        }
        else if(mapIt.key()=="Mechanism1_2ReleaseStatus")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }

            // 逐位解析状态（高有效）
            bool mechanism2ReleaseOk = (rawValue >> 7) & 0x01;  // B7：机构2释放好（3取1）
            bool mechanism1ReleaseOk = (rawValue >> 6) & 0x01;  // B6：机构1释放好（3取1）
            bool sfh1_7_3_2 = (rawValue >> 5) & 0x01;           // B5：SFH1_7.3_2（高有效）
            bool sfh1_7_2_2 = (rawValue >> 4) & 0x01;           // B4：SFH1_7.2_2（高有效）
            bool sfh1_7_1_2 = (rawValue >> 3) & 0x01;           // B3：SFH1_7.1_2（高有效）
            bool sfh1_7_3_1 = (rawValue >> 2) & 0x01;           // B2：SFH1_7.3_1（高有效）
            bool sfh1_7_2_1 = (rawValue >> 1) & 0x01;           // B1：SFH1_7.2_1（高有效）
            bool sfh1_7_1_1 = rawValue & 0x01;                  // B0：SFH1_7.1_1（高有效）

            // 状态存入m_ledStates
            m_ledStates["机构2-释放好"] = mechanism2ReleaseOk;
            m_ledStates["机构1-释放好"] = mechanism1ReleaseOk;
            m_ledStates["机构1-释放到位"] = mechanism1ReleaseOk;
            m_ledStates["机构2-释放到位"] = mechanism2ReleaseOk;
            m_ledStates["SFH2_7.3"] = sfh1_7_3_2;
            m_ledStates["SFH2_7.2"] = sfh1_7_2_2;
            m_ledStates["SFH2_7.1"] = sfh1_7_1_2;
            m_ledStates["SFH1_7.3_1"] = sfh1_7_3_1;
            m_ledStates["SFH1_7.2_1"] = sfh1_7_2_1;
            m_ledStates["SFH1_7.1_1"] = sfh1_7_1_1;
        }
        else if(mapIt.key()=="Mechanism3_4ReleaseStatus")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }

            // 逐位解析状态（高有效）
            bool mechanism4ReleaseOk = (rawValue >> 7) & 0x01;  // B7：机构4释放好（3取1）
            bool mechanism3ReleaseOk = (rawValue >> 6) & 0x01;  // B6：机构3释放好（3取1）
            bool sfh1_7_3_4 = (rawValue >> 5) & 0x01;           // B5：SFH1_7.3_4（高有效）
            bool sfh1_7_2_4 = (rawValue >> 4) & 0x01;           // B4：SFH1_7.2_4（高有效）
            bool sfh1_7_1_4 = (rawValue >> 3) & 0x01;           // B3：SFH1_7.1_4（高有效）
            bool sfh1_7_3_3 = (rawValue >> 2) & 0x01;           // B2：SFH1_7.3_3（高有效）
            bool sfh1_7_2_3 = (rawValue >> 1) & 0x01;           // B1：SFH1_7.2_3（高有效）
            bool sfh1_7_1_3 = rawValue & 0x01;                  // B0：SFH1_7.1_3（高有效）

            // 状态存入m_ledStates
            m_ledStates["机构4-释放好"] = mechanism4ReleaseOk;
            m_ledStates["机构3-释放好"] = mechanism3ReleaseOk;
            m_ledStates["机构3-释放到位"] = mechanism3ReleaseOk;
            m_ledStates["机构4-释放到位"] = mechanism4ReleaseOk;
            m_ledStates["SFH4_7.3"] = sfh1_7_3_4;
            m_ledStates["SFH4_7.2"] = sfh1_7_2_4;
            m_ledStates["SFH4_7.1"] = sfh1_7_1_4;
            m_ledStates["SFH3_7.3"] = sfh1_7_3_3;
            m_ledStates["SFH3_7.2"] = sfh1_7_2_3;
            m_ledStates["SFH3_7.1"] = sfh1_7_1_3;
        }
        else if(mapIt.key()=="CommandCodeHigh")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>())
                rawValue = item.varParaValue.value<quint8>();
            m_editValues["指令码高8位"] = QString::number(rawValue, 16).toUpper();
        }
        else if(mapIt.key()=="CommandCodeLow")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>())
                rawValue = item.varParaValue.value<quint8>();
            m_editValues["指令码低8位"] = QString::number(rawValue, 16).toUpper();
        }
        else if(mapIt.key()=="CommandOperateReq")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>())
                rawValue = item.varParaValue.value<quint8>();
            m_editValues["操作要求"] = QString::number(rawValue);
        }
        else if(mapIt.key()=="ParamBindingStatus")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>())
                rawValue = item.varParaValue.value<quint8>();
            m_editValues["参数绑定状态"] = QString::number(rawValue, 16).toUpper();
        }

        else if(mapIt.key()=="Mechanism1UnlockTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["牵制释放好时间(s)"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism1UnlockInPlaceTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构1-解锁到位时间(s)"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism1ReleaseTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构1-释放好时间(s)"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism1ReleaseInPlaceTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构1-释放到位时间(s)"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism1InitiatorDetonateTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构1-火引爆时间"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism2UnlockTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["牵制释放好时间(s)"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism2UnlockInPlaceTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构2-解锁到位时间(s)"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism2ReleaseTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构2-释放好时间(s)"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism2ReleaseInPlaceTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构2-释放到位时间(s)"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism2InitiatorDetonateTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构2-火引爆时间"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism3UnlockTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["牵制释放好时间(s)"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism3UnlockInPlaceTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构3-解锁到位时间(s)"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism3ReleaseTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构3-释放好时间(s)"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism3ReleaseInPlaceTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构3-释放到位时间(s)"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism3InitiatorDetonateTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构3-火引爆时间"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism4UnlockTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["牵制释放好时间(s)"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism4UnlockInPlaceTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构4-解锁到位时间(s)"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism4ReleaseTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构4-释放好时间(s)"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism4ReleaseInPlaceTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构4-释放到位时间(s)"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism4InitiatorDetonateTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构4-火引爆时间"] =  QString::number(rawValue);
        }
        for(int i=1;i<5;i++)
        {
            if(mapIt.key() == QString("Collector%1DataFlag").arg(i))
            {
                quint8 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::UChar)) {
                    rawValue = item.varParaValue.value<quint8>();
                }

//                // 拆分位域：bit[7:4] = ID，bit[3:0] = 帧类型
//                quint8 telemetryID = (rawValue >> 4) & 0x0F;   // 高4位：遥测ID
//                quint8 frameType = rawValue & 0x0F;           // 低4位：帧类型

//                // 解析遥测ID
//                QString idDesc;
//                switch (telemetryID) {
//                case 0x00: idDesc = "默认值(未指定ID)"; break;
//                case 0x01: idDesc = "1"; break;
//                case 0x02: idDesc = "2"; break;
//                case 0x03: idDesc = "3"; break;
//                case 0x04: idDesc = "4"; break;
//                default: idDesc = "未知"; break;
//                }

//                // 解析帧类型（固定0001）
//                QString typeDesc = (frameType == 0x01) ? "标准帧类型" : "未知帧类型";

                // 存入状态字典
                m_editValues[QString("采集器标识%1").arg(i)] =QString::number(rawValue) ;
            }
            else if(mapIt.key() == QString("ProximitySwitchSQ1_1Status%1").arg(i))
            {
                // 3字节状态值，使用32位无符号整型存储
                qint32 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::UInt)) {
                    rawValue = item.varParaValue.value<qint32>();
                }
                // 仅提取bit0：1ms前的状态值
                bool sq1_1_Status = (rawValue & 0x01);

                // 存入状态字典
                m_ledStates[QString("SQ1.1-1%1").arg(i)] = sq1_1_Status;
            }
            else if(mapIt.key() == QString("ProximitySwitchSQ2_1Status%1").arg(i))
            {
                // 3字节状态值，使用32位无符号整型存储
                qint32 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::UInt)) {
                    rawValue = item.varParaValue.value<qint32>();
                }
                // 仅提取bit0：1ms前的状态值
                bool sq1_1_Status = (rawValue & 0x01);

                // 存入状态字典
                m_ledStates[QString("SQ2.1-1%1").arg(i)] = sq1_1_Status;
            }
            else if(mapIt.key() == QString("ProximitySwitchSQ3_1Status%1").arg(i))
            {
                // 3字节状态值，使用32位无符号整型存储
                qint32 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::UInt)) {
                    rawValue = item.varParaValue.value<qint32>();
                }
                // 仅提取bit0：1ms前的状态值
                bool sq1_1_Status = (rawValue & 0x01);

                // 存入状态字典
                m_ledStates[QString("SQ3.1-1%1").arg(i)] = sq1_1_Status;
            }
            else if(mapIt.key() == QString("ProximitySwitchSQ5_1Status%1").arg(i))
            {
                // 3字节状态值，使用32位无符号整型存储
                qint32 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::UInt)) {
                    rawValue = item.varParaValue.value<qint32>();
                }
                // 仅提取bit0：1ms前的状态值
                bool sq1_1_Status = (rawValue & 0x01);

                // 存入状态字典
                m_ledStates[QString("SQ5.1-1%1").arg(i)] = sq1_1_Status;
            }
            else if(mapIt.key() == QString("ProximitySwitchSQ6_1Status%1").arg(i))
            {
                // 3字节状态值，使用32位无符号整型存储
                qint32 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::UInt)) {
                    rawValue = item.varParaValue.value<qint32>();
                }
                // 仅提取bit0：1ms前的状态值
                bool sq1_1_Status = (rawValue & 0x01);

                // 存入状态字典
                m_ledStates[QString("SQ6.1-1%1").arg(i)] = sq1_1_Status;
            }
            else if(mapIt.key() == QString("ProximitySwitchSQ1_2Status%1").arg(i))
            {
                // 3字节状态值，使用32位无符号整型存储
                qint32 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::UInt)) {
                    rawValue = item.varParaValue.value<qint32>();
                }
                // 仅提取bit0：1ms前的状态值
                bool sq1_1_Status = (rawValue & 0x01);

                // 存入状态字典
                m_ledStates[QString("SQ1.2-1%1").arg(i)] = sq1_1_Status;
            }
            else if(mapIt.key() == QString("ProximitySwitchSQ2_2Status%1").arg(i))
            {
                // 3字节状态值，使用32位无符号整型存储
                qint32 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::UInt)) {
                    rawValue = item.varParaValue.value<qint32>();
                }
                // 仅提取bit0：1ms前的状态值
                bool sq1_1_Status = (rawValue & 0x01);

                // 存入状态字典
                m_ledStates[QString("SQ2.2-1%1").arg(i)] = sq1_1_Status;
            }
            else if(mapIt.key() == QString("ProximitySwitchSQ3_2Status%1").arg(i))
            {
                // 3字节状态值，使用32位无符号整型存储
                qint32 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::UInt)) {
                    rawValue = item.varParaValue.value<qint32>();
                }
                // 仅提取bit0：1ms前的状态值
                bool sq1_1_Status = (rawValue & 0x01);

                // 存入状态字典
                m_ledStates[QString("SQ3.2-1%1").arg(i)] = sq1_1_Status;
            }
            else if(mapIt.key() == QString("ProximitySwitchSQ5_2Status%1").arg(i))
            {
                // 3字节状态值，使用32位无符号整型存储
                qint32 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::UInt)) {
                    rawValue = item.varParaValue.value<qint32>();
                }
                // 仅提取bit0：1ms前的状态值
                bool sq1_1_Status = (rawValue & 0x01);

                // 存入状态字典
                m_ledStates[QString("SQ5.2-1%1").arg(i)] = sq1_1_Status;
            }
            else if(mapIt.key() == QString("ProximitySwitchSQ6_2Status%1").arg(i))
            {
                // 3字节状态值，使用32位无符号整型存储
                qint32 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::UInt)) {
                    rawValue = item.varParaValue.value<qint32>();
                }
                // 仅提取bit0：1ms前的状态值
                bool sq1_1_Status = (rawValue & 0x01);

                // 存入状态字典
                m_ledStates[QString("SQ6.2-1%1").arg(i)] = sq1_1_Status;
            }
            else if(mapIt.key() == QString("TensionAmplifier1%1").arg(i))
            {
                qint16 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::Short)) {
                    rawValue = item.varParaValue.value<qint16>();
                }

                // 提取低12位有效采集值
                qint16 adValue = rawValue & 0x0FFF;
                // 线性换算电压：低12位全1(4095)=10V，全0=0V
//                double voltage = (adValue / 4095.0) * 10.0;
//                emit requestResult(0,i-1,adValue);
                // 存入状态字典
                double res=calculateCoeff(adValue,0,i-1);
                bool tensionOut = isParamOutOfRange(res, ParamType::Tension);
                m_ledStates[QString("机构%1-牵制拉力异常").arg(i)] = tensionOut;

                m_editValues[QString("机构%1-牵制拉力1(kN)").arg(i)] = QString::number(res, 'f', 2);
            }
            else if(mapIt.key() == QString("TensionAmplifier2%1").arg(i))
            {
                qint16 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::Short)) {
                    rawValue = item.varParaValue.value<qint16>();
                }

                // 提取低12位有效采集值
                qint16 adValue = rawValue & 0x0FFF;
                double res=calculateCoeff(adValue,1,i-1);
                m_editValues[QString("机构%1-牵制拉力2(kN)").arg(i)] = QString::number(res, 'f', 2);
            }
            else if(mapIt.key() == QString("AngleSensorValue%1").arg(i))
            {
                qint16 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::Short)) {
                    rawValue = item.varParaValue.value<qint16>();
                }

                // 线性换算角度：0=0度，32767=360度
//                double angle = (rawValue / 32767.0) * 360.0;
                double res=calculateCoeff(rawValue,2,i-1);

                // 存入状态字典
                m_editValues[QString("机构%1-牵制状态角(°)").arg(i)] = QString::number(res, 'f', 2);
            }
            else if(mapIt.key() == QString("PressureAmplifier1%1").arg(i))
            {
                qint16 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::Short)) {
                    rawValue = item.varParaValue.value<qint16>();
                }

                // 提取低12位有效采集值
                 qint16 adValue = rawValue & 0x0FFF;
                 double res=calculateCoeff(adValue,3,i-1);
                 bool tensionOut = isParamOutOfRange(res, ParamType::Pressure);
                 m_ledStates[QString("机构%1-储气罐压力异常").arg(i)] = tensionOut;
                // 存入状态字典
                m_editValues[QString("机构%1-储气罐压力1(MPa)").arg(i)] = QString::number(res, 'f', 2);
            }
            else if(mapIt.key() == QString("PressureAmplifier2%1").arg(i))
            {
                qint16 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::Short)) {
                    rawValue = item.varParaValue.value<qint16>();
                }

                // 提取低12位有效采集值
                qint16 adValue = rawValue & 0x0FFF;
                // 第一步：线性换算电压 (0~4095 对应 0~5V)
//                double voltage = (adValue / 4095.0) * 5.0;
//                // 第二步：线性换算压力 (1V=0KPa，5V=1600KPa)
//                double pressure = (voltage - 1.0) * 400.0;
//                // 限制压力最小值为0
//                pressure = qMax(pressure, 0.0);
                double res=calculateCoeff(adValue,4,i-1);
                // 存入状态字典
                m_editValues[QString("机构%1-储气罐压力2(MPa)").arg(i)] = QString::number(res ,'f', 2);
            }
            // 温度变换器1
            else if(mapIt.key() == QString("TemperatureConverter1%1").arg(i))
            {
                qint16 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::Short)) {
                    rawValue = item.varParaValue.value<qint16>();
                }
                // 提取低12位有效采集值
                qint16 adValue = rawValue & 0x0FFF;
//                // 转换电压：0~4095 → 0~5V
//                double voltage = (adValue / 4095.0) * 5.0;
//                // 转换温度：0.2V=-70℃，4.9V=200℃
//                double temperature = (voltage - 0.2) / 4.7 * 270.0 - 70.0;
//                // 限制合理范围
//                temperature = qBound(-70.0, temperature, 200.0);
                double res=calculateCoeff(adValue,5,i-1);

                m_editValues[QString("机构%1-内部温度1(°C)").arg(i)] = QString::number(res ,'f', 2);
            }
            // 温度变换器2
            else if(mapIt.key() == QString("TemperatureConverter2%1").arg(i))
            {
                qint16 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::Short)) {
                    rawValue = item.varParaValue.value<qint16>();
                }
                qint16 adValue = rawValue & 0x0FFF;
//                double voltage = (adValue / 4095.0) * 5.0;
//                double temperature = (voltage - 0.2) / 4.7 * 270.0 - 70.0;
//                temperature = qBound(-70.0, temperature, 200.0);
                double res=calculateCoeff(adValue,5,i-1);

                m_editValues[QString("机构%1-内部温度2(°C)").arg(i)] = QString::number(res ,'f', 2);
            }
            // 28V+ 采集
            else if(mapIt.key() == QString("Voltage28VPlus%1").arg(i))
            {
                qint16 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::Short)) {
                    rawValue = item.varParaValue.value<qint16>();
                }
                qint16 adValue = rawValue & 0x0FFF;
                double voltage = (adValue / 4095.0) * 105.0;
                m_editValues[QString("28V%1").arg(i)] = QString::number(voltage, 'f', 2);
            }
            // 24V+ 采集
            else if(mapIt.key() == QString("Voltage24VPlus%1").arg(i))
            {
                qint16 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::Short)) {
                    rawValue = item.varParaValue.value<qint16>();
                }
                qint16 adValue = rawValue & 0x0FFF;
                double voltage = (adValue / 4095.0) * 55.0;
                m_editValues[QString("24V%1").arg(i)] = QString::number(voltage, 'f', 2);
            }
            else if(mapIt.key() == QString("Voltage15VMinus%1").arg(i))
            {
                qint16 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::Short)) {
                    rawValue = item.varParaValue.value<qint16>();
                }
                // 提取低12位有效采集值
                qint16 adValue = rawValue & 0x0FFF;
                // 线性转换：0~4095 对应 0V ~ -25V
                double voltage = (adValue / 4095.0) * (-25.0);

                m_editValues[QString("-15V%1").arg(i)] = QString::number(voltage, 'f', 2);
            }
            // 15V+ 采集
            else if(mapIt.key() == QString("Voltage15VPlus%1").arg(i))
            {
                qint16 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::Short)) {
                    rawValue = item.varParaValue.value<qint16>();
                }
                qint16 adValue = rawValue & 0x0FFF;
                double voltage = (adValue / 4095.0) * 30.0;
                m_editValues[QString("15V%1").arg(i)] = QString::number(voltage, 'f', 2);
            }
            else if(mapIt.key() == QString("Voltage5VPlus%1").arg(i))
            {
                qint16 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::Short)) {
                    rawValue = item.varParaValue.value<qint16>();
                }
                qint16 adValue = rawValue & 0x0FFF;
                double voltage = (adValue / 4095.0) * 10.0;
                m_editValues[QString("5V+%1").arg(i)] = QString::number(voltage, 'f', 2);
            }
            // 3.3V采集
            else if(mapIt.key() == QString("Voltage3_3V%1").arg(i))
            {
                qint16 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::Short)) {
                    rawValue = item.varParaValue.value<qint16>();
                }
                qint16 adValue = rawValue & 0x0FFF;
                double voltage = (adValue / 4095.0) * 5.0;
                m_editValues[QString("3.3V%1").arg(i)] = QString::number(voltage, 'f', 2);
            }
            // 1.8V采集
            else if(mapIt.key() == QString("Voltage1_8V%1").arg(i))
            {
                qint16 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::Short)) {
                    rawValue = item.varParaValue.value<qint16>();
                }
                qint16 adValue = rawValue & 0x0FFF;
                double voltage = (adValue / 4095.0) * 5.0;
                m_editValues[QString("1.8V%1").arg(i)] = QString::number(voltage, 'f', 2);
            }
            // 1.0V采集
            else if(mapIt.key() == QString("Voltage1_0V%1").arg(i))
            {
                qint16 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::Short)) {
                    rawValue = item.varParaValue.value<qint16>();
                }
                qint16 adValue = rawValue & 0x0FFF;
                double voltage = (adValue / 4095.0) * 5.0;
                m_editValues[QString("1.0V%1").arg(i)] = QString::number(voltage, 'f', 2);
            }
            // 温度采集1（系统不解析，仅存储原始值）
            else if(mapIt.key() == QString("TemperatureCollection1%1").arg(i))
            {
                qint16 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::Short)) {
                    rawValue = item.varParaValue.value<qint16>();
                }
                qint16 adValue = rawValue & 0x0FFF;
                m_editValues[QString("热敏电阻%1").arg(i)] = QString::number(adValue);
            }
            // 温度采集2（系统不解析，仅存储原始值）
            else if(mapIt.key() == QString("TemperatureCollection2%1").arg(i))
            {
                qint16 rawValue = 0;
                if (item.varParaValue.canConvert(QMetaType::Short)) {
                    rawValue = item.varParaValue.value<qint16>();
                }
                qint16 adValue = rawValue & 0x0FFF;
                m_editValues[QString("温度采集2%1").arg(i)] = QString::number(adValue);
            }
            // 校验和 CRC16
            else if(mapIt.key() == QString("Checksum%1").arg(i))
            {
                qint16 crcValue = 0;
                if (item.varParaValue.canConvert(QMetaType::Short)) {
                    crcValue = item.varParaValue.value<qint16>();
                }
            }
        }
    }
    //    updateControllerFrameUI(m_ledStates,m_editValues/*,m_ledStates,m_editValues*/);

}

void FrameWorker::paramProcess_A6(STParamInfo &m_param, QMap<QString, bool> &m_ledStates, QMap<QString, QString> &m_editValues)
{
    for (auto mapIt = m_param.mapParams.cbegin(); mapIt != m_param.mapParams.cend(); ++mapIt)
    {
        const STParamItem& item = mapIt.value();

        // 承接你原有代码的 if-else 结构
         if(mapIt.key()=="OE_KJ1")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }
            // 解析OE_KJ1 电磁阀机构Ⅰ状态
            bool signalForward1   = (rawValue & 0x80) != 0;  // B7：转发发控允许释放信号1
            bool xf01a1Check1     = (rawValue & 0x40) != 0;  // B6：XF01A1状态监测点1
            bool dcf1Y3_Supply   = (rawValue & 0x10) != 0;  // B4：机构Ⅰ供气DCF1-Y3
            bool dcf1Y2_Lock     = (rawValue & 0x04) != 0;  // B2：机构Ⅰ锁定DCF1-Y2
            bool dcf1Y1_2_Bak    = (rawValue & 0x02) != 0;  // B1：机构Ⅰ释放(备)DCF1-Y1-2
            bool dcf1Y1_1_Main   = (rawValue & 0x01) != 0;  // B0：机构Ⅰ释放(主)DCF1-Y1-1

            // 存入状态字典
            m_ledStates["转发释放信号1"] = signalForward1;
            m_ledStates["XF01A1监测1"] = xf01a1Check1;
            m_ledStates["机构Ⅰ供气"] = dcf1Y3_Supply;
            m_ledStates["机构Ⅰ锁定"] = dcf1Y2_Lock;
            m_ledStates["机构Ⅰ释放(备)"] = dcf1Y1_2_Bak;
            m_ledStates["机构Ⅰ释放(主)"] = dcf1Y1_1_Main;
        }
        else if(mapIt.key()=="OE_KJ2")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }
            // 解析OE_KJ2 电磁阀机构Ⅱ状态
            bool signalForward2   = (rawValue & 0x80) != 0;  // B7：转发发控允许释放信号2
            bool xf01a1Check2     = (rawValue & 0x40) != 0;  // B6：XF01A1状态监测点2
            bool dcf2Y3_Supply   = (rawValue & 0x10) != 0;  // B4：机构Ⅱ供气DCF2-Y3
            bool dcf2Y2_Lock     = (rawValue & 0x04) != 0;  // B2：机构Ⅱ锁定DCF2-Y2
            bool dcf2Y1_2_Bak    = (rawValue & 0x02) != 0;  // B1：机构Ⅱ释放(备)DCF2-Y1-2
            bool dcf2Y1_1_Main   = (rawValue & 0x01) != 0;  // B0：机构Ⅱ释放(主)DCF2-Y1-1

            m_ledStates["转发释放信号2"] = signalForward2;
            m_ledStates["XF01A1监测2"] = xf01a1Check2;
            m_ledStates["机构Ⅱ供气"] = dcf2Y3_Supply;
            m_ledStates["机构Ⅱ锁定"] = dcf2Y2_Lock;
            m_ledStates["机构Ⅱ释放(备)"] = dcf2Y1_2_Bak;
            m_ledStates["机构Ⅱ释放(主)"] = dcf2Y1_1_Main;
        }
        else if(mapIt.key()=="OE_KJ4")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }
            // 解析OE_KJ4 电磁阀机构Ⅲ状态
            bool xf01a2Check1     = (rawValue & 0x40) != 0;  // B6：XF01A2状态监测点1
            bool dcf3Y3_Supply   = (rawValue & 0x10) != 0;  // B4：机构Ⅲ供气DCF3-Y3
            bool dcf3Y2_Lock     = (rawValue & 0x04) != 0;  // B2：机构Ⅲ锁定DCF3-Y2
            bool dcf3Y1_2_Bak    = (rawValue & 0x02) != 0;  // B1：机构Ⅲ释放(备)DCF3-Y1-2
            bool dcf3Y1_1_Main   = (rawValue & 0x01) != 0;  // B0：机构Ⅲ释放(主)DCF3-Y1-1

            m_ledStates["XF01A2监测1"] = xf01a2Check1;
            m_ledStates["机构Ⅲ供气"] = dcf3Y3_Supply;
            m_ledStates["机构Ⅲ锁定"] = dcf3Y2_Lock;
            m_ledStates["机构Ⅲ释放(备)"] = dcf3Y1_2_Bak;
            m_ledStates["机构Ⅲ释放(主)"] = dcf3Y1_1_Main;
        }
        else if(mapIt.key()=="OE_KJ5")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }
            // 解析OE_KJ5 电磁阀机构Ⅳ状态
            bool xf01a2Check2     = (rawValue & 0x40) != 0;  // B6：XF01A2状态监测点2
            bool dcf4Y3_Supply   = (rawValue & 0x10) != 0;  // B4：机构Ⅳ供气DCF4-Y3
            bool dcf4Y2_Lock     = (rawValue & 0x04) != 0;  // B2：机构Ⅳ锁定DCF4-Y2
            bool dcf4Y1_2_Bak    = (rawValue & 0x02) != 0;  // B1：机构Ⅳ释放(备)DCF4-Y1-2
            bool dcf4Y1_1_Main   = (rawValue & 0x01) != 0;  // B0：机构Ⅳ释放(主)DCF4-Y1-1

            m_ledStates["XF01A2监测2"] = xf01a2Check2;
            m_ledStates["机构Ⅳ供气"] = dcf4Y3_Supply;
            m_ledStates["机构Ⅳ锁定"] = dcf4Y2_Lock;
            m_ledStates["机构Ⅳ释放(备)"] = dcf4Y1_2_Bak;
            m_ledStates["机构Ⅳ释放(主)"] = dcf4Y1_1_Main;
        }
        else if(mapIt.key()=="SolenoidValveRelayPath") // 22字节 电磁阀波形采集继电器通路
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }
            QString relayStatus;
            switch (rawValue) {
            case 0x00: relayStatus = "全部断开"; break;
            case 0x11: relayStatus = "第1路闭合"; break;
            case 0x22: relayStatus = "第2路闭合"; break;
            case 0x33: relayStatus = "第3路闭合"; break;
            case 0x44: relayStatus = "第4路闭合"; break;
            case 0x1F: case 0x2F: case 0x3F: case 0x4F:
                relayStatus = "指令异常"; break;
            default: relayStatus = "未知状态"; break;
            }
            m_editValues["波形采集继电器"] = relayStatus;
        }
        // ====================== t1-t12 电磁阀波形电压采集 (22-70字节) ======================
        else if(mapIt.key().startsWith("AIN4_t")) // 机构Ⅰ t1-t12
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }
            // 每次采样覆盖上一次，最终显示为 t12
            m_editValues["机构1电磁阀电压"] = QString::number(rawValue * 0.01952 / 0.51, 'f', 2);
        }
        else if(mapIt.key().startsWith("AIN5_t")) // 机构Ⅱ t1-t12
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }
            m_editValues["机构2电磁阀电压"] = QString::number(rawValue * 0.01952 / 0.51, 'f', 2);
        }
        else if(mapIt.key().startsWith("AIN6_t")) // 机构Ⅲ t1-t12
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }
            m_editValues["机构3电磁阀电压"] = QString::number(rawValue * 0.01952 / 0.51, 'f', 2);
        }
        else if(mapIt.key().startsWith("AIN7_t")) // 机构Ⅳ t1-t12
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }
            m_editValues["机构4电磁阀电压"] = QString::number(rawValue * 0.01952 / 0.51, 'f', 2);
        }
        // ====================== 火工品时间/预留/工作模式/指令码 ======================
        else if(mapIt.key()=="InitiatorDetonateTime") // 70-71 火工品引爆时间
        {
            quint16 rawValue = 0;
            if (item.varParaValue.canConvert<quint16>()) {
                rawValue = item.varParaValue.value<quint16>();
            }
            m_editValues["引爆时间(ms)"] = QString::number(rawValue);
        }
        else if(mapIt.key()=="InitiatorRelayCloseTime") //72-73 火工品引爆继电器关闭时间
        {
            quint16 rawValue = 0;
            if (item.varParaValue.canConvert<quint16>()) {
                rawValue = item.varParaValue.value<quint16>();
            }
            m_editValues["继电器关闭时间(ms)"] = QString::number(rawValue);
        }
        else if(mapIt.key()=="WorkMode") //76 工作模式
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }
            QString mode;
            switch (rawValue) {
            case 0xAA: mode = "测试模式"; break;
            case 0xBB: mode = "手动模式"; break;
            case 0xCC: mode = "自动模式"; break;
            default: mode = "未知模式"; break;
            }
            m_editValues["工作模式"] = mode;
        }
        else if(mapIt.key()=="CommandCode") //77-78 指令码(UInt16)，拆分高/低位
        {
            quint16 rawValue = 0;
            if (item.varParaValue.canConvert<quint16>()) {
                rawValue = item.varParaValue.value<quint16>();
            }
            quint8 high = (rawValue >> 8) & 0xFF;
            quint8 low  = rawValue & 0xFF;
            m_editValues["指令码高8位"] = QString::number(high, 16).toUpper();
            m_editValues["指令码低8位"] = QString::number(low, 16).toUpper();
        }
        else if(mapIt.key()=="OperationReq") //79 操作要求
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }
            m_editValues["操作要求"] = QString::number(rawValue);
        }
        // ====================== 火工品保护状态 ======================
        else if(mapIt.key()=="OE_KJ7") //81 火工品保护状态1
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }
            quint8 b3_b0 = rawValue & 0x0F;
            bool k1State = (b3_b0 & 0x01) != 0;
            bool k2State = (b3_b0 & 0x02) != 0;
            bool k3State = (b3_b0 & 0x04) != 0;
            bool k4State = (b3_b0 & 0x08) != 0;

            m_ledStates["火保K1"] = k1State;
            m_ledStates["火保K2"] = k2State;
            m_ledStates["火保K3"] = k3State;
            m_ledStates["火保K4"] = k4State;
        }
        else if(mapIt.key()=="OE_KJ8") //82 火工品保护状态2
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }
            bool protect1 = (rawValue & 0x01) != 0; // B0：第1路保护
            bool protect2 = (rawValue & 0x02) != 0; // B1：第2路保护
            bool protect3 = (rawValue & 0x04) != 0; // B2：第3路保护
            bool protect4 = (rawValue & 0x08) != 0; // B3：第4路保护

            m_ledStates["火工品保护1"] = protect1;
            m_ledStates["火工品保护2"] = protect2;
            m_ledStates["火工品保护3"] = protect3;
            m_ledStates["火工品保护4"] = protect4;
        }
        else if(mapIt.key()=="OE_KJ9") //83 解控/继电器状态
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }
            bool elecTestRelay = (rawValue & 0x40) != 0; // B6：电爆电路测试继电器
            bool jk_unlock = (rawValue & 0x20) != 0;     // B5：解控状态JKt
            bool jkback_unlock = (rawValue & 0x10) != 0; // B4：解控状态JKbackt
            bool j1 = (rawValue & 0x01) != 0;            // B0：解控继电器J1
            bool j2 = (rawValue & 0x02) != 0;            // B1：解控继电器J2
            bool j3 = (rawValue & 0x04) != 0;            // B2：解控继电器J3
            bool j4 = (rawValue & 0x08) != 0;            // B3：解控继电器J4

            m_ledStates["电爆测试继电器"] = elecTestRelay;
            m_ledStates["解控状态(JKt)"] = jk_unlock;
            m_ledStates["解控状态(最终)"] = jkback_unlock;
            m_ledStates["解控J1"] = j1;
            m_ledStates["解控J2"] = j2;
            m_ledStates["解控J3"] = j3;
            m_ledStates["解控J4"] = j4;
        }
        else if(mapIt.key()=="OE_KJ10") //84 引爆+电爆电路状态
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }
            // B7-B4：电爆电路4-1路；B3-B0：引爆继电器4-1路
            bool circuit1 = (rawValue & 0x10) != 0;
            bool circuit2 = (rawValue & 0x20) != 0;
            bool circuit3 = (rawValue & 0x40) != 0;
            bool circuit4 = (rawValue & 0x80) != 0;
            bool detonator1 = (rawValue & 0x01) != 0;
            bool detonator2 = (rawValue & 0x02) != 0;
            bool detonator3 = (rawValue & 0x04) != 0;
            bool detonator4 = (rawValue & 0x08) != 0;

            m_ledStates["电爆电路1"] = circuit1;
            m_ledStates["电爆电路2"] = circuit2;
            m_ledStates["电爆电路3"] = circuit3;
            m_ledStates["电爆电路4"] = circuit4;
            m_ledStates["引爆继电器1"] = detonator1;
            m_ledStates["引爆继电器2"] = detonator2;
            m_ledStates["引爆继电器3"] = detonator3;
            m_ledStates["引爆继电器4"] = detonator4;
        }
        // ====================== 电压采集 ======================
        else if(mapIt.key()=="AIN8") //85 数字供电电压
        {
            quint16 rawValue = 0;
            if (item.varParaValue.canConvert<quint16>()) {
                rawValue = item.varParaValue.value<quint16>();
            }
            double volt = rawValue * 0.75656;
            m_editValues["数字供电电压"] = QString::number(volt, 'f', 2);
        }
        else if(mapIt.key()=="AIN9") //86 驱动供电电压1
        {
            quint16 rawValue = 0;
            if (item.varParaValue.canConvert<quint16>()) {
                rawValue = item.varParaValue.value<quint16>();
            }
            double volt = rawValue * 0.75656;
            m_editValues["驱动供电电压1"] = QString::number(volt, 'f', 2);
        }
        else if(mapIt.key()=="AIN9_2") //87 驱动供电电压2
        {
            quint16 rawValue = 0;
            if (item.varParaValue.canConvert<quint16>()) {
                rawValue = item.varParaValue.value<quint16>();
            }
            double volt = rawValue * 0.75656;
            m_editValues["驱动供电电压2"] = QString::number(volt, 'f', 2);
        }
        else if(mapIt.key()=="AIN10") //88 数字5V1
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }
            double volt = rawValue * 0.0293;
            m_editValues["数字5V1"] = QString::number(volt, 'f', 2);
        }
        else if(mapIt.key()=="AIN12") //89 数字5V2
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }
            double volt = rawValue * 0.0293;
            m_editValues["数字5V2"] = QString::number(volt, 'f', 2);
        }
        else if(mapIt.key()=="AIN13") //90 数字5V3
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>()) {
                rawValue = item.varParaValue.value<quint8>();
            }
            double volt = rawValue * 0.0293;
            m_editValues["数字5V3"] = QString::number(volt, 'f', 2);
        }
        // ====================== 公共帧头字段 ======================
        else if(mapIt.key()=="FrameLength")
        {
            quint32 rawValue = 0;
            if (item.varParaValue.canConvert<quint32>())
                rawValue = item.varParaValue.value<quint32>();
            m_editValues["帧长"] = QString::number(rawValue);
            m_editValues["时序_帧长"] = QString::number(rawValue);
        }
        else if(mapIt.key()=="FrameCount")
        {
            qint32 rawValue = 0;
            if (item.varParaValue.canConvert<qint32>())
                rawValue = item.varParaValue.value<qint32>();
            m_editValues["帧计数"] = QString::number(rawValue);
            m_editValues["帧计数1"] = QString::number(rawValue);
            m_editValues["帧计数2"] = QString::number(rawValue);
            m_editValues["帧计数3"] = QString::number(rawValue);
            m_editValues["帧计数4"] = QString::number(rawValue);
        }
        else if(mapIt.key()=="FrameType")
        {
            quint8 rawValue = 0;
            if (item.varParaValue.canConvert<quint8>())
                rawValue = item.varParaValue.value<quint8>();
            QString type;
            switch (rawValue) {
            case 0x01: type = "终端"; break;
            case 0x02: type = "串口服务器"; break;
            default:   type = "未知"; break;
            }
            m_editValues["帧类型"] = QString::number(rawValue, 16).toUpper();
            m_editValues["帧类型_校验"] = type;
        }
        else if(mapIt.key()=="TimeFlag")
        {
            quint32 rawValue = 0;
            if (item.varParaValue.canConvert<quint32>())
                rawValue = item.varParaValue.value<quint32>();
            m_editValues["时间标志"] = QString::number(rawValue);
        }
    }
}

// ================= 构造函数 =================
ControllerPanel::ControllerPanel(QWidget *parent) : QWidget(parent)
  , m_updateTimer(new QTimer(this))
  , m_worker(new FrameWorker())
  , m_workerThread(new QThread(this))
{

    setupUI();
    // 根据控制器名称动态设置窗口标题
    auto* handle = DataInteractionManager::getInstance().getMsgHandle();

    // 订阅实时数据
    handle->subMessage(this, ESubDataType::E_RealTimeData);
    // 1. 基础窗口设置
    initWorkerThread();
    m_updateTimer_ser=new QTimer(this);
    m_updateTimer_ser->setSingleShot(true);  // 单次触发，避免重复执行
    m_updateTimer_ser->setInterval(10);      // 合并10ms内的所有数据更新
    connect(m_updateTimer_ser, &QTimer::timeout, this, &ControllerPanel::onTimerTimeout_ser);
    // 3. 配置防抖定时器：10ms单次触发（可根据业务调整）
    m_updateTimer->setSingleShot(true);  // 单次触发，避免重复执行
    m_updateTimer->setInterval(10);      // 合并10ms内的所有数据更新

    connect(m_updateTimer, &QTimer::timeout, this, &ControllerPanel::onTimerTimeout);

}

ControllerPanel::~ControllerPanel()
{
    DataInteractionManager::getInstance()
        .getMsgHandle()
        ->unSubMessageAll(this);
    m_workerThread->quit();
    m_workerThread->wait();
}

void ControllerPanel::onMessage(IEvent *pEvent)
{
    if (!pEvent) return;

    switch (pEvent->getType()) {

    case EventType::E_InitiativeMsg: {
         // 设备数据（TCP / UDP / 串口 解析后投递）
         auto* pInit = static_cast<InitiativeMsgEvent*>(pEvent);
         const STParamInfo& param = pInit->getParamData();
         qDebug() << "[UI] 收到设备数据 deviceId=" << param.unSourceID;
//          updateDisplay(param);
        break;
    }

    case EventType::E_InternalMsg: {

        break;
    }

    default:
        break;

    }
}

void ControllerPanel::appendData(const QByteArray &data)
{

    QMutexLocker locker(&m_cacheMutex);
    m_dataCache.append(data);

    // 2. 重启防抖定时器（10ms内有新数据则重新计时，只处理最后一次）
    m_updateTimer->start();
}

void ControllerPanel::clearPlaybackCache()
{
    QMutexLocker locker(&m_cacheMutex);
    m_dataCache.clear();
    m_updateTimer->stop();
}

void ControllerPanel::onTimerTimeout()
{
    // 定时器超时，处理缓存的所有数据
    QMutexLocker locker(&m_cacheMutex);
    if (m_dataCache.isEmpty()) return;

    // 拷贝缓存数据并清空（释放锁，避免阻塞主线程）
    QByteArray dataToProcess = m_dataCache;
    m_dataCache.clear();
    locker.unlock();

    // 3. 触发子线程处理数据（耗时操作移到子线程）
    QMap<QString, bool> ledStates;
    QMap<QString, QString> editValues;
    QMetaObject::invokeMethod(m_worker, "processData",
                              Qt::QueuedConnection,
                              Q_ARG(QByteArray, dataToProcess));

}

void ControllerPanel::onTimerTimeout_ser()
{
    QMutexLocker locker(&m_cacheMutex);
    if (m_param.getID()!="serial_E") return;

    // 拷贝缓存数据并清空（释放锁，避免阻塞主线程）
    QByteArray dataToProcess = m_dataCache;
    STParamInfo param = m_param;
    m_param.channelId="";
    locker.unlock();
    QMetaObject::invokeMethod(m_worker, "processData_sel",
                              Qt::QueuedConnection,
                              Q_ARG(STParamInfo, param));

}

void ControllerPanel::initWorkerThread()
{
    m_worker->moveToThread(m_workerThread);
    // 连接信号：子线程处理完成 → 主线程更新UI
    connect(m_worker, &FrameWorker::dataProcessed,
            this, &ControllerPanel::onDataProcessed, Qt::QueuedConnection);
    // 启动子线程
    m_workerThread->start();
}

void ControllerPanel::onDataProcessed(const QMap<QString, bool> &ledStates, const QMap<QString, QString> &editValues)
{
    updateControllerFrameUI(ledStates,editValues);

}

void ControllerPanel::updateControllerFrameUI(const QMap<QString, bool> &ledStates, const QMap<QString, QString> &editValues)
{
    for (auto it = ledStates.cbegin(); it != ledStates.cend(); ++it) {
        const QString &ledName = it.key();
        bool isOn = it.value();

        if (m_ledMap.contains(ledName)) {
            StyledLedLabel *led = m_ledMap[ledName];
            if (led) {
                led->setOn(isOn);
                led->setDisabledLed(false); // 解除禁用（如需保持禁用可注释）
            }
        } else {
            qWarning() << "[更新UI] 未找到LED控件：" << ledName;
        }
    }

    // 2. 更新LineEdit输入框值
    for (auto it = editValues.cbegin(); it != editValues.cend(); ++it) {
        const QString &editName = it.key();
        const QString &value = it.value();

        if (m_valueMap.contains(editName)) {
            StyledLineEdit *edit = m_valueMap[editName];
            if (edit) {
                edit->setText(value);
                edit->setGrayInputMode(false); // 取消灰色模式，显示正常文本
            }
        } else {
            qWarning() << "[更新UI] 未找到LineEdit控件：" << editName;
        }
    }
}

void ControllerPanel::setParam(const STParamInfo &param)
{
    QMutexLocker locker(&m_cacheMutex); // 加锁保证线程安全
    m_param = param;
    m_updateTimer_ser->start();
}

// ================= 辅助函数：创建数值行 (第1、2列使用) =================
DataWidgetRow ControllerPanel::createValueRow(const QString &text)
{
    DataWidgetRow row;
    row.type = RowType::Value; // 标记类型

    // 1. 替换为自定义数值框（样式/尺寸已在StyledLineEdit中定义）
    row.valueBox = new StyledLineEdit(this);
    // 2. 再设置初始文本
    row.valueBox->setText("0.000");
    // 2. 标签（保留原有文本样式，非样式类管控部分）
    row.textLabel = new QLabel(text);
    row.textLabel->setFixedHeight(22);
    row.textLabel->setFont(QFont("Microsoft YaHei", 9));
    row.textLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    return row;
}

// ================= 辅助函数：创建状态行 (第3、4列使用) =================
DataWidgetRow ControllerPanel::createStatusRow(const QString &text, bool isAlarm)
{
    DataWidgetRow row;
    row.type = RowType::Status;
    row.isAlarmType = isAlarm;
    row.valueBox = nullptr;

    // 1. 替换为自定义指示灯（基础样式/尺寸已在StyledLedLabel中定义）
    row.lightLabel = new StyledLedLabel();

    // 2. 文字标签（保留原有文本样式）
    row.textLabel = new QLabel(text);
    row.textLabel->setFont(QFont("Microsoft YaHei", 9));
    row.textLabel->setAlignment(Qt::AlignVCenter | Qt::AlignLeft);

    return row;
}

// ================= UI 布局构建 =================
void ControllerPanel::setupUI()
{
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(5, 5, 5, 5);
    mainLayout->setSpacing(20);

    // --- 第 1 列：拉力、角度、压力 ---
    QVBoxLayout *col1 = new QVBoxLayout();
    col1->setSpacing(2);

    // 宏定义：简化数值行的添加 ✅【修改点】添加LineEdit到映射表 m_valueMap
    #define ADD_VAL_ROW(layout, vec, txt) \
        { \
            DataWidgetRow r = createValueRow(txt); \
            QHBoxLayout *hl = new QHBoxLayout(); \
            hl->addWidget(r.valueBox); \
            hl->addWidget(r.textLabel); \
            hl->addStretch(); \
            layout->addLayout(hl); \
            vec.append(r); \
            /* 关键：将输入框绑定到映射表，key为文本字符串 */ \
            if (r.valueBox) m_valueMap[txt] = r.valueBox; \
        }

    // 机构1-4 拉力
    for(int i=1; i<=4; i++) {
        ADD_VAL_ROW(col1, m_rowsCol1, QString("机构%1-牵制拉力1(kN)").arg(i));
        ADD_VAL_ROW(col1, m_rowsCol1, QString("机构%1-牵制拉力2(kN)").arg(i));
    }
    // 状态角
    for(int i=1; i<=4; i++) {
        ADD_VAL_ROW(col1, m_rowsCol1, QString("机构%1-牵制状态角(°)").arg(i));
    }
    // 储气罐压力
    for(int i=1; i<=4; i++) {
        ADD_VAL_ROW(col1, m_rowsCol1, QString("机构%1-储气罐压力1(MPa)").arg(i));
        ADD_VAL_ROW(col1, m_rowsCol1, QString("机构%1-储气罐压力2(MPa)").arg(i));
    }
    // 内部温度
    for(int i=1; i<=4; i++) {
        ADD_VAL_ROW(col1, m_rowsCol1, QString("机构%1-内部温度1(°C)").arg(i));
        ADD_VAL_ROW(col1, m_rowsCol1, QString("机构%1-内部温度2(°C)").arg(i));
    }
    col1->addStretch();

    // --- 第 2 列：时间参数 ---
    QVBoxLayout *col2 = new QVBoxLayout();
    col2->setSpacing(2);

    // 解锁到位时间
    for(int i=1; i<=4; i++) {
        ADD_VAL_ROW(col2, m_rowsCol2, QString("机构%1-解锁到位时间(s)").arg(i));
    }
    // 释放好时间
    for(int i=1; i<=4; i++) {
        ADD_VAL_ROW(col2, m_rowsCol2, QString("机构%1-释放好时间(s)").arg(i));
    }
    ADD_VAL_ROW(col2, m_rowsCol2, "牵制释放好时间(s)");

    // 释放到位时间
    for(int i=1; i<=4; i++) {
        ADD_VAL_ROW(col2, m_rowsCol2, QString("机构%1-释放到位时间(s)").arg(i));
    }

    // 火引爆时间
    for(int i=1; i<=4; i++) {
        ADD_VAL_ROW(col2, m_rowsCol2, QString("机构%1-火引爆时间").arg(i));
    }
    col2->addStretch();

    // --- 第 3 列：状态灯 (正常状态) ---
    QVBoxLayout *col3 = new QVBoxLayout();
    col3->setSpacing(2);

    // 宏定义：简化状态行的添加 ✅【修改点】添加LED到映射表 m_ledMap
    #define ADD_STATUS_ROW(layout, vec, txt) \
        { \
            DataWidgetRow r = createStatusRow(txt, false); \
            QHBoxLayout *hl = new QHBoxLayout(); \
            hl->addWidget(r.lightLabel); \
            hl->addWidget(r.textLabel); \
            hl->addStretch(); \
            layout->addLayout(hl); \
            vec.append(r); \
            /* 关键：将状态灯绑定到映射表，key为文本字符串 */ \
            if (r.lightLabel) m_ledMap[txt] = r.lightLabel; \
        }

    // 快速分离解锁到位
    for(int i=1; i<=4; i++) ADD_STATUS_ROW(col3, m_rowsCol3, QString("机构%1-快速分离解锁到位").arg(i));
    // 释放到位
    for(int i=1; i<=4; i++) ADD_STATUS_ROW(col3, m_rowsCol3, QString("机构%1-释放到位").arg(i));
    // 牵制臂复位到位
    for(int i=1; i<=4; i++) ADD_STATUS_ROW(col3, m_rowsCol3, QString("机构%1-牵制臂复位到位").arg(i));
    // 锁定到位
    for(int i=1; i<=4; i++) ADD_STATUS_ROW(col3, m_rowsCol3, QString("机构%1-锁定到位").arg(i));
    // 释放好
    for(int i=1; i<=4; i++) ADD_STATUS_ROW(col3, m_rowsCol3, QString("机构%1-释放好").arg(i));
    ADD_STATUS_ROW(col3, m_rowsCol3, "释放好4机构");

    col3->addStretch();

    // --- 第 4 列：状态灯 (异常与连接) ---
    QVBoxLayout *col4 = new QVBoxLayout();
    col4->setSpacing(2);

    // 宏定义：添加异常类状态行 ✅【修改点】添加告警LED到映射表
    #define ADD_ALARM_ROW(layout, vec, txt) \
        { \
            DataWidgetRow r = createStatusRow(txt, true); \
            QHBoxLayout *hl = new QHBoxLayout(); \
            hl->addWidget(r.lightLabel); \
            hl->addWidget(r.textLabel); \
            hl->addStretch(); \
            layout->addLayout(hl); \
            vec.append(r); \
            /* 关键：将告警灯绑定到映射表 */ \
            if (r.lightLabel) m_ledMap[txt] = r.lightLabel; \
        }

    // 牵制拉力异常
    for(int i=1; i<=4; i++) ADD_ALARM_ROW(col4, m_rowsCol4, QString("机构%1-牵制拉力异常").arg(i));
    // 储气罐压力异常
    for(int i=1; i<=4; i++) ADD_ALARM_ROW(col4, m_rowsCol4, QString("机构%1-储气罐压力异常").arg(i));

    // 控制电缆连接情况
    for(int i=1; i<=4; i++) ADD_STATUS_ROW(col4, m_rowsCol4, QString("机构%1-控制电缆连接情况").arg(i));

    // 其他连接状态
    ADD_STATUS_ROW(col4, m_rowsCol4, "允辉连接(XF01E)");
    ADD_STATUS_ROW(col4, m_rowsCol4, "转释好连接(XF01D)");
    ADD_STATUS_ROW(col4, m_rowsCol4, "箭上允释");
    ADD_STATUS_ROW(col4, m_rowsCol4, "地面允释");
    ADD_STATUS_ROW(col4, m_rowsCol4, "转发释放好1");
    ADD_STATUS_ROW(col4, m_rowsCol4, "转发释放好2");

    // 火保
    for(int i=1; i<=4; i++) ADD_STATUS_ROW(col4, m_rowsCol4, QString("火保K%1").arg(i));
    for(int i=1; i<=4; i++) ADD_STATUS_ROW(col4, m_rowsCol4, QString("火保%1").arg(i));

    col4->addStretch();

    // --- 将所有列加入主布局 ---
    mainLayout->addLayout(col1);
    mainLayout->addLayout(col2);
    mainLayout->addLayout(col3);
    mainLayout->addLayout(col4);

    #undef ADD_VAL_ROW
    #undef ADD_STATUS_ROW
    #undef ADD_ALARM_ROW
}

// ================= 数据更新逻辑 =================
