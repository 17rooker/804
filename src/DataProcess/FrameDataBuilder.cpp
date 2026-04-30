#include "FrameDataBuilder.h"

#include <QDebug>
#include <cstring>  // memcpy
#include "src/Common/ConstDefine.h"

// ─── build ───────────────────────────────────────────────────────────────────
QByteArray FrameDataBuilder::build(const QString&               channelId,
                                   const QMap<QString, QVariant>& userValues,
                                   bool bigEndian)
{
    STFrameFormat fmt;
    if (!MessageFrameConfig::getInstance().getFrameFormat(channelId, fmt))
    {
        qDebug() << "FrameDataBuilder: no config for channelId" << channelId;
        return {};
    }

    if (fmt.totalBytes <= 0)
    {
        qDebug() << "FrameDataBuilder: totalBytes invalid for" << channelId;
        return {};
    }

    // 分配缓冲区并清零
    QByteArray buf(fmt.totalBytes, '\0');

    writeFieldList(buf, fmt.frameHeader, userValues, fmt, bigEndian);
    writeFieldList(buf, fmt.frameBody,   userValues, fmt, bigEndian);
    writeFieldList(buf, fmt.frameTail,   userValues, fmt, bigEndian);

    return buf;
}

// ─── writeFieldList ──────────────────────────────────────────────────────────
void FrameDataBuilder::writeFieldList(QByteArray&                    buf,
                                      const QList<STFrameField>&     fields,
                                      const QMap<QString, QVariant>& userValues,
                                      const STFrameFormat&           fmt,
                                      bool                           bigEndian)
{
    for (const STFrameField& field : fields)
    {
        const int startByte = parseStartByte(field.byteOffset);

        if (startByte < 0 || startByte + field.byteCount > buf.size())
        {
            qDebug() << "FrameDataBuilder: field" << field.id
                     << "out of range (offset=" << startByte
                     << "byteCount=" << field.byteCount << ")";
            continue;
        }

        // Float / Double 单独处理
        if (field.dataType == EAnaysisDataType::Float ||
            field.dataType == EAnaysisDataType::Double)
        {
            double dVal = 0.0;
            if (userValues.contains(field.id))
                dVal = userValues.value(field.id).toDouble();
            writeFloat(buf, startByte, field.byteCount, dVal, bigEndian);
            continue;
        }

        // 整型字段：确定最终值（含自动填 Command、位域打包）
        quint64 rawVal = resolveFieldValue(field, userValues, fmt);
        writeInt(buf, startByte, field.byteCount, rawVal, bigEndian);
    }
}

// ─── resolveFieldValue ───────────────────────────────────────────────────────
quint64 FrameDataBuilder::resolveFieldValue(const STFrameField&            field,
                                            const QMap<QString, QVariant>& userValues,
                                            const STFrameFormat&           fmt)
{
    // 优先级 1：用户显式提供整字段值
    if (userValues.contains(field.id))
        return userValues.value(field.id).toULongLong();

    // 优先级 2：Command 恰好只有 1 个固定值（如帧同步）
    if (field.command.size() == 1)
        return parseHex(field.command.first());

    // 优先级 3：fixed_length 时帧长字段自动计算
    //   通用标识：id == "ZC" 且 Command 为空视为帧长字段
    if (fmt.fixedLength && field.id == "ZC" && field.command.isEmpty())
        return static_cast<quint64>(fmt.totalBytes);

    // 优先级 4：位子字段打包
    if (!field.bitFields.isEmpty())
    {
        quint64 packed = 0;
        for (const STBitField& bf : field.bitFields)
        {
            if (!userValues.contains(bf.id)) continue;
            int     width = bf.bitHigh - bf.bitLow + 1;
            quint64 mask  = (width < 64) ? ((quint64(1) << width) - 1) : ~quint64(0);
            quint64 bits  = userValues.value(bf.id).toULongLong() & mask;
            packed |= (bits << bf.bitLow);
        }
        return packed;
    }

    return 0;
}

// ─── writeInt ────────────────────────────────────────────────────────────────
// bigEndian=true 时调用 ConstDefine.h 的 swapIntEndian 做字节翻转
// 注意：swapIntEndian 依赖编译期宏 BIG_DATA==1 才真正交换，当前项目已定义为 1
// 必须先 cast 到与 byteCount 匹配的类型，再 swap，否则 quint64 swap 后拷贝低位字节结果错误
void FrameDataBuilder::writeInt(QByteArray& buf, int startByte, int byteCount, quint64 value, bool bigEndian)
{
    unsigned char* dst = reinterpret_cast<unsigned char*>(buf.data() + startByte);

    switch (byteCount)
    {
    case 1:
    {
        quint8 v = static_cast<quint8>(value);
        memcpy(dst, &v, 1);
        break;
    }
    case 2:
    {
        quint16 v = static_cast<quint16>(value);
        if (bigEndian) swapIntEndian(v);
        memcpy(dst, &v, 2);
        break;
    }
    case 4:
    {
        quint32 v = static_cast<quint32>(value);
        if (bigEndian) swapIntEndian(v);
        memcpy(dst, &v, 4);
        break;
    }
    default: // 8 或其他，按 quint64 处理
    {
        int count = qMin(byteCount, static_cast<int>(sizeof(quint64)));
        if (bigEndian) swapIntEndian(value);
        memcpy(dst, &value, count);
        break;
    }
    }
}

// ─── writeFloat ──────────────────────────────────────────────────────────────
// bigEndian=true 时调用 ConstDefine.h 的 swapFloatEndian 做字节翻转
void FrameDataBuilder::writeFloat(QByteArray& buf, int startByte, int byteCount, double value, bool bigEndian)
{
    unsigned char* dst = reinterpret_cast<unsigned char*>(buf.data() + startByte);

    if (byteCount == 4)
    {
        float f = static_cast<float>(value);
        if (bigEndian) swapFloatEndian(f);
        memcpy(dst, &f, 4);
    }
    else if (byteCount == 8)
    {
        if (bigEndian) swapFloatEndian(value);
        memcpy(dst, &value, 8);
    }
}

// ─── parseStartByte ──────────────────────────────────────────────────────────
int FrameDataBuilder::parseStartByte(const QString& byteOffset)
{
    int idx = byteOffset.indexOf('-');
    if (idx == -1)
        return byteOffset.trimmed().toInt();
    return byteOffset.left(idx).trimmed().toInt();
}

// ─── parseHex ────────────────────────────────────────────────────────────────
quint64 FrameDataBuilder::parseHex(const QString& hexStr)
{
    QString s = hexStr.trimmed();
    if (s.startsWith("0x") || s.startsWith("0X"))
        s = s.mid(2);
    bool ok = false;
    return s.toULongLong(&ok, 16);
}
