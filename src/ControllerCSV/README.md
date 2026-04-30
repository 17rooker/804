# CsvController — 基于 fast-cpp-csv-parser 的线程安全 CSV 控制类

## 环境要求

| 项目 | 要求 |
|------|------|
| Qt 版本 | 5.12.9 |
| 构建工具 | qmake + MinGW 64-bit |
| 目标平台 | Windows 10/11、银河麒麟 V10 (x86_64 / aarch64) |
| 外部依赖 | [fast-cpp-csv-parser](https://github.com/ben-strasser/fast-cpp-csv-parser)（header-only，仅需 `csv.h`） |

## 快速集成

### 1. 获取 fast-cpp-csv-parser

```bash
# 将 csv.h 下载到本目录
wget https://raw.githubusercontent.com/ben-strasser/fast-cpp-csv-parser/master/csv.h
# 或直接从 GitHub Release 下载
```

### 2. 在 .pro 文件中引入

```qmake
# your_project.pro
QT += core
CONFIG += c++14

include($$PWD/csv_controller/csv_controller.pri)

# 如果使用 QtConcurrent（多线程示例需要）
QT += concurrent
```

### 3. 目录结构

```
your_project/
├── your_project.pro
├── main.cpp
└── csv_controller/
    ├── csv_controller.pri      # qmake 引入文件
    ├── CsvController.h         # 头文件
    ├── CsvController.cpp       # 实现
    ├── csv.h                   # fast-cpp-csv-parser (需自行放入)
    └── example_usage.cpp       # 使用示例
```

## 核心特性

### 线程安全模型

```
┌─────────────────────────────────────────────────────┐
│                   CsvController                      │
│                                                      │
│   ┌─────────────────────┐  ┌──────────────────────┐ │
│   │  QReadWriteLock      │  │  QMutex              │ │
│   │  (m_dataLock)        │  │  (m_writeMutex)      │ │
│   │                      │  │                      │ │
│   │  保护内存缓存数据     │  │  串行化文件写操作     │ │
│   │  - m_header          │  │  - write()           │ │
│   │  - m_data            │  │  - append()          │ │
│   │                      │  │  - writeStream()     │ │
│   │  读操作并发不阻塞     │  │                      │ │
│   │  写操作独占           │  │  写文件互斥           │ │
│   └─────────────────────┘  └──────────────────────┘ │
│                                                      │
│  read() ── 加写锁更新缓存，读文件本身无锁             │
│  header()/data()/row()/cell() ── 加读锁，可并发       │
│  readStream() ── 不修改缓存，无需加锁                 │
└─────────────────────────────────────────────────────┘
```

- **多读并发**：`header()`, `data()`, `row()`, `cell()` 使用 `QReadLocker`，多线程可同时调用
- **读写互斥**：`read()` 更新缓存时使用 `QWriteLocker`，阻塞并发读
- **写串行化**：所有写文件操作通过 `QMutex` 串行化，避免文件损坏
- **流式读取无锁**：`readStream()` 不修改内部状态，天然线程安全

### 编码支持

| 编码 | 读 | 写 | 说明 |
|------|----|----|------|
| UTF-8 | ✅ | ✅ | 默认编码，支持 BOM |
| GBK | ✅ | ✅ | 银河麒麟 V10 常见 |
| GB18030 | ✅ | ✅ | 国标全字符集 |
| Latin-1 | ✅ | ✅ | 西欧编码 |
| 其他 | ✅ | ✅ | 任何 QTextCodec 支持的编码 |

### 性能策略

| 场景 | API | 内存特点 |
|------|-----|----------|
| 小文件 (<10MB) | `read()` + `data()` | 全量加载到内存 |
| 大文件 (10MB~1GB) | `readStream()` | 恒定内存，逐行回调 |
| 大量写入 | `writeStream()` | 恒定内存，逐行供给 |
| 追加写入 | `append()` | 不读取原文件 |

### 写入安全

- 覆盖写入采用**临时文件 + 原子重命名**策略，防止写入中断导致数据丢失
- 每 1000 行 flush 一次，平衡性能与数据安全
- 自动创建不存在的目录

## API 速览

```cpp
CsvController csv;

// 读取
csv.read("data.csv");                           // UTF-8 默认配置
csv.read("data.csv", {.encoding = "GBK"});      // GBK 编码

// 访问数据
QStringList h = csv.header();       // 表头
CsvData d = csv.data();            // 全部数据
CsvRow r = csv.row(0);             // 第 0 行
QString c = csv.cell(0, 1);        // 第 0 行第 1 列

// 写入
csv.write("out.csv", header, data);
csv.append("out.csv", newRows);

// 流式（大文件）
csv.readStream("big.csv", [](int i, const CsvRow &row) {
    // 处理每一行
    return true;  // false 停止
});

// 进度监听
connect(&csv, &CsvController::progressChanged, [](int pct) {
    qDebug() << pct << "%";
});
```

## 跨平台注意事项

### Windows
- 中文路径通过 `toLocal8Bit()` 转换，MinGW64 完全兼容
- 写入时默认使用 `\r\n` 换行符
- 建议写 UTF-8 BOM 以便 Excel 正确识别

### 银河麒麟 V10
- 默认 UTF-8 编码，但遗留系统可能使用 GBK
- 路径通过 `toUtf8()` 转换
- 写入时默认使用 `\n` 换行符
- aarch64 架构完全兼容（fast-cpp-csv-parser 为纯 C++ 实现）

## 编译

```bash
cd your_project
qmake your_project.pro
make -j$(nproc)   # Linux
mingw32-make -j4   # Windows
```

## License

CsvController 本身为示例代码，可自由使用。
fast-cpp-csv-parser 使用 BSD-3-Clause 许可证。
