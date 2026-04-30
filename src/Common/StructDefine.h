/*******************************************************************************
* @File: StructDefine.h
* @Description: 公共结构体等定义头文件
* @Author: lyj
* @Date: 2020-12-18
* @Note:
*******************************************************************************/

#ifndef STRUCTDEFINE_H
#define STRUCTDEFINE_H

#include <QString>
#include <QMap>
#include <QVariant>
#include <QDateTime>
#include <QString>
#include <QDebug>
#include <math.h>
#include <QSharedPointer>
#include <QDataStream>

#include "ConstDefine.h"
#include "FormatDefine.h"

#pragma pack (1)
// 时间
struct STDateTime
{
    QDateTime dateTime; // 时间 年-月-日 时:分:秒
    qint64    usuS = 0; // 毫秒微妙 000 000
    STDateTime()
    {
        dateTime = QDateTime::currentDateTime();
    }
    STDateTime(const STDateTime& dt)
    {
        dateTime = dt.dateTime;
        usuS = dt.usuS;

    }

    STDateTime& operator=(const STDateTime& other)
    {
        if (this == &other) {
            return *this;
        }

        dateTime = other.dateTime;
        usuS = other.usuS;

        return *this;
    }

    bool operator!=(const STDateTime& other) const
    {
        return dateTime != other.dateTime ||
               usuS != other.usuS ;
    }

    STDateTime(QDateTime tmpDateTime, qint64 usUSecond = 0)
    {
        int mSecond = tmpDateTime.time().msec();

        usuS     =  mSecond * 1000;
        dateTime = tmpDateTime.addMSecs(-mSecond);
        usuS     = usUSecond + usuS;
    }

    // 转换至QDateTime 丢失微秒
    QDateTime convertToDateTime()
    {
        QDateTime tmpDateTime = this->dateTime;
        quint16   usMs        = this->usuS / pow(10, 3);

        return tmpDateTime.addMSecs(usMs);
    }

    // 添加微妙
    STDateTime addUSecond(int nUSecond)
    {
        STDateTime stTmpDateTime = *this;
        qint64 lTimeUs = stTmpDateTime.toTime_t() + nUSecond;
        stTmpDateTime.fromTime_t(lTimeUs);

        return stTmpDateTime;
    }

    // 转微妙 1970-1-1
    qint64 toTime_t() const
    {
        qint64 usTime = dateTime.toMSecsSinceEpoch() * 1000 + usuS;

        return usTime;
    }

    // 微妙转时间
    void fromTime_t(qint64 ulTime)
    {
        quint64 ulPow = pow(10,6);
        qint64 usMs = ulTime/ulPow*1000;
        usuS = ulTime% ulPow;
        dateTime = QDateTime::fromMSecsSinceEpoch(usMs);
    }

    // 转换为 2021-12-12 12:12:12.123456
    QString toQString()
    {
        return dateTime.toString("yyyy-MM-dd hh:mm:ss.")
        + QString("%1").arg(usuS + dateTime.time().msec() * 1000, 6, 10, QChar('0'));
    }

    // 字符转时间 yyyy-MM-dd hh:mm:ss.zzzzzz
    void fromString(const QString& strDateTime)
    {
        QStringList strTmpDateTimeLst = strDateTime.split(" ");

        if (strTmpDateTimeLst.size() == 2)
        {
            // 年月日
            QString strDate        = strTmpDateTimeLst.at(0);
            QStringList strDateLst = strDate.split("-");

            if (strDateLst.size() == 3)
            {
                dateTime.setDate(QDate(strDateLst.at(0).toInt(),
                                       strDateLst.at(1).toInt(),
                                       strDateLst.at(2).toInt()));
            }

            // 时分秒 微妙
            QString strTime        = strTmpDateTimeLst.at(1);
            QStringList strTimeLst = strTime.split(".");

            if (strTimeLst.size() == 2)
            {
                QStringList strHMSLst = strTimeLst.at(0).split(":");

                if (strHMSLst.size() == 3)
                {
                    dateTime.setTime(QTime(strHMSLst.at(0).toInt(),
                                           strHMSLst.at(1).toInt(),
                                           strHMSLst.at(2).toInt()));
                }
                usuS = strTimeLst.at(1).toInt();
            }
        }
    }
};
Q_DECLARE_METATYPE(STDateTime)

/********************应用层数据结构体定义*********************/

struct STPackage
{
    quint32                unSourceID;    // 帧标识
    QString                channelId;      //通道号
    EChannelType           channelType;    // 通信方式
    EDataType              eDataType;     // 数据类型（静态数据、录波数据、事件、控制指令回复等）
    quint32                unExtendCode;  // 扩展码（如：流水号、子功能码等）
    quint64                ulDataLen;     // 数据长度，数据内容baData的长度
    QByteArray             baDataSend;     // 数据内容(发送采用 temp)
    QByteArray             baDataRecv;     // 数据内容（接收采用）
    STDateTime             stArrDateTime; // 数据到达时间
    //可添加发送类型
    STPackage()
    {
        unSourceID          = {};
        channelId           =QString();
        channelType         = {};
        eDataType           = {};
        unExtendCode        = {};
        ulDataLen           = {};
        baDataSend          = {};
        baDataRecv          = {};
        stArrDateTime       = {};

    }
    STPackage(const STPackage& package)
    {
        unSourceID = package.unSourceID;
        channelId   =package.channelId;
        channelType = package.channelType;
        eDataType       = package.eDataType      ;
        unExtendCode    = package.unExtendCode   ;
        ulDataLen       = package.ulDataLen      ;
        baDataRecv      = package.baDataRecv     ;
        baDataSend      = package.baDataSend     ;
        stArrDateTime   = package.stArrDateTime  ;
    }

    STPackage& operator=(const STPackage& other)
    {
        if (this == &other) {
            return *this;
        }

        unSourceID = other.unSourceID;
        channelId=other.channelId;
        channelType = other.channelType;
        eDataType       = other.eDataType      ;
        unExtendCode    = other.unExtendCode   ;
        ulDataLen       = other.ulDataLen      ;
        baDataRecv      = other.baDataRecv     ;
        baDataSend      = other.baDataSend     ;
        stArrDateTime   = other.stArrDateTime  ;
        return *this;
    }

    bool operator!=(const STPackage& other) const //This codes was added by CyrusChen in 2024/09/23
    {
        return unSourceID != other.unSourceID ||
               channelId!=other.channelId||
               channelType != other.channelType ||
               eDataType       != other.eDataType       ||
               unExtendCode    != other.unExtendCode    ||
               ulDataLen       != other.ulDataLen       ||
               baDataRecv      != other.baDataRecv      ||
               baDataSend      != other.baDataSend      ||
               stArrDateTime   != other.stArrDateTime   ;
    }

    QString getID() const
    {
        QString ID{};
        return ID;

    }



};
Q_DECLARE_METATYPE(STPackage)
DECLARE_SHAREADPTR_CLASS(STPackage)

/********************解析后数据项结构体定义*********************/
struct STParamItem
{
    int            bDataStatus = 0;              //数据状态，0：正常值，1 :告警值，2：故障值
    QVariant       varParaValue;                 // 数据值
    QVariant::Type paramType;                    // 数据类型
    QVariant::Type subType = QVariant::Invalid;  // 若(paramType)参数类型(录波数据)是容器类(如QList<T>)，该字段容器存放的数据<T>的类型

    STParamItem()
    {
        bDataStatus={};
        varParaValue={};
        paramType={};
        subType={};
    }
    STParamItem(const STParamItem& item)
    {
        bDataStatus = item.bDataStatus;
        varParaValue = item.varParaValue;
        paramType = item.paramType;
        subType = item.subType;
    }

    STParamItem& operator=(const STParamItem& other)
    {
        if (this == &other) {
            return *this;
        }

        bDataStatus = other.bDataStatus;
        varParaValue = other.varParaValue;
        paramType = other.paramType;
        subType = other.subType;
        return *this;
    }

    bool operator==(const STParamItem& other) const {
               bDataStatus == other.bDataStatus&&
               varParaValue == other.varParaValue&&
               paramType == other.paramType&&
               subType == other.subType;
    }
    bool operator!=(const STParamItem& other) const
    {
        return bDataStatus != other.bDataStatus||
               varParaValue != other.varParaValue||
               paramType != other.paramType||
               subType != other.subType;
    }

    friend QDataStream& operator<<(QDataStream&stream,const STParamItem& data)
    {
        stream<<data.bDataStatus;
        stream<<data.varParaValue;
        stream<<data.paramType;
        stream<<data.subType;
        return stream;
    }

    friend QDataStream& operator>>(QDataStream&stream,STParamItem& data)
    {
        stream>>data.bDataStatus;
        stream>>data.varParaValue;
        stream>>data.paramType;
        stream >> data.subType;
        return stream;
    }};
Q_DECLARE_METATYPE(STParamItem)

/********************解析后数据结构体定义*********************/
struct STParamInfo
{
    quint32                   unSourceID;    // 源ID(数据发送方ID(内部))
    QString                   channelId;     //通道号
    EChannelType              channelType;    // 通信方式
    quint32                   unExtendCode;  // 扩展码(如：流水号、子功能码等)
    EDataType                 eDataType;     // 数据类型(静态数据、录波数据、事件、控制指令回复等)
    QMap<QString, STParamItem>mapParams;     // 数据库字段(key:id)，数据项结构
    STDateTime                stArrDateTime; // 收到报文时间


    QString getID() const
    {
        // 报文唯一ID 设备ID_数据类型_流水号
        return QString("%1_%2_%3").arg(this->unSourceID).arg(static_cast<int>(this->eDataType)).arg(this->unExtendCode);
    }
    STParamInfo()
    {
        unSourceID={};
        channelId=QString();
        unExtendCode={};
        channelType={};
        eDataType={};
        mapParams=QMap<QString,STParamItem>();
        stArrDateTime={};
    }

    STParamInfo(const STParamInfo& info)
    {
        unSourceID = info.unSourceID;
        channelId=info.channelId;
        channelType=info.channelType;
        unExtendCode = info.unExtendCode;
        eDataType = info.eDataType;
        mapParams = info.mapParams;
        stArrDateTime = info.stArrDateTime;
    }

    STParamInfo& operator=(const STParamInfo& other)
    {
        if (this == &other) {
            return *this;
        }

        unSourceID = other.unSourceID;
        channelId=other.channelId;
        channelType=other.channelType;
        unExtendCode = other.unExtendCode;
        eDataType = other.eDataType;
        mapParams = other.mapParams;
        stArrDateTime = other.stArrDateTime;
        return *this;
    }

    bool operator!=(const STParamInfo& other) const
    {
        return  unSourceID != other.unSourceID||
               channelId !=other.channelId||
               channelType !=other.channelType||
               unExtendCode != other.unExtendCode||
               eDataType != other.eDataType||
               mapParams != other.mapParams||
               stArrDateTime != other.stArrDateTime;
    }

    friend QDataStream& operator<<(QDataStream&stream,const STParamInfo& data)
    {

        stream<<data.unSourceID;
        stream<<data.channelId;
        int nChannelType = static_cast<int>(data.channelType);
        stream<<nChannelType;
        stream<<data.unExtendCode;
        int nDataType = static_cast<int>(data.eDataType);
        stream<<nDataType;
        stream<<data.mapParams;
        STDateTime dateTime = data.stArrDateTime;
        stream<<dateTime.toQString();

        return stream;
    }

    friend QDataStream& operator>>(QDataStream&stream,STParamInfo& data)
    {
        stream>>data.unSourceID;
        stream>>data.channelId;
        int nChannelType;
        stream>>nChannelType;
        data.channelType = (EChannelType)nChannelType;
        stream>>data.unExtendCode;
        int nDataType;
        stream>>nDataType;
        data.eDataType = (EDataType)nDataType;
        stream>>data.mapParams;
        STDateTime dateTime = data.stArrDateTime;
        QString strTime = dateTime.toQString();
        stream>>strTime;
        return stream;
    }

};
Q_DECLARE_METATYPE(STParamInfo)
Q_DECLARE_METATYPE(QSharedPointer<STParamInfo>)

/************************设备通讯连接状态***************************/

// 计时
struct STTimeDuration
{
    quint32 unHour      = 0;              // 时
    QTime   timeCounter = QTime(0, 0, 0); // 时间计数

    void addSecond()
    {
        timeCounter = timeCounter.addSecs(1);

        if (timeCounter.secsTo(QTime(1, 0, 0)) == 0)
        {
            timeCounter = QTime(0, 0, 0);
            unHour++;
        }
    }
    void clear()
    {
        timeCounter = QTime(0, 0, 0);
        unHour = 0;
    }

    bool operator<(const STTimeDuration& stTime)
    {
        if (this->unHour > stTime.unHour)
        {
            return false;
        }

        if (stTime.timeCounter.secsTo(this->timeCounter) > 0)
        {
            return false;
        }
        return true;
    }
};



/***************************界面下发数据接口******************************/

/**
 * @brief The STControlMsg struct
 * 下发控制命令数据
 */
struct STControlMsg
{
    quint32    unCommondNo = 1; // 命令序号
    quint32    unCommondOP = 1; // 命令动作
    quint32    unParam1    = 0; // 被控方设备号/操作状态信息
    quint32    unParam2    = 0; // 预留/被控支路设备号
    quint32    unParam3    = 0; // 预留/被控支路设备号
    EEventName eEventType;      // 控制类型

    const QString getMsgUniqueID(bool bIsFeedback)
    {
        if (!bIsFeedback)
        {
            return QString("%1_%2_%3").arg(unCommondOP).arg(unParam1).arg(unParam2);
        }

        return QString("%1_%2_%3").arg(unCommondOP).arg(unParam2).arg(unParam3);
    }

    STControlMsg() {}
};
Q_DECLARE_METATYPE(STControlMsg)



/**
 * @brief The STControlMsgFeedBack struct
 * 控制命令数据反馈结果
 */
struct STControlMsgFeedBack
{
    quint32 unDestID; // 发送的目的设备ID

    STControlMsg  stCtrlMsgFeedBack;

    ECtrlResState eCtrlResState;

    /**
     * @brief compareCtrlMsg
     * 判断下发指令是否与反馈指令一致
     * @param stCtrlMsg
     * @return
     */
    bool compareCtrlMsg(STControlMsg& stCtrlMsg)
    {
        if (stCtrlMsg.getMsgUniqueID(false) ==
            this->stCtrlMsgFeedBack.getMsgUniqueID(true))
        {
            return true;
        }
        return false;
    }
};
Q_DECLARE_METATYPE(STControlMsgFeedBack)
DECLARE_SHAREADPTR_CLASS(STControlMsgFeedBack)

// 下发接口
struct STDataPrcSendMsg
{
    quint32                unDestID;          // 目的ID（数据接收方ID(内部)）
    QString                channelId;
    EChannelType           channelType;         // 通信方式
    EDataType              eDataType;         // 数据类型
    quint32                unExtendCode = 0;  // 扩展码（如：流水号、子功能码等）
    QByteArray             btData{};            // 数据
    STDateTime             stCreateDateTime;  // 创建时间
    bool                   bFeedBack = false; // 是否需要反馈超时
    EEventName eEventType;      // 控制类型
    // 反馈一般消息ID定义为通信方式_设备ID_控制回复类型_流水号

    STDataPrcSendMsg() {
        unDestID=0;
        channelId=QString();
        channelType=EChannelType::NoDefine;
        eDataType=EDataType::E_Unknown;
        unExtendCode=0;
        btData.clear();
        stCreateDateTime=STDateTime{};
        bFeedBack=false;

    }
    STDataPrcSendMsg(const STDataPrcSendMsg& msg)
    {
        unDestID = msg.unDestID;
        channelId=msg.channelId;
        channelType = msg.channelType;
        eDataType = msg.eDataType;
        unExtendCode = msg.unExtendCode;
        btData = msg.btData;
        stCreateDateTime = msg.stCreateDateTime;
        bFeedBack = msg.bFeedBack;

    }

    STDataPrcSendMsg& operator=(const STDataPrcSendMsg& other)
    {
        if (this == &other) {
            return *this;
        }
        unDestID = other.unDestID;
        channelId=other.channelId;
        channelType = other.channelType;
        eDataType = other.eDataType;
        unExtendCode = other.unExtendCode;
        btData = other.btData;
        stCreateDateTime = other.stCreateDateTime;
        bFeedBack = other.bFeedBack;

        return *this;
    }

    bool operator!=(const STDataPrcSendMsg& other) const
    {
        return unDestID != other.unDestID||
               channelId!=other.channelId||
               channelType != other.channelType||
               eDataType != other.eDataType||
               unExtendCode != other.unExtendCode||
               btData != other.btData||
               stCreateDateTime != other.stCreateDateTime||
               bFeedBack != other.bFeedBack;
    }


};
Q_DECLARE_METATYPE(STDataPrcSendMsg)
DECLARE_SHAREADPTR_CLASS(STDataPrcSendMsg)


// 下发失败通知
struct STSendFailedNotify
{
    quint32    unDestID;     // 目的ID（数据接收方ID(内部)）
    EDataType  eDataType;    // 数据类型（静态数据、录波数据、事件、控制指令回复等）
    quint32    unExtendCode; // 流水号
    QByteArray btData;       // 原始数据
    STSendFailedNotify() {}
};
Q_DECLARE_METATYPE(STSendFailedNotify)

// 下发超时通知
struct STTimeoutNotify
{
    quint32                unDestID;     // 目的ID（数据接收方ID(内部)）
    EDataType              eDataType;    // 数据类型（静态数据、录波数据、事件、控制指令回复等）
    EChannelType            channelType;    // 通信方式
    quint32                unExtendCode; // 扩展码（如：流水号、子功能码等）
    QByteArray             btData;       // 原始数据
    STTimeoutNotify() {}
};
Q_DECLARE_METATYPE(STTimeoutNotify)



// 回放操作状态
struct STPlatBackOPLoadInfo
{
    // 加载状态
    DATAPLAYBACKSPACE::EDataPlayBackLoadingState eLoadingState =
        DATAPLAYBACKSPACE::EDataPlayBackLoadingState::E_StartLoad;

    // 开始时间
    QDateTime beginTime;

    // 结束时间
    QDateTime endTime;
    STPlatBackOPLoadInfo() {}
};
Q_DECLARE_METATYPE(STPlatBackOPLoadInfo)
// 封装通用取值函数
template <typename T>
T getParamValue(const STParamInfo& paramInfo, const QString& fieldId, bool& ok) {
    ok = false;
    // 1. 检查字段是否存在
    if (!paramInfo.mapParams.contains(fieldId)) {
        qDebug() << "字段ID不存在：" << fieldId;
        return T();
    }
    // 2. 取出STParamItem
    const STParamItem& item = paramInfo.mapParams[fieldId];
    // 3. 转换为目标类型
    T value = item.varParaValue.value<T>(&ok);
    if (!ok) {
        qDebug() << "字段" << fieldId << "类型转换失败，期望类型：" << typeid(T).name();
    }
    return value;
}
struct STPlayBackOPInfo
{
    DATAPLAYBACKSPACE::EDataPlayBackState eState =
        DATAPLAYBACKSPACE::EDataPlayBackState::E_Stop; // 回放状态
    QDateTime beginTime;                               // 开始时间
    STPlayBackOPInfo() {}
};
Q_DECLARE_METATYPE(STPlayBackOPInfo)
#pragma pack ()
// ─────────────────────────────────────────────
//  回调类型
// ─────────────────────────────────────────────
using DataReceivedCallback = std::function<void(STPackage)>;
using ChannelStateCallback = std::function<void(const QString& channelId,
                                                EChannelState  state)>;

#endif // STRUCTDEFINE_H
