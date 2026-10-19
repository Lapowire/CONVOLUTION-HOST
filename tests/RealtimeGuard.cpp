#include "RealtimeGuard.h"
#include <cstdlib>
#include <new>
void* operator new(std::size_t n) {if(test::realtimeChecking)++test::realtimeAllocations;if(void* p=std::malloc(n?n:1))return p;throw std::bad_alloc();}
void* operator new[](std::size_t n){return ::operator new(n);}
void operator delete(void* p) noexcept {if(test::realtimeChecking && p)++test::realtimeAllocations;std::free(p);}
void operator delete[](void* p) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t) noexcept {::operator delete(p);}
void operator delete[](void* p,std::size_t) noexcept {::operator delete(p);}
void* operator new(std::size_t n,std::align_val_t a){if(test::realtimeChecking)++test::realtimeAllocations;void* p=nullptr;if(posix_memalign(&p,std::size_t(a),n?n:1)==0)return p;throw std::bad_alloc();}
void operator delete(void* p,std::align_val_t) noexcept {::operator delete(p);}
void operator delete(void* p,std::size_t,std::align_val_t) noexcept {::operator delete(p);}
