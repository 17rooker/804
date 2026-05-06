# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
# Build (debug) — 增量编译即可，不要 clean
qmake Restricting_Release_Control.pro
mingw32-make -j4

# Build (release)
qmake Restricting_Release_Control.pro "CONFIG+=release"
mingw32-make -j4

# Output: bin/Restricting_Release_Control.exe
# Config: bin/config/Info.ini loaded at runtime
```

- Requires Qt 5.12.9+ (Core, GUI, Widgets, SerialPort, XML, PrintSupport, Script)
- MinGW 64-bit (Windows) / GCC (Linux / 银河麒麟 V10 x86_64 + aarch64)
- C++17 required

## Project Overview

**牵制释放终端软件** — Qt desktop application for monitoring and controlling a rocket/missile launch restricting-and-release system. Communicates with multiple hardware controllers over TCP, UDP multicast, and serial ports simultaneously. Two protocol variants (A5 = 428 bytes, A6 = 97 bytes) can arrive on the same channel, distinguished by data length.

## Architecture

### Layer Stack

```
UI Layer (view/)                      — Widgets, panels, dialogs
    ↓ STParamInfo (parsed data)
DataInteractionManager (CustomMessage/) — Singleton mediating UI ↔ communication
    ↓ IEvent (Qt custom events)
Data Processing (DataProcess/)        — Analysis, frame building, scheduled sends
    ↓ STPackage (raw frames)
CommManager                           — Facade over all channels + processor
    ↓
Communication Channels (LogicCommunication/) — TCP, UDP Multicast, Serial
```

### Multi-Protocol Routing

Both A5 (428 bytes) and A6 (97 bytes) arrive on `serial_E` with identical frame header `0xFDB18540`. `MessageFrameConfig::findFrameFormat(channelId, dataLength)` selects the correct format by exact frame size. `CommDataProcessor::process()` iterates all parsers and accepts the first one whose `mapParams` is non-empty after parsing.

### Frame Data Flow (Three Parallel Workers)

```
ControllerPanel::appendData()          LaunchFrameDialog::appendData()      LaunchProcessDialog::appendData()
  → m_dataCache (QMutex)                 → m_dataCache                        → m_dataCache
  → m_updateTimer (10ms)                 → m_updateTimer                      → m_updateTimer
  → onTimerTimeout()                     → onTimerTimeout()                   → onTimerTimeout()
  → worker::processData(data)            → worker::processData(data)          → worker::processData(data)
```

Each worker's `processData()`:
1. Iterates accumulated buffer, extracts frames by header `0xFDB18540` + big-endian frame length
2. Calls `FrameDataAnalysis::parseData()` per frame → `paramProcess()` (or `paramProcess_A6()` for A6 frames)
3. Computes CRC16/XMODEM on bytes 4..total-10, compares with stored CRC at total-6..total-5
4. Saves result in `editValues["校验结果"]` = "校验正确" / "校验错误"
5. Emits `dataProcessed` every `EMIT_INTERVAL=5` frames (throttled), capped at `MAX_FRAMES=100`
6. Always emits the last processed frame after the loop

### Key Components

- **`CommManager`**: Singleton facade. Registers parsers/processors, manages channel lifecycle. Signals `channelStateChanged(channelId, state)` for connection monitoring. Supports runtime channel update via `updateTcpChannel()` / `updateUdpMulticastChannel()` / `updateSerialChannel()` (remove + add pattern).

- **`MessageFrameConfig`**: Singleton. Stores `QMap<QString, QList<STFrameFormat>>` — one channelId maps to multiple frame formats. `findFrameFormat(channelId, dataLength)` matches by `fixedLength && totalBytes == dataLength`.

- **`FrameDataAnalysis`**: Primary parser. Static method `crc16Xmodem(data, start, len)` provides CRC16/XMODEM verification (polynomial 0x1021, init 0x0000, table-based).

- **`RemoteSettingWidget`**: TCP/UDP parameters can be hot-applied. Saves to Info.ini via `ConfigHelper::setValue()`, then calls `CommManager::updateXxxChannel()`.

- **MainWindow**: CEC status LED reflects TCP remote connection (green/gray). 远控记录 QTextEdit logs connection/disconnection events with timestamps (transition-only, using `static prevState`). Right-click "清除记录" context menu.
- **Mechanism Ready**: `ControllerPanel::onDataProcessed()` checks sensor criteria: 拉力(3~60kN, any of 2) + 压力(0.6~1.3MPa, any of 2). Emits `mechanismReadyChanged(bool)` → MainWindow's `m_lblStatusLight` LED (green=ready, gray=not). Same `ChartWidget::updateSeries` also removed `rescaleAxes()` to preserve user zoom.
- **CSV Storage**: `ControllerPanel::onDataProcessed()` collects sensor data (拉力/角度/压力/温度/时序) into `CsvController`. File at `Storage/DataPath` from `Info.ini`, auto-named yyyyMMdd_HHmmss_controller.csv. Buffered: flushes every 50 rows or 5s timer, plus on destructor.
- **PostAnalysisDialog**: Dialog opened from MainWindow "事后分析" button. Contains ChartWidget for data analysis curves, zoom/pan tool buttons, and parameter display panel.
- **ProgramPowerSupply**: Dialog opened from MainWindow "程序电源" button. Shows 6 power channels with voltage/current setting, remote control status, power output control, and fault status.

### RemoteSettingWidget (运控设置)

- TCP/UDP parameters hot-configurable via "应用" buttons. Saves to `Info.ini`, calls `CommManager::updateXxxChannel()`.
- Subscribes to `E_RealTimeData`, `onMessage` parses TCP protocol (32-byte header + info words), displays frame type/count/source/destination/date/time.

### UI Widget Mapping Pattern

All data display widgets (ControllerPanel, LaunchFrameDialog, LaunchProcessDialog, CopyFrameDialog) follow:
- `m_ledMap[QString]` = `StyledLedLabel*` for status indicators
- `m_valueMap[QString]` = `StyledLineEdit*` for value display
- `updateControllerFrameUI()` iterates received maps and updates matching widgets
- Unknown keys are silently ignored

**CopyFrameDialog (测试帧)**: Embedded as a tab in EmissionTab. Uses `setParam()` which calls `updateData()` immediately. Maps A5/A6 field IDs directly to LEDs and value fields. Has its own `FrameCopyWorker` for A6 playback.

**LaunchProcessDialog**: Supports three-channel voting. `setParam()` stores data per channelId (`m_paramE/F/G`), `updateVoting()` polls all 3 channels and updates existing LEDs (green=all 1, dim=all 0, yellow=mixed). Mode displays consensus or "模式错误".

**wavechart**: 4 ChartWidget instances (机构1~4). `setParam` stores param, 50ms repeating timer triggers `plotAinData()` which extracts `AIN4_t1~AIN7_t12` (UInt8→double `*0.01952/0.51`). ChartWidget `updateSeries` no longer calls `rescaleAxes(true)` to preserve user zoom.

### Data Replay

- `DataReadWorker` reads binary files, emits `rawDataReady(batchBuffer)`
- `DataPlaybackDialog::onRawDataReady()` parses frames (header `0xFDB18540` + big-endian length), routes by frame length
- `m_playbackActive` flag + `clearPlaybackCache()` slots prevent lingering signals after stop
- All five target dialogs receive playback data: ControllerPanel, LaunchFrameDialog, LaunchProcessDialog, CopyFrameDialog, wavechart

### Serial Channel Architecture

- Three serial ports (serial_E/F/G) map to three EmissionTab instances (控制器1/2/3)
- `MessageFrameConfig.findFrameFormat(channelId, dataLength)` looks up the correct format per channel
- `EmissionTab::onMessage()` filters by `m_controllerName→channelId` mapping; A5 → LaunchFrameDialog, A6 → CopyFrameDialog+wavechart+LaunchFrameDialog
- ControllerPanel and LaunchProcessDialog only accept serial_E (shared singletons); LaunchFrameDialog accepts any serial_ (per-tab instance)

### Configuration Files

- `bin/config/Info.ini` — IPs, ports, serial params, channel names
- `bin/config/MessageFrame.json` — Frame format definitions per channelId (serial_E/F/G each with A5+A6)
- `bin/config/coeff_config.ini` — Collection coefficient formulas
- `bin/config/criteria_config.ini` — Param range criteria

### Third-Party Dependencies (header-only)

| Library | Path | Purpose |
|---------|------|---------|
| asio | thirdParty/asio/ | Async networking |
| moodycamel::ConcurrentQueue | thirdParty/concurrentqueue-master/ | Lock-free MPSC queue |
| BS::thread_pool | thirdParty/BS_thread_pool/ | Thread pool |
| spdlog | thirdParty/spdlog/ | Logging |
| nlohmann/json | thirdParty/nlohmann/ | JSON parsing |
| fast-cpp-csv-parser | thirdParty/fast-cpp-csv-parser-master/ | CSV reading |
| QCustomPlot | src/chart/qcustomplot.h | Charting |
