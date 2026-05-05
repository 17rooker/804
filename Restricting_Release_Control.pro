QT += core gui widgets serialport xml printsupport script

greaterThan(QT_MAJOR_VERSION, 4): QT += widgets

CONFIG += c++17

# The following define makes your compiler emit warnings if you use
# any Qt feature that has been marked deprecated (the exact warnings
# depend on your compiler). Please consult the documentation of the
# deprecated API in order to know how to port your code away from it.
DEFINES += QT_DEPRECATED_WARNINGS

# You can also make your code fail to compile if it uses deprecated APIs.
# In order to do so, uncomment the following line.
# You can also select to disable deprecated APIs only up to a certain version of Qt.
#DEFINES += QT_DISABLE_DEPRECATED_BEFORE=0x060000    # disables all the APIs deprecated before Qt 6.0.0
CODECFORSRC = UTF-8

BIN_DIR = $$PWD/bin
DESTDIR = $$BIN_DIR
include(./thirdParty/spdlog/spdlog.prf)
include(./src/CustomMessage/CustomMessage.pri)
include(./src/CustomEvent/CustomEvent.pri)
include(./src/ControllerCSV/csv_controller.pri)
SOURCES += \
    src/CommManager.cpp \
    src/Common/ConfigHelper.cpp \
    src/Common/LoggerManager.cpp \
    src/DataProcess/CommDataProcessor.cpp \
    src/DataProcess/ControlDataProcess.cpp \
    src/DataProcess/DataAnalysis/ExpandAnalysis.cpp \
    src/DataProcess/DataAnalysis/FrameDataAnalysis.cpp \
    src/DataProcess/DataAnalysis/IDataAnalysis.cpp \
    src/DataProcess/FrameDataBuilder.cpp \
    src/DataProcess/IDataProcess.cpp \
    src/DataProcess/MessageFrameConfig.cpp \
    src/DataProcess/ScheduledSendService.cpp \
    src/DataProcess/StaticDataProcess.cpp \
    src/LogicCommunication/CommChannelFactory.cpp \
    src/LogicCommunication/SerialCommunication.cpp \
    src/LogicCommunication/SerialInterface/SerialChannel.cpp \
    src/LogicCommunication/TCPCommunication.cpp \
    src/LogicCommunication/TCPInterface/TcpChannel.cpp \
    src/LogicCommunication/UDPCommunication.cpp \
    src/LogicCommunication/UDPInterface/UdpMulticastChannel.cpp \
    src/StyleEventFilter.cpp \
    src/chart/chartwidget.cpp \
    src/chart/qcustomplot.cpp \
    view/bookbindingwgt.cpp \
    view/collectioncoeffwidget.cpp \
    view/collectioncriteriawidge.cpp \
    view/controller422dialog.cpp \
    view/controllerpanel.cpp \
    view/copyframedialog.cpp \
    view/datacenter.cpp \
    view/dataplaybackdialo.cpp \
    view/doublespinwidget.cpp \
    view/emissiontab.cpp \
    view/framestatisticswidget.cpp \
    view/gassupplywidget.cpp \
    view/indicatorlamp.cpp \
    view/indicatorlight.cpp \
    view/launchframedialog.cpp \
    view/launchprocessdialog.cpp \
    view/ledindicator.cpp \
    main.cpp \
    mainwindow.cpp \
    view/powersettingwidge.cpp \
    view/programpowersupply.cpp \
    view/remotesettingwidget.cpp \
    view/serial422dialog.cpp \
    view/simulateddata.cpp \
    view/spinslider.cpp \
    view/styledledlabel.cpp \
    view/postanalysisdialog.cpp \
    view/styledlineedit.cpp \
    view/wavechart.cpp

HEADERS += \
    src/CommManager.h \
    src/Common/CommTypes.h \
    src/Common/ConfigHelper.h \
    src/Common/ConstDefine.h \
    src/Common/FormatDefine.h \
    src/Common/LoggerManager.h \
    src/Common/StructDefine.h \
    src/DataProcess/CommDataProcessor.h \
    src/DataProcess/ControlDataProcess.h \
    src/DataProcess/DataAnalysis/ExpandAnalysis.h \
    src/DataProcess/DataAnalysis/FrameDataAnalysis.h \
    src/DataProcess/DataAnalysis/IDataAnalysis.h \
    src/DataProcess/FrameDataBuilder.h \
    src/DataProcess/IDataProcess.h \
    src/DataProcess/MessageFrameConfig.h \
    src/DataProcess/ScheduledSendService.h \
    src/DataProcess/ScheduledTask.h \
    src/DataProcess/StaticDataProcess.h \
    src/LogicCommunication/CommChannelFactory.h \
    src/LogicCommunication/ICommChannel.h \
    src/LogicCommunication/SerialCommunication.h \
    src/LogicCommunication/SerialInterface/SerialChannel.h \
    src/LogicCommunication/TCPCommunication.h \
    src/LogicCommunication/TCPInterface/TcpChannel.h \
    src/LogicCommunication/UDPCommunication.h \
    src/LogicCommunication/UDPInterface/UdpMulticastChannel.h \
    src/StyleEventFilter.h \
    src/chart/chartwidget.h \
    src/chart/qcustomplot.h \
    view/bookbindingwgt.h \
    view/collectioncoeffwidget.h \
    view/collectioncriteriawidge.h \
    view/controller422dialog.h \
    view/controllerpanel.h \
    view/copyframedialog.h \
    view/datacenter.h \
    view/dataplaybackdialo.h \
    view/datastruct.h \
    view/doublespinwidget.h \
    view/emissiontab.h \
    view/framestatisticswidget.h \
    view/gassupplywidget.h \
    view/indicatorlamp.h \
    view/indicatorlight.h \
    view/launchframedialog.h \
    view/launchprocessdialog.h \
    view/ledindicator.h \
    mainwindow.h \
    view/powersettingwidge.h \
    view/programpowersupply.h \
    view/postanalysisdialog.h \
    view/remotesettingwidget.h \
    view/serial422dialog.h \
    view/simulateddata.h \
    view/spinslider.h \
    view/styledledlabel.h \
    view/styledlineedit.h \
#    view/testframewgt.h \
    view/topstatusbar.h \
    view/wavechart.h

FORMS += \
    mainwindow.ui \
    view/wavechart.ui
#    view/testframewgt.ui
INCLUDEPATH += \
    $$PWD/thirdParty/asio/include \
    $$PWD/thirdParty/concurrentqueue-master \
    $$PWD/thirdParty/BS_thread_pool/include     \
    $$PWD/thirdParty/fast-cpp-csv-parser-master     \
    $$PWD/thirdParty/nlohmann   \

DEFINES += \
    ASIO_STANDALONE \
    ASIO_NO_DEPRECATED  \
    # 定义为 header-only 模式
    SPDLOG_HEADER_ONLY  \

win32 {
    DEFINES += _WIN32_WINNT=0x0601 WIN32_LEAN_AND_MEAN NOMINMAX
    LIBS    += -lws2_32 -lmswsock -lwinpthread
}

unix {
    LIBS += -lpthread
    contains($$system(uname -m), aarch64): LIBS += -latomic
}

# Default rules for deployment.
qnx: target.path = /tmp/$${TARGET}/bin
else: unix:!android: target.path = /opt/$${TARGET}/bin
!isEmpty(target.path): INSTALLS += target

RESOURCES += \
    res.qrc
