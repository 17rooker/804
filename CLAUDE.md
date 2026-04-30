# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build & Run

```bash
# Generate Makefile
qmake Restricting_Release_Control.pro

# Debug build
qmake Restricting_Release_Control.pro "CONFIG+=debug" && mingw32-make -j4

# Release build
qmake Restricting_Release_Control.pro "CONFIG+=release" && mingw32-make -j4

# Cross-platform: use `make -j$(nproc)` on Linux/Kylin
```

- **Qt 5.14.2**, toolchain: `D:\installPath\qt5.14\5.14.2\mingw73_64\bin\`
- **C++17**, qmake project
- Output binary goes to `bin/` directory
- Config file loaded at runtime from `config/Info.ini` (relative to executable)
- **No tests** (no test framework configured in the project)

## High-Level Architecture

This is a real-time launch control terminal application ("牵制释放终端软件") that communicates with devices via multiple protocols simultaneously.

### Communication Layer (`src/LogicCommunication/`)

- **`ICommChannel`** — abstract interface for all communication channels (TCP, UDP multicast, Serial)
- Each channel runs its own IO thread; channels are created via `CommChannelFactory`
- **`CommManager`** (Singleton, Facade) — manages all channels, routing, and processing lifecycle

### Data Processing Pipeline (`src/DataProcess/`)

```
Channel IO thread → CommDataProcessor::enqueue()
    → moodycamel::ConcurrentQueue (lock-free)
    → dispatchLoop thread → BS::thread_pool workers
    → IDataAnalysis::parseData() (e.g. FrameDataAnalysis, ExpandAnalysis)
    → IDataProcess::dataProcess() (e.g. StaticDataProcess, ControlDataProcess)
    → DataInteractionManager::post*Event() (Qt custom events → main thread)
```

- `CommDataProcessor` — central hub: lock-free concurrent queue + BS thread pool
- `IDataAnalysis` — data parser interface (frame parsing, expansion parsing)
- `IDataProcess` — post-parse processing interface (static data, control data)

### Data Interaction (`src/CustomMessage/`)

- `DataInteractionManager` (Singleton) — bridges processing threads to UI thread via Qt custom events
- `IMessage` — UI widgets implement this to receive data on the main thread
- `IEvent` — custom event types for cross-thread data delivery

### UI Layer (`view/` + `mainwindow.cpp`)

- `MainWindow` — top-level window with black title bar, button bar, and central split layout (data panel 75% / log 25%)
- `ControllerPanel` — main data display widget, uses `QThread` + `FrameWorker` for offloading parameter range checking
- `EmissionTab` — per-controller dialog (3 instances: 控制器1/2/3), contains tabs for various views
- Various dialogs: `LaunchProcessDialog`, `Controller422Dialog`, `Serial422Dialog`, `SimulatedData`, `LaunchFrameDialog`, etc.

### Common (`src/Common/`)

- `ConfigHelper` (Singleton) — wraps `QSettings` for `Info.ini` key-value config
- `LoggerManager` (Singleton) — wraps `spdlog` (async, multi-channel, daily/rotating files, Qt signal for UI output)
- `StructDefine.h` — shared data types: `STPackage` (raw frame), `STParamInfo` (parsed data), `STControlMsg`, etc.

### Third-Party (header-only except asio)

| Library | Path | Usage |
|---------|------|-------|
| asio (standalone) | `thirdParty/asio/` | TCP/UDP networking (with Qt serialport for serial) |
| spdlog | `thirdParty/spdlog/` | Async logging (header-only mode) |
| moodycamel/concurrentqueue | `thirdParty/concurrentqueue-master/` | Lock-free queue in CommDataProcessor |
| BS_thread_pool | `thirdParty/BS_thread_pool/` | Thread pool for data parsing |
| nlohmann/json | `thirdParty/nlohmann/` | JSON handling |
| fast-cpp-csv-parser | `thirdParty/fast-cpp-csv-parser-master/` | CSV parsing |
| QCustomPlot | `src/chart/qcustomplot.*` | Charting widget |

### Key Design Patterns

- **Facade**: `CommManager` wraps factory, channels, and processor
- **Strategy/Pipeline**: Pluggable parsers (`IDataAnalysis`) and processors (`IDataProcess`) registered at startup
- **Singleton**: `CommManager`, `DataInteractionManager`, `ConfigHelper`, `LoggerManager`
- **Observer**: `DataInteractionManager` + `IMessage` interface for UI data subscription
- **Thread pool + lock-free queue**: Backbone of the data processing pipeline
- **Cross-thread Qt custom events**: Processing threads → UI thread via `QCoreApplication::postEvent`
