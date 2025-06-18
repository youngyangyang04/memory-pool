#include "../include/CentralCache.h"
#include "../include/PageCache.h"
#include <cassert>
#include <thread>
#include <chrono>

namespace Kama_memoryPool
{

const std::chrono::milliseconds CentralCache::DELAY_INTERVAL{1000};

// 每次从PageCache获取span大小（以页为单位）
static const size_t SPAN_PAGES = 8;

CentralCache::CentralCache()
{
    for (auto& ptr : centralFreeList_)
    {
        ptr.store(nullptr, std::memory_order_relaxed);
    }
    for (auto& lock : locks_)
    {
        lock.clear();
    }
    // 初始化延迟归还相关的成员变量
    for (auto& count : delayCounts_)
    {
        count.store(0, std::memory_order_relaxed);
    }
    for (auto& time : lastReturnTimes_)
    {
        time = std::chrono::steady_clock::now();
    }
    spanCount_.store(0, std::memory_order_relaxed);
}

void* CentralCache::fetchRange(size_t index)
{
    // 索引检查，当索引大于等于FREE_LIST_SIZE时，说明申请内存过大应直接向系统申请
    if (index >= FREE_LIST_SIZE) 
        return nullptr;

    // 自旋锁保护
    while (locks_[index].test_and_set(std::memory_order_acquire))
    {
        std::this_thread::yield(); // 添加线程让步，避免忙等待，避免过度消耗CPU
    }

    void* result = nullptr;
    try 
    {
        // 尝试从中心缓存获取内存块
        result = centralFreeList_[index].load(std::memory_order_relaxed);

        if (!result)
        {
            // 如果中心缓存为空，从页缓存获取新的内存块
            size_t size = (index + 1) * ALIGNMENT;
            result = fetchFromPageCache(size);

            if (!result)
            {
                locks_[index].clear(std::memory_order_release);
                return nullptr;
            }

            // 将获取的内存块切分成小块
            char* start = static_cast<char*>(result);

            // 计算实际分配的页数
            size_t numPages = (size <= SPAN_PAGES * PageCache::PAGE_SIZE) ? 
                             SPAN_PAGES : (size + PageCache::PAGE_SIZE - 1) / PageCache::PAGE_SIZE;
            // 使用实际页数计算块数
            size_t blockNum = (numPages * PageCache::PAGE_SIZE) / size;
            
            if (blockNum > 1) 
            {  // 确保至少有两个块才构建链表
                for (size_t i = 1; i < blockNum; ++i) 
                {
                    void* current = start + (i - 1) * size;
                    void* next = start + i * size;
                    *reinterpret_cast<void**>(current) = next;
                }
                *reinterpret_cast<void**>(start + (blockNum - 1) * size) = nullptr;
                
                // 保存result的下一个节点
                void* next = *reinterpret_cast<void**>(result);
                // 将result与链表断开
                *reinterpret_cast<void**>(result) = nullptr;
                // 更新中心缓存
                centralFreeList_[index].store(
                    next, 
                    std::memory_order_release
                );
                
                // 使用无锁方式记录span信息
                size_t trackerIndex = spanCount_++;
                if (trackerIndex < spanTrackers_.size())
                {
                    spanTrackers_[trackerIndex].spanAddr.store(start, std::memory_order_release);
                    spanTrackers_[trackerIndex].numPages.store(numPages, std::memory_order_release);
                    spanTrackers_[trackerIndex].blockCount.store(blockNum, std::memory_order_release); // 共分配了blockNum个内存块
                    spanTrackers_[trackerIndex].freeCount.store(blockNum - 1, std::memory_order_release); // 第一个块result已被分配出去，所以初始空闲块数为blockNum - 1
                }
            }
        } 
        else 
        {
            // 保存result的下一个节点
            void* next = *reinterpret_cast<void**>(result);
            // 将result与链表断开
            *reinterpret_cast<void**>(result) = nullptr;
            
            // 更新中心缓存
            centralFreeList_[index].store(next, std::memory_order_release);

             // 更新span的空闲计数
            SpanTracker* tracker = getSpanTracker(result);
            if (tracker)
            {
                // 减少一个空闲块
                tracker->freeCount.fetch_sub(1, std::memory_order_release);
            }
        }
    }
    catch (...) 
    {
        locks_[index].clear(std::memory_order_release);
        throw;
    }

    // 释放锁
    locks_[index].clear(std::memory_order_release);
    return result;
}

void CentralCache::returnRange(void* start, size_t size, size_t index)
{
    if (!start || index >= FREE_LIST_SIZE) 
        return;

    size_t blockSize = (index + 1) * ALIGNMENT;
    size_t blockCount = size / blockSize;    

    while (locks_[index].test_and_set(std::memory_order_acquire)) 
    {
        std::this_thread::yield();
    }

    try 
    {
        // 1. 将归还的链表连接到中心缓存
        void* end = start;
        size_t count = 1;
        while (*reinterpret_cast<void**>(end) != nullptr && count < blockCount) {
            end = *reinterpret_cast<void**>(end);
            count++;
        }
        void* current = centralFreeList_[index].load(std::memory_order_relaxed);
        *reinterpret_cast<void**>(end) = current; // 头插法（将原有链表接在归还链表后边）
        centralFreeList_[index].store(start, std::memory_order_release);
        
        // 2. 更新延迟计数
        size_t currentCount = delayCounts_[index].fetch_add(1, std::memory_order_relaxed) + 1;
        auto currentTime = std::chrono::steady_clock::now();
        
        // 3. 检查是否需要执行延迟归还
        if (shouldPerformDelayedReturn(index, currentCount, currentTime))
        {
            performDelayedReturn(index);
        }
    }
    catch (...) 
    {
        locks_[index].clear(std::memory_order_release);
        throw;
    }

    locks_[index].clear(std::memory_order_release);
}

// 检查是否需要执行延迟归还
bool CentralCache::shouldPerformDelayedReturn(size_t index, size_t currentCount, 
    std::chrono::steady_clock::time_point currentTime)
{
    // 基于计数和时间的双重检查
    if (currentCount >= MAX_DELAY_COUNT)
    {
        return true;
    }
    
    auto lastTime = lastReturnTimes_[index];
    return (currentTime - lastTime) >= DELAY_INTERVAL;
}

// 执行延迟归还
void CentralCache::performDelayedReturn(size_t index)
{
    // 重置延迟计数
    delayCounts_[index].store(0, std::memory_order_relaxed);
    // 更新最后归还时间
    lastReturnTimes_[index] = std::chrono::steady_clock::now();
    
    // 统计每个span的空闲块数
    std::unordered_map<SpanTracker*, size_t> spanFreeCounts;
    void* currentBlock = centralFreeList_[index].load(std::memory_order_relaxed);
    
    while (currentBlock)
    {
        SpanTracker* tracker = getSpanTracker(currentBlock);
        if (tracker)
        {
            spanFreeCounts[tracker]++;
        }
        currentBlock = *reinterpret_cast<void**>(currentBlock);
    }
    
    // 更新每个span的空闲计数并检查是否可以归还
    for (const auto& [tracker, newFreeBlocks] : spanFreeCounts)
    {
        updateSpanFreeCount(tracker, newFreeBlocks, index);
    }
}

void CentralCache::updateSpanFreeCount(SpanTracker* tracker, size_t newFreeBlocks, size_t index)
{
    size_t oldFreeCount = tracker->freeCount.load(std::memory_order_relaxed);
    size_t newFreeCount = oldFreeCount + newFreeBlocks;
    tracker->freeCount.store(newFreeCount, std::memory_order_release);
    
    // 如果所有块都空闲，归还span
    if (newFreeCount == tracker->blockCount.load(std::memory_order_relaxed))
    {
        void* spanAddr = tracker->spanAddr.load(std::memory_order_relaxed);
        size_t numPages = tracker->numPages.load(std::memory_order_relaxed);
        
        // 从自由链表中移除这些块
        void* head = centralFreeList_[index].load(std::memory_order_relaxed);
        void* newHead = nullptr;
        void* prev = nullptr;
        void* current = head;
        
        while (current)
        {
            void* next = *reinterpret_cast<void**>(current);
            if (current >= spanAddr && 
                current < static_cast<char*>(spanAddr) + numPages * PageCache::PAGE_SIZE)
            {
                if (prev)
                {
                    *reinterpret_cast<void**>(prev) = next;
                }
                else
                {
                    newHead = next;
                }
            }
            else
            {
                prev = current;
            }
            current = next;
        }
        
        centralFreeList_[index].store(newHead, std::memory_order_release);
        PageCache::getInstance().deallocateSpan(spanAddr, numPages);
    }
}

void* CentralCache::fetchFromPageCache(size_t size)
{   
    // 1. 计算实际需要的页数
    size_t numPages = (size + PageCache::PAGE_SIZE - 1) / PageCache::PAGE_SIZE;

    // 2. 根据大小决定分配策略
    if (size <= SPAN_PAGES * PageCache::PAGE_SIZE) 
    {
        // 小于等于32KB的请求，使用固定8页
        return PageCache::getInstance().allocateSpan(SPAN_PAGES);
    } 
    else 
    {
        // 大于32KB的请求，按实际需求分配
        return PageCache::getInstance().allocateSpan(numPages);
    }
}

SpanTracker* CentralCache::getSpanTracker(void* blockAddr)
{
    // 遍历spanTrackers_数组，找到blockAddr所属的span
    for (size_t i = 0; i < spanCount_.load(std::memory_order_relaxed); ++i)
    {
        void* spanAddr = spanTrackers_[i].spanAddr.load(std::memory_order_relaxed);
        size_t numPages = spanTrackers_[i].numPages.load(std::memory_order_relaxed);
        
        if (blockAddr >= spanAddr && 
            blockAddr < static_cast<char*>(spanAddr) + numPages * PageCache::PAGE_SIZE)
        {
            return &spanTrackers_[i];
        }
    }
    return nullptr;
}

} // namespace memoryPool