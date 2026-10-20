#pragma once
#include "PreparedIr.h"
namespace ch
{
class IrDecoder
{
  public:
    static constexpr std::size_t budgetBytes = 512ull * 1024 * 1024;
    DecodeResult decode(const IrRequest &, const std::function<bool()>& cancelled = {}) const;
    static std::size_t estimateBytes(int samples, int routes, int blockSize);
};
} // namespace ch
