/*******************************************************************************
 * @File: ScheduledTask.h
 * @Description: 定时发送任务描述符
 *******************************************************************************/

#ifndef SCHEDULEDTASK_H
#define SCHEDULEDTASK_H

#include <QString>
#include <QMap>
#include <QVariant>
#include <functional>
#include <optional>

#include "src/Common/CommTypes.h"
#include "src/Common/StructDefine.h"

/**
 * @brief ScheduledTask  定时发送任务描述符
 *
 * 两种使用方式（二选一）：
 *
 * 1. 简单模式：填写 channelId / channelType / eDataType / frameValues，
 *    服务内部自动调用 FrameDataBuilder::build() 构帧。
 *
 * 2. 自定义模式：提供 frameBuilder lambda，完全自行构造 STDataPrcSendMsg。
 *    返回 std::nullopt 时跳过本次发送（可用于连接断开等条件判断）。
 *
 * 使用示例（简单模式）：
 * @code
 *   ScheduledTask heartbeat;
 *   heartbeat.taskName    = "heartbeat";
 *   heartbeat.intervalMs  = 5000;
 *   heartbeat.channelId   = "TCPRemote";
 *   heartbeat.channelType = EChannelType::TCP;
 *   heartbeat.eDataType   = EDataType::E_Control;
 *   heartbeat.frameValues["ZLX"] = 0xF3;
 *   heartbeat.frameValues["ZBS"] = 0xAA00;
 *   sss.addTask(heartbeat);
 * @endcode
 *
 * 使用示例（自定义模式）：
 * @code
 *   ScheduledTask poll;
 *   poll.taskName   = "poll_status";
 *   poll.intervalMs = 1000;
 *   poll.frameBuilder = [channelId]() -> std::optional<STDataPrcSendMsg> {
 *       // 条件判断：如不满足条件则返回 nullopt 跳过本次
 *       // if (!isConnected) return std::nullopt;
 *       STDataPrcSendMsg msg;
 *       msg.channelId   = channelId;
 *       msg.channelType = EChannelType::TCP;
 *       msg.eDataType   = EDataType::E_Control;
 *       msg.btData      = FrameDataBuilder::build(channelId, {{"ZLX", 0xF4}});
 *       return msg;
 *   };
 *   sss.addTask(poll);
 * @endcode
 */
struct ScheduledTask
{
    QString  taskName;           // 任务唯一名称（如 "heartbeat", "poll_status"）
    int      intervalMs  = 1000; // 发送间隔（毫秒）
    bool     singleShot  = false; // true = 只发送一次后自动移除

    // ── 简单模式（不提供 frameBuilder 时使用）──────────────────────────
    QString                 channelId;
    EChannelType            channelType = EChannelType::NoDefine;
    EDataType               eDataType   = EDataType::E_Unknown;
    QMap<QString, QVariant> frameValues;  // 传给 FrameDataBuilder::build()
    bool                    bigEndian    = false;

    // ── 自定义模式（提供此 lambda 则忽略上面的简单模式字段）──────────────
    // 返回 std::nullopt 时跳过本次发送
    using FrameBuilderFunc = std::function<std::optional<STDataPrcSendMsg>()>;
    FrameBuilderFunc frameBuilder;
};

#endif // SCHEDULEDTASK_H
