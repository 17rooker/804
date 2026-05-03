#include "launchprocessdialog.h"
#include <QGridLayout>
#include <QHBoxLayout>
#include <QMenu>
#include <QVBoxLayout>
#include <QLabel>
#include <QGroupBox>
#include <QPushButton>
#include <QFont>
#include <QFrame>
#include <QTimer>
#include <QTime>
#include <qtextedit.h>
#include <QObject>
#include <QDir>
#include <QCoreApplication>
#include <QScriptEngine>
#include <QSettings>
#include "src/CustomMessage/DataInteractionManager.h"
#include "src/Common/LoggerManager.h"
#include "src/StyleEventFilter.h"
#include "src/CommManager.h"
#include "InitiativeMsgEvent.h"
#include "src/Common/ConfigHelper.h"
double FrameDatWorker::calculateCoeff(double x, int row, int col)
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


void FrameDatWorker::processData(const QByteArray &data)
{
    QByteArray buf = data;
    const int MAX_FRAMES = 100;
    const int EMIT_INTERVAL = 5;
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
        pack.channelType = EChannelType::Serial;
        pack.baDataRecv = frameData;

        STParamInfo param{};
        analy.parseData(pack, param);

        QMap<QString, bool> ledStates;
        QMap<QString, QString> editValues;
        paramProcess(param, ledStates, editValues);
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
        lastLed = ledStates;
        lastEdit = editValues;

        if (framesProcessed % EMIT_INTERVAL == 0)
            emit dataProcessed(lastLed, lastEdit);

        ++framesProcessed;
    }

    if (framesProcessed > 0 && framesProcessed % EMIT_INTERVAL != 0)
        emit dataProcessed(lastLed, lastEdit);
}

void FrameDatWorker::processData_sel( STParamInfo &param)
{
    QMap<QString, bool> ledStates;
    QMap<QString, QString> editValues;
    paramProcess(param,ledStates,editValues);
    emit dataProcessed(ledStates, editValues);
}

void FrameDatWorker::paramProcess(STParamInfo &m_param, QMap<QString, bool> &m_ledStates,  QMap<QString, QString> &m_editValues)
{
    for (auto mapIt = m_param.mapParams.cbegin(); mapIt != m_param.mapParams.cend(); ++mapIt) {
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
            m_editValues["工作模式"] = workMode;         // 数值
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
            m_editValues["数字5V1"] = QString::number(digital5V1, 'f', 3);
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
            m_editValues["数字5V2"] = QString::number(digital5V1, 'f', 2);
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
            m_editValues["数字5V3"] = QString::number(digital5V1, 'f', 2);
        }
        else if(mapIt.key()=="DigitalBoardTemperature")
        {
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }
            // 存入状态字典
            m_editValues["温度"] = QString::number(rawValue, 'f', 2);
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
            m_editValues["解锁时间1"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism1UnlockInPlaceTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构1_解锁到位"] =  QString::number(rawValue)+("ms");
        }
        else if(mapIt.key()=="Mechanism1ReleaseTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构1_释放好"] =  QString::number(rawValue)+("ms");
        }
        else if(mapIt.key()=="Mechanism1ReleaseInPlaceTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构1_释放到位"] =  QString::number(rawValue)+("ms");
        }
        else if(mapIt.key()=="Mechanism1InitiatorDetonateTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["火引爆到位时间1"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism2UnlockTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["解锁时间2"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism2UnlockInPlaceTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构2_解锁到位"] =  QString::number(rawValue)+("ms");
        }
        else if(mapIt.key()=="Mechanism2ReleaseTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构2_释放好"] =  QString::number(rawValue)+("ms");
        }
        else if(mapIt.key()=="Mechanism2ReleaseInPlaceTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构2_释放到位"] =  QString::number(rawValue)+("ms");
        }
        else if(mapIt.key()=="Mechanism2InitiatorDetonateTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["火引爆到位时间2"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism3UnlockTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["解锁时间3"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism3UnlockInPlaceTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构3_解锁到位"] =  QString::number(rawValue)+("ms");
        }
        else if(mapIt.key()=="Mechanism3ReleaseTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构3_释放好"] =  QString::number(rawValue)+("ms");
        }
        else if(mapIt.key()=="Mechanism3ReleaseInPlaceTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构3_释放到位"] =  QString::number(rawValue)+("ms");
        }
        else if(mapIt.key()=="Mechanism3InitiatorDetonateTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["火引爆到位时间3"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism4UnlockTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["解锁时间4"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism4UnlockInPlaceTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构4_解锁到位"] =  QString::number(rawValue)+("ms");
        }
        else if(mapIt.key()=="Mechanism4ReleaseTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["机构4_释放好"] =  QString::number(rawValue)+("ms");
        }
        else if(mapIt.key()=="Mechanism4ReleaseInPlaceTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["释放到位时间4"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism4InitiatorDetonateTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["火引爆到位时间4"] =  QString::number(rawValue);
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

                m_editValues[QString("机构%1_牵制拉力1").arg(i)] = QString::number(res, 'f', 2)+("KN");
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
                m_editValues[QString("机构%1_牵制拉力2").arg(i)] = QString::number(res, 'f', 2);
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
                m_editValues[QString("机构%1_牵制状态角").arg(i)] = QString::number(res, 'f', 2)+("°");
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
                m_editValues[QString("机构%1_储气罐压力1").arg(i)] = QString::number(res, 'f', 2)+("MPa");
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
                m_editValues[QString("机构%1_储气罐压力2").arg(i)] = QString::number(res ,'f', 2)+("MPa");
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

                m_editValues[QString("机构%1_内部温度1").arg(i)] = QString::number(res ,'f', 2)+("°C");
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

                m_editValues[QString("机构%1_内部温度2").arg(i)] = QString::number(res ,'f', 2)+"(°C)";
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

LaunchProcessDialog::LaunchProcessDialog(QWidget *parent)
    : QDialog(parent)
    , m_updateTimer(new QTimer(this))
    , m_worker(new FrameDatWorker())
    , m_workerThread(new QThread(this))
{
    this->setWindowTitle("发射流程界面-20251020");
    this->resize(1600, 950);
    setupUI();
    // 根据控制器名称动态设置窗口标题
    auto* handle = DataInteractionManager::getInstance().getMsgHandle();

    // 订阅实时数据
    handle->subMessage(this, ESubDataType::E_RealTimeData);
    // 1. 基础窗口设置
    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &LaunchProcessDialog::updateTime);
    m_timer->start(1000);
    updateTime();

    initWorkerThread();
    m_updateTimer_ser=new QTimer(this);
    m_updateTimer_ser->setSingleShot(true);  // 单次触发，避免重复执行
    m_updateTimer_ser->setInterval(10);      // 合并10ms内的所有数据更新
    connect(m_updateTimer_ser, &QTimer::timeout, this, &LaunchProcessDialog::onTimerTimeout_ser);
    // 3. 配置防抖定时器：10ms单次触发（可根据业务调整）
    m_updateTimer->setSingleShot(true);  // 单次触发，避免重复执行
    m_updateTimer->setInterval(10);      // 合并10ms内的所有数据更新

    connect(m_updateTimer, &QTimer::timeout, this, &LaunchProcessDialog::onTimerTimeout);

    // 监听测发控TCP连接状态（仅在状态切换时打印日志）
    QString tcpChannelId = ConfigHelper::getInstance().getValue("Communication/TCP_Name_ServerRemote", "tcp_device_serverRemote").toString();
    connect(&CommManager::instance(), &CommManager::channelStateChanged,
        this, [tcpChannelId, this](const QString& channelId, EChannelState state) {
            if (channelId != tcpChannelId) return;
            static EChannelState prevState = EChannelState::Disconnected;
            QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd hh:mm:ss");
            if (state == EChannelState::Connected && prevState != EChannelState::Connected) {
                if (m_logText) m_logText->append(QString("[%1] 与测发控建立连接").arg(timeStr));
            } else if ((state == EChannelState::Disconnected || state == EChannelState::Error) && prevState == EChannelState::Connected) {
                if (m_logText) m_logText->append(QString("[%1] 与测发控断开连接").arg(timeStr));
            }
            prevState = state;
        });

    // 运控记录支持右键清除
    if (m_logText) {
        m_logText->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(m_logText, &QWidget::customContextMenuRequested, this, [this](const QPoint& pos) {
            QMenu menu;
            menu.addAction("清除记录", this, [this]() {
                if (m_logText) m_logText->clear();
            });
            menu.exec(m_logText->mapToGlobal(pos));
        });
    }
}

LaunchProcessDialog::~LaunchProcessDialog() {
    m_workerThread->quit();
    m_workerThread->wait();
    DataInteractionManager::getInstance()
        .getMsgHandle()
        ->unSubMessageAll(this);
}
void LaunchProcessDialog::appendData(const QByteArray &data)
{
    QMutexLocker locker(&m_cacheMutex);
    m_dataCache.append(data);

    // 2. 重启防抖定时器（10ms内有新数据则重新计时，只处理最后一次）
    m_updateTimer->start();
}

void LaunchProcessDialog::clearPlaybackCache()
{
    QMutexLocker locker(&m_cacheMutex);
    m_dataCache.clear();
    m_updateTimer->stop();
}

void LaunchProcessDialog::onTimerTimeout()
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

void LaunchProcessDialog::onTimerTimeout_ser()
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

void LaunchProcessDialog::initWorkerThread()
{
    m_worker->moveToThread(m_workerThread);
    // 连接信号：子线程处理完成 → 主线程更新UI
    connect(m_worker, &FrameDatWorker::dataProcessed,
            this, &LaunchProcessDialog::onDataProcessed, Qt::QueuedConnection);
    // 启动子线程
    m_workerThread->start();
}

void LaunchProcessDialog::onDataProcessed(const QMap<QString, bool> &ledStates, const QMap<QString, QString> &editValues)
{
    updateControllerFrameUI(ledStates,editValues);

}

void LaunchProcessDialog::updateControllerFrameUI(const QMap<QString, bool> &ledStates, const QMap<QString, QString> &editValues)
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

void LaunchProcessDialog::setParam(const STParamInfo &param)
{
    QMutexLocker locker(&m_cacheMutex); // 加锁保证线程安全
    m_param = param;
    m_updateTimer_ser->start();
}

void LaunchProcessDialog::onMessage(IEvent *pEvent)
{
    if (!pEvent) return;

    switch (pEvent->getType()) {

    case EventType::E_InitiativeMsg: {
         // 设备数据（TCP / UDP / 串口 解析后投递）
         auto* pInit = static_cast<InitiativeMsgEvent*>(pEvent);
         const STParamInfo& param = pInit->getParamData();
         setParam(param);
        break;
    }

    case EventType::E_InternalMsg: {

        break;
    }

    default:
        break;

    }
}
void LaunchProcessDialog::setupUI()
{
    // ===================== 核心：清空映射表，避免重复添加 =====================
    m_ledMap.clear();
    m_valueMap.clear();

    // 全局样式表
    this->setStyleSheet(R"(
        QWidget { font-family: "SimHei"; font-size: 15px; }
        QLabel { color: black; }
        QGroupBox { font-weight: bold; border: 1px solid gray; border-radius: 5px; margin-top: 10px; padding-top: 15px; font-size: 16px; }
        StyledLineEdit {
            background-color: black;
            color: #00FF00;
            border: 1px solid #555;
            padding: 2px;
            font-weight: bold;
            font-family: "Courier New";
            font-size: 18px;
        }
        QPushButton {
            border: 1px solid gray;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #E0E0E0, stop:1 #A0A0A0);
            font-weight: bold;
        }
        QPushButton:pressed { background: #808080; }
    )");

    constexpr int colBaseWidth = 70;

    // ===================== 创建输入框 + 自动加入valueMap（中文Key） =====================
    auto createEdit = [&](const QString &editKey, const QString &val = "0.000", int width = -1) -> StyledLineEdit* {
        StyledLineEdit *edit = new StyledLineEdit();
        edit->setText(val);
        edit->setAlignment(Qt::AlignCenter);
        edit->setFixedHeight(30);
        edit->setGrayInputMode(false);
        edit->setNormalBackground(QColor(0, 0, 0));
        edit->setFocusBackground(QColor(0, 0, 0));

        int finalWidth = (width == -1) ? (colBaseWidth * 4) : width;
        edit->setCustomFixedWidth(finalWidth);

        // ✅ 中文Key存入映射表
        m_valueMap[editKey] = edit;
        return edit;
    };

    auto createDataLabel = [](const QString &text) {
        QLabel *lbl = new QLabel(text);
        lbl->setAlignment(Qt::AlignCenter);
        lbl->setFont(QFont("SimHei", 13, QFont::Bold));
        return lbl;
    };

    QVBoxLayout *mainLayout = new QVBoxLayout(this);
    mainLayout->setSpacing(10);
    mainLayout->setContentsMargins(10, 10, 10, 10);

    // ==========================================
    // 1. 顶部区域：电源数据 + 状态灯 + 时间 + 记录
    // ==========================================
    QHBoxLayout *topLayout = new QHBoxLayout();
    topLayout->setSpacing(10);

    // 1.1 电源数据（中文Key）
    QGroupBox *grpPower = new QGroupBox("电源数据");
    QGridLayout *gridPower = new QGridLayout(grpPower);
    gridPower->setHorizontalSpacing(10);
    gridPower->setVerticalSpacing(5);

    for(int i=0; i<6; i++) {
        gridPower->addWidget(createDataLabel(QString("电源电压 %1").arg(i+1)), 0, i);
        // Key：电源电压1、电源电压2...
        StyledLineEdit *boxV = createEdit(QString("电源电压%1").arg(i+1), "0.000V");
        boxV->setCustomFixedHeight(50);
        boxV->setCustomFixedWidth(110);
        gridPower->addWidget(boxV, 1, i);

        gridPower->addWidget(createDataLabel(QString("电源电流 %1").arg(i+1)), 2, i);
        // Key：电源电流1、电源电流2...
        StyledLineEdit *boxA = createEdit(QString("电源电流%1").arg(i+1), "0.000A");
        boxA->setCustomFixedWidth(110);
        boxA->setCustomFixedHeight(50);
        gridPower->addWidget(boxA, 3, i);
    }

    // 1.2 系统状态灯（中文Key）
    QGroupBox *grpStatus = new QGroupBox("系统状态");
    QGridLayout *gridStatus = new QGridLayout(grpStatus);
    gridStatus->setAlignment(Qt::AlignCenter);

    auto createStatusItem = [&](const QString &ledKey, QString name, StyledLedLabel *&light) {
        QWidget *w = new QWidget();
        QVBoxLayout *l = new QVBoxLayout(w);
        l->setAlignment(Qt::AlignCenter);
        l->setSpacing(5);
        QLabel *txt = new QLabel(name);
        txt->setFont(QFont("SimHei", 13, QFont::Bold));
        txt->setAlignment(Qt::AlignCenter);
        light = new StyledLedLabel();
        light->setFixedSize(60, 30);
        light->setStyleSheet(R"(
            QLabel { background-color: #274811; border-radius: 15px; border: 1px solid #444; }
        )");
        light->setSwitchable(false);
        light->setDisabledLed(false);
        // ✅ 中文Key存入LED映射表
        m_ledMap[ledKey] = light;
        l->addWidget(txt);
        l->addWidget(light, 0, Qt::AlignCenter);
        return w;
    };

    // 状态灯：纯中文Key
    gridStatus->addWidget(createStatusItem("电源",      "电源",     m_lightPower),      0, 0);
    gridStatus->addWidget(createStatusItem("控制器",    "控制器",   m_lightCtrl),       0, 1);
    gridStatus->addWidget(createStatusItem("测发控",    "测发控",   m_lightMeasure),    0, 2);
    gridStatus->addWidget(createStatusItem("机构好",    "机构好",   m_lightMechGood),   0, 3);
    gridStatus->addWidget(createStatusItem("发射条件",  "发射条件", m_lightCondition),  0, 4);

    // 1.3 时间与记录
    QVBoxLayout *rightTopLayout = new QVBoxLayout();
    rightTopLayout->setSpacing(10);
    QFrame *frameTime = new QFrame();
    frameTime->setStyleSheet("background-color: black; border: 2px solid gray;");
    QVBoxLayout *layTime = new QVBoxLayout(frameTime);
    layTime->setAlignment(Qt::AlignCenter);
    layTime->setContentsMargins(0,0,0,0);
    layTime->setSpacing(0);

    QLabel *lblTitle = new QLabel("北京时间");
    lblTitle->setAlignment(Qt::AlignCenter);
    lblTitle->setStyleSheet("color: #00FF00; font-weight: bold; font-size: 20px;");
    m_lblTime = new QLabel("09:19:03");
    m_lblTime->setAlignment(Qt::AlignCenter);
    m_lblTime->setStyleSheet("font-size: 30px; font-weight: bold; color: #00FF00;");
    layTime->addWidget(lblTitle);
    layTime->addWidget(m_lblTime);

    QGroupBox *grpRecord = new QGroupBox("远控记录");
    QVBoxLayout *layRecord = new QVBoxLayout(grpRecord);
    m_logText = new QTextEdit();
    m_logText->setReadOnly(true);
    m_logText->setStyleSheet("background: white; color: black;");
    layRecord->addWidget(m_logText);

    rightTopLayout->addWidget(frameTime);
    rightTopLayout->addWidget(grpRecord);
    topLayout->addWidget(grpPower, 6);
    topLayout->addWidget(grpStatus, 4);
    topLayout->addLayout(rightTopLayout, 3);

    // ==========================================
    // 2. 中部区域：工作模式 + 电缆状态灯
    // ==========================================
    QFrame *midFrame = new QFrame();
    midFrame->setFrameShape(QFrame::Box);
    QVBoxLayout *vMidLayout = new QVBoxLayout(midFrame);
    vMidLayout->setSpacing(15);
    vMidLayout->setContentsMargins(10, 10, 10, 10);

    // 2.1 数据条（中文Key）
    QHBoxLayout *topMidLayout = new QHBoxLayout();
    QVBoxLayout *layMode = new QVBoxLayout();
    layMode->setAlignment(Qt::AlignCenter);
    layMode->setSpacing(8);
    QLabel *lblModeTitle = new QLabel("工作模式");
    lblModeTitle->setFont(QFont("SimHei", 13, QFont::Bold));
    lblModeTitle->setAlignment(Qt::AlignCenter);
    QLabel *lblModeVal = new QLabel("NORMAL");
    lblModeVal->setStyleSheet("background: black; color: #00FF00; padding: 5px; border: 1px solid gray;");
    lblModeVal->setAlignment(Qt::AlignCenter);
    lblModeVal->setFixedHeight(30);
    lblModeVal->setFixedWidth(180);
    layMode->addWidget(lblModeTitle);
    layMode->addWidget(lblModeVal);

    QStringList headers = {"数字供电", "驱动供电1", "驱动供电2", "数字5V1", "数字5V2", "数字5V3", "温度"};
    QHBoxLayout *layDataStrip = new QHBoxLayout();
    layDataStrip->setSpacing(8);
    for(int i=0; i<headers.size(); i++) {
        QVBoxLayout *l = new QVBoxLayout();
        l->setAlignment(Qt::AlignCenter);
        l->setSpacing(5);
        QLabel *title = new QLabel(headers[i]);
        title->setFont(QFont("SimHei", 13, QFont::Bold));
        title->setAlignment(Qt::AlignCenter);
        // ✅ 直接用标题做中文Key
        StyledLineEdit *edit = createEdit(headers[i], "0.000");
        edit->setFixedWidth(100);
        l->addWidget(title);
        l->addWidget(edit);
        layDataStrip->addLayout(l);
    }

    topMidLayout->addLayout(layMode, 1);
    topMidLayout->addLayout(layDataStrip, 4);
    vMidLayout->addLayout(topMidLayout);

    // 2.2 电缆状态灯（纯中文Key）
    QHBoxLayout *bottomStatusLayout = new QHBoxLayout();
    bottomStatusLayout->addStretch();
    QGridLayout *gridBottomStatus = new QGridLayout();
    gridBottomStatus->setAlignment(Qt::AlignCenter);
    gridBottomStatus->setHorizontalSpacing(20);

    auto addLightItem = [&](const QString &ledKey, QString txt, StyledLedLabel *&light) {
        QWidget *w = new QWidget();
        QVBoxLayout *l = new QVBoxLayout(w);
        l->setAlignment(Qt::AlignCenter);
        l->setSpacing(5);
        QLabel *label = new QLabel(txt);
        label->setFont(QFont("SimHei", 13, QFont::Bold));
        light = new StyledLedLabel();
        light->setFixedSize(50, 25);
        light->setStyleSheet(R"(
            QLabel { background-color: #274811; border-radius: 12px; border: 1px solid #444; }
        )");
        light->setSwitchable(false);
        light->setDisabledLed(false);
        // ✅ 中文Key
        m_ledMap[ledKey] = light;
        l->addWidget(label);
        l->addWidget(light);
        return w;
    };

    gridBottomStatus->addWidget(addLightItem("电缆连接",  "电缆连接",  m_lightCable),     0, 0);
    gridBottomStatus->addWidget(addLightItem("5VK状态",  "5VK状态",   m_light5VK),       0, 1);
    gridBottomStatus->addWidget(addLightItem("锁定到位",  "锁定到位",  m_lightLockPos),   0, 2);
    gridBottomStatus->addWidget(addLightItem("复位到位",  "复位到位",  m_lightResetPos),  0, 3);
    gridBottomStatus->addWidget(addLightItem("牵制拉力",  "牵制拉力",  m_lightPullForce), 0, 4);
    gridBottomStatus->addWidget(addLightItem("储罐压力",  "储罐压力",  m_lightTankPress), 0, 5);
    gridBottomStatus->addWidget(addLightItem("供气状态",  "供气状态",  m_lightGasState),  0, 6);
    gridBottomStatus->addWidget(addLightItem("火保解除",  "火保解除",  m_lightFireSafe),  0, 7);
    gridBottomStatus->addWidget(addLightItem("释放准备好","释放准备好",m_lightReady),     0, 8);

    bottomStatusLayout->addLayout(gridBottomStatus);
    bottomStatusLayout->addStretch();
    vMidLayout->addLayout(bottomStatusLayout);

    // ==========================================
    // 3. 下部区域：机构 1-4（中文Key：机构1_牵制拉力1）
    // ==========================================
    QHBoxLayout *bottomLayout = new QHBoxLayout();
    QGridLayout *gridMech = new QGridLayout();
    gridMech->setSpacing(10);

    auto createMechBox = [&](int id) -> QGroupBox* {
        QGroupBox *box = new QGroupBox(QString("机构%1").arg(id));
        QGridLayout *l = new QGridLayout(box);
        l->setHorizontalSpacing(5);
        l->setVerticalSpacing(8);
        l->setContentsMargins(5, 20, 5, 5);

        // 中文Key前缀：机构1_、机构2_...
        QString keyPrefix = QString("机构%1_").arg(id);

        l->addWidget(createDataLabel("牵制拉力1"), 0, 0, 1, 2);
        l->addWidget(createDataLabel("牵制拉力2"), 0, 2, 1, 2);
        l->addWidget(createEdit(keyPrefix + "牵制拉力1", "0.000kN", colBaseWidth * 2), 1, 0, 1, 2);
        l->addWidget(createEdit(keyPrefix + "牵制拉力2", "0.000kN", colBaseWidth * 2), 1, 2, 1, 2);

        l->addWidget(createDataLabel("牵制状态角"), 2, 0, 1, 4);
        l->addWidget(createEdit(keyPrefix + "牵制状态角", "0.000°"), 3, 0, 1, 4);

        l->addWidget(createDataLabel("储气罐压力1"), 4, 0, 1, 2);
        l->addWidget(createDataLabel("储气罐压力2"), 4, 2, 1, 2);
        l->addWidget(createEdit(keyPrefix + "储气罐压力1", "0.000MPa", colBaseWidth * 2), 5, 0, 1, 2);
        l->addWidget(createEdit(keyPrefix + "储气罐压力2", "0.000MPa", colBaseWidth * 2), 5, 2, 1, 2);

        l->addWidget(createDataLabel("内部温度1"), 6, 0, 1, 2);
        l->addWidget(createDataLabel("内部温度2"), 6, 2, 1, 2);
        l->addWidget(createEdit(keyPrefix + "内部温度1", "0.000°C", colBaseWidth * 2), 7, 0, 1, 2);
        l->addWidget(createEdit(keyPrefix + "内部温度2", "0.000°C", colBaseWidth * 2), 7, 2, 1, 2);

        l->addWidget(createDataLabel("解锁到位"), 8, 0, 1, 1, Qt::AlignCenter);
        l->addWidget(createDataLabel("释放好"),   8, 1, 1, 2, Qt::AlignCenter);
        l->addWidget(createDataLabel("释放到位"), 8, 3, 1, 1, Qt::AlignCenter);

        l->addWidget(createEdit(keyPrefix + "解锁到位", "0ms", colBaseWidth * 1), 9, 0, 1, 1, Qt::AlignCenter);
        l->addWidget(createEdit(keyPrefix + "释放好",   "0ms", colBaseWidth * 2), 9, 1, 1, 2, Qt::AlignCenter);
        l->addWidget(createEdit(keyPrefix + "释放到位", "0ms", colBaseWidth * 1), 9, 3, 1, 1, Qt::AlignCenter);

        l->setColumnStretch(0, 1);
        l->setColumnStretch(1, 1);
        l->setColumnStretch(2, 1);
        l->setColumnStretch(3, 1);
        return box;
    };

    gridMech->addWidget(createMechBox(1), 0, 0);
    gridMech->addWidget(createMechBox(2), 0, 1);
    gridMech->addWidget(createMechBox(3), 0, 2);
    gridMech->addWidget(createMechBox(4), 0, 3);

    // ==========================================
    // 4. 右侧操作按钮栏（无改动）
    // ==========================================
    QVBoxLayout *rightBtnLayout = new QVBoxLayout();
    rightBtnLayout->setSpacing(15);

    // 自动供气
    QHBoxLayout *btnRow1 = new QHBoxLayout();
    QLabel *indicator1 = new QLabel();
    indicator1->setFixedSize(20, 30);
    indicator1->setStyleSheet("background: black; border: 1px solid gray;");
    m_btnAutoGas = new QPushButton("自动供气");
    m_btnAutoGas->setFixedHeight(50);
    m_btnAutoGas->setFont(QFont("SimHei", 14, QFont::Bold));
    m_btnAutoGas->setCheckable(true);
    m_statusBarAutoGas = new QLabel();
    m_statusBarAutoGas->setFixedSize(15, 40);
    m_statusBarAutoGas->setStyleSheet("background: #274811; border: 1px solid gray;");
    btnRow1->addWidget(indicator1);
    btnRow1->addWidget(m_btnAutoGas);
    btnRow1->addWidget(m_statusBarAutoGas);
    rightBtnLayout->addLayout(btnRow1);
    connect(m_btnAutoGas, &QPushButton::clicked, this, &LaunchProcessDialog::onToggleButtonClicked);

    // 手动补气
    QHBoxLayout *btnRow2 = new QHBoxLayout();
    QLabel *indicator2 = new QLabel();
    indicator2->setFixedSize(20, 30);
    indicator2->setStyleSheet("background: black; border: 1px solid gray;");
    m_btnManualGas = new QPushButton("手动补气");
    m_btnManualGas->setFixedHeight(50);
    m_btnManualGas->setFont(QFont("SimHei", 14, QFont::Bold));
    m_btnManualGas->setCheckable(true);
    m_statusBarManualGas = new QLabel();
    m_statusBarManualGas->setFixedSize(15, 40);
    m_statusBarManualGas->setStyleSheet("background: #274811; border: 1px solid gray;");
    btnRow2->addWidget(indicator2);
    btnRow2->addWidget(m_btnManualGas);
    btnRow2->addWidget(m_statusBarManualGas);
    rightBtnLayout->addLayout(btnRow2);
    connect(m_btnManualGas, &QPushButton::clicked, this, &LaunchProcessDialog::onToggleButtonClicked);

    // 切换自动模式
    QHBoxLayout *btnRow3 = new QHBoxLayout();
    QLabel *indicator3 = new QLabel();
    indicator3->setFixedSize(20, 30);
    indicator3->setStyleSheet("background: black; border: 1px solid gray;");
    m_btnSwitchAutoMode = new QPushButton("切换自动模式");
    m_btnSwitchAutoMode->setFixedHeight(50);
    m_btnSwitchAutoMode->setFont(QFont("SimHei", 14, QFont::Bold));
    m_btnSwitchAutoMode->setCheckable(true);
    m_statusBarSwitchAutoMode = new QLabel();
    m_statusBarSwitchAutoMode->setFixedSize(15, 40);
    m_statusBarSwitchAutoMode->setStyleSheet("background: #274811; border: 1px solid gray;");
    btnRow3->addWidget(indicator3);
    btnRow3->addWidget(m_btnSwitchAutoMode);
    btnRow3->addWidget(m_statusBarSwitchAutoMode);
    rightBtnLayout->addLayout(btnRow3);
    connect(m_btnSwitchAutoMode, &QPushButton::clicked, this, &LaunchProcessDialog::onToggleButtonClicked);

    // 火工品解保
    QHBoxLayout *btnRow4 = new QHBoxLayout();
    QLabel *indicator4 = new QLabel();
    indicator4->setFixedSize(20, 30);
    indicator4->setStyleSheet("background: black; border: 1px solid gray;");
    m_btnFireSafeRelease = new QPushButton("火工品解保");
    m_btnFireSafeRelease->setFixedHeight(50);
    m_btnFireSafeRelease->setFont(QFont("SimHei", 14, QFont::Bold));
    m_btnFireSafeRelease->setCheckable(true);
    m_statusBarFireSafeRelease = new QLabel();
    m_statusBarFireSafeRelease->setFixedSize(15, 40);
    m_statusBarFireSafeRelease->setStyleSheet("background: #274811; border: 1px solid gray;");
    btnRow4->addWidget(indicator4);
    btnRow4->addWidget(m_btnFireSafeRelease);
    btnRow4->addWidget(m_statusBarFireSafeRelease);
    rightBtnLayout->addLayout(btnRow4);
    connect(m_btnFireSafeRelease, &QPushButton::clicked, this, &LaunchProcessDialog::onToggleButtonClicked);

    rightBtnLayout->addStretch();
    QPushButton *btnExit = new QPushButton("退出");
    btnExit->setFixedHeight(40);
    btnExit->setFont(QFont("SimHei", 14, QFont::Bold));
    connect(btnExit, &QPushButton::clicked, this, &LaunchProcessDialog::onExitClicked);
    rightBtnLayout->addWidget(btnExit);

    bottomLayout->addLayout(gridMech, 4);
    bottomLayout->addLayout(rightBtnLayout, 1);
    mainLayout->addLayout(topLayout, 3);
    mainLayout->addWidget(midFrame, 2);
    mainLayout->addLayout(bottomLayout, 5);
}
void LaunchProcessDialog::updateTime()
{
    m_lblTime->setText(QTime::currentTime().toString("hh:mm:ss"));
}

void LaunchProcessDialog::onExitClicked()
{
    this->close();
}

void LaunchProcessDialog::onToggleButtonClicked(bool checked)
{
    QPushButton *btn = qobject_cast<QPushButton*>(sender());
    if (!btn) return;

    // 按钮选中/取消的样式
    QString btnStyle = checked ?
        R"(
            border: 2px solid #00FF00;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #606060, stop:1 #202020);
            font-weight: bold;
            color: #00FF00;
        )" :
        R"(
            border: 1px solid gray;
            background: qlineargradient(x1:0, y1:0, x2:0, y2:1, stop:0 #E0E0E0, stop:1 #A0A0A0);
            font-weight: bold;
            color: black;
        )";
    btn->setStyleSheet(btnStyle);

    // 对应的状态条颜色
    QLabel *statusBar = nullptr;
    if (btn == m_btnAutoGas) {
        statusBar = m_statusBarAutoGas;
        // 自动供气按钮单独处理：按下绿色，取消墨绿
        if (statusBar) {
            QString statusStyle = checked ?
                QString("background: #00FF00; border: 1px solid gray;") : // 按下绿色
                QString("background: #274811; border: 1px solid gray;"); // 取消墨绿
            statusBar->setStyleSheet(statusStyle);
        }
    } else if (btn == m_btnManualGas) {
        statusBar = m_statusBarManualGas;
    } else if (btn == m_btnSwitchAutoMode) {
        statusBar = m_statusBarSwitchAutoMode;
    } else if (btn == m_btnFireSafeRelease) {
        statusBar = m_statusBarFireSafeRelease;
    }

    // 其他按钮保持原有颜色逻辑（选中红，未选中绿）
    if (statusBar && btn != m_btnAutoGas) {
        QString statusStyle = checked ?
                    QString("background: #00FF00; border: 1px solid gray;") : // 按下绿色
                    QString("background: #274811; border: 1px solid gray;"); // 取消墨绿
        statusBar->setStyleSheet(statusStyle);
    }
}
