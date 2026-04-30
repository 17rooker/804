#ifndef COMMDATAPROCESSOR_H
#define COMMDATAPROCESSOR_H

#include "src/Common/CommTypes.h"

// moodycamel 无锁并发队列
#include "concurrentqueue.h"

// BS 线程池
#include <BS_thread_pool.hpp>
#include "src/DataProcess/DataAnalysis/IDataAnalysis.h"
#include "src/DataProcess/IDataProcess.h"
#include "src/Common/StructDefine.h"
#include <atomic>
#include <functional>
#include <memory>
#include <thread>
#include <chrono>



// ─────────────────────────────────────────────

/**
 * @brief CommDataProcessor  通信数据处理中枢
 *
 * 职责链：
 *   通道回调 → enqueue() → moodycamel::ConcurrentQueue
 *          → BS::thread_pool 消费者 → IDataParser::parse()
 *          → DataInteractionManager::post*()（Qt 自定义事件）
 *
 * 线程模型：
 *  - 生产者（N 个）：各通道 IO 线程，调用 enqueue()，无锁、无等待
 *  - 消费者（M 个）：BS::thread_pool 管理，数量 = std::thread::hardware_concurrency()
 *  - dispatch 线程：单独的轮询线程，从队列取帧并提交给线程池处理
 */
class CommDataProcessor {
public:

    enum class EAnalysisType
    {
        E_DataAnalysis,           // 通用解析
        E_ExpandAnalysis,         // 扩展解析
    };

    // 数据处理类别
    enum class EProcessCategory
    {
        E_StaticData,  // 静态数据
        E_ControlData, // 控制类数据
    };
    /**
     * @param threadCount  线程池大小，0 = 自动（CPU核心数）
     */
    explicit CommDataProcessor(size_t threadCount = 0);
    ~CommDataProcessor();

    void init();

    // ── 生命周期 ────────────────────────────────
    void start();
    void stop();

    // ── 解析器注册 ──────────────────────────────
    /**
     * @brief registerParser  注册数据解析器（线程安全，需在 start() 前调用）
     */
    void registerParser(EAnalysisType type,IDataAnalysisPtr parser);
    void registerProcessor(EProcessCategory type,IDataProcessPtr processor);

    // ── 数据入队（线程安全，无锁） ──────────────
    /**
     * @brief enqueue  将原始帧推入并发队列（供通道回调调用）
     *
     * 此函数是核心热路径，必须极低延迟：
     *  - moodycamel::ConcurrentQueue 无锁入队，O(1) 均摊
     *  - 不做任何解析、不持有任何锁
     */
    void enqueue(STPackage data);

    // ── 统计 ────────────────────────────────────
    size_t queueSize() const;
    size_t processedCount() const;
    size_t droppedCount() const;

private:
    void dispatchLoop();   // 轮询队列，提交任务给线程池
    void process(const STPackage& stPackage);
    void dataProcessing(const STParamInfo& stParam);

private:
    // 无锁并发队列（多生产者安全）
    moodycamel::ConcurrentQueue<STPackage>  m_queue;

    // BS 线程池（专门处理 CPU 密集解析）
    std::unique_ptr<BS::thread_pool<>>          m_threadPool;

    // 派发线程（单线程轮询，将帧提交给线程池）
    std::thread                               m_dispatchThread;
    std::atomic<bool>                         m_running { false };

    // 解析器列表（只读访问，start() 后不修改）
    std::map<EAnalysisType,IDataAnalysisPtr>               m_parsers;
    // 数据处理类
    std::map<EProcessCategory, IDataProcessPtr>             m_processors;

    // 统计
    std::atomic<size_t> m_processedCount { 0 };
    std::atomic<size_t> m_droppedCount   { 0 };

    // 线程池大小
    size_t m_threadCount;

    // 队列空时的自旋休眠（微秒），避免忙等
    static constexpr int kIdleSleepUs = 200;

    // 单次最大批处理数（减少轮询开销）
    static constexpr size_t kBatchSize = 64;


};

#endif // COMMDATAPROCESSOR_H
