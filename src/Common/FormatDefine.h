#ifndef FORMATDEFINE_H
#define FORMATDEFINE_H

#include <QString>
#include <QList>
#include <QDebug>
#include "ConstDefine.h"

// 帧类型
enum class EDataType
{
    E_Unknown=0,              // 未知
    E_ServerControl_BrakeRelease_A5,         // 控制器E回传A5
    E_ServerControl_BrakeRelease_A6,         // 控制器E回传A6
    F_ServerControl_BrakeRelease_A5,        // 控制器F回传A5
    F_ServerControl_BrakeRelease_A6,        // 控制器F回传A5
    G_ServerControl_BrakeRelease_A5,        // 控制器G回传A5
    G_ServerControl_BrakeRelease_A6,        // 控制器G回传A5
};

// 解析数据类型
enum class EAnaysisDataType
{
    Invalid = 0,

    // 数值类型
    Bool    =1,
    Int8    =2,
    Int16   =3,
    Int32   =4,
    Int64   =5,
    UInt8   =6,
    UInt16  =7,
    UInt32  =8,
    UInt64  =9,
    Float   =10,
    Double  =11,

    // 文本类型
    Char    =12,
    String  =13,
    WString =14,
};


/**
 * @brief 位子字段描述（对应 bit_fields 数组中的每个元素）
 *        bit_range[0]=起始位, bit_range[1]=结束位，bit0 = LSB
 *        与 NormalMsgParamConfig::STParam::bitRangePair 语义一致
 */
struct STBitField
{
    QString          id;
    QString          description;
    QString          csvField;
    EAnaysisDataType dataType  = EAnaysisDataType::Invalid;
    int              bitLow    = 0;   // 起始位（含），bit0 = LSB
    int              bitHigh   = 0;   // 结束位（含）
};

/**
 * @brief 帧字段描述（对应 MessageFrame.json 中 frame_header/body/tail 的每个元素）
 *        若字段含位域，bitFields 非空；否则 bitFields 为空。
 */
struct STFrameField
{
    int              index       = 0;
    QString          byteOffset;           // e.g. "0-1" or "4"
    QString          id;                   // 字段标识符
    QString          description;
    QString          csvField;
    EAnaysisDataType dataType    = EAnaysisDataType::Invalid;
    int              byteCount   = 0;
    QVariant         upperLimit;           // null -> invalid QVariant
    QVariant         lowerLimit;
    QVariant         precision;
    QString          unit;
    QStringList      command;              // e.g. ["0xEB90"]
    QStringList      returnCommand;
    QList<STBitField> bitFields;           // 位子字段，无则为空
};

/**
 * @brief 单个通道的完整帧格式（对应 frame_formats 数组中的一个元素）
 */
struct STFrameFormat
{
    QString             channelId;
    int                 frameType;
    int                 frameId      = 0;
    bool                postback     = false;
    bool                fixedLength  = false;
    bool                bigEndian    = false;
    QString             description;
    QList<STFrameField> frameHeader;
    QList<STFrameField> frameBody;
    QList<STFrameField> frameTail;
    int                 totalBytes   = 0;
};



#endif // FORMATDEFINE_H
