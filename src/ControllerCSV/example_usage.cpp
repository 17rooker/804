/**
 * @file example_usage.cpp
 * @brief CsvController 使用示例
 *
 * 演示：基本读写、流式读写、多线程并发读取、进度监听
 */

#include "CsvController.h"

#include <QCoreApplication>
#include <QDebug>
#include <QThread>
#include <QtConcurrent>
#include <QFutureWatcher>

// =====================================================================
// 示例 1：基本读写
// =====================================================================
void example_basic()
{
    CsvController csv;

    // ---- 监听进度 ----
    QObject::connect(&csv, &CsvController::progressChanged, [](int pct) {
        qDebug() << "Progress:" << pct << "%";
    });

    // ---- 写入 ----
    QStringList header = {"姓名", "年龄", "城市", "备注"};
    CsvData data;
    data.append({"张三",   "28", "北京", "包含,逗号"});
    data.append({"李四",   "35", "上海", "包含\"引号\""});
    data.append({"王五",   "42", "深圳", "包含\n换行"});
    data.append({"Alice",  "30", "Singapore", "Normal text"});

    CsvWriteConfig wCfg;
    wCfg.writeBOM = true;  // 写 BOM，方便 Windows Excel 识别 UTF-8

    CsvError err = csv.write("/tmp/test_output.csv", header, data, wCfg);
    if (err != CsvError::NoError) {
        qWarning() << "Write failed:" << CsvController::errorString(err)
                   << csv.lastErrorDetail();
        return;
    }
    qDebug() << "Write OK!";

    // ---- 读取 ----
    CsvReadConfig rCfg;
    rCfg.encoding = "UTF-8";

    err = csv.read("/tmp/test_output.csv", rCfg);
    if (err != CsvError::NoError) {
        qWarning() << "Read failed:" << CsvController::errorString(err);
        return;
    }

    qDebug() << "Header:" << csv.header();
    qDebug() << "Row count:" << csv.rowCount();
    for (int i = 0; i < csv.rowCount(); ++i) {
        qDebug() << "  Row" << i << ":" << csv.row(i);
    }
    qDebug() << "Cell(1,0):" << csv.cell(1, 0);  // "李四"
}

// =====================================================================
// 示例 2：流式读取大文件（不占用大量内存）
// =====================================================================
void example_stream_read()
{
    CsvController csv;

    int totalRows = 0;
    double sumAge = 0;

    CsvError err = csv.readStream(
        "/tmp/large_data.csv",
        [&](int rowIndex, const CsvRow &row) -> bool {
            // 逐行处理，内存占用恒定
            totalRows++;
            if (row.size() > 1) {
                sumAge += row.at(1).toDouble();
            }
            // 返回 false 可提前终止
            return true;
        }
    );

    if (err == CsvError::NoError) {
        qDebug() << "Processed" << totalRows << "rows"
                 << "avg age:" << (totalRows > 0 ? sumAge / totalRows : 0);
    }
}

// =====================================================================
// 示例 3：流式写入超大数据集
// =====================================================================
void example_stream_write()
{
    CsvController csv;

    QObject::connect(&csv, &CsvController::progressChanged, [](int pct) {
        if (pct % 20 == 0) qDebug() << "Writing..." << pct << "%";
    });

    QStringList header = {"id", "value", "timestamp"};
    int total = 1000000;  // 100 万行

    CsvError err = csv.writeStream(
        "/tmp/big_output.csv",
        header,
        total,
        [](int rowIndex) -> CsvRow {
            return {
                QString::number(rowIndex),
                QString::number(qrand() % 10000 / 100.0, 'f', 2),
                QStringLiteral("2026-04-02T12:00:00")
            };
        }
    );

    qDebug() << "Stream write:" << CsvController::errorString(err);
}

// =====================================================================
// 示例 4：多线程并发读取（读写分离，互不阻塞）
// =====================================================================
void example_concurrent_read()
{
    // 共享同一个 CsvController 实例
    auto csv = QSharedPointer<CsvController>::create();

    // 先加载数据
    csv->read("/tmp/test_output.csv");

    // 多线程并发访问已加载的数据（读锁保护，不阻塞）
    QList<QFuture<void>> futures;

    for (int t = 0; t < 4; ++t) {
        QFuture<void> future = QtConcurrent::run([csv, t]() {
            // 安全地并发读取
            qDebug() << "Thread" << t << "header:" << csv->header();
            for (int i = 0; i < csv->rowCount(); ++i) {
                CsvRow row = csv->row(i);
                Q_UNUSED(row)
            }
            qDebug() << "Thread" << t << "done, rows:" << csv->rowCount();
        });
        futures.append(future);
    }

    // 等待所有线程完成
    for (auto &f : futures)
        f.waitForFinished();

    qDebug() << "All threads completed.";
}

// =====================================================================
// 示例 5：GBK 编码文件读写（银河麒麟 V10 常见场景）
// =====================================================================
void example_gbk()
{
    CsvController csv;

    // 写 GBK 编码文件
    QStringList header = {"编号", "名称", "数量"};
    CsvData data;
    data.append({"001", "螺丝", "1000"});
    data.append({"002", "螺母", "2000"});

    CsvWriteConfig wCfg;
    wCfg.encoding = "GBK";

    csv.write("/tmp/gbk_test.csv", header, data, wCfg);

    // 读 GBK 编码文件
    CsvReadConfig rCfg;
    rCfg.encoding = "GBK";

    csv.read("/tmp/gbk_test.csv", rCfg);
    qDebug() << "GBK header:" << csv.header();
    qDebug() << "GBK data:"   << csv.data();
}

// =====================================================================
// 示例 6：追加数据
// =====================================================================
void example_append()
{
    CsvController csv;

    CsvData newRows;
    newRows.append({"赵六", "55", "广州", "追加行1"});
    newRows.append({"钱七", "22", "杭州", "追加行2"});

    CsvError err = csv.append("/tmp/test_output.csv", newRows);
    qDebug() << "Append:" << CsvController::errorString(err);
}

// =====================================================================
// 示例 7：自定义分隔符（TSV / 分号分隔）
// =====================================================================
void example_tsv()
{
    CsvController csv;

    QStringList header = {"col1", "col2", "col3"};
    CsvData data;
    data.append({"a", "b", "c"});

    CsvWriteConfig wCfg;
    wCfg.separator = '\t';  // TSV

    csv.write("/tmp/test.tsv", header, data, wCfg);

    CsvReadConfig rCfg;
    rCfg.separator = '\t';
    csv.read("/tmp/test.tsv", rCfg);
    qDebug() << "TSV:" << csv.header() << csv.data();
}

// =====================================================================
// main
// =====================================================================
int main(int argc, char *argv[])
{
    QCoreApplication app(argc, argv);

    qDebug() << "=== Basic Read/Write ===";
    example_basic();

    qDebug() << "\n=== Append ===";
    example_append();

    qDebug() << "\n=== GBK Encoding ===";
    example_gbk();

    qDebug() << "\n=== TSV ===";
    example_tsv();

    qDebug() << "\n=== Concurrent Read ===";
    example_concurrent_read();

    qDebug() << "\n=== Stream Write (large) ===";
    example_stream_write();

    return 0;
}