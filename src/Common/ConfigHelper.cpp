#include "ConfigHelper.h"
#include <QtXml/QDomDocument>
#include <QFile>
#include <QDebug>
#include <QSettings>
#include <QTextCodec>
#include "src/Common/LoggerManager.h"

ConfigHelper::ConfigHelper()
{

}

ConfigHelper::~ConfigHelper()
{

}

QVariant ConfigHelper::getValue(const QString &key, const QVariant &defaultValue) const
{
    if(m_setting.isNull())
    {
        return QVariant();
    }
    return m_setting->value(key, defaultValue);
}

void ConfigHelper::setValue(const QString &key, const QVariant &value)
{
    if(m_setting.isNull())
    {
        return;
    }
    m_setting->setValue(key, value);
    m_setting->sync();
}

void ConfigHelper::remove(const QString &key)
{
    if(m_setting.isNull())
    {
        return;
    }
    m_setting->remove(key);
    m_setting->sync();
}

void ConfigHelper::clear()
{
    if(m_setting.isNull())
    {
        return;
    }
    m_setting->clear();
    m_setting->sync();
}



void ConfigHelper::loadConfig(const QString& strFilePath)
{
    QFile file(strFilePath);
    if(!file.exists())
    {
        spdlog::error("文件不存在: {}",strFilePath.toStdString());
        return;
    }
    m_setting=QSharedPointer<QSettings>(new QSettings(strFilePath, QSettings::IniFormat));

    m_setting->setIniCodec(QTextCodec::codecForName("system"));
}
