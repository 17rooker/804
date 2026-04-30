#include "MessageFrameConfig.h"

#include <QCoreApplication>
#include <QFile>
#include <QDebug>

#include "json.hpp"

using json = nlohmann::json;

// ─── 内部辅助：解析单个字段对象 ──────────────────────────────────────────────
static STFrameField parseField(const json& j)
{
    STFrameField f;

    f.index       = j.value("index", 0);
    f.byteOffset  = QString::fromStdString(j.value("byte_offset", std::string{}));
    f.id          = QString::fromStdString(j.value("id",          std::string{}));
    f.description = QString::fromStdString(j.value("description", std::string{}));
    f.csvField    = QString::fromStdString(j.value("csv_field",   std::string{}));
    f.dataType    = static_cast<EAnaysisDataType>(j.value("data_type", 0));
    f.byteCount   = j.value("byte_count", 0);

    // null 字段用 QVariant() 表示无效值
    if (!j["upper_limit"].is_null())
        f.upperLimit = QVariant(j["upper_limit"].get<double>());

    if (!j["lower_limit"].is_null())
        f.lowerLimit = QVariant(j["lower_limit"].get<double>());

    if (!j["Precision"].is_null())
        f.precision  = QVariant(j["Precision"].get<double>());

    if (!j["unit"].is_null())
        f.unit = QString::fromStdString(j["unit"].get<std::string>());

    for (const auto& cmd : j.value("Command", json::array()))
        f.command.append(QString::fromStdString(cmd.get<std::string>()));

    for (const auto& cmd : j.value("ReturnCommand", json::array()))
        f.returnCommand.append(QString::fromStdString(cmd.get<std::string>()));

    // 位子字段（可选）
    for (const auto& bf : j.value("bit_fields", json::array()))
    {
        STBitField bitField;
        bitField.id          = QString::fromStdString(bf.value("id",          std::string{}));
        bitField.description = QString::fromStdString(bf.value("description", std::string{}));
        bitField.csvField    = QString::fromStdString(bf.value("csv_field",   std::string{}));
        bitField.dataType    = static_cast<EAnaysisDataType>(bf.value("data_type", 0));

        const auto& range = bf.value("bit_range", json::array({0, 0}));
        if (range.is_array() && range.size() == 2)
        {
            bitField.bitLow  = range[0].get<int>();
            bitField.bitHigh = range[1].get<int>();
        }
        f.bitFields.append(bitField);
    }

    return f;
}

// ─── 内部辅助：解析字段数组 ──────────────────────────────────────────────────
static QList<STFrameField> parseFieldList(const json& arr)
{
    QList<STFrameField> list;
    if (!arr.is_array()) return list;
    for (const auto& item : arr)
        list.append(parseField(item));
    return list;
}

// ─── MessageFrameConfig ──────────────────────────────────────────────────────
MessageFrameConfig::MessageFrameConfig()
{
    initConfig();
}

void MessageFrameConfig::initConfig()
{
    const QString filePath = qApp->applicationDirPath() + "/config/MessageFrame.json";
    QFile file(filePath);

    if (!file.open(QIODevice::ReadOnly))
    {
        qDebug() << "MessageFrameConfig: cannot open" << filePath;
        return;
    }

    const QByteArray rawData = file.readAll();
    file.close();

    json root;
    try
    {
        root = json::parse(rawData.constData(), rawData.constData() + rawData.size());
    }
    catch (const json::parse_error& e)
    {
        qDebug() << "MessageFrameConfig: JSON parse error:" << e.what();
        return;
    }

    const auto& formats = root.value("frame_formats", json::array());
    for (const auto& entry : formats)
    {
        // 跳过空对象 {}
        if (!entry.contains("channel_id")) continue;

        STFrameFormat fmt;
        fmt.channelId   = QString::fromStdString(entry.value("channel_id",  std::string{}));
        fmt.frameType     = entry.value("frame_type",    0);
        fmt.frameId     = entry.value("frame_id",    0);
        fmt.postback    = entry.value("postback",    false);
        fmt.fixedLength = entry.value("fixed_length",false);
        fmt.bigEndian = entry.value("big_endian",false);
        fmt.description = QString::fromStdString(entry.value("description", std::string{}));
        fmt.totalBytes  = entry.value("total_bytes", 0);

        fmt.frameHeader = parseFieldList(entry.value("frame_header", json::array()));
        fmt.frameBody   = parseFieldList(entry.value("frame_body",   json::array()));
        fmt.frameTail   = parseFieldList(entry.value("frame_tail",   json::array()));

        m_frameFormatMap[fmt.channelId].append(fmt);
    }

    qDebug() << "MessageFrameConfig: loaded" << m_frameFormatMap.size() << "channel(s)";
}

bool MessageFrameConfig::getFrameFormat(const QString& channelId, STFrameFormat& out) const
{
    auto it = m_frameFormatMap.find(channelId);
    if (it == m_frameFormatMap.end() || it.value().isEmpty()) return false;
    out = it.value().first();
    return true;
}

bool MessageFrameConfig::findFrameFormat(const QString& channelId, int dataLength, STFrameFormat& out) const
{
    auto it = m_frameFormatMap.find(channelId);
    if (it == m_frameFormatMap.end()) return false;

    for (const auto& fmt : it.value()) {
        if (fmt.fixedLength && fmt.totalBytes == dataLength) {
            out = fmt;
            return true;
        }
    }
    return false;
}

bool MessageFrameConfig::contains(const QString& channelId) const
{
    return m_frameFormatMap.contains(channelId);
}

const QMap<QString, QList<STFrameFormat>>& MessageFrameConfig::allFormats() const
{
    return m_frameFormatMap;
}
