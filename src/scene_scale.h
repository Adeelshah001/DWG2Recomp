#pragma once
#include "render_scale.h"
#include <cstdint>
namespace dwg2 {
// GetHostViewportInfo produces integer bounds at the numerator scale and
// compensates clipping and half-pixel alignment in its NDC transform. Fold the
// denominator into both, preserving the same rasterized pixel positions.
template<class Viewport> inline void FoldSceneViewport(Viewport& viewport,RenderScale x,RenderScale y) {
 const RenderScale scales[]={x,y};
 for(unsigned axis=0;axis<2;++axis) {
  const uint32_t denominator=scales[axis].denominator;
  if(denominator==1)continue;
  const uint32_t offset=viewport.xy_offset[axis],extent=viewport.xy_extent[axis];
  const uint32_t left=offset/denominator;
  const uint32_t right=uint32_t((uint64_t(offset)+extent)/denominator);
  const uint32_t folded=right-left;
  viewport.xy_offset[axis]=left;viewport.xy_extent[axis]=folded;
  if(!folded){viewport.ndc_scale[axis]=1;viewport.ndc_offset[axis]=0;continue;}
  const double ratio=double(extent)/(double(denominator)*folded);
  viewport.ndc_scale[axis]=float(double(viewport.ndc_scale[axis])*ratio);
  viewport.ndc_offset[axis]=float((double(viewport.ndc_offset[axis])+1)*ratio-1+
    2.0*double(offset%denominator)/(double(denominator)*folded));
 }
}
}
