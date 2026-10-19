#pragma once
#include <cstddef>
namespace test {
inline thread_local bool realtimeChecking=false;
inline thread_local std::size_t realtimeAllocations=0;
struct RealtimeGuard {
 RealtimeGuard(){realtimeAllocations=0;realtimeChecking=true;}
 ~RealtimeGuard(){realtimeChecking=false;}
};
}
