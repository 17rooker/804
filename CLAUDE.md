# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
# Build (debug)
qmake Restricting_Release_Control.pro
mingw32-make -j$(nproc)

# Build (release)
qmake Restricting_Release_Control.pro "CONFIG+=release"
mingw32-make -j$(nproc)

# Clean
mingw32-make clean

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

### UI Widget Mapping Pattern

All data display widgets (ControllerPanel, LaunchFrameDialog, LaunchProcessDialog, CopyFrameDialog) follow:
- `m_ledMap[QString]` = `StyledLedLabel*` for status indicators
- `m_valueMap[QString]` = `StyledLineEdit*` for value display
- `updateControllerFrameUI()` iterates received maps and updates matching widgets
- Unknown keys are silently ignored

**CopyFrameDialog (测试帧)**: Embedded as a tab in EmissionTab. Uses `setParam()` which calls `updateData()` immediately. Maps A5/A6 field IDs directly to LEDs and value fields.

### Data Replay

- `DataReadWorker` reads binary files, emits `rawDataReady(batchBuffer)`
- `DataPlaybackDialog::onRawDataReady()` parses frames (header `0xFDB18540` + big-endian length), routes by frame length
- `m_playbackActive` flag + `clearPlaybackCache()` slots prevent lingering signals after stop

### Configuration Files

- `bin/config/Info.ini` — IPs, ports, serial params, channel names
- `bin/config/MessageFrame.json` — Frame format definitions per channelId
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
