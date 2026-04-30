// LoggerManager.h
#ifndef LOGGERMANAGER_H
#define LOGGERMANAGER_H

#include <spdlog/spdlog.h>
#include <spdlog/async.h>
#include <spdlog/sinks/base_sink.h>
#include <spdlog/sinks/daily_file_sink.h>
#include <spdlog/sinks/rotating_file_sink.h>
#include <spdlog/sinks/stdout_sinks.h>

#include <functional>
#include <memory>
#include <mutex>
#include <string>
#include <unordered_map>
#include <vector>

#include <QObject>
#include <QString>

// ── 平台适配 ──────────────────────────────────────────────────────────────────
#ifdef _WIN32
#  include <windows.h>
#  define PLATFORM_INIT_CONSOLE()  \
SetConsoleOutputCP(CP_UTF8); \
    system("chcp 65001 > nul")

    // UTF-8 → 当前 ANSI 代码页（中文 Windows = GBK/CP936）
    // MinGW 的 fopen 只认 ANSI，必须在传路径给 spdlog 前转换
    inline std::string utf8ToAnsi(const std::string& utf8)
{
    if (utf8.empty()) return utf8;
    // Step1: UTF-8 → UTF-16
    int wlen = ::MultiByteToWideChar(CP_UTF8, 0,
                                     utf8.c_str(), -1,
                                     nullptr, 0);
    if (wlen <= 0) return utf8;
    std::wstring wstr(wlen - 1, L'\0');
    ::MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), -1,
                          &wstr[0], wlen);
    // Step2: UTF-16 → ANSI (GBK)
    int alen = ::WideCharToMultiByte(CP_ACP, 0,
                                     wstr.c_str(), -1,
                                     nullptr, 0, nullptr, nullptr);
    if (alen <= 0) return utf8;
    std::string astr(alen - 1, '\0');
    ::WideCharToMultiByte(CP_ACP, 0, wstr.c_str(), -1,
                          &astr[0], alen, nullptr, nullptr);
    return astr;
}
#  define NATIVE_PATH(utf8Str)  utf8ToAnsi(utf8Str)

#else
// 麒麟 v10 / Linux 文件系统本身就是 UTF-8，直接透传
#  define PLATFORM_INIT_CONSOLE()  (void)0
#  define NATIVE_PATH(utf8Str)     (utf8Str)
#endif

// ── 便捷宏（携带文件名 + 行号）────────────────────────────────────────────────
#define LOG_TRACE(name, ...)    LoggerManager::get(name)->trace   (__VA_ARGS__)
#define LOG_DEBUG(name, ...)    LoggerManager::get(name)->debug   (__VA_ARGS__)
#define LOG_INFO(name,  ...)    LoggerManager::get(name)->info    (__VA_ARGS__)
#define LOG_WARN(name,  ...)    LoggerManager::get(name)->warn    (__VA_ARGS__)
#define LOG_ERROR(name, ...)    LoggerManager::get(name)->error   (__VA_ARGS__)
#define LOG_CRITICAL(name, ...) LoggerManager::get(name)->critical(__VA_ARGS__)

// ══════════════════════════════════════════════════════════════════════════════
// QtLogSink  —— 自定义 spdlog sink，将日志转发给 Qt 信号
// ══════════════════════════════════════════════════════════════════════════════
class QtLogSink : public spdlog::sinks::base_sink<std::mutex>
{
public:
    using Callback = std::function<void(const QString& channel,
                                        const QString& message,
                                        spdlog::level::level_enum level)>;

    explicit QtLogSink(QString channelName, Callback cb)
        : channelName_(std::move(channelName))
        , callback_(std::move(cb))
    {}

protected:
    void sink_it_(const spdlog::details::log_msg& msg) override
    {
        spdlog::memory_buf_t buf;
        base_sink<std::mutex>::formatter_->format(msg, buf);
        QString text = QString::fromStdString(fmt::to_string(buf)).trimmed();
        callback_(channelName_, text, msg.level);
    }
    void flush_() override {}

private:
    QString  channelName_;
    Callback callback_;
};

// ══════════════════════════════════════════════════════════════════════════════
// LoggerManager  —— QObject 单例，管理多通道日志并对外发射 logMessage 信号
// ══════════════════════════════════════════════════════════════════════════════
class LoggerManager : public QObject
{
    Q_OBJECT

public:
    enum class RollPolicy {
        DAILY,
        ON_STARTUP
    };

    struct LogChannel {
        std::string name;       // spdlog 内部注册名，建议用 ASCII（如 "operation"）

        // ★ 新增：实际写入磁盘的文件名前缀，支持中文；留空则退回使用 name
        //   调用方传入 UTF-8 字符串即可，内部会按平台做编码转换
        //   示例：filename = u8"操作日志"
        std::string filename;

        RollPolicy  policy       = RollPolicy::DAILY;
        bool        enableUI     = false;
        spdlog::level::level_enum consoleLevel = spdlog::level::debug;
        spdlog::level::level_enum fileLevel    = spdlog::level::trace;
        spdlog::level::level_enum uiLevel      = spdlog::level::info;
    };

    static LoggerManager* instance();

    void init(const std::vector<LogChannel>& channels,
              bool enableConsole = true);

    static std::shared_ptr<spdlog::logger> get(const std::string& name);

    void addChannel(const LogChannel& ch, bool enableConsole = true);

    void setUIOutputEnabled(bool enabled);
    bool isUIOutputEnabled() const;

    void shutdown();

signals:
    void logMessage(const QString& channel,
                    const QString& message,
                    int            level);

private:
    explicit LoggerManager(QObject* parent = nullptr);

    std::shared_ptr<spdlog::async_logger>
    buildLogger(const LogChannel&  ch,
                const std::string& logDir,
                const std::string& startupTag,
                bool               enableConsole);

    spdlog::sink_ptr
    buildFileSink(const LogChannel&  ch,
                  const std::string& logDir,
                  const std::string& startupTag);

    bool uiOutputEnabled_ = true;
};

#endif // LOGGERMANAGER_H

#if 0

// 写操作日志
LOG_INFO("operation", "用户 [{}] 登录系统", username);
LOG_WARN("operation", "权限不足，访问被拒绝");

// 写数据日志
LOG_DEBUG("data", "收到数据包，长度: {} 字节", len);
LOG_ERROR("data", "数据库写入失败: {}", errMsg);

// 写通信日志
LOG_ERROR("network", "连接超时: {}", host);

// 用默认通道（即第一个注册的通道）
spdlog::info("程序启动完成");

MainWindow::MainWindow(QWidget *parent) : QMainWindow(parent)
{
    // 1. 初始化日志（operation 通道输出到 UI，network 不输出）
    LoggerManager::instance()->init({
        {"operation", LoggerManager::RollPolicy::DAILY,
         /*enableUI=*/true,  spdlog::level::debug,
         spdlog::level::trace, spdlog::level::info },  // UI 只显示 info 以上

        {"data",      LoggerManager::RollPolicy::ON_STARTUP,
         /*enableUI=*/true,  spdlog::level::debug,
         spdlog::level::trace, spdlog::level::debug},  // UI 显示 debug 以上

        {"network",   LoggerManager::RollPolicy::ON_STARTUP,
         /*enableUI=*/false },                          // 不输出到 UI
    });

    // 2. 连接信号 —— 跨线程自动使用 QueuedConnection，UI 线程安全
    connect(LoggerManager::instance(), &LoggerManager::logMessage,
            this, &MainWindow::onLogMessage);
}

// 3. 槽函数：按级别着色追加到 QTextEdit
void MainWindow::onLogMessage(const QString& channel,
                              const QString& message,
                              int            level)
{
    static const QMap<int, QString> colorMap = {
        { spdlog::level::trace,    "#888888" },
        { spdlog::level::debug,    "#4fc3f7" },
        { spdlog::level::info,     "#ffffff" },
        { spdlog::level::warn,     "#ffb74d" },
        { spdlog::level::err,      "#ef5350" },
        { spdlog::level::critical, "#ff1744" },
    };

    const QString color = colorMap.value(level, "#ffffff");
    const QString html  = QString("<font color='%1'>%2</font>")
                          .arg(color, message.toHtmlEscaped());

    ui->logTextEdit->append(html);          // QTextEdit::append 本身线程安全
    ui->logTextEdit->moveCursor(QTextCursor::End); // 自动滚到最新一行
}

// 4. 运行时控制是否输出到 UI（比如绑定一个 CheckBox）
void MainWindow::on_uiLogCheckBox_toggled(bool checked)
{
    LoggerManager::instance()->setUIOutputEnabled(checked);
}
#endif
