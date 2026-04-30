# =====================================================================
# csv_controller.pri — 将此文件 include 到你的 .pro 中即可使用
#
# 用法:  在 .pro 中添加 include($$PWD/csv_controller/csv_controller.pri)
#
# 依赖:  fast-cpp-csv-parser (header-only)
#         将 csv.h 放到本目录或通过 INCLUDEPATH 指定路径
# =====================================================================

CSV_CTRL_DIR = $$PWD

INCLUDEPATH += $$CSV_CTRL_DIR

HEADERS += \
    $$CSV_CTRL_DIR/CsvController.h

SOURCES += \
    $$CSV_CTRL_DIR/CsvController.cpp

# ---------- 平台特定配置 ----------

# Windows MinGW: 确保大文件支持
win32-g++ {
    DEFINES += _FILE_OFFSET_BITS=64
}

# 银河麒麟 V10 (Linux): 启用 large file support
linux {
    DEFINES += _LARGEFILE64_SOURCE _FILE_OFFSET_BITS=64
}

# fast-cpp-csv-parser 在 MinGW 下建议禁用内部线程以避免 pthread 链接问题
# 我们使用 Qt 自己的线程模型
mingw {
    DEFINES += CSV_IO_NO_THREAD
}
