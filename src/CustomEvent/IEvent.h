#ifndef IEVENT_H
#define IEVENT_H

#include <QEvent>
#include <QVariant>
#include <QVariantHash>

#include "src/Common/ConstDefine.h"


/**
 * @brief IEvent  Qt 自定义事件基类
 *
 * 所有业务事件均继承此类，通过 QCoreApplication::postEvent()
 * 投递到主线程 MessageHandle 进行分发，实现线程安全的跨线程数据推送。
 *
 * 生命周期：由 Qt 事件系统管理，postEvent() 后勿手动 delete。
 */
class  IEvent : public QEvent {
public:
    explicit IEvent(ESubDataType subType  = ESubDataType::E_RealTimeData,
                    EventType    msgType  = EventType::E_InitiativeMsg,
                    QObject*     parent   = nullptr);

    // ── 主数据 ──────────────────────────────────
    void             setData(const QVariant& varData);
    const QVariant&  getData() const;

    // ── 扩展 KV 数据 ────────────────────────────
    void                 setExpandData(const QString& key, const QVariant& value);
    const QVariantHash&  getExpandData() const;
    bool                 getExpandData(const QString& key, QVariant& outValue) const;

    // ── 类型查询 ────────────────────────────────
    EventType    getType()       const;
    ESubDataType getMSgSubType() const;
    void         setMSgSubType(ESubDataType subType);

private:
    EventType    m_type;
    ESubDataType m_msgDataType = ESubDataType::E_RealTimeData;
    QVariant     m_data;
    QVariantHash m_expandMap;
};

#endif // IEVENT_H
