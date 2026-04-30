#ifndef CSVCONTROLLER_H
#define CSVCONTROLLER_H

/**
 * @file CsvController.h
 * @brief 基于 fast-cpp-csv-parser 的线程安全 CSV 读写控制类
 *
 * 适配平台: Windows (MinGW64) / 银河麒麟 V10 (aarch64/x86_64)
 * Qt 版本:  5.12.9, qmake 构建
 *
 * 读操作 —— 使用 fast-cpp-csv-parser（高性能、零拷贝）
 * 写操作 —— 自研实现，符合 RFC 4180，支持流式追加
 * 线程安全 —— QReadWriteLock 读写分离；写操作串行化
 */

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>
#include <QVariant>
#include <QReadWriteLock>
#include <QMutex>
#include <QTextCodec>
#include <functional>

// ========================== 类型别名 ==========================
using CsvRow  = QStringList;              // 一行数据
using CsvData = QVector<CsvRow>;          // 整表数据（不含表头）

// ========================== 错误码 ============================
enum class CsvError {
    NoError = 0,
    FileNotFound,
    FileOpenFailed,
    FileWriteFailed,
    ParseError,
    ColumnMismatch,
    InvalidParameter,
    EncodingError
};

// ========================== 读写配置 ==========================
struct CsvReadConfig {
    QChar   separator    = ',';           // 分隔符
    QChar   quoteChar    = '"';           // 引用符
    bool    hasHeader    = true;          // 首行是否为表头
    QString encoding     = "UTF-8";       // 文件编码 (UTF-8 / GBK / GB18030 ...)
    int     skipRows     = 0;             // 跳过前 N 行（不含表头行）
    int     maxRows      = -1;            // 最大读取行数, -1 表示不限
    bool    trimFields   = true;          // 是否去除字段首尾空白
};

struct CsvWriteConfig {
    QChar   separator    = ',';
    QChar   quoteChar    = '"';
    bool    writeHeader  = true;
    QString encoding     = "UTF-8";
    bool    writeBOM     = false;         // UTF-8 BOM（某些 Windows 工具需要）
    QString lineEnding   = QString();     // 为空时自动检测平台：Win=\r\n, Linux=\n
    bool    forceQuote   = false;         // true=所有字段加引号; false=仅需要时加
};

// ========================== CsvController ======================
class CsvController : public QObject
{
    Q_OBJECT

public:
    explicit CsvController(QObject *parent = nullptr);
    ~CsvController() override;

    // ---------------------- 读操作 ----------------------------

    /**
     * @brief 读取整个 CSV 文件到内存
     * @param filePath  文件路径（支持中文路径）
     * @param config    读取配置
     * @return CsvError 错误码
     *
     * 读取完成后通过 header() / data() 获取数据。
     * 线程安全：多线程可并发调用 read，内部加写锁更新缓存。
     */
    CsvError read(const QString &filePath,
                  const CsvReadConfig &config = CsvReadConfig());

    /**
     * @brief 流式逐行读取（适合超大文件）
     * @param filePath  文件路径
     * @param callback  每行回调 (行号从0开始, 字段列表) -> bool (返回false停止)
     * @param config    读取配置
     * @return CsvError 错误码
     *
     * 不缓存数据到内存，直接流式处理。
     */
    CsvError readStream(const QString &filePath,
                        const std::function<bool(int rowIndex, const CsvRow &row)> &callback,
                        const CsvReadConfig &config = CsvReadConfig());

    // ---------------------- 写操作 ----------------------------

    /**
     * @brief 将内存数据写入 CSV 文件（覆盖写）
     */
    CsvError write(const QString &filePath,
                   const QStringList &header,
                   const CsvData &data,
                   const CsvWriteConfig &config = CsvWriteConfig());

    /**
     * @brief 追加行到已有 CSV 文件
     */
    CsvError append(const QString &filePath,
                    const CsvData &rows,
                    const CsvWriteConfig &config = CsvWriteConfig());

    /**
     * @brief 流式写入（适合超大数据集）
     * @param filePath   文件路径
     * @param header     表头
     * @param totalRows  总行数（用于进度信号）
     * @param rowSupplier 行供给回调，返回空 QStringList 表示结束
     * @param config     写入配置
     */
    CsvError writeStream(const QString &filePath,
                         const QStringList &header,
                         int totalRows,
                         const std::function<CsvRow(int rowIndex)> &rowSupplier,
                         const CsvWriteConfig &config = CsvWriteConfig());

    // ---------------------- 数据访问 --------------------------

    QStringList header()    const;   ///< 获取表头（读锁保护）
    CsvData     data()      const;   ///< 获取全部数据行（读锁保护）
    int         rowCount()  const;   ///< 数据行数
    int         colCount()  const;   ///< 列数（基于表头）
    CsvRow      row(int i)  const;   ///< 获取第 i 行
    QString     cell(int row, int col) const; ///< 获取单元格
    void        clear();             ///< 清空缓存数据

    // ---------------------- 工具方法 --------------------------

    /// 错误码转可读文本
    static QString errorString(CsvError err);

    /// 获取最后一次错误的详细描述
    QString lastErrorDetail() const;

signals:
    /// 读写进度信号 (0~100)
    void progressChanged(int percent);

    /// 错误信号
    void errorOccurred(CsvError error, const QString &detail);

private:
    // ---- 内部实现 ----

    /// 使用 fast-cpp-csv-parser 的 io::LineReader (mmap) 读取
    /// 适用于 UTF-8/ASCII 编码，逗号/自定义分隔符
    CsvError readWithLineReader(const QString &filePath, const CsvReadConfig &config);

    /// 使用 fast-cpp-csv-parser 的 io::CSVReader<N> 模板读取
    /// 适用于 UTF-8 + 逗号分隔 + 已知列数的最高性能场景
    /// 内部通过 probeColumnCount() 检测列数后模板分发
    CsvError readWithCsvReader(const QString &filePath, const CsvReadConfig &config);

    /// 通用回退解析器（非 UTF-8 编码或特殊引号字符时使用）
    CsvError readGeneric(const QString &filePath, const CsvReadConfig &config);

    /// 流式读取内部实现（基于 io::LineReader）
    CsvError readStreamInternal(const QString &filePath,
                                const std::function<bool(int, const CsvRow &)> &callback,
                                const CsvReadConfig &config);

    /// 探测 CSV 文件列数（读取首行）
    int  probeColumnCount(const QString &filePath, const CsvReadConfig &config);

    /// 获取平台适配的文件路径字符串（Windows 用 Local8Bit，Linux 用 UTF-8）
    static std::string toStdPath(const QString &filePath);

    /// 检查编码是否为 UTF-8 兼容
    static bool isUtf8Compatible(const QString &encoding);

    /// 准备文件路径：非 UTF-8 编码时转码为 UTF-8 临时文件，返回实际读取路径
    /// tempFile 非空表示创建了临时文件，调用方负责删除
    QString prepareUtf8File(const QString &filePath, const QString &encoding,
                            QString &tempFile);

    QString    escapeField(const QString &field, const CsvWriteConfig &config) const;
    QString    resolveLineEnding(const CsvWriteConfig &config) const;
    QByteArray encodeString(const QString &str, const CsvWriteConfig &config) const;

    // ---- 数据成员 ----
    mutable QReadWriteLock  m_dataLock;   // 读写锁（保护缓存数据）
    QMutex                  m_writeMutex; // 写文件互斥锁（串行化写操作）

    QStringList  m_header;
    CsvData      m_data;
    QString      m_lastErrorDetail;
};

#endif // CSVCONTROLLER_H
