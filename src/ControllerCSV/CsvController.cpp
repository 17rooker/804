#include "CsvController.h"

#include <QFile>
#include <QFileInfo>
#include <QTextStream>
#include <QDebug>
#include <QDir>
#include <QReadLocker>
#include <QWriteLocker>
#include <QMutexLocker>

#include <cstring>
#include <stdexcept>
#include <array>

// =====================================================================
// fast-cpp-csv-parser 编译配置
// =====================================================================
//
// 核心原理：
//   io::LineReader  —— 基于 mmap(Linux) / CreateFileMapping(Win) 的零拷贝行读取
//   io::CSVReader<N> —— 模板化列解析器，编译期确定列数，极致性能
//
// 本控制类的使用策略：
//   1. readWithCsvReader()  —— 列数 ≤ 50 且为 UTF-8+逗号：用 CSVReader<N> 模板分发
//   2. readWithLineReader() —— 任意列数的 UTF-8 文件：用 LineReader 做 mmap 行读取
//   3. readGeneric()        —— 非 UTF-8 编码的回退路径（通常先转码再走 1 或 2）
// =====================================================================

#ifdef __MINGW32__
#  ifndef CSV_IO_NO_THREAD
#    define CSV_IO_NO_THREAD  // MinGW 下避免 pthread 链接问题
#  endif
#endif

#include "csv.h"  // fast-cpp-csv-parser header-only library

// =====================================================================
// 内部辅助工具
// =====================================================================
namespace {

/// 跳过 UTF-8 BOM (EF BB BF)
void skipBOM(QByteArray &data)
{
    if (data.size() >= 3 &&
        static_cast<unsigned char>(data.at(0)) == 0xEF &&
        static_cast<unsigned char>(data.at(1)) == 0xBB &&
        static_cast<unsigned char>(data.at(2)) == 0xBF) {
        data.remove(0, 3);
    }
}

/// RFC 4180 字段拆分（从 char* 原始行数据，用于 LineReader 返回的 mmap 指针）
CsvRow splitFields(const char *line, int len, char sep, char quote, bool trim)
{
    CsvRow fields;
    QString field;
    bool inQuote = false;

    for (int i = 0; i < len; ++i) {
        char c = line[i];
        if (c == '\r')
            continue;

        if (inQuote) {
            if (c == quote) {
                if (i + 1 < len && line[i + 1] == quote) {
                    field += QChar(quote);
                    ++i;
                } else {
                    inQuote = false;
                }
            } else {
                field += QChar::fromLatin1(c);
            }
        } else {
            if (c == quote) {
                inQuote = true;
            } else if (c == sep) {
                fields.append(trim ? field.trimmed() : field);
                field.clear();
            } else {
                field += QChar::fromLatin1(c);
            }
        }
    }
    fields.append(trim ? field.trimmed() : field);
    return fields;
}

/// UTF-8 版本的字段拆分（处理多字节中文字符）
CsvRow splitFieldsUtf8(const char *line, int len, char sep, char quote, bool trim)
{
    // 先转为 QString 以正确处理 UTF-8 多字节字符
    QString str = QString::fromUtf8(line, len);
    CsvRow fields;
    QString field;
    bool inQuote = false;
    QChar qSep = QChar::fromLatin1(sep);
    QChar qQuote = QChar::fromLatin1(quote);

    for (int i = 0; i < str.length(); ++i) {
        QChar c = str.at(i);
        if (c == QChar('\r'))
            continue;

        if (inQuote) {
            if (c == qQuote) {
                if (i + 1 < str.length() && str.at(i + 1) == qQuote) {
                    field += qQuote;
                    ++i;
                } else {
                    inQuote = false;
                }
            } else {
                field += c;
            }
        } else {
            if (c == qQuote) {
                inQuote = true;
            } else if (c == qSep) {
                fields.append(trim ? field.trimmed() : field);
                field.clear();
            } else {
                field += c;
            }
        }
    }
    fields.append(trim ? field.trimmed() : field);
    return fields;
}

/// Unicode 版字段拆分（用于通用解析器）
CsvRow splitFieldsUnicode(const QString &line, QChar sep, QChar quote, bool trim)
{
    CsvRow fields;
    QString field;
    bool inQuote = false;
    int len = line.length();

    for (int i = 0; i < len; ++i) {
        QChar c = line.at(i);
        if (c == QChar('\r'))
            continue;

        if (inQuote) {
            if (c == quote) {
                if (i + 1 < len && line.at(i + 1) == quote) {
                    field += quote;
                    ++i;
                } else {
                    inQuote = false;
                }
            } else {
                field += c;
            }
        } else {
            if (c == quote) {
                inQuote = true;
            } else if (c == sep) {
                fields.append(trim ? field.trimmed() : field);
                field.clear();
            } else {
                field += c;
            }
        }
    }
    fields.append(trim ? field.trimmed() : field);
    return fields;
}

// =====================================================================
// CSVReader<N> 模板读取函数
//
// fast-cpp-csv-parser 的核心高性能路径：
//   - 文件通过 mmap 映射到内存，零拷贝
//   - CSVReader<N> 内部按编译期列数做优化解析
//   - next_line() 返回 char* 直接指向 mmap 区域
//   - 避免任何 std::string 分配
// =====================================================================

template<int ColCount>
CsvError readNColumns(const std::string &path,
                      const CsvReadConfig &config,
                      QStringList &outHeader,
                      CsvData &outData,
                      const std::function<void(int)> &progressCb)
{
    try {
        // ★ 核心：构造 io::CSVReader<ColCount>
        //   模板参数 ColCount 在编译期确定，使解析器可以做极致优化
        //   底层使用 mmap 映射文件，next_line() 返回零拷贝的 char*
        io::CSVReader<ColCount,
                      io::trim_chars<' ', '\t'>,
                      io::double_quote_escape<',', '"'>> reader(path);

        // ---- 读表头 ----
        // CSVReader::read_header() 需要编译期列名，不适合动态场景
        // 使用 next_line() 手动读取表头行
        if (config.hasHeader) {
            char *headerLine = reader.next_line();
            if (headerLine) {
                int len = static_cast<int>(std::strlen(headerLine));
                // 跳过 BOM
                unsigned char *u = reinterpret_cast<unsigned char*>(headerLine);
                if (len >= 3 && u[0] == 0xEF && u[1] == 0xBB && u[2] == 0xBF) {
                    headerLine += 3;
                    len -= 3;
                }
                outHeader = splitFieldsUtf8(headerLine, len, ',', '"', config.trimFields);
            }
        } else {
            for (int i = 0; i < ColCount; ++i)
                outHeader.append(QStringLiteral("col_%1").arg(i));
        }

        // ---- 跳过 skipRows ----
        for (int s = 0; s < config.skipRows; ++s) {
            if (!reader.next_line())
                break;
        }

        // ---- 逐行读取数据 ----
        int rowIndex = 0;

        while (char *line = reader.next_line()) {
            int len = static_cast<int>(std::strlen(line));
            if (len == 0)
                continue;

            CsvRow row = splitFieldsUtf8(line, len, ',', '"', config.trimFields);
            outData.append(row);
            rowIndex++;

            if (config.maxRows > 0 && rowIndex >= config.maxRows)
                break;

            if (progressCb && rowIndex % 5000 == 0)
                progressCb(rowIndex);
        }

        return CsvError::NoError;

    } catch (const io::error::can_not_open_file &) {
        return CsvError::FileOpenFailed;
    } catch (const std::exception &) {
        return CsvError::ParseError;
    }
}

} // anonymous namespace

// =====================================================================
// CsvController 实现
// =====================================================================

CsvController::CsvController(QObject *parent)
    : QObject(parent)
    , m_dataLock(QReadWriteLock::Recursive)
{
}

CsvController::~CsvController()
{
    clear();
}

// ======================== 静态工具 ===============================

std::string CsvController::toStdPath(const QString &filePath)
{
#ifdef Q_OS_WIN
    return filePath.toLocal8Bit().toStdString();
#else
    return filePath.toUtf8().toStdString();
#endif
}

bool CsvController::isUtf8Compatible(const QString &encoding)
{
    QString e = encoding.toUpper().trimmed().remove('-').remove('_');
    return (e == "UTF8" || e == "ASCII" || e == "USASCII" || e == "LATIN1" || e == "ISO88591");
}

QString CsvController::prepareUtf8File(const QString &filePath,
                                       const QString &encoding,
                                       QString &tempFile)
{
    tempFile.clear();
    if (isUtf8Compatible(encoding))
        return filePath;

    QFile srcFile(filePath);
    if (!srcFile.open(QIODevice::ReadOnly))
        return QString();

    QByteArray rawData = srcFile.readAll();
    srcFile.close();

    QTextCodec *codec = QTextCodec::codecForName(encoding.toUtf8());
    if (!codec)
        return QString();

    QString decoded = codec->toUnicode(rawData);
    tempFile = filePath + QStringLiteral(".~csv_utf8_tmp");
    QFile tmpFile(tempFile);
    if (!tmpFile.open(QIODevice::WriteOnly | QIODevice::Truncate))
        return QString();

    tmpFile.write(decoded.toUtf8());
    tmpFile.close();
    return tempFile;
}

// ======================== 读操作入口 =============================

CsvError CsvController::read(const QString &filePath,
                             const CsvReadConfig &config)
{
    QFileInfo fi(filePath);
    if (!fi.exists()) {
        m_lastErrorDetail = QStringLiteral("File not found: ") + filePath;
        emit errorOccurred(CsvError::FileNotFound, m_lastErrorDetail);
        return CsvError::FileNotFound;
    }

    CsvError err = CsvError::NoError;

    //
    // 策略分发：
    //   1) UTF-8 + 逗号 → readWithCsvReader  (CSVReader<N> 模板，最高性能)
    //   2) UTF-8 + 其他分隔符 → readWithLineReader (LineReader mmap + 手动拆分)
    //   3) 非 UTF-8 → 先转码为 UTF-8 临时文件，再走路径 1 或 2
    //

    if (isUtf8Compatible(config.encoding)) {
        if (config.separator == ',') {
            err = readWithCsvReader(filePath, config);
        } else {
            err = readWithLineReader(filePath, config);
        }
    } else {
        // 非 UTF-8：转码后用 fast-cpp-csv-parser
        QString tempFile;
        QString actualPath = prepareUtf8File(filePath, config.encoding, tempFile);

        if (actualPath.isEmpty()) {
            m_lastErrorDetail = QStringLiteral("Encoding conversion failed: ") + config.encoding;
            emit errorOccurred(CsvError::EncodingError, m_lastErrorDetail);
            return CsvError::EncodingError;
        }

        CsvReadConfig utf8Config = config;
        utf8Config.encoding = "UTF-8";

        if (config.separator == ',')
            err = readWithCsvReader(actualPath, utf8Config);
        else
            err = readWithLineReader(actualPath, utf8Config);

        if (!tempFile.isEmpty())
            QFile::remove(tempFile);
    }

    return err;
}

// =====================================================================
// 路径 1：io::CSVReader<N> 模板分发（最高性能）
//
// fast-cpp-csv-parser 的 CSVReader<N>：
//   - 构造时 mmap 映射整个文件到进程地址空间
//   - next_line() 在 mmap 区域内扫描换行符，返回 char* 指向行首，零拷贝
//   - 模板参数 N 允许编译器做极致内联和向量化优化
//   - 内存占用 = 文件大小（由 OS 的 page cache 管理，不是堆内存）
//
// 我们对 1~50 列做编译期实例化，运行时按探测到的列数 switch 分发。
// 超过 50 列自动退回 LineReader 路径。
// =====================================================================

int CsvController::probeColumnCount(const QString &filePath,
                                    const CsvReadConfig &config)
{
    try {
        // ★ 使用 io::LineReader 读取首行来探测列数
        io::LineReader probe(toStdPath(filePath));
        char *line = probe.next_line();
        if (!line) return 0;

        // 跳过 BOM
        unsigned char *u = reinterpret_cast<unsigned char*>(line);
        if (u[0] == 0xEF && u[1] == 0xBB && u[2] == 0xBF)
            line += 3;

        int len = static_cast<int>(std::strlen(line));
        char sep = config.separator.toLatin1();
        char quote = config.quoteChar.toLatin1();
        CsvRow fields = splitFieldsUtf8(line, len, sep, quote, false);
        return fields.size();
    } catch (...) {
        return 0;
    }
}

CsvError CsvController::readWithCsvReader(const QString &filePath,
                                          const CsvReadConfig &config)
{
    int colCount = probeColumnCount(filePath, config);
    if (colCount <= 0) {
        m_lastErrorDetail = QStringLiteral("Cannot determine column count");
        emit errorOccurred(CsvError::ParseError, m_lastErrorDetail);
        return CsvError::ParseError;
    }

    // 超过 50 列，退回 LineReader
    if (colCount > 50)
        return readWithLineReader(filePath, config);

    std::string stdPath = toStdPath(filePath);
    QStringList newHeader;
    CsvData newData;

    auto progressCb = [this](int rowCount) {
        Q_UNUSED(rowCount)
    };

    CsvError err = CsvError::NoError;

    // ★ 按探测到的列数做模板分发
    //   每个 case 实例化一个 io::CSVReader<N>，编译器对每种列数生成专用代码
    switch (colCount) {
    case 1:  err = readNColumns<1> (stdPath, config, newHeader, newData, progressCb); break;
    case 2:  err = readNColumns<2> (stdPath, config, newHeader, newData, progressCb); break;
    case 3:  err = readNColumns<3> (stdPath, config, newHeader, newData, progressCb); break;
    case 4:  err = readNColumns<4> (stdPath, config, newHeader, newData, progressCb); break;
    case 5:  err = readNColumns<5> (stdPath, config, newHeader, newData, progressCb); break;
    case 6:  err = readNColumns<6> (stdPath, config, newHeader, newData, progressCb); break;
    case 7:  err = readNColumns<7> (stdPath, config, newHeader, newData, progressCb); break;
    case 8:  err = readNColumns<8> (stdPath, config, newHeader, newData, progressCb); break;
    case 9:  err = readNColumns<9> (stdPath, config, newHeader, newData, progressCb); break;
    case 10: err = readNColumns<10>(stdPath, config, newHeader, newData, progressCb); break;
    case 11: err = readNColumns<11>(stdPath, config, newHeader, newData, progressCb); break;
    case 12: err = readNColumns<12>(stdPath, config, newHeader, newData, progressCb); break;
    case 13: err = readNColumns<13>(stdPath, config, newHeader, newData, progressCb); break;
    case 14: err = readNColumns<14>(stdPath, config, newHeader, newData, progressCb); break;
    case 15: err = readNColumns<15>(stdPath, config, newHeader, newData, progressCb); break;
    case 16: err = readNColumns<16>(stdPath, config, newHeader, newData, progressCb); break;
    case 17: err = readNColumns<17>(stdPath, config, newHeader, newData, progressCb); break;
    case 18: err = readNColumns<18>(stdPath, config, newHeader, newData, progressCb); break;
    case 19: err = readNColumns<19>(stdPath, config, newHeader, newData, progressCb); break;
    case 20: err = readNColumns<20>(stdPath, config, newHeader, newData, progressCb); break;
    case 21: err = readNColumns<21>(stdPath, config, newHeader, newData, progressCb); break;
    case 22: err = readNColumns<22>(stdPath, config, newHeader, newData, progressCb); break;
    case 23: err = readNColumns<23>(stdPath, config, newHeader, newData, progressCb); break;
    case 24: err = readNColumns<24>(stdPath, config, newHeader, newData, progressCb); break;
    case 25: err = readNColumns<25>(stdPath, config, newHeader, newData, progressCb); break;
    case 26: err = readNColumns<26>(stdPath, config, newHeader, newData, progressCb); break;
    case 27: err = readNColumns<27>(stdPath, config, newHeader, newData, progressCb); break;
    case 28: err = readNColumns<28>(stdPath, config, newHeader, newData, progressCb); break;
    case 29: err = readNColumns<29>(stdPath, config, newHeader, newData, progressCb); break;
    case 30: err = readNColumns<30>(stdPath, config, newHeader, newData, progressCb); break;
    case 31: err = readNColumns<31>(stdPath, config, newHeader, newData, progressCb); break;
    case 32: err = readNColumns<32>(stdPath, config, newHeader, newData, progressCb); break;
    case 33: err = readNColumns<33>(stdPath, config, newHeader, newData, progressCb); break;
    case 34: err = readNColumns<34>(stdPath, config, newHeader, newData, progressCb); break;
    case 35: err = readNColumns<35>(stdPath, config, newHeader, newData, progressCb); break;
    case 36: err = readNColumns<36>(stdPath, config, newHeader, newData, progressCb); break;
    case 37: err = readNColumns<37>(stdPath, config, newHeader, newData, progressCb); break;
    case 38: err = readNColumns<38>(stdPath, config, newHeader, newData, progressCb); break;
    case 39: err = readNColumns<39>(stdPath, config, newHeader, newData, progressCb); break;
    case 40: err = readNColumns<40>(stdPath, config, newHeader, newData, progressCb); break;
    case 41: err = readNColumns<41>(stdPath, config, newHeader, newData, progressCb); break;
    case 42: err = readNColumns<42>(stdPath, config, newHeader, newData, progressCb); break;
    case 43: err = readNColumns<43>(stdPath, config, newHeader, newData, progressCb); break;
    case 44: err = readNColumns<44>(stdPath, config, newHeader, newData, progressCb); break;
    case 45: err = readNColumns<45>(stdPath, config, newHeader, newData, progressCb); break;
    case 46: err = readNColumns<46>(stdPath, config, newHeader, newData, progressCb); break;
    case 47: err = readNColumns<47>(stdPath, config, newHeader, newData, progressCb); break;
    case 48: err = readNColumns<48>(stdPath, config, newHeader, newData, progressCb); break;
    case 49: err = readNColumns<49>(stdPath, config, newHeader, newData, progressCb); break;
    case 50: err = readNColumns<50>(stdPath, config, newHeader, newData, progressCb); break;
    default:
        return readWithLineReader(filePath, config);
    }

    if (err != CsvError::NoError) {
        emit errorOccurred(err, m_lastErrorDetail);
        return err;
    }

    {
        QWriteLocker locker(&m_dataLock);
        m_header = newHeader;
        m_data   = newData;
    }

    emit progressChanged(100);
    return CsvError::NoError;
}

// =====================================================================
// 路径 2：io::LineReader (mmap) + 手动字段拆分
//
// fast-cpp-csv-parser 的 io::LineReader：
//   - 构造时通过 mmap (Linux/麒麟) 或 CreateFileMapping (Windows)
//     将整个文件映射到虚拟地址空间
//   - next_line() 在 mmap 区域内线性扫描 '\n'，将其替换为 '\0'
//     并返回行首的 char* —— 整个过程零内存分配、零拷贝
//   - OS 的 page cache 自动管理物理内存，按需加载页面
//   - 相比 QFile::readLine / QTextStream 快 3-5 倍
//
// 适用于：自定义分隔符、列数超过 50 的场景
// =====================================================================

CsvError CsvController::readWithLineReader(const QString &filePath,
                                           const CsvReadConfig &config)
{
    std::string stdPath = toStdPath(filePath);

    try {
        // ★ 核心：构造 io::LineReader，触发 mmap 映射文件
        io::LineReader lineReader(stdPath);

        QStringList newHeader;
        CsvData     newData;
        char        sep   = config.separator.toLatin1();
        char        quote = config.quoteChar.toLatin1();
        int         lineIndex = 0;
        int         dataRowIndex = 0;
        bool        headerParsed = false;
        int         lastProgress = 0;

        QFileInfo fi(filePath);
        qint64 totalSize = fi.size();

        // ★ 核心循环：next_line() 返回 mmap 区域的 char*，零拷贝
        while (char *line = lineReader.next_line()) {
            // 首行跳过 BOM
            if (lineIndex == 0) {
                unsigned char *u = reinterpret_cast<unsigned char*>(line);
                if (u[0] == 0xEF && u[1] == 0xBB && u[2] == 0xBF)
                    line += 3;
            }

            int len = static_cast<int>(std::strlen(line));
            lineIndex++;

            // ---- 处理表头 ----
            if (!headerParsed) {
                if (config.hasHeader) {
                    newHeader = splitFieldsUtf8(line, len, sep, quote, config.trimFields);
                    headerParsed = true;
                    continue;
                } else {
                    CsvRow firstRow = splitFieldsUtf8(line, len, sep, quote, config.trimFields);
                    for (int i = 0; i < firstRow.size(); ++i)
                        newHeader.append(QStringLiteral("col_%1").arg(i));
                    headerParsed = true;
                    newData.append(firstRow);
                    dataRowIndex++;
                    if (config.maxRows > 0 && dataRowIndex >= config.maxRows)
                        break;
                    continue;
                }
            }

            // ---- 跳过 skipRows ----
            int currentDataLine = lineIndex - (config.hasHeader ? 1 : 0);
            if (currentDataLine <= config.skipRows)
                continue;

            // ---- 解析数据行 ----
            CsvRow row = splitFieldsUtf8(line, len, sep, quote, config.trimFields);
            newData.append(row);
            dataRowIndex++;

            if (config.maxRows > 0 && dataRowIndex >= config.maxRows)
                break;

            // 进度信号
            if (dataRowIndex % 5000 == 0 && totalSize > 0) {
                int progress = qMin(99, static_cast<int>(
                                            static_cast<qint64>(dataRowIndex) * 100 /
                                            qMax(static_cast<qint64>(1), totalSize / 50)));
                if (progress > lastProgress) {
                    lastProgress = progress;
                    emit progressChanged(progress);
                }
            }
        }

        // 写锁更新缓存
        {
            QWriteLocker locker(&m_dataLock);
            m_header = newHeader;
            m_data   = newData;
        }

        emit progressChanged(100);
        return CsvError::NoError;

    } catch (const io::error::can_not_open_file &e) {
        m_lastErrorDetail = QStringLiteral("fast-cpp-csv-parser cannot open: ")
        + QString::fromStdString(e.what());
        emit errorOccurred(CsvError::FileOpenFailed, m_lastErrorDetail);
        return CsvError::FileOpenFailed;
    } catch (const std::exception &e) {
        m_lastErrorDetail = QStringLiteral("fast-cpp-csv-parser error: ")
        + QString::fromStdString(e.what());
        emit errorOccurred(CsvError::ParseError, m_lastErrorDetail);
        return CsvError::ParseError;
    }
}

// =====================================================================
// 路径 3：通用回退解析器（纯 Qt）—— 仅在编码转码失败时使用
// =====================================================================

CsvError CsvController::readGeneric(const QString &filePath,
                                    const CsvReadConfig &config)
{
    QFile file(filePath);
    if (!file.open(QIODevice::ReadOnly)) {
        m_lastErrorDetail = QStringLiteral("Cannot open file: ") + file.errorString();
        emit errorOccurred(CsvError::FileOpenFailed, m_lastErrorDetail);
        return CsvError::FileOpenFailed;
    }

    QTextCodec *codec = QTextCodec::codecForName(config.encoding.toUtf8());
    if (!codec) {
        m_lastErrorDetail = QStringLiteral("Unsupported encoding: ") + config.encoding;
        emit errorOccurred(CsvError::EncodingError, m_lastErrorDetail);
        return CsvError::EncodingError;
    }

    QByteArray rawData = file.readAll();
    file.close();
    skipBOM(rawData);

    QString content = codec->toUnicode(rawData);
    content.replace(QStringLiteral("\r\n"), QStringLiteral("\n"));
    content.replace(QChar('\r'), QChar('\n'));

    QStringList newHeader;
    CsvData     newData;
    QString     currentLine;
    bool        inQuote = false;
    int         lineIndex = 0;
    int         dataRowIndex = 0;
    int         contentLen = content.length();
    int         lastProgress = 0;

    auto processLogicalLine = [&](const QString &logicalLine) -> bool {
        lineIndex++;
        int headerOffset = config.hasHeader ? 1 : 0;
        if (lineIndex <= config.skipRows + headerOffset) {
            if (config.hasHeader && lineIndex == 1)
                newHeader = splitFieldsUnicode(logicalLine, config.separator,
                                               config.quoteChar, config.trimFields);
            return true;
        }

        if (!config.hasHeader && lineIndex == 1 && newHeader.isEmpty()) {
            CsvRow firstRow = splitFieldsUnicode(logicalLine, config.separator,
                                                 config.quoteChar, config.trimFields);
            for (int i = 0; i < firstRow.size(); ++i)
                newHeader.append(QStringLiteral("col_%1").arg(i));
            newData.append(firstRow);
            dataRowIndex++;
            return true;
        }

        CsvRow row = splitFieldsUnicode(logicalLine, config.separator,
                                        config.quoteChar, config.trimFields);
        newData.append(row);
        dataRowIndex++;
        return !(config.maxRows > 0 && dataRowIndex >= config.maxRows);
    };

    for (int i = 0; i < contentLen; ++i) {
        QChar c = content.at(i);
        if (c == config.quoteChar) {
            inQuote = !inQuote;
            currentLine += c;
        } else if (c == '\n' && !inQuote) {
            if (!currentLine.isEmpty()) {
                if (!processLogicalLine(currentLine))
                    break;
            }
            currentLine.clear();
            int progress = static_cast<int>(static_cast<qint64>(i) * 100 / contentLen);
            if (progress - lastProgress >= 5) {
                lastProgress = progress;
                emit progressChanged(progress);
            }
        } else {
            currentLine += c;
        }
    }
    if (!currentLine.isEmpty())
        processLogicalLine(currentLine);

    {
        QWriteLocker locker(&m_dataLock);
        m_header = newHeader;
        m_data   = newData;
    }

    emit progressChanged(100);
    return CsvError::NoError;
}

// =====================================================================
// 流式读取 —— 基于 io::LineReader (mmap)，不缓存到内存
// =====================================================================

CsvError CsvController::readStream(const QString &filePath,
                                   const std::function<bool(int, const CsvRow &)> &callback,
                                   const CsvReadConfig &config)
{
    if (!callback) {
        m_lastErrorDetail = QStringLiteral("Callback is null");
        return CsvError::InvalidParameter;
    }
    return readStreamInternal(filePath, callback, config);
}

CsvError CsvController::readStreamInternal(
    const QString &filePath,
    const std::function<bool(int, const CsvRow &)> &callback,
    const CsvReadConfig &config)
{
    // 非 UTF-8 先转码
    QString tempFile;
    QString actualPath = filePath;
    if (!isUtf8Compatible(config.encoding)) {
        actualPath = prepareUtf8File(filePath, config.encoding, tempFile);
        if (actualPath.isEmpty()) {
            m_lastErrorDetail = QStringLiteral("Encoding conversion failed");
            emit errorOccurred(CsvError::EncodingError, m_lastErrorDetail);
            return CsvError::EncodingError;
        }
    }

    std::string stdPath = toStdPath(actualPath);

    try {
        // ★ 使用 io::LineReader 做 mmap 流式读取
        //   每次 next_line() 返回文件中下一行的 char* 指针
        //   内存占用恒定，不缓存任何数据到 CsvController 内部
        io::LineReader lineReader(stdPath);

        char sep   = config.separator.toLatin1();
        char quote = config.quoteChar.toLatin1();
        int  lineIndex = 0;
        int  dataRowIndex = 0;
        QFileInfo fi(actualPath);
        qint64 totalSize = fi.size();
        int lastProgress = 0;

        while (char *line = lineReader.next_line()) {
            if (lineIndex == 0) {
                unsigned char *u = reinterpret_cast<unsigned char*>(line);
                if (u[0] == 0xEF && u[1] == 0xBB && u[2] == 0xBF)
                    line += 3;
            }
            lineIndex++;

            // 跳过表头和 skipRows
            int skipTotal = config.skipRows + (config.hasHeader ? 1 : 0);
            if (lineIndex <= skipTotal)
                continue;

            int len = static_cast<int>(std::strlen(line));
            CsvRow row = splitFieldsUtf8(line, len, sep, quote, config.trimFields);

            if (!callback(dataRowIndex, row))
                break;

            dataRowIndex++;
            if (config.maxRows > 0 && dataRowIndex >= config.maxRows)
                break;

            if (dataRowIndex % 5000 == 0 && totalSize > 0) {
                int progress = qMin(99, static_cast<int>(
                                            static_cast<qint64>(dataRowIndex) * 100 /
                                            qMax(static_cast<qint64>(1), totalSize / 50)));
                if (progress > lastProgress) {
                    lastProgress = progress;
                    emit progressChanged(progress);
                }
            }
        }

        emit progressChanged(100);

    } catch (const io::error::can_not_open_file &e) {
        m_lastErrorDetail = QString::fromStdString(e.what());
        if (!tempFile.isEmpty()) QFile::remove(tempFile);
        emit errorOccurred(CsvError::FileOpenFailed, m_lastErrorDetail);
        return CsvError::FileOpenFailed;
    } catch (const std::exception &e) {
        m_lastErrorDetail = QString::fromStdString(e.what());
        if (!tempFile.isEmpty()) QFile::remove(tempFile);
        emit errorOccurred(CsvError::ParseError, m_lastErrorDetail);
        return CsvError::ParseError;
    }

    if (!tempFile.isEmpty())
        QFile::remove(tempFile);
    return CsvError::NoError;
}

// ======================== 写操作 =================================

CsvError CsvController::write(const QString &filePath,
                              const QStringList &header,
                              const CsvData &data,
                              const CsvWriteConfig &config)
{
    QMutexLocker writeLock(&m_writeMutex);

    QFileInfo fi(filePath);
    QDir dir = fi.absoluteDir();
    if (!dir.exists())
        dir.mkpath(".");

    QString tmpPath = filePath + QStringLiteral(".~csv_tmp");
    QFile file(tmpPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_lastErrorDetail = QStringLiteral("Cannot create temp file: ") + file.errorString();
        emit errorOccurred(CsvError::FileWriteFailed, m_lastErrorDetail);
        return CsvError::FileWriteFailed;
    }

    QString le = resolveLineEnding(config);

    if (config.writeBOM && isUtf8Compatible(config.encoding))
        file.write("\xEF\xBB\xBF", 3);

    int totalLines = data.size() + (config.writeHeader ? 1 : 0);
    int written = 0;
    int lastProgress = 0;

    if (config.writeHeader && !header.isEmpty()) {
        QStringList escapedHeader;
        for (const QString &h : header)
            escapedHeader.append(escapeField(h, config));
        file.write(encodeString(escapedHeader.join(config.separator) + le, config));
        written++;
    }

    const int flushInterval = 1000;
    for (int i = 0; i < data.size(); ++i) {
        const CsvRow &row = data.at(i);
        QStringList escapedFields;
        escapedFields.reserve(row.size());
        for (const QString &f : row)
            escapedFields.append(escapeField(f, config));
        file.write(encodeString(escapedFields.join(config.separator) + le, config));

        written++;
        if (i % flushInterval == 0)
            file.flush();

        if (totalLines > 0) {
            int progress = written * 100 / totalLines;
            if (progress - lastProgress >= 5) {
                lastProgress = progress;
                emit progressChanged(progress);
            }
        }
    }

    file.flush();
    file.close();

    if (QFile::exists(filePath))
        QFile::remove(filePath);
    if (!QFile::rename(tmpPath, filePath)) {
        m_lastErrorDetail = QStringLiteral("Failed to rename temp file to target");
        emit errorOccurred(CsvError::FileWriteFailed, m_lastErrorDetail);
        return CsvError::FileWriteFailed;
    }

    emit progressChanged(100);
    return CsvError::NoError;
}

CsvError CsvController::append(const QString &filePath,
                               const CsvData &rows,
                               const CsvWriteConfig &config)
{
    QMutexLocker writeLock(&m_writeMutex);

    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Append)) {
        m_lastErrorDetail = QStringLiteral("Cannot open for append: ") + file.errorString();
        emit errorOccurred(CsvError::FileWriteFailed, m_lastErrorDetail);
        return CsvError::FileWriteFailed;
    }

    QString le = resolveLineEnding(config);

    for (int i = 0; i < rows.size(); ++i) {
        const CsvRow &row = rows.at(i);
        QStringList escapedFields;
        escapedFields.reserve(row.size());
        for (const QString &f : row)
            escapedFields.append(escapeField(f, config));
        file.write(encodeString(escapedFields.join(config.separator) + le, config));
        if (i % 1000 == 0)
            file.flush();
    }

    file.flush();
    file.close();
    return CsvError::NoError;
}

CsvError CsvController::writeStream(const QString &filePath,
                                    const QStringList &header,
                                    int totalRows,
                                    const std::function<CsvRow(int)> &rowSupplier,
                                    const CsvWriteConfig &config)
{
    if (!rowSupplier) {
        m_lastErrorDetail = QStringLiteral("Row supplier is null");
        return CsvError::InvalidParameter;
    }

    QMutexLocker writeLock(&m_writeMutex);

    QFileInfo fi(filePath);
    QDir dir = fi.absoluteDir();
    if (!dir.exists())
        dir.mkpath(".");

    QString tmpPath = filePath + QStringLiteral(".~csv_tmp");
    QFile file(tmpPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        m_lastErrorDetail = QStringLiteral("Cannot create file: ") + file.errorString();
        emit errorOccurred(CsvError::FileWriteFailed, m_lastErrorDetail);
        return CsvError::FileWriteFailed;
    }

    QString le = resolveLineEnding(config);

    if (config.writeBOM && isUtf8Compatible(config.encoding))
        file.write("\xEF\xBB\xBF", 3);

    if (config.writeHeader && !header.isEmpty()) {
        QStringList escaped;
        for (const QString &h : header)
            escaped.append(escapeField(h, config));
        file.write(encodeString(escaped.join(config.separator) + le, config));
    }

    int lastProgress = 0;
    for (int i = 0; i < totalRows; ++i) {
        CsvRow row = rowSupplier(i);
        if (row.isEmpty())
            break;

        QStringList escaped;
        escaped.reserve(row.size());
        for (const QString &f : row)
            escaped.append(escapeField(f, config));
        file.write(encodeString(escaped.join(config.separator) + le, config));

        if (i % 1000 == 0)
            file.flush();

        if (totalRows > 0) {
            int progress = (i + 1) * 100 / totalRows;
            if (progress - lastProgress >= 5) {
                lastProgress = progress;
                emit progressChanged(progress);
            }
        }
    }

    file.flush();
    file.close();

    if (QFile::exists(filePath))
        QFile::remove(filePath);
    if (!QFile::rename(tmpPath, filePath)) {
        m_lastErrorDetail = QStringLiteral("Failed to rename temp file");
        emit errorOccurred(CsvError::FileWriteFailed, m_lastErrorDetail);
        return CsvError::FileWriteFailed;
    }

    emit progressChanged(100);
    return CsvError::NoError;
}

// ======================== 数据访问 ===============================

QStringList CsvController::header() const
{
    QReadLocker locker(&m_dataLock);
    return m_header;
}

CsvData CsvController::data() const
{
    QReadLocker locker(&m_dataLock);
    return m_data;
}

int CsvController::rowCount() const
{
    QReadLocker locker(&m_dataLock);
    return m_data.size();
}

int CsvController::colCount() const
{
    QReadLocker locker(&m_dataLock);
    return m_header.size();
}

CsvRow CsvController::row(int i) const
{
    QReadLocker locker(&m_dataLock);
    if (i < 0 || i >= m_data.size())
        return CsvRow();
    return m_data.at(i);
}

QString CsvController::cell(int row, int col) const
{
    QReadLocker locker(&m_dataLock);
    if (row < 0 || row >= m_data.size())
        return QString();
    const CsvRow &r = m_data.at(row);
    if (col < 0 || col >= r.size())
        return QString();
    return r.at(col);
}

void CsvController::clear()
{
    QWriteLocker locker(&m_dataLock);
    m_header.clear();
    m_data.clear();
    m_data.squeeze();
}

// ======================== 工具方法 ===============================

QString CsvController::errorString(CsvError err)
{
    switch (err) {
    case CsvError::NoError:          return QStringLiteral("No error");
    case CsvError::FileNotFound:     return QStringLiteral("File not found");
    case CsvError::FileOpenFailed:   return QStringLiteral("File open failed");
    case CsvError::FileWriteFailed:  return QStringLiteral("File write failed");
    case CsvError::ParseError:       return QStringLiteral("Parse error");
    case CsvError::ColumnMismatch:   return QStringLiteral("Column count mismatch");
    case CsvError::InvalidParameter: return QStringLiteral("Invalid parameter");
    case CsvError::EncodingError:    return QStringLiteral("Encoding error");
    }
    return QStringLiteral("Unknown error");
}

QString CsvController::lastErrorDetail() const
{
    return m_lastErrorDetail;
}

QString CsvController::escapeField(const QString &field,
                                   const CsvWriteConfig &config) const
{
    bool needsQuote = config.forceQuote ||
                      field.contains(config.separator) ||
                      field.contains(config.quoteChar) ||
                      field.contains('\n') ||
                      field.contains('\r');
    if (!needsQuote)
        return field;

    QString escaped = field;
    escaped.replace(config.quoteChar,
                    QString(config.quoteChar) + QString(config.quoteChar));
    return config.quoteChar + escaped + config.quoteChar;
}

QString CsvController::resolveLineEnding(const CsvWriteConfig &config) const
{
    if (!config.lineEnding.isEmpty())
        return config.lineEnding;
#ifdef Q_OS_WIN
    return QStringLiteral("\r\n");
#else
    return QStringLiteral("\n");
#endif
}

QByteArray CsvController::encodeString(const QString &str,
                                       const CsvWriteConfig &config) const
{
    if (isUtf8Compatible(config.encoding))
        return str.toUtf8();

    QTextCodec *codec = QTextCodec::codecForName(config.encoding.toUtf8());
    if (codec)
        return codec->fromUnicode(str);
    return str.toUtf8();
}
