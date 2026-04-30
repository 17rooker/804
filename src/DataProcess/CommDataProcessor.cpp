#include "CommDataProcessor.h"
#include "src/DataProcess/StaticDataProcess.h"
#include "src/DataProcess/ControlDataProcess.h"
#include <QDebug>
#include <algorithm>

CommDataProcessor::CommDataProcessor(size_t threadCount)
    : m_threadCount(threadCount == 0
                    ? std::max(2u, std::thread::hardware_concurrency())
                    : threadCount)
{
    this->init();
}

CommDataProcessor::~CommDataProcessor() {
    stop();
}

void CommDataProcessor::init()
{

}

void CommDataProcessor::start() {
    if (m_running) return;

    // 创建线程池（BS_thread_pool 构造即启动）
    m_threadPool = std::make_unique<BS::thread_pool<>>(m_threadCount);

    m_running = true;

    // 启动调度线程
    m_dispatchThread = std::thread([this] { dispatchLoop(); });

    qInfo() << "[CommDataProcessor] Started, threadPool size ="
            << m_threadCount;
}

void CommDataProcessor::stop() {
    if (!m_running) return;

    m_running = false;

    if (m_dispatchThread.joinable()) {
        m_dispatchThread.join();
    }

    // 等待线程池中的所有任务完成
    if (m_threadPool) {
        m_threadPool->wait();
        m_threadPool.reset();
    }

    qInfo() << "[CommDataProcessor] Stopped."
            << "processed=" << m_processedCount.load()
            << "dropped="   << m_droppedCount.load();
}

void CommDataProcessor::registerParser(EAnalysisType type,IDataAnalysisPtr parser) {
    if (parser) {
        m_parsers[type]=std::move(parser);
    }
}

void CommDataProcessor::registerProcessor(EProcessCategory type, IDataProcessPtr processor)
{
    if (processor) {
        m_processors[type]=std::move(processor);
    }
}

void CommDataProcessor::enqueue(STPackage data) {
    // 热路径：无锁入队，O(1)
    m_queue.enqueue(std::move(data));
}

size_t CommDataProcessor::queueSize() const {
    return m_queue.size_approx();
}

size_t CommDataProcessor::processedCount() const {
    return m_processedCount.load(std::memory_order_relaxed);
}

size_t CommDataProcessor::droppedCount() const {
    return m_droppedCount.load(std::memory_order_relaxed);
}

// ─────────────────────────────────────────────
//  调度循环（单线程，负责出队 + 提交线程池）
// ─────────────────────────────────────────────

void CommDataProcessor::dispatchLoop() {
    // 预分配批量缓冲区，避免反复 heap 分配
    std::vector<STPackage> batch;
    batch.reserve(kBatchSize);

    while (m_running) {
        // 批量出队（减少原子操作次数）
        STPackage item{};
        size_t dequeued = 0;

        while (dequeued < kBatchSize && m_queue.try_dequeue(item)) {
            batch.push_back(std::move(item));
            ++dequeued;
        }

        if (dequeued == 0) {
            // 队列为空，短暂休眠避免忙等（麒麟V10 scheduler 友好）
            std::this_thread::sleep_for(
                std::chrono::microseconds(kIdleSleepUs));
            continue;
        }

        // 将每帧提交给线程池异步处理
        for (auto& raw : batch) {
            // 按值捕获（move）避免悬空引用
            m_threadPool->detach_task([this, r = std::move(raw)]() mutable {
                process(std::move(r));
            });
        }

        batch.clear();
    }

    // 退出前排空队列（保证数据不丢失）
    STPackage item{};
    while (m_queue.try_dequeue(item)) {
        process(std::move(item));
    }
}

// ─────────────────────────────────────────────
//  处理单帧（在线程池线程执行）
// ─────────────────────────────────────────────

void CommDataProcessor::process(const STPackage& stPackage) {
    for (auto& [type, parser] : m_parsers)
    {
        if (!parser) continue;

        try {
            if (parser->canHandle(stPackage.channelType, stPackage.channelId))
            {
                STParamInfo stParaminfo{};
                parser->reciveData(stPackage, stParaminfo);

                // 解析成功（mapParams 非空）则上报并返回，否则继续尝试下一个 parser
                if (!stParaminfo.mapParams.isEmpty()) {
                    this->dataProcessing(stParaminfo);
                    ++m_processedCount;
                    return;
                }
            }

        } catch (const std::exception& e) {
            ++m_droppedCount;
            qCritical() << "[CommDataProcessor] parse() exception:"
                        << e.what()
                        << "channel:" << stPackage.unSourceID;
        }
    }

    ++m_droppedCount;
}
void CommDataProcessor::dataProcessing(const STParamInfo& stParam)
{
    STParamInfo stRecvParam =std::move(stParam);
    // switch (stRecvParam.eDataType) {
    // case EDataType::E_Static:

    // {
    //     m_processors[EProcessCategory::E_StaticData]->dataProcess(stRecvParam);
    //     break;
    // }

    // case EDataType::E_ControlRes:
    //     m_processors[EProcessCategory::E_ControlData]->dataProcess(stRecvParam);
    //     break;
    // default:
    //     break;
    // }
}

