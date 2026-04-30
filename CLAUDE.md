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

# The output binary goes to bin/Restricting_Release_Control.exe
# Config files are loaded from bin/config/Info.ini at runtime
```

- Requires Qt 5.12.9+ (Core, GUI, Widgets, SerialPort, XML, PrintSupport, Script modules)
- MinGW 64-bit on Windows; GCC on Linux / 银河麒麟 V10 (x86_64 + aarch64)
- C++17 required (`CONFIG += c++17`)

## Project Overview

**牵制释放终端软件** — A Qt desktop application for monitoring and controlling a rocket/missile launch restricting-and-release system. Communicates with multiple hardware controllers over TCP, UDP multicast, and serial ports simultaneously.

## Architecture

### Layer Stack

```
UI Layer (view/)                      — Qt Widgets, panels, dialogs
    ↓ STParamInfo (parsed data)
DataInteractionManager (CustomMessage/) — Singleton mediating UI ↔ communication
    ↓ IEvent (Qt custom events)
Data Processing (DataProcess/)        — Analysis, frame building, scheduled sends
    ↓ STPackage (raw frames)
CommManager (CommManager.h/.cpp)      — Facade over all channels + processor
    ↓
Communication Channels (LogicCommunication/) — TCP, UDP Multicast, Serial
```

### Threading Model

- **IO threads**: Each channel (TCP/UDP/Serial) has its own IO thread calling `CommDataProcessor::enqueue()` 
- **Lock-free queue**: `moodycamel::ConcurrentQueue` — multi-producer, single consumer dispatch
- **Thread pool**: `BS::thread_pool` (size = CPU core count) processes parsed frames
- **Qt main thread**: All UI updates happen via `QCoreApplication::postEvent` (custom `IEvent` objects)
- **ScheduledSendService**: Worker thread with `QTimer` instances for periodic frame sending
- **Worker threads** in `ControllerPanel`/`LaunchFrameDialog`/`LaunchProcessDialog`: Each has a dedicated `QThread` for frame parsing, using batched merge-and-emit to avoid flooding the UI thread.

### Key Components

- **`CommManager`** (src/CommManager.h): Singleton facade. Registers parsers/processors, adds channels, manages lifecycle. Channels are identified by string `channelId`.

- **`CommDataProcessor`** (src/DataProcess/CommDataProcessor.h): Central processing hub. Uses moodycamel ConcurrentQueue + BS::thread_pool. Iterates all registered parsers, calling each `canHandle()` then `reciveData()`. Accepts the first parser whose parse result (`mapParams`) is non-empty. Falls through to next parser if parsing fails.

- **`MessageFrameConfig`** (src/DataProcess/MessageFrameConfig.h): Singleton. Stores frame formats per channelId. **Supports multiple formats per channelId** via `QMap<QString, QList<STFrameFormat>>`. The key method `findFrameFormat(channelId, dataLength)` matches frames by **exact data length** (`fixedLength && totalBytes == dataLength`), enabling the same channel to handle different protocols (e.g. A5 at 428 bytes, A6 at 97 bytes on `serial_E`).

- **`FrameDataAnalysis`** (src/DataProcess/DataAnalysis/FrameDataAnalysis.h): The primary parser. Uses `MessageFrameConfig::findFrameFormat()` to select the correct format by received data length. Each format can have independent `bigEndian` setting. Field-level parsing uses the format's endianness.

- **`DataInteractionManager`** (src/CustomMessage/DataInteractionManager.h): Singleton that bridges communication layer to UI. Uses Qt custom events (`postEvent`) to deliver parsed data to main thread. UI subscribes via `MessageHandle`.

- **`ScheduledSendService`** (src/DataProcess/ScheduledSendService.h): Manages periodic data sending tasks in a dedicated worker thread (QTimer-based). Supports custom lambda frame builders or automatic frame construction via `FrameDataBuilder`.

- **`ICommChannel`** (src/LogicCommunication/ICommChannel.h): Interface with `start()`, `stop()`, `send()`, state callbacks. Implementations: TCP (with auto-reconnect), UDP multicast (with join/leave), Serial (Qt SerialPort).

- **UI layer** (view/): Main window with `ControllerPanel` (core panel), `EmissionTab` (3 controller tabs), `LaunchProcessDialog`, `LaunchFrameDialog`, data playback dialogs, and various indicator widgets.

### Data Replay (DataPlaybackDialog)

- **`DataReadWorker`** reads raw binary files in a worker thread, emits `rawDataReady(batchBuffer)` via `Qt::QueuedConnection`
- **`DataPlaybackDialog::onRawDataReady()`** manually parses frames from the buffer: searches for header `0xFDB18540`, reads 4-byte big-endian frame length, extracts complete frames
- Frames are routed by length: 428 → A5 (sent to all 3 targets), other → A6 (sent to LaunchProcess + LaunchFrameDialog only)
- Targets (`ControllerPanel`/`LaunchFrameDialog`/`LaunchProcessDialog`) receive data via `appendData()` → debounce timer → worker `processData()`
- Each worker's `processData()` has a **multi-frame extraction loop** with frame count limit (`MAX_FRAMES = 100`), **merges results from all processed frames**, and **emits once** with merged state. This prevents UI thread flooding.
- On stop: `m_playbackActive` flag discards lingering signals; `clearPlaybackCache()` clears caches and stops timers in all three targets.

### Configuration

- `bin/config/Info.ini` — All runtime config (IPs, ports, serial params, channel names)
- `bin/config/MessageFrame.json` — Message frame definitions (multiple formats per channelId supported)
- `ConfigHelper` singleton reads from INI, used throughout

### Third-Party Dependencies (header-only)

| Library | Path | Purpose |
|---------|------|---------|
| asio | thirdParty/asio/ | Standalone async networking |
| moodycamel::ConcurrentQueue | thirdParty/concurrentqueue-master/ | Lock-free MPSC queue |
| BS::thread_pool | thirdParty/BS_thread_pool/ | Thread pool for data processing |
| spdlog | thirdParty/spdlog/ | Logging (header-only mode) |
| nlohmann/json | thirdParty/nlohmann/ | JSON parsing |
| fast-cpp-csv-parser | thirdParty/fast-cpp-csv-parser-master/ | CSV reading |
| QCustomPlot | src/chart/qcustomplot.h | Charting widget |
