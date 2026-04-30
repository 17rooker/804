#ifndef FRAMEDATABUILDER_H
#define FRAMEDATABUILDER_H

#include <QByteArray>
#include <QMap>
#include <QString>
#include <QVariant>

#include "src/DataProcess/MessageFrameConfig.h"
#include "src/Common/StructDefine.h"

/**
 * @brief FrameDataBuilder  按 MessageFrame.json 定义组装发送帧
 *
 * 字段值来源优先级（由高到低）：
 *   1. userValues[field.id]          —— 用户显式提供，优先级最高
 *   2. Command 恰好只有 1 个值时     —— 自动填入固定值（如帧同步 0xEB90）
 *   3. 位子字段打包：userValues[bitField.id] 逐位 OR 到父字段
 *   4. 0                             —— 其余未指定字段默认为 0
 *
 * 特殊自动处理：
 *   - 若 fixed_length=true，id=="ZC"（帧长）且 userValues 中未提供时，
 *     自动写入 totalBytes。
 *
 * 使用示例：
 * @code
 *   QMap<QString, QVariant> vals;
 *   vals["ZLX"]  = 0xF3;    // 帧类型（Command 有多个候选值，需手动选）
 *   vals["ZBS"]  = 0xAA00;  // 帧标识（发送方向）
 *   vals["Encoding"]       = quint32(0x0001);
 *   vals["StandardValue"]  = 1234.567;
 *   // ZTB(帧同步) 只有一个 Command 值，自动填入，无需手动写
 *
 *   QByteArray frame = FrameDataBuilder::build("tcp_device_serverRemote", vals);
 *   if (frame.isEmpty()) { // 错误处理 }
 *
 *   STDataPrcSendMsg msg;
 *   msg.channelId   = "tcp_device_serverRemote";
 *   msg.channelType = EChannelType::TCP;
 *   msg.eDataType   = EDataType::E_Control;
 *   msg.btData      = frame;
 *   DataInteractionManager::getInstance().sendMsg(msg);
 * @endcode
 */
class FrameDataBuilder
{
public:
    /**
     * @brief build  按 channelId 对应的帧格式构造发送缓冲区
     * @param channelId    MessageFrame.json 中的 channel_id
     * @param userValues   变量字段值映射（key = STFrameField::id 或 STBitField::id）
     * @return 构造好的帧字节流；失败时返回空 QByteArray
     */
    static QByteArray build(const QString&               channelId,
                            const QMap<QString, QVariant>& userValues = {},
                            bool bigEndian = false);

private:
    /**
     * @brief writeFieldList  将一组字段写入缓冲区
     */
    static void writeFieldList(QByteArray&                    buf,
                               const QList<STFrameField>&     fields,
                               const QMap<QString, QVariant>& userValues,
                               const STFrameFormat&           fmt,
                               bool                           bigEndian);

    /**
     * @brief resolveFieldValue  确定字段最终写入值（见优先级规则）
     * @param field       字段定义
     * @param userValues  用户提供的值映射
     * @param fmt         帧格式（用于 fixed_length + totalBytes 自动填帧长）
     * @return 最终使用的 quint64 整数值
     */
    static quint64 resolveFieldValue(const STFrameField&            field,
                                     const QMap<QString, QVariant>& userValues,
                                     const STFrameFormat&           fmt);

    /**
     * @brief writeInt  将 value 的低 byteCount 字节按指定端序写入 buf[startByte]
     */
    static void writeInt(QByteArray& buf, int startByte, int byteCount, quint64 value, bool bigEndian);

    /**
     * @brief writeFloat  将 float/double 按指定端序写入 buf[startByte]
     */
    static void writeFloat(QByteArray& buf, int startByte, int byteCount, double value, bool bigEndian);

    /**
     * @brief parseStartByte  "0-1"→0, "4"→4
     */
    static int parseStartByte(const QString& byteOffset);

    /**
     * @brief parseHex  解析 "0xEB90" → 0xEB90
     */
    static quint64 parseHex(const QString& hexStr);
};

#endif // FRAMEDATABUILDER_H
