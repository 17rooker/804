#include "IEvent.h"

IEvent::IEvent(ESubDataType subType, EventType msgType, QObject* parent)
    : QEvent(QEvent::Type(msgType))
    , m_type(msgType)
    , m_msgDataType(subType)
{
    Q_UNUSED(parent)
}

void IEvent::setData(const QVariant& varData) {
    m_data = varData;
}

const QVariant& IEvent::getData() const {
    return m_data;
}

void IEvent::setExpandData(const QString& key, const QVariant& value) {
    m_expandMap[key] = value;
}

const QVariantHash& IEvent::getExpandData() const {
    return m_expandMap;
}

bool IEvent::getExpandData(const QString& key, QVariant& outValue) const {
    auto it = m_expandMap.constFind(key);
    if (it == m_expandMap.constEnd()) return false;
    outValue = it.value();
    return true;
}

EventType IEvent::getType() const {
    return m_type;
}

ESubDataType IEvent::getMSgSubType() const {
    return m_msgDataType;
}

void IEvent::setMSgSubType(ESubDataType subType) {
    m_msgDataType = subType;
}
