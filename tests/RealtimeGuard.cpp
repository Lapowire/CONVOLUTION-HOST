#include "RealtimeGuard.h"
#include <cstdlib>
#include <new>
#if defined(_WIN32)
#include <malloc.h>
#endif
void *operator new(std::size_t n)
{
    if (test::realtimeChecking)
        ++test::realtimeAllocations;
    if (void *p = std::malloc(n ? n : 1))
        return p;
    throw std::bad_alloc();
}
void *operator new[](std::size_t n)
{
    return ::operator new(n);
}
void operator delete(void *p) noexcept
{
    if (test::realtimeChecking && p)
        ++test::realtimeAllocations;
    std::free(p);
}
void operator delete[](void *p) noexcept
{
    ::operator delete(p);
}
void operator delete(void *p, std::size_t) noexcept
{
    ::operator delete(p);
}
void operator delete[](void *p, std::size_t) noexcept
{
    ::operator delete(p);
}
void *operator new(std::size_t n, std::align_val_t a)
{
    if (test::realtimeChecking)
        ++test::realtimeAllocations;
    void *p = nullptr;
#if defined(_WIN32)
    p = _aligned_malloc(n ? n : 1, std::size_t(a));
    if (p)
        return p;
#else
    if (posix_memalign(&p, std::size_t(a), n ? n : 1) == 0)
        return p;
#endif
    throw std::bad_alloc();
}
void operator delete(void *p, std::align_val_t) noexcept
{
#if defined(_WIN32)
    if (test::realtimeChecking && p)
        ++test::realtimeAllocations;
    _aligned_free(p);
#else
    ::operator delete(p);
#endif
}
void operator delete(void *p, std::size_t, std::align_val_t a) noexcept
{
    ::operator delete(p, a);
}
#if defined(CH_WRAP_ALLOCATIONS)
extern "C" void *__real_malloc(std::size_t);
extern "C" void *__real_calloc(std::size_t, std::size_t);
extern "C" void *__real_realloc(void *, std::size_t);
extern "C" void __real_free(void *);
extern "C" void *__wrap_malloc(std::size_t size)
{
    if (test::realtimeChecking)
        ++test::realtimeAllocations;
    return __real_malloc(size);
}
extern "C" void *__wrap_calloc(std::size_t count, std::size_t size)
{
    if (test::realtimeChecking)
        ++test::realtimeAllocations;
    return __real_calloc(count, size);
}
extern "C" void *__wrap_realloc(void *data, std::size_t size)
{
    if (test::realtimeChecking)
        ++test::realtimeAllocations;
    return __real_realloc(data, size);
}
extern "C" void __wrap_free(void *data)
{
    if (test::realtimeChecking && data)
        ++test::realtimeAllocations;
    __real_free(data);
}
#endif
