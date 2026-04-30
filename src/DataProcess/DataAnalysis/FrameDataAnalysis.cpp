#include "FrameDataAnalysis.h"

#include "src/CustomMessage/DataInteractionManager.h"

#include <QDebug>
#include <cstring>  // memcpy

// ─── canHandle ───────────────────────────────────────────────────────────────
bool FrameDataAnalysis::canHandle(EChannelType /*type*/, const QString& channelId) const
{
    return MessageFrameConfig::getInstance().contains(channelId);
}

// ─── parse ───────────────────────────────────────────────────────────────────
bool FrameDataAnalysis::parseData(const STPackage& stPackage, STParamInfo& stParamInfo)
{
    STFrameFormat fmt;

    // 按实际数据长度匹配合适的帧格式（同一通道可能有多种协议）
    if (!MessageFrameConfig::getInstance().findFrameFormat(stPackage.channelId,
                                                           stPackage.baDataRecv.size(),
                                                           fmt))
        return false;

    // 定长帧：长度必须精确匹配
    if (fmt.fixedLength && stPackage.baDataRecv.size() != fmt.totalBytes)
    {
        qDebug() << "FrameDataParser: length mismatch for" << stPackage.channelId
                 << "expected" << fmt.totalBytes << "got" << stPackage.baDataRecv.size();
        return false;
    }

    stParamInfo.unSourceID   = static_cast<quint32>(fmt.frameId);
    stParamInfo.eDataType    = static_cast<EDataType>(fmt.frameType);

    parseFieldList(fmt.frameHeader, stPackage.baDataRecv, stParamInfo,fmt.bigEndian);
    parseFieldList(fmt.frameBody,   stPackage.baDataRecv, stParamInfo,fmt.bigEndian);
    parseFieldList(fmt.frameTail,   stPackage.baDataRecv, stParamInfo,fmt.bigEndian);
    return true;
}

// ─── parseFieldList ──────────────────────────────────────────────────────────
void FrameDataAnalysis::parseFieldList(const QList<STFrameField>& fields,
                                       const QByteArray&          payload,
                                       STParamInfo&               paramInfo,
                                       bool                       bigEndian) const
{
    for (const STFrameField& field : fields)
        parseOneField(field, payload, paramInfo,bigEndian);
}

// ─── parseOneField ───────────────────────────────────────────────────────────
void FrameDataAnalysis::parseOneField(const STFrameField& field,
                                      const QByteArray&   payload,
                                      STParamInfo&        paramInfo,
                                      bool                bigEndian) const
{
    const int startByte = parseStartByte(field.byteOffset);

    // 越界保护
    if (startByte < 0 || startByte + field.byteCount > payload.size())
    {
        qDebug() << "FrameDataParser: field" << field.id
                 << "out of range (offset=" << startByte
                 << "byteCount=" << field.byteCount
                 << "payloadSize=" << payload.size() << ")";
        return;
    }

    // ── 1. 解析整字段值 ──────────────────────────────────────────────────────
    QVariant value = readValue(payload, startByte, field.byteCount, field.dataType,bigEndian);
    if (!value.isValid())
        return;

    STParamItem item{};
    item.varParaValue = value;
    item.paramType    = value.type();
    paramInfo.mapParams.insert(field.id, item);

    // ── 2. 展开位子字段（如有）──────────────────────────────────────────────
    if (field.bitFields.isEmpty())
        return;

    // 将整字段值转为 quint64，便于位操作
    bool ok    = false;
    quint64 raw = value.toULongLong(&ok);
    if (!ok) return;

    for (const STBitField& bf : field.bitFields)
    {
        if (bf.bitLow < 0 || bf.bitHigh < bf.bitLow || bf.bitHigh >= field.byteCount * 8)
        {
            qDebug() << "FrameDataParser: invalid bit_range for" << bf.id;
            continue;
        }

        quint64 bits = extractBits(raw, bf.bitLow, bf.bitHigh);

        STParamItem bitItem;
        bitItem.varParaValue = QVariant(static_cast<quint32>(bits));
        bitItem.paramType    = QVariant::UInt;
        paramInfo.mapParams.insert(bf.id, bitItem);
    }
}

// ─── readValue ───────────────────────────────────────────────────────────────
QVariant FrameDataAnalysis::readValue(const QByteArray& payload,
                                      int               startByte,
                                      int               byteCount,
                                      EAnaysisDataType  dataType,
                                      bool bigEndian)
{
#if 0
    const char* buf = payload.constData() + startByte;
#else
    const char* buf = payload.constData();
    quint64 pos=startByte;
#endif
    // 小端数据直接 memcpy，不做字节翻转
    switch (dataType)
    {
    case EAnaysisDataType::Bool:
    case EAnaysisDataType::UInt8:
    {
        quint8 v = 0;
#if 0
        memcpy(&v, buf, 1);
#else
        copyIntData(buf,pos,v,bigEndian);
#endif
        return QVariant(static_cast<quint32>(v));
    }
    case EAnaysisDataType::Int8:
    {
        qint8 v = 0;
        copyIntData(buf,pos,v,bigEndian);
        return QVariant(static_cast<int>(v));
    }
    case EAnaysisDataType::UInt16:
    {
        quint16 v = 0;
        copyIntData(buf,pos,v,bigEndian);
        return QVariant(static_cast<quint32>(v));
    }
    case EAnaysisDataType::Int16:
    {
        qint16 v = 0;
        copyIntData(buf,pos,v,bigEndian);
        return QVariant(static_cast<int>(v));
    }
    case EAnaysisDataType::UInt32:
    {
        quint32 v = 0;
        copyIntData(buf,pos,v,bigEndian);
        return QVariant(v);
    }
    case EAnaysisDataType::Int32:
    {
        qint32 v = 0;
        copyIntData(buf,pos,v,bigEndian);
        return QVariant(static_cast<int>(v));
    }
    case EAnaysisDataType::UInt64:
    {
        quint64 v = 0;
        copyIntData(buf,pos,v,bigEndian);
        return QVariant(static_cast<quint64>(v));
    }
    case EAnaysisDataType::Int64:
    {
        qint64 v = 0;
        copyIntData(buf,pos,v,bigEndian);
        return QVariant(static_cast<qint64>(v));
    }
    case EAnaysisDataType::Float:
    {
        float v = 0.f;
        copyFloatData(buf,pos,v,bigEndian);
        return QVariant(static_cast<double>(v));
    }
    case EAnaysisDataType::Double:
    {
        double v = 0.0;
        copyFloatData(buf,pos,v,bigEndian);
        return QVariant(v);
    }
    case EAnaysisDataType::Char:
    {
        // byteCount 个字节作为 ASCII 字符串
        return QVariant(QString::fromLatin1(buf, byteCount));
    }
    default:
        return QVariant();
    }
}

// ─── extractBits ─────────────────────────────────────────────────────────────
quint64 FrameDataAnalysis::extractBits(quint64 raw, int bitLow, int bitHigh)
{
    int     width = bitHigh - bitLow + 1;
    quint64 mask  = (width < 64) ? ((quint64(1) << width) - 1) : ~quint64(0);
    return (raw >> bitLow) & mask;
}

// ─── parseStartByte ──────────────────────────────────────────────────────────
int FrameDataAnalysis::parseStartByte(const QString& byteOffset)
{
    int idx = byteOffset.indexOf('-');
    if (idx == -1)
        return byteOffset.trimmed().toInt();
    return byteOffset.left(idx).trimmed().toInt();
}

// ─── crc16Xmodem ─────────────────────────────────────────────────────────────
quint16 FrameDataAnalysis::crc16Xmodem(const QByteArray& data, int start, int len)
{
    static const quint16 table[256] = {
        0x0000,0x1021,0x2042,0x3063,0x4084,0x50a5,0x60c6,0x70e7,
        0x8108,0x9129,0xa14a,0xb16b,0xc18c,0xd1ad,0xe1ce,0xf1ef,
        0x1231,0x0210,0x3273,0x2252,0x52b5,0x4294,0x72f7,0x62d6,
        0x9339,0x8318,0xb37b,0xa35a,0xd3bd,0xc39c,0xf3ff,0xe3de,
        0x2462,0x3443,0x0420,0x1401,0x64e6,0x74c7,0x44a4,0x5485,
        0xa56a,0xb54b,0x8528,0x9509,0xe5ee,0xf5cf,0xc5ac,0xd58d,
        0x3653,0x2672,0x1611,0x0630,0x76d7,0x66f6,0x5695,0x46b4,
        0xb75b,0xa77a,0x9719,0x8738,0xf7df,0xe7fe,0xd79d,0xc7bc,
        0x48c4,0x58e5,0x6886,0x78a7,0x0840,0x1861,0x2802,0x3823,
        0xc9cc,0xd9ed,0xe98e,0xf9af,0x8948,0x9969,0xa90a,0xb92b,
        0x5af5,0x4ad4,0x7ab7,0x6a96,0x1a71,0x0a50,0x3a33,0x2a12,
        0xdbfd,0xcbdc,0xfbbf,0xeb9e,0x9b79,0x8b58,0xbb3b,0xab1a,
        0x6ca6,0x7c87,0x4ce4,0x5cc5,0x2c22,0x3c03,0x0c60,0x1c41,
        0xedae,0xfd8f,0xcdec,0xddcd,0xad2a,0xbd0b,0x8d68,0x9d49,
        0x7e97,0x6eb6,0x5ed5,0x4ef4,0x3e13,0x2e32,0x1e51,0x0e70,
        0xff9f,0xefbe,0xdfdd,0xcffc,0xbf1b,0xaf3a,0x9f59,0x8f78,
        0x9188,0x81a9,0xb1ca,0xa1eb,0xd10c,0xc12d,0xf14e,0xe16f,
        0x1080,0x00a1,0x30c2,0x20e3,0x5004,0x4025,0x7046,0x6067,
        0x83b9,0x9398,0xa3fb,0xb3da,0xc33d,0xd31c,0xe37f,0xf35e,
        0x02b1,0x1290,0x22f3,0x32d2,0x4235,0x5214,0x6277,0x7256,
        0xb5ea,0xa5cb,0x95a8,0x8589,0xf56e,0xe54f,0xd52c,0xc50d,
        0x34e2,0x24c3,0x14a0,0x0481,0x7466,0x6447,0x5424,0x4405,
        0xa7db,0xb7fa,0x8799,0x97b8,0xe75f,0xf77e,0xc71d,0xd73c,
        0x26d3,0x36f2,0x0691,0x16b0,0x6657,0x7676,0x4615,0x5634,
        0xd94c,0xc96d,0xf90e,0xe92f,0x99c8,0x89e9,0xb98a,0xa9ab,
        0x5844,0x4865,0x7806,0x6827,0x18c0,0x08e1,0x3882,0x28a3,
        0xcb7d,0xdb5c,0xeb3f,0xfb1e,0x8bf9,0x9bd8,0xabbb,0xbb9a,
        0x4a75,0x5a54,0x6a37,0x7a16,0x0af1,0x1ad0,0x2ab3,0x3a92,
        0xfd2e,0xed0f,0xdd6c,0xcd4d,0xbdaa,0xad8b,0x9de8,0x8dc9,
        0x7c26,0x6c07,0x5c64,0x4c45,0x3ca2,0x2c83,0x1ce0,0x0cc1,
        0xef1f,0xff3e,0xcf5d,0xdf7c,0xaf9b,0xbfba,0x8fd9,0x9ff8,
        0x6e17,0x7e36,0x4e55,0x5e74,0x2e93,0x3eb2,0x0ed1,0x1ef0
    };
    quint16 crc = 0x0000;
    for (int i = 0; i < len; i++) {
        quint8 byte = static_cast<quint8>(data.at(start + i));
        crc = static_cast<quint16>((crc << 8) ^ table[((crc >> 8) ^ byte) & 0xFF]);
    }
    return crc;
}
