/*******************************************************************************
* @File: ConstDefine.h
* @Description: 常量定义头文件，用于定义常量，枚举等
* @Author: xxq
* @Date: 2020-12-15
* @Note:
*******************************************************************************/

#ifndef CONSTDEFINE_H
#define CONSTDEFINE_H

#include <QString>
#include <QVariant>
#include <QBitArray>
#include <QDataStream>
#include <QDateTime>
#include <memory>
#include "CommTypes.h"


// 数据超时状态时间(s)
static const int DATATIMEOUSECOND = 15;

// 是否采用大端数据
#define BIG_DATA 1

// 时间格式
#define STR_DATETIME_FORMAT  "yyyy-MM-dd hh:mm:ss.zzz"
#define DATETIME_FORMAT "yyyy-MM-dd hh:mm:ss"
#define DB_DATETIME_FORMAT "YYYY-MM-DDHH24:MI:SS"


#define qDebugTime(str) qDebug() << str << ":" << \
QDateTime::currentDateTime().toString(    \
    STR_DATETIME_FORMAT)

/*共享指针类定义*/
#define DECLARE_SHAREADPTR_CLASS(class_name)             \
    typedef QSharedPointer<class_name>class_name ## Ptr; \
    Q_DECLARE_METATYPE(class_name ## Ptr)

// 中文转换
#define CN(str) QStringLiteral(str)

// 单例宏
#define DECLAREINSTANCE(className)  \
    public:                             \
    \
    static className& getInstance() \
{                               \
        static className s_ins;     \
    \
        return s_ins;               \
}                               \



// 数据订阅类型
enum class ESubDataType
{
    E_RealTimeData,    // 实时数据
    E_DataPlayBackData // 回放数据
};

namespace DATAPLAYBACKSPACE {
// 数据回放状态
enum class EDataPlayBackState
{
    E_Stop = 0, // 停止
    E_Play,     // 开始
    E_Pause,    // 暂停
    E_Reset
};

// 缓存状态
enum class EDataPlayBackLoadingState
{
    E_StartLoad,    // 开始加载
    E_Loading,      // 加载中
    E_StopLoading,  // 停止加载
    E_LoadingFinish // 加载完成
};
}

// 界面订阅事件类型
enum class EventType
{
    E_User = 1000,
    E_InitiativeMsg, // 主动上报数据（设备上报的数据）
    E_InternalMsg,   // 内部消息(设备通讯链路状态上报、数据发送失败、控制类数据超时结果)

};


/**
 * @brief The ECtrlResState enum
 * 控制回复状态信息
 */
enum class ECtrlResState
{
    E_CtrlResSuccess = 1,  // 成功
    E_CtrlResFailed  = 15, // 失败
    E_CtrlResTimeout = 16, // 超时
    E_CtrlResOPNotFit      // 操作不一致
};


// 事件类型
enum class ELogEventType
{
    E_UnknowType, // 未知
    E_OperateLog, // 操作日志
    E_EventLog    // 事件日志
};

// 数据类型
enum class EStaticMsgType
{
    E_RealTimeMsg, // 实时数据
    E_TimeoutMsg   // 超时未收到设备数据
};
// 事件名称 告警、故障、数据不一致、录波
enum class EEventName
{
    E_Warnning = 0,       // 告警
    E_Erro,               // 故障
    E_DataNoDiff,         // 数据不一致
    E_SwitchCtrl,         // 开关控制
    E_DevCtrl,            // 设备控制
    E_WriteParam,         // 写参数

};


/**
 * 大小端转换
 */
template<typename T>
inline void swapIntEndian(T& bigValue)
{
    T liValue = bigValue;

#if BIG_DATA == 1
    unsigned short sizeCount = sizeof(T);

    switch (sizeCount)
    {
    case 1:
    {
        liValue = bigValue;
        break;
    }

    case 2:
    {
        liValue = ((bigValue & 0xFF00) >> 8)
        | ((bigValue & 0x00FF) << 8);
    } break;

    case 4:
    {
        liValue = ((bigValue & 0xFF000000) >> 24)
        | ((bigValue & 0x00FF0000) >> 8)
            | ((bigValue & 0x0000FF00) << 8)
            | ((bigValue & 0x000000FF) << 24);
    } break;

    case 8:
    {
        liValue = ((bigValue & 0xFF00000000000000) >> 56)
        | ((bigValue & 0x00FF000000000000) >> 40)
            | ((bigValue & 0x0000FF0000000000) >> 24)
            | ((bigValue & 0x000000FF00000000) >> 8)
            | ((bigValue & 0x00000000FF000000) << 8)
            | ((bigValue & 0x0000000000FF0000) << 24)
            | ((bigValue & 0x000000000000FF00) << 40)
            | ((bigValue & 0x00000000000000FF) << 56);
    }
    }
    bigValue = liValue;
#endif // if BIG_DATA == 1
}

template<typename T>
inline void swapFloatEndian(T& bigValue)
{
    T fResult = bigValue;

#if BIG_DATA == 1
    unsigned short int sizeCount = sizeof(T);

    if (sizeCount == 4)
    {
        unsigned char s[4], t[4];
        memcpy(s, &bigValue, sizeof(float));
        t[0] = s[3];
        t[1] = s[2];
        t[2] = s[1];
        t[3] = s[0];
        memcpy(&fResult, t, sizeof(float));
    }
    else if (sizeCount == 8)
    {
        unsigned char s[8], t[8];
        memcpy(s, &bigValue, sizeof(double));
        t[0] = s[7];
        t[1] = s[6];
        t[2] = s[5];
        t[3] = s[4];
        t[4] = s[3];
        t[5] = s[2];
        t[6] = s[1];
        t[7] = s[0];
        memcpy(&fResult, t, sizeof(double));
    }
    bigValue = fResult;
#endif // if BIG_DATA == 1
}

/**
 * 修改某一位值
 */
template<class T>
static inline T alertBitValue(T unSrcValue, T unNewValue, int nNewIndex)
{
    quint64 ulValue = 0;
    int     nBtSize = sizeof(T);

    switch (nBtSize) {
    case 1:
        ulValue = 0xF;
        break;

    case 2:
        ulValue = 0xFF;
        break;

    case 4:
        ulValue = 0xFFFF;
        break;

    case 8:
        ulValue = 0xFFFFFFFF;
        break;
    }
    T unTmpValue = 1;
    return (unSrcValue & (ulValue - (unTmpValue << nNewIndex))) +
           (unNewValue << nNewIndex);
}

/**
 * 添加数据
 */
template<class T>
static inline void appendIntMsg(QByteArray     & btData,
                                T              & data,
                                bool flag = false)
{
    if (flag)
    {
        if( sizeof(T) != 1)
        {
            swapIntEndian(data);
        }
    }
    btData.append((char *)&data, sizeof(T));
}

template<class T>
static inline void appendFloatMsg(QByteArray& btData, T& data,
                                  bool flag = false)
{
    if (flag)
    {
        if( sizeof(T) != 1)
        {
            swapFloatEndian(data);
        }
    }
    btData.append((char *)&data, sizeof(T));
}
template<class T>
inline void appendMsg(QByteArray& btData, const T& data)
{

    btData.append((char *)&data, sizeof(T));
}


/**
 *  拷贝数据
 */


template<class T>
inline void copyData(const char *pData, T& data)
{
    memcpy(&data, pData, sizeof(T));
}


template<class T>
static inline void copyIntData(const char *pData, quint64& nPos, T& value,
                               bool flag = false)
{
    value = *(T *)(pData + nPos);
    nPos += sizeof(T);

    if (flag)
    {
        swapIntEndian(value);
    }
}

/**
 *  拷贝数据
 */
template<class T>
static inline void copyFloatData(const char *pData, quint64& nPos, T& value,
                                 bool flag = false)
{
    value = *(T *)(pData + nPos);
    nPos += sizeof(T);

    if (flag)
    {
        swapFloatEndian(value);
    }
}



static inline QByteArray bitsToBytes(const QBitArray &bits,bool reversal = false)
{
    QByteArray bytes;
    bytes.resize(bits.count()/8 + ((bits.count()%8)?1:0));
    bytes.fill(0x00);
    for(int b = 0;b<bits.count();++b)
    {
        if(reversal == false)
        {
            int index = bytes.count()-1 - (b/8);
            bytes[index] = (bytes.at(index)|((bits[b]?1:0)<<(b%8)));
        }
        else
        {
            int index = b/8;
            bytes[index] = (bytes.at(index)|((bits[b]?1:0)<<(7-(b%8))));
        }
        //        qDebug()<<b<<"=="<<bits[b]<<" "<<index<<"=="<<(bytes.at(index)|((bits[b]?1:0)<<(7-(b%8))))<<"<<"<<(b%8);
    }
    return bytes;
}
/**
 * @brief BytesToBits byteArray转bitArray
 * @param qba   byteArray
 * @param reversal 是否需要反转
 * @return
 */
static inline QBitArray BytesToBits(QByteArray qba,bool reversal = false)
{
    QBitArray bitArray;
    int qbaSize = qba.size();
    int bitSize = qbaSize * 8;
    bitArray.clear();
    bitArray.resize(bitSize);
    for(int i = 0;i<qbaSize;i++)
    {
        for(int b = 0;b<8;b++)
        {
            if(reversal == false)
            {
                bitArray.setBit(bitSize-(i*8+b)-1,qba.at(i)&(1<<(7-b)));
            }
            else
            {
                bitArray.setBit(i*8+b,qba.at(i)&(1<<(7-b)));
            }
        }
    }
    return bitArray;
}


#endif // CONSTDEFINE_H
