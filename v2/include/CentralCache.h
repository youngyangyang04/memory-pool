#pragma once
#include "Common.h"
#include <mutex>
#include <unordered_map>
#include <array>
#include <atomic>
#include <chrono>

namespace Kama_memoryPool
{

// 使用无锁的span信息存储
struct SpanTracker {
    std::atomic<void*> spanAddr{nullptr};
    std::atomic<size_t> numPages{0};
    std::atomic<size_t> blockCount{0};
    std::atomic<size_t> freeCount{0}; // 用于追踪spn中还有多少块是空闲的，如果所有块都空闲，则归还span给PageCache
};

class CentralCache
{
public:
    static CentralCache& getInstance()
    {
        static CentralCache instance;
        return instance;
    }

    void* fetchRange(size_t index);
    void returnRange(void* start, size_t size, size_t index);

private:
    // 相互是还所有原子指针为nullptr
    CentralCache();
    // 从页缓存获取内存
    void* fetchFromPageCache(size_t size);

    // 获取span信息
    SpanTracker* getSpanTracker(void* blockAddr);

    // 更新span的空闲计数并检查是否可以归还
    void updateSpanFreeCount(SpanTracker* tracker, size_t newFreeBlocks, size_t index);

private:
    // 中心缓存的自由链表
    std::array<std::atomic<void*>, FREE_LIST_SIZE> centralFreeList_;

    // 用于同步的自旋锁
    std::array<std::atomic_flag, FREE_LIST_SIZE> locks_;
    
    // 使用数组存储span信息，避免map的开销
    std::array<SpanTracker, 1024> spanTrackers_;
    std::atomic<size_t> spanCount_{0};

    // 延迟归还相关的成员变量
    static const size_t MAX_DELAY_COUNT = 48;  // 最大延迟计数
    std::array<std::atomic<size_t>, FREE_LIST_SIZE> delayCounts_;  // 每个大小类的延迟计数
    std::array<std::chrono::steady_clock::time_point, FREE_LIST_SIZE> lastReturnTimes_;  // 上次归还时间
    static const std::chrono::milliseconds DELAY_INTERVAL;  // 延迟间隔

    bool shouldPerformDelayedReturn(size_t index, size_t currentCount, std::chrono::steady_clock::time_point currentTime);
    void performDelayedReturn(size_t index);
};

} // namespace memoryPool