#include "launchframedialog.h"
#include <QGridLayout>
#include <QGroupBox>
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QFrame>
#include <QLineEdit>
#include <QSpacerItem>
#include <QDebug>
#include <QtEndian>
#include <QTimer>
// 引入自定义控件头文件
#include "styledledlabel.h"
#include "styledlineedit.h"
#include "src/DataProcess/DataAnalysis/FrameDataAnalysis.h"
#include <QCoreApplication>
#include <QSettings>
#include <QDir>
#include <QScriptEngine>
#include <QDebug>
#include <cmath>

double FrameDataWorker::calculateCoeff(double x, int row, int col)
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
void FrameDataWorker::processData(const QByteArray &data)
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

void FrameDataWorker::processData_sel( STParamInfo &param)
{
    QMap<QString, bool> ledStates;
    QMap<QString, QString> editValues;
    paramProcess(param,ledStates,editValues);
    emit dataProcessed(ledStates, editValues);
}

void FrameDataWorker::paramProcess(STParamInfo &m_param, QMap<QString, bool> &m_ledStates,  QMap<QString, QString> &m_editValues)
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
            m_ledStates["转发允释1"] = signalForward1;
            m_ledStates["机构1连接"] = xf01a1Status1;
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
            m_ledStates["转发允释2"] = signalForward2;
            m_ledStates["机构2连接"] = xf01a1Status2;
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
            m_ledStates["机构3连接"] = xf01a2Status1;
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
            m_ledStates["机构4连接"] = xf01a2Status2;
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
            m_ledStates["XF01D连接"] = xf01dConnectStatus;
            m_ledStates["XF01E连接"] = xf01eConnectStatus;
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
            m_ledStates["机构2释放好"] = mechanism2ReleaseOk;
            m_ledStates["机构1释放好"] = mechanism1ReleaseOk;
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
            m_ledStates["机构4释放好"] = mechanism4ReleaseOk;
            m_ledStates["机构3释放好"] = mechanism3ReleaseOk;
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
            m_editValues["解锁到位时间1"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism1ReleaseTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["释放好时间1"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism1ReleaseInPlaceTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["释放到位时间1"] =  QString::number(rawValue);
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
            m_editValues["解锁到位时间2"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism2ReleaseTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["释放好时间2"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism2ReleaseInPlaceTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["释放到位时间2"] =  QString::number(rawValue);
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
            m_editValues["解锁到位时间3"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism3ReleaseTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["释放好时间3"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism3ReleaseInPlaceTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["释放到位时间3"] =  QString::number(rawValue);
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
            m_editValues["解锁到位时间4"] =  QString::number(rawValue);
        }
        else if(mapIt.key()=="Mechanism4ReleaseTime")
        {
            // 2字节采集值，高字节在前，低字节在后
            qint16 rawValue = 0;
            if (item.varParaValue.canConvert<qint16>()) {
                rawValue = item.varParaValue.value<qint16>();
            }

            // 将计算后的电压值存入状态字典
            m_editValues["释放好时间4"] =  QString::number(rawValue);
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
                m_ledStates[QString("校验%1").arg(i)] = (rawValue != 0);
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
                m_editValues[QString("拉力1%1").arg(i)] = QString::number(res, 'f', 2);
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
                m_editValues[QString("拉力2%1").arg(i)] = QString::number(res, 'f', 2);
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
                m_editValues[QString("角度%1").arg(i)] = QString::number(res, 'f', 2);
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

                // 存入状态字典
                m_editValues[QString("压力1%1").arg(i)] = QString::number(res, 'f', 2);
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
                m_editValues[QString("压力2%1").arg(i)] = QString::number(res ,'f', 2);
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

                m_editValues[QString("温度1%1").arg(i)] = QString::number(res ,'f', 2);
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

                m_editValues[QString("温度2%1").arg(i)] = QString::number(res ,'f', 2);
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

LaunchFrameDialog::LaunchFrameDialog(QWidget *parent)
    : QWidget(parent)
    , m_updateTimer(new QTimer(this))
    , m_worker(new FrameDataWorker())
    , m_workerThread(new QThread(this))
{
    setWindowTitle("发射帧解析详情");
    resize(1800, 650);
//    m_analy=new FrameDataAnalysis;
    setupUI();
    // 2. 初始化子线程（分离耗时操作）
    initWorkerThread();
    m_updateTimer_ser=new QTimer(this);
    m_updateTimer_ser->setSingleShot(true);  // 单次触发，避免重复执行
    m_updateTimer_ser->setInterval(10);      // 合并10ms内的所有数据更新
    connect(m_updateTimer_ser, &QTimer::timeout, this, &LaunchFrameDialog::onTimerTimeout_ser);
    // 3. 配置防抖定时器：10ms单次触发（可根据业务调整）
    m_updateTimer->setSingleShot(true);  // 单次触发，避免重复执行
    m_updateTimer->setInterval(10);      // 合并10ms内的所有数据更新

    connect(m_updateTimer, &QTimer::timeout, this, &LaunchFrameDialog::onTimerTimeout);

}
void LaunchFrameDialog::initWorkerThread()
{
    // 将工作对象移到子线程
    m_worker->moveToThread(m_workerThread);
    // 连接信号：子线程处理完成 → 主线程更新UI
    connect(m_worker, &FrameDataWorker::dataProcessed,
            this, &LaunchFrameDialog::onDataProcessed, Qt::QueuedConnection);
    // 启动子线程
    m_workerThread->start();
}
LaunchFrameDialog::~LaunchFrameDialog()
{
    // 安全停止子线程
    m_workerThread->quit();
    m_workerThread->wait();
}
// 重置UI到初始状态
void LaunchFrameDialog::resetUI()
{
    // 重置所有LED为关闭状态
    for (StyledLedLabel *led : m_allLeds) {
        if (led) {
            led->setOn(false);
            led->setDisabledLed(false); // 恢复禁用状态为默认
        }
    }

    // 重置所有输入框为初始值
    for (StyledLineEdit *edit : m_allLineEdits) {
        if (edit) {
            // 区分不同初始值：帧长/校验等初始为0，其他为--
            if (edit->text() == "0" || edit->property("isInitZero").toBool()) {
                edit->setText("0");
            } else {
                edit->setText("--");
            }
            edit->setGrayInputMode(true);
        }
    }
}

void LaunchFrameDialog::setParam(const STParamInfo &param)
{
    QMutexLocker locker(&m_cacheMutex); // 加锁保证线程安全
    m_param = param;
    m_updateTimer_ser->start();
}

// 新增：更新控制器发射帧解析结果UI
void LaunchFrameDialog::updateControllerFrameUI(const QMap<QString, bool> &ledStates, const QMap<QString, QString> &editValues)
{
    // 1. 更新LED状态（灯亮/灭）
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

void LaunchFrameDialog::appendData(const QByteArray &data)
{
    QMutexLocker locker(&m_cacheMutex);
    m_dataCache.append(data);

    // 2. 重启防抖定时器（10ms内有新数据则重新计时，只处理最后一次）
    m_updateTimer->start();

    //    upDataUi();
}

void LaunchFrameDialog::clearPlaybackCache()
{
    QMutexLocker locker(&m_cacheMutex);
    m_dataCache.clear();
    m_updateTimer->stop();
}

void LaunchFrameDialog::onTimerTimeout_ser()
{
    // 定时器超时，处理缓存的所有数据
    QMutexLocker locker(&m_cacheMutex);
    if (!m_param.getID().startsWith("serial_")) return;

    // 拷贝缓存数据并清空（释放锁，避免阻塞主线程）
    STParamInfo param = m_param;
    m_param.channelId="";
    locker.unlock();
    QMetaObject::invokeMethod(m_worker, "processData_sel",
                              Qt::QueuedConnection,
                              Q_ARG(STParamInfo, param));
}

void LaunchFrameDialog::onTimerTimeout()
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
void LaunchFrameDialog::onDataProcessed(const QMap<QString, bool> &ledStates, const QMap<QString, QString> &editValues)
{
    m_ledStates=ledStates;
    m_editValues=editValues;
    updateControllerFrameUI(m_ledStates,m_editValues);
}

// 完整函数实现
void LaunchFrameDialog::setupUI()
{
    // 清空控件列表（防止重复创建）
    m_allLeds.clear();
    m_allLineEdits.clear();
    m_ledMap.clear();   // 清空LED映射
    m_valueMap.clear(); // 清空输入框映射

    // 主布局：水平布局（左右分栏）
    QHBoxLayout *mainLayout = new QHBoxLayout(this);
    mainLayout->setContentsMargins(10, 10, 10, 10);
    mainLayout->setSpacing(10);

    // 区域1：控制器发射帧解析结果
    QGroupBox *topBox = new QGroupBox("控制器发射帧解析结果");
    QVBoxLayout *topBoxLayout = new QVBoxLayout(topBox);
    topBoxLayout->setContentsMargins(10, 20, 10, 10);
    QGridLayout *grid = new QGridLayout;
    grid->setHorizontalSpacing(15);
    grid->setVerticalSpacing(2);

    auto createLedItem = [&](const QString &text, bool disabled = false) {
        QHBoxLayout *hLay = new QHBoxLayout;
        hLay->setContentsMargins(0, 0, 0, 0);
        hLay->setSpacing(4);
        StyledLedLabel *led = new StyledLedLabel(this);
        led->setOn(false);
        led->setFixedSize(20, 20);
        if (disabled) led->setDisabledLed(true);
        // 记录所有LED控件 + 按名称映射
        m_allLeds.append(led);
        m_ledMap[text] = led;

        QLabel *lbl = new QLabel(text);
        hLay->addWidget(led, 0, Qt::AlignLeft | Qt::AlignVCenter);
        hLay->addWidget(lbl, 1, Qt::AlignLeft | Qt::AlignVCenter);
        QWidget *w = new QWidget;
        w->setLayout(hLay);
        return w;
    };

    QStringList c1 = {"火保K1", "火保K2", "火保K3", "火保K4", "火保1", "火保2", "火保3", "火保4", "非解控JKt", "非解控JKb", "解控K1", "解控K2", "解控K3", "解控K4"};
    QStringList c2 = {"XF01D连接", "XF01E连接", "机构1连接", "机构1释放主", "机构1释放备", "机构1锁定", "机构1供气", "SFH1_7.1_1", "SFH1_7.2_1", "SFH1_7.3_1", "机构1释放好", "火引爆1", "5VK打开"};
    QStringList c3 = {"地面允释", "箭上允释", "机构2连接", "机构2释放主", "机构2释放备", "机构2锁定", "机构2供气", "SFH2_7.1", "SFH2_7.2", "SFH2_7.3", "机构2释放好", "火引爆2"};
    QStringList c4 = {"转发允释1", "转发允释2", "机构3连接", "机构3释放主", "机构3释放备", "机构3锁定", "机构3供气", "SFH3_7.1", "SFH3_7.2", "SFH3_7.3", "机构3释放好", "火引爆3"};
    QStringList c5 = {"开锁", "牵制释放好", "机构4连接", "机构4释放主", "机构4释放备", "机构4锁定", "机构4供气", "SFH4_7.1", "SFH4_7.2", "SFH4_7.3", "机构4释放好", "火引爆4"};

    auto fillCol = [&](const QStringList &list, int col) {
        for (int i = 0; i < list.size(); ++i) {
            bool disabled = (i >= 2);
            grid->addWidget(createLedItem(list[i], disabled), i, col);
        }
    };
    fillCol(c1, 0);
    fillCol(c2, 1);
    fillCol(c3, 2);
    fillCol(c4, 3);
    fillCol(c5, 4);

    QFrame *vLine = new QFrame;
    vLine->setFrameShape(QFrame::VLine);
    vLine->setFrameShadow(QFrame::Sunken);
    grid->addWidget(vLine, 0, 5, 14, 1);

    QStringList rightKeys = {"帧长", "帧计数", "帧类型", "时间标志", "数字供电", "驱动供电1", "驱动供电2", "5V1", "5V2", "5V3", "数字板温度", "火引爆装订时间", "继电器关闭时间"};
    for (int i = 0; i < rightKeys.size(); ++i) {
        QLabel *k = new QLabel(rightKeys[i] + ":");
        k->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        k->setFixedWidth(70);
        StyledLineEdit *v = new StyledLineEdit(this);
        v->setFixedWidth(80);
        v->setText("--");
        v->setGrayInputMode(true);
        // 记录所有输入框 + 按名称映射
        m_allLineEdits.append(v);
        m_valueMap[rightKeys[i]] = v;

        QHBoxLayout *h = new QHBoxLayout;
        h->setContentsMargins(0, 0, 0, 0);
        h->addWidget(k);
        h->addWidget(v);
        QWidget *w = new QWidget;
        w->setLayout(h);
        grid->addWidget(w, i, 6);
    }
    topBoxLayout->addLayout(grid);

    // ====================== 区域2：时序数据与校验（精准匹配截图样式） ======================
    QGroupBox *leftGroupBox = new QGroupBox("时序数据与校验");
    QGridLayout *mergeLayout = new QGridLayout(leftGroupBox);
    mergeLayout->setContentsMargins(10, 15, 10, 15);
    mergeLayout->setHorizontalSpacing(8);
    mergeLayout->setVerticalSpacing(0);
    mergeLayout->setAlignment(Qt::AlignTop);

    // 行1：帧长 / 校验结果标签
    QLabel *lblFrameLen = new QLabel("帧长");
    lblFrameLen->setAlignment(Qt::AlignCenter);
    mergeLayout->addWidget(lblFrameLen, 1, 0);

    QLabel *lblCheck = new QLabel("校验结果");
    lblCheck->setAlignment(Qt::AlignCenter);
    mergeLayout->addWidget(lblCheck, 1, 1);

    // 行2：帧长 / 校验结果输入框
    StyledLineEdit *leFrameLen = new StyledLineEdit(this);
    leFrameLen->setFixedSize(40, 22);
    leFrameLen->setText("0");
    leFrameLen->setAlignment(Qt::AlignCenter);
    leFrameLen->setProperty("isInitZero", true);
    m_allLineEdits.append(leFrameLen);
    m_valueMap["时序_帧长"] = leFrameLen;
    mergeLayout->addWidget(leFrameLen, 2, 0);

    StyledLineEdit *leCheck = new StyledLineEdit(this);
    leCheck->setFixedHeight(22);
    leCheck->setFixedWidth(80);
    leCheck->setAlignment(Qt::AlignCenter);
    leCheck->setGrayInputMode(true);
    m_allLineEdits.append(leCheck);
    m_valueMap["校验结果"] = leCheck;
    mergeLayout->addWidget(leCheck, 2, 1);

    // 数据配置：严格匹配截图
    QStringList leftValues = {"7B", "0", "0", "0", ""};       // 左侧数值（火引爆行空）
    QStringList rightLabels = {"帧类型", "工作模式", "FPGA_ID", "命令码", ""}; // 描述标签（火引爆行空）
    QStringList timeLabels = {"解锁时间", "解锁到位时间", "释放好时间", "释放到位时间", "火引爆到位时间"}; // 时间标签
    const int dataRowStart = 3;

    // 循环创建5行数据（前4行有校验框，第5行无）
    for(int i = 0; i < 5; ++i) {
        int currentRow = dataRowStart + i;

        // 1. 左侧数值输入框（仅前4行显示，第5行占位）
        if (!leftValues[i].isEmpty()) {
            StyledLineEdit *valEdit = new StyledLineEdit(this);
            valEdit->setFixedSize(40, 22);
            valEdit->setAlignment(Qt::AlignCenter);
            valEdit->setText(leftValues[i]);
            valEdit->setProperty("isInitZero", leftValues[i] == "0");
            if(i > 0) valEdit->setGrayInputMode(true); // 仅帧类型可编辑，其余置灰
            m_allLineEdits.append(valEdit);
           if(rightLabels[i]=="帧类型")
           {
               m_valueMap["时序_帧类型"] = valEdit;

           }
           else
           {
               m_valueMap[rightLabels[i]] = valEdit;
           }
            mergeLayout->addWidget(valEdit, currentRow, 0);
        } else {
            mergeLayout->addItem(new QSpacerItem(40,22), currentRow, 0);
        }

        // 2. 校验结果列（核心修复：仅前4行创建输入框，第5行占位）
        if (i < 4) { // 仅帧类型/工作模式/FPGA_ID/命令码行显示校验框
            StyledLineEdit *checkEdit = new StyledLineEdit(this);
            checkEdit->setFixedSize(80, 22);
            checkEdit->setAlignment(Qt::AlignCenter);
            checkEdit->setGrayInputMode(true);
            m_allLineEdits.append(checkEdit);
            m_valueMap[rightLabels[i] + "_校验"] = checkEdit;
            mergeLayout->addWidget(checkEdit, currentRow, 1);
        } else { // 火引爆行：校验列用占位符，无输入框
            mergeLayout->addItem(new QSpacerItem(80,22), currentRow, 1);
        }

        // 3. 描述标签（仅前4行显示，第5行占位）
        if (!rightLabels[i].isEmpty()) {
            QLabel *descLabel = new QLabel(rightLabels[i]);
            descLabel->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
            descLabel->setFixedWidth(60);
            mergeLayout->addWidget(descLabel, currentRow, 2);
        } else {
            mergeLayout->addItem(new QSpacerItem(60,22), currentRow, 2);
        }

        // 4. 4列时间输入框（5行都显示，匹配截图）
        for(int col = 3; col <= 6; ++col) {
            StyledLineEdit *blackEdit = new StyledLineEdit(this);
            blackEdit->setFixedSize(40, 22);
            blackEdit->setAlignment(Qt::AlignCenter);
            m_allLineEdits.append(blackEdit);
            int colNum = col - 2;
            m_valueMap[timeLabels[i] + QString::number(colNum)] = blackEdit;
            mergeLayout->addWidget(blackEdit, currentRow, col);
        }

        // 5. 时间标签（5行都显示，匹配截图）
        QLabel *timeLbl = new QLabel(timeLabels[i]);
        timeLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
        timeLbl->setFixedWidth(120);
        mergeLayout->addWidget(timeLbl, currentRow, 7);
    }

    mergeLayout->setColumnStretch(7, 1);
    // ==========================================================================================

    // 右侧：采集器汇总解析结果
    QGroupBox *collectorBox = new QGroupBox("采集器汇总解析结果");
    m_collectorGrid = new QGridLayout(collectorBox);
    m_collectorGrid->setContentsMargins(10, 20, 10, 10);
    m_collectorGrid->setHorizontalSpacing(8);
    m_collectorGrid->setVerticalSpacing(2);
    m_collectorGrid->setAlignment(Qt::AlignTop);

    for(int c = 0; c < 4; ++c) {
        QLabel *colLbl = new QLabel(QString::number(c+1));
        colLbl->setFixedWidth(16);
        colLbl->setAlignment(Qt::AlignCenter);
        m_collectorGrid->addWidget(colLbl, 0, c);
    }

    struct CollectorRow {
        QString paramName;
        QString rowLabel;
        bool hasLed;
        bool ledOn;
    };
    QList<CollectorRow> collectorRows = {
        {"采集器标识", "SQ1.1-1", true, true},
        {"帧计数", "SQ2.1-1", true, true},
        {"拉力1", "SQ3.1-1", true, true},
        {"拉力2", "SQ5.1-1", true, true},
        {"角度", "SQ6.1-1", true, true},
        {"压力1", "SQ1.2-1", true, true},
        {"压力2", "SQ2.2-1", true, true},
        {"温度1", "SQ3.2-1", true, true},
        {"温度2", "SQ5.2-1", true, true},
        {"28V", "SQ6.2-1", true, true},
        {"24V", "校验", true, true},
        {"15V", "", false, false},
        {"-15V", "", false, false},
        {"5V+", "", false, false},
        {"3.3V", "", false, false},
        {"1.8V", "", false, false},
        {"1.0V", "", false, false},
        {"热敏电阻", "", false, false}
    };

    for(int rowIdx = 0; rowIdx < collectorRows.size(); ++rowIdx) {
        const auto &row = collectorRows[rowIdx];
        int gridRow = rowIdx + 1;

        if(row.hasLed) {
            for(int c = 0; c < 4; ++c) {
                StyledLedLabel *led = new StyledLedLabel(this);
                led->setOn(false);
                led->setFixedSize(14, 14);
                m_allLeds.append(led);
                QString ledKey = row.rowLabel + QString::number(c + 1);
                m_ledMap[ledKey] = led;
                m_collectorGrid->addWidget(led, gridRow, c);
            }
        } else {
            for(int c = 0; c < 4; ++c) {
                QSpacerItem *spacer = new QSpacerItem(14, 14, QSizePolicy::Fixed, QSizePolicy::Fixed);
                m_collectorGrid->addItem(spacer, gridRow, c);
            }
        }

        if(!row.rowLabel.isEmpty()) {
            QLabel *rowLbl = new QLabel(row.rowLabel);
            rowLbl->setFixedWidth(60);
            rowLbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
            m_collectorGrid->addWidget(rowLbl, gridRow, 4);
        } else {
            QSpacerItem *spacer = new QSpacerItem(60, 20, QSizePolicy::Fixed, QSizePolicy::Minimum);
            m_collectorGrid->addItem(spacer, gridRow, 4);
        }

        for(int c = 0; c < 4; ++c) {
            StyledLineEdit *edit = new StyledLineEdit(this);
            edit->setFixedSize(60, 22);
            edit->setAlignment(Qt::AlignCenter);
            m_allLineEdits.append(edit);
            QString editKey = row.paramName + QString::number(c + 1);
            m_valueMap[editKey] = edit;
            m_collectorGrid->addWidget(edit, gridRow, 5 + c);
        }

        QLabel *paramLbl = new QLabel(row.paramName);
        paramLbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
        paramLbl->setFixedWidth(70);
        m_collectorGrid->addWidget(paramLbl, gridRow, 9);
    }

    m_collectorGrid->setColumnStretch(4, 1);
    m_collectorGrid->setColumnStretch(9, 1);

    // 左侧上下布局：控制器 + 时序
    QVBoxLayout *leftMainLayout = new QVBoxLayout;
    leftMainLayout->setSpacing(10);
    leftMainLayout->addWidget(topBox, 3);
    leftMainLayout->addWidget(leftGroupBox, 2);

    // 主布局：左侧 + 右侧采集器
    mainLayout->addLayout(leftMainLayout, 2);
    mainLayout->addWidget(collectorBox, 3);
}
void LaunchFrameDialog::upDataUi()
{
    updateControllerFrameUI(m_ledStates,m_editValues/*,m_ledStates,m_editValues*/);
}

