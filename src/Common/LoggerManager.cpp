// LoggerManager.cpp
#include "LoggerManager.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QtDebug>

// ── 单例 ──────────────────────────────────────────────────────────────────────
LoggerManager* LoggerManager::instance()
{
    // 利用 Qt 父子对象机制：挂在 qApp 下，应用退出时自动析构
    static LoggerManager* inst = nullptr;
    if (!inst) {
        inst = new LoggerManager(qApp);
    }
    return inst;
}

LoggerManager::LoggerManager(QObject* parent)
    : QObject(parent)
{}

// ── init ──────────────────────────────────────────────────────────────────────
void LoggerManager::init(const std::vector<LogChannel>& channels,
                         bool enableConsole)
{
    try {
        PLATFORM_INIT_CONSOLE();

        const QString logDir =
            QCoreApplication::applicationDirPath() + "/logs";
        QDir().mkpath(logDir);

        spdlog::init_thread_pool(8192, 1);

        const std::string startupTag =
            QDateTime::currentDateTime()
                .toString("yyyyMMdd_HHmmss")
                .toStdString();  // 纯 ASCII，无需转换

        // ★ logDir 转成 UTF-8 std::string 交给 buildLogger/buildFileSink 使用
        //   buildFileSink 内部再调用 NATIVE_PATH() 做最终平台转换
        const std::string logDirUtf8 = logDir.toStdString();

        for (const auto& ch : channels) {
            auto logger = buildLogger(ch, logDirUtf8,
                                      startupTag, enableConsole);
            spdlog::register_logger(logger);

            if (!spdlog::get("default")) {
                auto def = std::make_shared<spdlog::async_logger>(
                    "default",
                    logger->sinks().begin(),
                    logger->sinks().end(),
                    spdlog::thread_pool(),
                    spdlog::async_overflow_policy::block);
                def->set_level(spdlog::level::trace);
                spdlog::register_logger(def);
                spdlog::set_default_logger(def);
            }
        }

        spdlog::flush_every(std::chrono::seconds(3));
        spdlog::info("=== 日志系统初始化完成，共 {} 个通道 ===",
                     channels.size());
    }
    catch (const spdlog::spdlog_ex& ex) {
        qCritical() << "日志初始化失败:" << ex.what();
    }
}

// ── get ───────────────────────────────────────────────────────────────────────
std::shared_ptr<spdlog::logger> LoggerManager::get(const std::string& name)
{
    auto logger = spdlog::get(name);
    if (!logger) {
        logger = spdlog::default_logger();
        static std::unordered_map<std::string, bool> warned;
        if (!warned[name]) {
            logger->warn("未找到日志通道 [{}]，已回退到默认通道", name);
            warned[name] = true;
        }
    }
    return logger;
}

// ── addChannel ────────────────────────────────────────────────────────────────
void LoggerManager::addChannel(const LogChannel& ch, bool enableConsole)
{
    if (spdlog::get(ch.name)) {
        spdlog::warn("日志通道 [{}] 已存在，跳过", ch.name);
        return;
    }
    const QString logDir =
        QCoreApplication::applicationDirPath() + "/logs";
    const std::string startupTag =
        QDateTime::currentDateTime()
            .toString("yyyyMMdd_HHmmss")
            .toStdString();

    // ★ 同 init，传 UTF-8，由 buildFileSink 内部转换
    auto logger = buildLogger(ch, logDir.toStdString(),
                              startupTag, enableConsole);
    spdlog::register_logger(logger);
}

// ── UI 总开关 ─────────────────────────────────────────────────────────────────
void LoggerManager::setUIOutputEnabled(bool enabled)
{
    uiOutputEnabled_ = enabled;
}

bool LoggerManager::isUIOutputEnabled() const
{
    return uiOutputEnabled_;
}

// ── shutdown ──────────────────────────────────────────────────────────────────
void LoggerManager::shutdown()
{
    spdlog::info("=== 日志系统正常关闭 ===");
    spdlog::shutdown();
}

// ── buildLogger（私有）────────────────────────────────────────────────────────
std::shared_ptr<spdlog::async_logger>
LoggerManager::buildLogger(const LogChannel&  ch,
                           const std::string& logDir,
                           const std::string& startupTag,
                           bool               enableConsole)
{
    std::vector<spdlog::sink_ptr> sinks;

    // 1. 控制台 sink
    if (enableConsole) {
        auto console = std::make_shared<spdlog::sinks::stdout_sink_mt>();
        console->set_level(ch.consoleLevel);
        console->set_pattern(
            "[%H:%M:%S.%e] [%^%-8l%$] [" + ch.name + "] [%!:%#] %v");
        sinks.push_back(console);
    }

    // 2. 文件 sink
    sinks.push_back(buildFileSink(ch, logDir, startupTag));

    // 3. Qt UI sink（按通道 enableUI 标志决定是否添加）
    if (ch.enableUI) {
        const QString   chName   = QString::fromStdString(ch.name);
        const auto      uiLevel  = ch.uiLevel;
        // 捕获 this（单例生命周期与 qApp 相同，安全）
        auto uiSink = std::make_shared<QtLogSink>(
            chName,
            [this, uiLevel](const QString& channel,
                            const QString& message,
                            spdlog::level::level_enum level)
            {
                // 双重检查：通道级别 + 全局开关
                if (!uiOutputEnabled_ || level < uiLevel)
                    return;

                // emit 在 spdlog 工作线程中调用；
                // Qt::QueuedConnection（跨线程自动选择）确保投递到 UI 线程
                emit logMessage(channel, message, static_cast<int>(level));
            });

        uiSink->set_level(ch.uiLevel);
        uiSink->set_pattern(
            "[%H:%M:%S.%e] [%-8l] [" + ch.name + "] %v");
        sinks.push_back(uiSink);
    }

    auto logger = std::make_shared<spdlog::async_logger>(
        ch.name,
        sinks.begin(),
        sinks.end(),
        spdlog::thread_pool(),
        spdlog::async_overflow_policy::block);

    logger->set_level(spdlog::level::trace);
    return logger;
}

// ── buildFileSink（私有）──────────────────────────────────────────────────────
spdlog::sink_ptr
LoggerManager::buildFileSink(const LogChannel&  ch,
                             const std::string& logDir,   // UTF-8
                             const std::string& startupTag)
{
    // ★ 文件名前缀：优先使用 filename 字段，回退到 name
    //   两者都是 UTF-8；NATIVE_PATH() 在 Windows 转 GBK，Linux 透传
    const std::string filePrefix =
        ch.filename.empty() ? ch.name : ch.filename;

    spdlog::sink_ptr fileSink;

    if (ch.policy == RollPolicy::DAILY) {
        // ★ 将完整路径做一次平台转换再传给 spdlog
        const std::string path =
            NATIVE_PATH(logDir + "/" + filePrefix + ".log");

        auto sink = std::make_shared<spdlog::sinks::daily_file_sink_mt>(
            path, 0, 0);
        sink->set_level(ch.fileLevel);
        sink->set_pattern(
            "[%Y-%m-%d %H:%M:%S.%e] [%-8l] [%t] [%!:%#] %v");
        fileSink = sink;
    } else {
        const std::string path =
            NATIVE_PATH(logDir + "/" + filePrefix + "_" + startupTag + ".log");

        auto sink = std::make_shared<spdlog::sinks::rotating_file_sink_mt>(
            path, 256 * 1024 * 1024, 1);
        sink->set_level(ch.fileLevel);
        sink->set_pattern(
            "[%Y-%m-%d %H:%M:%S.%e] [%-8l] [%t] [%!:%#] %v");
        fileSink = sink;
    }

    return fileSink;
}
