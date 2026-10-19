#pragma once
#include <atomic>
#include <cstddef>
#include <thread>
namespace test
{
inline std::atomic<std::size_t> failWorkerAllocationSize{0};
inline std::thread::id allocationMainThread;
inline thread_local bool realtimeChecking = false;
inline thread_local std::size_t realtimeAllocations = 0;
struct RealtimeGuard
{
    RealtimeGuard()
    {
        realtimeAllocations = 0;
        realtimeChecking = true;
    }
    ~RealtimeGuard() { realtimeChecking = false; }
};
} // namespace test
