#include "fileloadworker.h"
#include "src/DataProcess/DataAnalysis/FrameDataAnalysis.h"
#include "src/DataProcess/MessageFrameConfig.h"
#include "src/Common/CommTypes.h"

#include <QFile>
#include <QProcess>
#include <QThread>
#include <QDebug>

void FileLoadWorker::loadFile(const QString &filePath)
{
    m_headers.clear();
    m_plotData.clear();

    // XLSX：仅Python解析首行表头（无数据行支持，速度快不用进度条）
    if (filePath.endsWith(".xlsx", Qt::CaseInsensitive)) {
        QProcess py;
        QString script = QStringLiteral(
            "import sys,zipfile,xml.etree.ElementTree as ET\n"
            "f=sys.argv[1]\n"
            "ns={'s':'http://schemas.openxmlformats.org/spreadsheetml/2006/main'}\n"
            "with zipfile.ZipFile(f) as z:\n"
            "  si=[]\n"
            "  if 'xl/sharedStrings.xml' in z.namelist():\n"
            "    t=ET.parse(z.open('xl/sharedStrings.xml'))\n"
            "    for n in t.getroot().findall('.//s:t',ns): si.append(n.text or '')\n"
            "  t=ET.parse(z.open('xl/worksheets/sheet1.xml'))\n"
            "  r=t.getroot().find('.//s:sheetData/s:row',ns)\n"
            "  if r is not None:\n"
            "    o=[]\n"
            "    for c in r.findall('s:c',ns):\n"
            "      v=c.find('s:v',ns)\n"
            "      if v is not None and v.text:\n"
            "        val=v.text\n"
            "        if c.get('t')=='s' and val.isdigit() and int(val)<len(si): val=si[int(val)]\n"
            "        o.append(val)\n"
            "    sys.stdout.write(','.join(o))");
        py.start("python3", {"-c", script, filePath});
        py.waitForFinished();
        QString out = QString::fromUtf8(py.readAllStandardOutput()).trimmed();
        if (!py.exitCode() && !out.isEmpty()) m_headers = out.split(',');
        emit finished();
        return;
    }

    // 二进制协议文件（A5/A6）
    if (filePath.endsWith(".xls", Qt::CaseInsensitive) || filePath.endsWith(".dat", Qt::CaseInsensitive)) {
        QFile f(filePath);
        if (!f.open(QIODevice::ReadOnly)) { emit finished(); return; }
        QByteArray raw = f.readAll();
        f.close();

        // 构建 csvField → field.id 映射
        QMap<QString, QString> csvFieldToId;
        QMap<QString, int> csvFieldOrder;
        int order = 0;
        STFrameFormat fmt;
        auto collectFields = [&](int frameLen) {
            if (MessageFrameConfig::getInstance().findFrameFormat("serial_E", frameLen, fmt)) {
                for (const auto *fv : {&fmt.frameHeader, &fmt.frameBody, &fmt.frameTail})
                    for (const auto &field : *fv) {
                        QString csvName = field.csvField.isEmpty() ? field.id : field.csvField;
                        if (!csvFieldToId.contains(csvName)) {
                            csvFieldToId[csvName] = field.id;
                            csvFieldOrder[csvName] = order++;
                        }
                    }
            }
        };
        collectFields(428);
        collectFields(97);
        if (csvFieldToId.isEmpty()) { emit finished(); return; }

        // 按字段顺序排序
        QMap<int, QString> orderedHeaders;
        for (auto it = csvFieldToId.cbegin(); it != csvFieldToId.cend(); ++it)
            orderedHeaders[csvFieldOrder[it.key()]] = it.key();
        m_headers.clear();
        for (auto it = orderedHeaders.cbegin(); it != orderedHeaders.cend(); ++it)
            m_headers.append(it.value());

        // 第一遍：扫描所有帧边界
        struct FramePos { int pos; int len; };
        QVector<FramePos> frames;
        for (int pos = 0; pos + 8 < raw.size(); pos++) {
            if (raw[pos] != char(0xFD) || raw[pos+1] != char(0xB1) || raw[pos+2] != char(0x85) || raw[pos+3] != char(0x40))
                continue;
            int fl = (quint8(raw[pos+4])<<24)|(quint8(raw[pos+5])<<16)|(quint8(raw[pos+6])<<8)|quint8(raw[pos+7]);
            if ((fl != 428 && fl != 97) || pos + fl > raw.size()) continue;
            frames.append({pos, fl});
        }
        if (frames.isEmpty()) { emit finished(); return; }

        // 第二遍：逐帧解析
        emit progressChanged(0, QString("解析 0/%1 帧").arg(frames.size()));
        FrameDataAnalysis analy;
        int total = frames.size();
        for (int i = 0; i < total; i++) {
            if (QThread::currentThread()->isInterruptionRequested()) break;

            QByteArray frameData = raw.mid(frames[i].pos, frames[i].len);
            STPackage p;
            p.channelId = "serial_E"; p.channelType = EChannelType::Serial; p.baDataRecv = frameData;

            STParamInfo paramInfo;
            analy.parseData(p, paramInfo);

            for (auto it = csvFieldToId.cbegin(); it != csvFieldToId.cend(); ++it) {
                if (paramInfo.mapParams.contains(it.value())) {
                    double val = paramInfo.mapParams[it.value()].varParaValue.toDouble();
                    m_plotData[it.key()].append(val);
                }
            }

            if (i % 20 == 0)
                emit progressChanged(i * 100 / total, QString("解析 %1/%2 帧").arg(i+1).arg(total));
        }
        emit progressChanged(100, "完成");
        emit finished();
        return;
    }

    // 纯CSV/TXT文件
    QFile f(filePath);
    if (!f.open(QIODevice::ReadOnly)) { emit finished(); return; }
    QByteArray raw = f.readAll();
    f.close();
    if (raw.contains('\0')) { emit finished(); return; }

    QString content;
    if (raw.size() >= 3 && (quint8)raw[0] == 0xEF && (quint8)raw[1] == 0xBB && (quint8)raw[2] == 0xBF)
        content = QString::fromUtf8(raw.constData() + 3, raw.size() - 3);
    else
        content = QString::fromUtf8(raw);
    if (content.isEmpty() || content.contains(QChar(0xFFFD)))
        content = QString::fromLocal8Bit(raw);

    QStringList lines = content.split('\n');
    for (int i = lines.size() - 1; i >= 0; i--) {
        lines[i] = lines[i].remove('\r').trimmed();
        if (lines[i].isEmpty()) lines.removeAt(i);
    }
    if (lines.isEmpty()) { emit finished(); return; }

    m_headers = lines[0].split(',');
    for (auto &h : m_headers) h = h.trimmed();

    emit progressChanged(0, QString("解析 0/%1 行").arg(lines.size() - 1));
    int total = lines.size() - 1;
    for (int i = 1; i <= total; i++) {
        if (QThread::currentThread()->isInterruptionRequested()) break;

        QStringList vals = lines[i].split(',');
        if (vals.size() == m_headers.size()) {
            for (int col = 0; col < m_headers.size(); col++) {
                bool ok;
                double v = vals[col].trimmed().toDouble(&ok);
                if (ok) m_plotData[m_headers[col]].append(v);
            }
        }
        if (i % 200 == 0)
            emit progressChanged(i * 100 / total, QString("解析 %1/%2 行").arg(i).arg(total));
    }
    emit progressChanged(100, "完成");
    emit finished();
}
