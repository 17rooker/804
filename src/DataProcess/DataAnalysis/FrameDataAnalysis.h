#ifndef FRAMEDATAANALYSIS_H
#define FRAMEDATAANALYSIS_H

#include "src/DataProcess/DataAnalysis/IDataAnalysis.h"
#include "src/DataProcess/MessageFrameConfig.h"
#include "src/Common/StructDefine.h"

/**
 * @brief FrameDataAnalysis
 *
 * 根据 MessageFrame.json 的配置，将 CommRawData 中的小端二进制帧
 * 解析为 STParamInfo，并通过 DataInteractionManager::postStaticMsg 上报。
 *
 * 解析规则：
 *  - 每个字段的起始字节 = byte_offset 字符串的首个数字
 *  - 读取 byte_count 个字节，按 data_type 解释（小端，不做字节交换）
 *  - 若字段含 bit_fields，则在普通字段之外额外展开各位子字段
 *  - 结果存入 STParamInfo::mapParams，key = STFrameField::id（或 STBitField::id）
 */
class FrameDataAnalysis : public IDataAnalysis
{
public:
    FrameDataAnalysis() = default;
    ~FrameDataAnalysis() override = default;

    /**
     * @brief canHandle  channelId 在 MessageFrameConfig 中有配置时返回 true
     */
    bool canHandle(EChannelType type, const QString& channelId) const override;

    /**
     * @brief parse  解析帧并通过 DataInteractionManager 上报
     */
    bool parseData(const STPackage& stPackage, STParamInfo& stParamInfo) override;

private:
    /**
     * @brief parseFieldList  解析一组字段列表，结果写入 paramInfo.mapParams
     */
    void parseFieldList(const QList<STFrameField>& fields,
                        const QByteArray&          payload,
                        STParamInfo&               paramInfo,
                        bool                       bigEndian) const;

    /**
     * @brief parseOneField  解析单个字段（含位子字段展开）
     */
    void parseOneField(const STFrameField& field,
                       const QByteArray&   payload,
                       STParamInfo&        paramInfo,
                       bool                bigEndian) const;

    /**
     * @brief readValue  从 payload[startByte] 处读取 byteCount 字节，
     *                   按 dataType 解释（小端，不交换字节序），返回 QVariant
     */
    static QVariant readValue(const QByteArray& payload,
                              int               startByte,
                              int               byteCount,
                              EAnaysisDataType  dataType,
                              bool              bigEndian);

    /**
     * @brief extractBits  从无符号整数 raw 中提取 [bitLow, bitHigh] 位（bit0=LSB）
     */
    static quint64 extractBits(quint64 raw, int bitLow, int bitHigh);

    /**
     * @brief parseStartByte  解析 "0-1"→0、"4"→4 这类 byte_offset 字符串的首字节
     */
    static int parseStartByte(const QString& byteOffset);
};

#endif // FRAMEDATAANALYSIS_H
