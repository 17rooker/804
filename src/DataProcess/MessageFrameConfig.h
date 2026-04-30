#ifndef MESSAGEFRAMECONFIG_H
#define MESSAGEFRAMECONFIG_H

#include <QString>
#include <QList>
#include <QMap>
#include <QVariant>

#include "src/Common/ConstDefine.h"
#include "src/Common/FormatDefine.h"

// 前向声明
struct STFrameFormat;

#if 0
{
  "index": 3,
  "byte_offset": "4",
  "id": "STATUS",
  "description": "状态字",
  "csv_field": "状态字",
  "data_type": 6,
  "byte_count": 1,
  "upper_limit": null,
  "lower_limit": null,
  "Precision": null,
  "unit": null,
  "Command": [],
  "ReturnCommand": [],
  "bit_fields": [
    {
      "id": "STATUS_RUN",
      "description": "运行状态",
      "csv_field": "运行状态",
      "bit_range": [0, 0],
      "data_type": 6
    },
    {
      "id": "STATUS_FAULT",
      "description": "故障状态",
      "csv_field": "故障状态",
      "bit_range": [1, 2],
      "data_type": 6
    },
    {
      "id": "STATUS_MODE",
      "description": "工作模式",
      "csv_field": "工作模式",
      "bit_range": [3, 5],
      "data_type": 6
    }
  ]
}
#endif



/**
 * @brief MessageFrameConfig 读取 MessageFrame.json，以 channel_id 为 key
 *        存储各通道帧格式，供解析器查询。
 */
class MessageFrameConfig
{
    DECLAREINSTANCE(MessageFrameConfig)

public:
    /**
     * @brief getFrameFormat  根据 channel_id 获取第一个匹配的帧格式（兼容旧调用）
     * @param channelId       通道 ID
     * @param out             输出帧格式
     * @return                是否找到
     */
    bool getFrameFormat(const QString& channelId, STFrameFormat& out) const;

    /**
     * @brief findFrameFormat  根据 channel_id + 数据长度匹配合适的帧格式
     * @param channelId       通道 ID
     * @param dataLength      收到的数据长度（字节）
     * @param out             输出帧格式
     * @return                是否找到匹配的格式
     *
     * 遍历同一 channelId 下所有帧格式，选择 fixedLength == true
     * 且 totalBytes == dataLength 的格式。若无精确匹配则返回 false。
     */
    bool findFrameFormat(const QString& channelId, int dataLength, STFrameFormat& out) const;

    /**
     * @brief contains  判断某 channel_id 是否存在配置
     */
    bool contains(const QString& channelId) const;

    /**
     * @brief allFormats  获取全部帧格式映射（只读）
     */
    const QMap<QString, QList<STFrameFormat>>& allFormats() const;

private:
    MessageFrameConfig();

    void initConfig();

private:
    QMap<QString, QList<STFrameFormat>> m_frameFormatMap;  // key: channel_id → 多格式列表
};

#endif // MESSAGEFRAMECONFIG_H
