// Exact host scale arithmetic. Raster boundaries are shared between adjacent
// rectangles; resource extents round up. Backing storage is a separate contract.
#pragma once
#include <cstdint>
#include <limits>
#include <stdexcept>

namespace dwg2 {
struct RenderScale {
  uint32_t numerator=1,denominator=1;
  constexpr bool SupportedProfile() const {
    return (denominator==1&&numerator>=1&&numerator<=3)||(numerator==3&&denominator==2);
  }
  constexpr int32_t Boundary(int32_t guest) const {
    if(!SupportedProfile())throw std::invalid_argument("Unsupported render scale");
    int64_t value=int64_t(guest)*numerator;
    int64_t result=value/int64_t(denominator);
    if(value<0&&value%denominator)--result;
    if(result<std::numeric_limits<int32_t>::min()||result>std::numeric_limits<int32_t>::max())
      throw std::overflow_error("Scaled coordinate overflow");
    return int32_t(result);
  }
  constexpr uint32_t ResourceExtent(uint32_t guest) const {
    if(!SupportedProfile())throw std::invalid_argument("Unsupported render scale");
    uint64_t result=(uint64_t(guest)*numerator+denominator-1)/denominator;
    if(result>std::numeric_limits<uint32_t>::max())throw std::overflow_error("Scaled extent overflow");
    return uint32_t(result);
  }
  constexpr uint64_t AlignedTileBytes(uint32_t width,uint32_t height,uint32_t bytes_per_pixel) const {
    if(!SupportedProfile()||width%denominator||height%denominator||!bytes_per_pixel)
      throw std::invalid_argument("Tile not aligned to rational scale");
    const uint64_t pixels=uint64_t(ResourceExtent(width))*ResourceExtent(height);
    if(pixels>std::numeric_limits<uint64_t>::max()/bytes_per_pixel)
      throw std::overflow_error("Scaled tile allocation overflow");
    return pixels*bytes_per_pixel;
  }
};
}
