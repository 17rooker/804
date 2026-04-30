#ifndef CONFIGHELPER_H
#define CONFIGHELPER_H

#include <QString>
#include <QMap>
#include <QSharedPointer>
#include <QSettings>
#include <QCoreApplication>

/**
 * @brief The ConfigHelper class
 * 本地字典
 */
class ConfigHelper
{

public:

    static ConfigHelper& getInstance()
    {
        static ConfigHelper s_ins;

        return s_ins;
    }
private:
    ConfigHelper();

    ~ConfigHelper();
public:
    /**
     * @brief getDictInfo 获取配置值信息
     * @param key
     * @param defaultValue
     * @return QVariant
     */
    QVariant getValue(const QString &key, const QVariant &defaultValue = QVariant()) const ;

    void setValue(const QString &key, const QVariant &value) ;

    void remove(const QString &key) ;

    // 清空所有配置项
    void clear() ;
    /**
     * @brief loadConfig 加载配置
     * @param strFilePath
     */
    void loadConfig(const QString& strFilePath);

private:
    QSharedPointer<QSettings> m_setting=nullptr;

};

#endif // CONFIGHELPER_H
