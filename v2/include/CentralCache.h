#pragma once
#include "Common.h"
#include <mutex>

namespace Kama_memoryPool
{

class CentralCache
{
public:
    static CentralCache& getInstance()
    {
        static CentralCache instance;
        return instance;
    }

    void* fetchRange(size_t index);
    void returnRange(void* start, size_t size, size_t bytes);

private:
    // 相互是还所有原子指针为nullptr
    CentralCache()
    {
        for (auto& ptr : centralFreeList_)
        {
            ptr.store(nullptr, std::memory_order_relaxed);
        }
        // 初始化所有锁
        for (auto& lock : locks_)
        {
            lock.clear();
        }
    }
    // 从页缓存获取内存
    void* fetchFromPageCache(size_t size);

private:
    // 中心缓存的自由链表
    std::array<std::atomic<void*>, FREE_LIST_SIZE> centralFreeList_;

    // 用于同步的自旋锁
    std::array<std::atomic_flag, FREE_LIST_SIZE> locks_;
};

} // namespace memoryPool