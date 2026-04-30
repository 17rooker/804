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

    if (!MessageFrameConfig::getInstance().getFrameFormat(stPackage.channelId, fmt))
        return false;

    // 定长帧：长度不足时直接丢弃
    if (fmt.fixedLength && stPackage.baDataRecv.size() < fmt.totalBytes)
    {
        // 数据存日志

        qDebug() << "FrameDataParser: payload too short for" << stPackage.channelId
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
