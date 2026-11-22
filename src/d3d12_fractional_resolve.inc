// First direct guest-command resolve path for a fully owned RGBA8 surface.
// Unqualified commands must never fall through to integer EDRAM addressing.
bool D3D12RenderTargetCache::Dwg2ResolveFractionalRGBA8(
 const draw_util::ResolveInfo& info,D3D12SharedMemory& shared,D3D12TextureCache& textures,
 uint32_t& written_address,uint32_t& written_length) {
 written_address=written_length=0;
 if(info.IsCopyingDepth())return Dwg2ResolveFractionalDepth24(info,shared,textures,written_address,written_length);
 if(dwg2_scene_denominator_!=2||GetPath()!=Path::kHostRenderTargets||
    info.IsCopyingDepth()||
    (info.color_edram_info.msaa_samples>xenos::MsaaSamples::k4X)||info.color_edram_info.format_is_64bpp||
    info.color_edram_info.format!=uint32_t(xenos::ColorRenderTargetFormat::k_8_8_8_8)||
    info.copy_dest_info.copy_dest_format!=xenos::ColorFormat::k_8_8_8_8||
    info.copy_dest_info.copy_dest_array||info.copy_dest_info.copy_dest_exp_bias||uint32_t(info.copy_dest_info.copy_dest_number)!=0||
    uint32_t(info.copy_dest_info.copy_dest_endian)>3||
    (info.color_edram_info.msaa_samples==xenos::MsaaSamples::k1X&&info.copy_dest_coordinate_info.copy_sample_select!=xenos::CopySampleSelect::k0)||
    info.copy_dest_coordinate_info.offset_x_div_8||info.copy_dest_coordinate_info.offset_y_div_8||
    info.copy_dest_base%4096||!info.copy_dest_extent_length)return false;
 const uint32_t width=info.coordinate_info.width_div_8*8,height=info.height_div_8*8;
 const uint32_t pitch=info.copy_dest_coordinate_info.pitch_aligned_div_32*32;
 if(!width||!height||width>8192||height>8192||pitch<width||pitch>16384)return false;
 uint32_t base,row_length,rows,source_pitch;info.GetCopyEdramTileSpan(base,row_length,rows,source_pitch);
 if(!rows||!row_length||!source_pitch||uint64_t(base)+(rows-1)*uint64_t(source_pitch)+row_length>xenos::kEdramTileCount)return false;
 std::vector<ResolveCopyDumpRectangle> owners;
 GetResolveCopyRectanglesToDump(base,row_length,rows,source_pitch,owners);
 if(owners.size()!=1)return false;
 const auto& owner=owners.front();
 if(!owner.render_target||owner.row_first||owner.rows!=rows||owner.row_first_start||owner.row_last_end!=row_length)return false;
 auto& target=*static_cast<D3D12RenderTarget*>(owner.render_target);const auto key=target.key();
 if(key.is_depth||key.msaa_samples!=info.color_edram_info.msaa_samples||
    key.GetColorFormat()!=xenos::ColorRenderTargetFormat::k_8_8_8_8||key.GetPitchTiles()!=source_pitch||
    info.color_edram_info.base_tiles<key.base_tiles)return false;
 const uint32_t offset=info.color_edram_info.base_tiles-key.base_tiles;
 if(key.msaa_samples!=xenos::MsaaSamples::k1X&&(offset||info.coordinate_info.edram_offset_x_div_8||info.coordinate_info.edram_offset_y_div_8))return false;
 const uint32_t x=(offset%source_pitch)*xenos::kEdramTileWidthSamples+info.coordinate_info.edram_offset_x_div_8*8;
 const uint32_t y=(offset/source_pitch)*xenos::kEdramTileHeightSamples+info.coordinate_info.edram_offset_y_div_8*8;
 const auto scale=Dwg2SceneScaleX();const auto desc=target.resource()->GetDesc();
 if(uint64_t(scale.Boundary(x))+scale.ResourceExtent(width)>desc.Width||
    uint64_t(scale.Boundary(y))+scale.ResourceExtent(height)>desc.Height)return false;
 const uint64_t length=uint64_t(pitch)*((height+31)/32*32)*4;
 if(uint64_t(info.copy_dest_base)+length>0x20000000ULL||!shared.RequestRange(info.copy_dest_base,uint32_t(length)))return false;
 // Preflight a separately owned depth clear before queuing any color copy.
 D3D12RenderTarget* depth_target=nullptr;D3D12_RECT depth_rectangle{};
 if(info.IsClearingDepth()) {
  const auto di=info.depth_edram_info;
  if(di.msaa_samples>xenos::MsaaSamples::k4X||(di.msaa_samples!=xenos::MsaaSamples::k1X&&di.format!=uint32_t(xenos::DepthRenderTargetFormat::kD24S8)))return false;
  uint32_t db,dl,dr;draw_util::GetResolveEdramTileSpan(di,info.coordinate_info,info.height_div_8,db,dl,dr);
  if(!di.pitch_tiles||!dr||uint64_t(db)+(dr-1)*uint64_t(di.pitch_tiles)+dl>xenos::kEdramTileCount)return false;
  std::vector<ResolveCopyDumpRectangle> depth_owners;GetResolveCopyRectanglesToDump(db,dl,dr,di.pitch_tiles,depth_owners);
  if(depth_owners.size()!=1)return false;const auto& owner=depth_owners[0];
  if(!owner.render_target||owner.row_first||owner.rows!=dr||owner.row_first_start||owner.row_last_end!=dl)return false;
  depth_target=static_cast<D3D12RenderTarget*>(owner.render_target);const auto dk=depth_target->key();
  if(!dk.is_depth||dk.msaa_samples!=di.msaa_samples||uint32_t(dk.GetDepthFormat())!=di.format||dk.GetPitchTiles()!=di.pitch_tiles||di.base_tiles<dk.base_tiles)return false;
  const uint32_t off=di.base_tiles-dk.base_tiles;
  if(di.msaa_samples!=xenos::MsaaSamples::k1X&&(off||info.coordinate_info.edram_offset_x_div_8||info.coordinate_info.edram_offset_y_div_8))return false;
  const uint32_t dx=off%di.pitch_tiles*(xenos::kEdramTileWidthSamples>>(di.msaa_samples>=xenos::MsaaSamples::k4X))+info.coordinate_info.edram_offset_x_div_8*8;
  const uint32_t dy=off/di.pitch_tiles*(xenos::kEdramTileHeightSamples>>(di.msaa_samples>=xenos::MsaaSamples::k2X))+info.coordinate_info.edram_offset_y_div_8*8;
  depth_rectangle={scale.Boundary(dx),scale.Boundary(dy),scale.Boundary(dx+width),scale.Boundary(dy+height)};
  const auto dd=depth_target->resource()->GetDesc();if(uint64_t(depth_rectangle.right)>dd.Width||uint32_t(depth_rectangle.bottom)>dd.Height)return false;
 }
 auto& cp=command_processor_;
 cp.PushTransitionBarrier(target.resource(),target.SetResourceState(D3D12_RESOURCE_STATE_COPY_SOURCE),D3D12_RESOURCE_STATE_COPY_SOURCE);
 InvalidateCommandListRenderTargets();cp.SubmitBarriers();
 const uint32_t fill=info.color_edram_info.fill_half_pixel_offset;
 auto retained=key.msaa_samples!=xenos::MsaaSamples::k1X?
   textures.Dwg2CaptureMSAARGBA8(target.resource(),D3D12_RESOURCE_STATE_COPY_SOURCE,width,height,pitch,0,0,uint32_t(info.copy_dest_coordinate_info.copy_sample_select),fill|(info.copy_dest_info.copy_dest_swap<<1),uint32_t(1)<<uint32_t(key.msaa_samples)):
   textures.Dwg2CaptureRetainedRGBA8(target.resource(),D3D12_RESOURCE_STATE_COPY_SOURCE,width,height,pitch,
   uint32_t(scale.Boundary(x)),uint32_t(scale.Boundary(y)),fill|(info.copy_dest_info.copy_dest_swap<<1));
 if(!retained)return false;
 shared.UseForWriting();
 const uint32_t endian=uint32_t(info.copy_dest_info.copy_dest_endian);
 if(!textures.Dwg2ReadbackRetainedRGBA8(retained.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,
    shared.GetBuffer(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,width,height,pitch,endian,0,info.copy_dest_base,fill))return false;
 shared.MarkUAVWritesCommitNeeded();
 if(!textures.Dwg2PublishRetainedRGBA8(info.copy_dest_base,width,height,pitch,endian,retained))return false;
 // The same target still owns every affected EDRAM tile. Clear only after
 // capturing its pre-clear pixels, without mutating the ownership map.
 if(info.IsClearingColor()) {
  float clear[4];for(uint32_t c=0;c<4;++c)clear[c]=float((info.rb_color_clear>>(c*8))&255)/255.f;
  D3D12_RECT rectangle{scale.Boundary(x),scale.Boundary(y),scale.Boundary(x+width),scale.Boundary(y+height)};
  cp.PushTransitionBarrier(target.resource(),target.SetResourceState(D3D12_RESOURCE_STATE_RENDER_TARGET),D3D12_RESOURCE_STATE_RENDER_TARGET);cp.SubmitBarriers();
  cp.GetDeferredCommandList().D3DClearRenderTargetView(target.descriptor_draw().GetHandle(),clear,1,&rectangle);
 }
 if(depth_target) {
  const uint32_t value=info.rb_depth_clear;const float depth=depth_target->key().GetDepthFormat()==xenos::DepthRenderTargetFormat::kD24S8?
    float(value>>8)*(1.f/float(0xFFFFFF)):xenos::Float20e4To32(value>>8)*0.5f;
  cp.PushTransitionBarrier(depth_target->resource(),depth_target->SetResourceState(D3D12_RESOURCE_STATE_DEPTH_WRITE),D3D12_RESOURCE_STATE_DEPTH_WRITE);cp.SubmitBarriers();
  cp.GetDeferredCommandList().D3DClearDepthStencilView(depth_target->descriptor_draw().GetHandle(),D3D12_CLEAR_FLAG_DEPTH|D3D12_CLEAR_FLAG_STENCIL,depth,UINT(value)&255,1,&depth_rectangle);
 }
 written_address=info.copy_dest_base;written_length=uint32_t(length);return true;
}

// Bounded D24S8 copy: retain packed depth/stencil, expose float depth to texture
// sampling, and publish a separate native tiled representation for guest reads.
bool D3D12RenderTargetCache::Dwg2ResolveFractionalDepth24(
 const draw_util::ResolveInfo& info,D3D12SharedMemory& shared,D3D12TextureCache& textures,
 uint32_t& written_address,uint32_t& written_length) {
 written_address=written_length=0;const auto di=info.depth_edram_info;
 if(dwg2_scene_denominator_!=2||GetPath()!=Path::kHostRenderTargets||!info.IsCopyingDepth()||
    di.msaa_samples>xenos::MsaaSamples::k4X||di.format!=uint32_t(xenos::DepthRenderTargetFormat::kD24S8)||
    uint32_t(info.copy_dest_info.copy_dest_format)!=uint32_t(xenos::TextureFormat::k_24_8)||
    info.copy_dest_info.copy_dest_array||info.copy_dest_info.copy_dest_exp_bias||uint32_t(info.copy_dest_info.copy_dest_number)||
    info.copy_dest_info.copy_dest_swap||uint32_t(info.copy_dest_info.copy_dest_endian)>3||info.copy_dest_base%4096||
    info.copy_dest_coordinate_info.offset_x_div_8||info.copy_dest_coordinate_info.offset_y_div_8||!info.copy_dest_extent_length)return false;
 const uint32_t width=info.coordinate_info.width_div_8*8,height=info.height_div_8*8,pitch=info.copy_dest_coordinate_info.pitch_aligned_div_32*32;
 if(!width||!height||width>8192||height>8192||pitch<width||pitch>16384||!di.pitch_tiles)return false;
 uint32_t base,row_length,rows,source_pitch;info.GetCopyEdramTileSpan(base,row_length,rows,source_pitch);
 if(!rows||!row_length||uint64_t(base)+(rows-1)*uint64_t(source_pitch)+row_length>xenos::kEdramTileCount)return false;
 std::vector<ResolveCopyDumpRectangle> owners;GetResolveCopyRectanglesToDump(base,row_length,rows,source_pitch,owners);if(owners.size()!=1)return false;const auto& owner=owners[0];
 if(!owner.render_target||owner.row_first||owner.rows!=rows||owner.row_first_start||owner.row_last_end!=row_length)return false;
 auto& target=*static_cast<D3D12RenderTarget*>(owner.render_target);const auto key=target.key();
 if(!key.is_depth||key.msaa_samples!=di.msaa_samples||key.GetDepthFormat()!=xenos::DepthRenderTargetFormat::kD24S8||key.GetPitchTiles()!=source_pitch||di.base_tiles<key.base_tiles)return false;
 const uint32_t off=di.base_tiles-key.base_tiles;
 // Nonzero MSAA tile/subrectangle origins remain rejected until separately qualified.
 if(di.msaa_samples!=xenos::MsaaSamples::k1X&&(off||info.coordinate_info.edram_offset_x_div_8||info.coordinate_info.edram_offset_y_div_8))return false;
 const uint32_t x=off%source_pitch*(xenos::kEdramTileWidthSamples>>(di.msaa_samples>=xenos::MsaaSamples::k4X))+info.coordinate_info.edram_offset_x_div_8*8,
 y=off/source_pitch*(xenos::kEdramTileHeightSamples>>(di.msaa_samples>=xenos::MsaaSamples::k2X))+info.coordinate_info.edram_offset_y_div_8*8;
 const auto scale=Dwg2SceneScaleX();const auto desc=target.resource()->GetDesc();D3D12_RECT rectangle{scale.Boundary(x),scale.Boundary(y),scale.Boundary(x+width),scale.Boundary(y+height)};
 if(uint64_t(rectangle.right)>desc.Width||uint32_t(rectangle.bottom)>desc.Height)return false;
 const uint64_t length=uint64_t(pitch)*((height+31)/32*32)*4;if(uint64_t(info.copy_dest_base)+length>0x20000000ULL||!shared.RequestRange(info.copy_dest_base,uint32_t(length)))return false;
 auto& cp=command_processor_;cp.PushTransitionBarrier(target.resource(),target.SetResourceState(D3D12_RESOURCE_STATE_COPY_SOURCE),D3D12_RESOURCE_STATE_COPY_SOURCE);InvalidateCommandListRenderTargets();cp.SubmitBarriers();
 const uint32_t half=di.fill_half_pixel_offset,endian=uint32_t(info.copy_dest_info.copy_dest_endian);
 auto retained=di.msaa_samples==xenos::MsaaSamples::k1X?
 textures.Dwg2CaptureDepth24(target.resource(),D3D12_RESOURCE_STATE_COPY_SOURCE,width,height,pitch,uint32_t(rectangle.left),uint32_t(rectangle.top),half):
 textures.Dwg2CaptureMSAADepth24(target.resource(),D3D12_RESOURCE_STATE_COPY_SOURCE,width,height,pitch,uint32_t(rectangle.left),uint32_t(rectangle.top),uint32_t(info.copy_dest_coordinate_info.copy_sample_select),half,1u<<uint32_t(di.msaa_samples));if(!retained)return false;
 shared.UseForWriting();if(!textures.Dwg2ReadbackRetainedRGBA8(retained.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,shared.GetBuffer(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,width,height,pitch,endian,0,info.copy_dest_base,half))return false;shared.MarkUAVWritesCommitNeeded();
 if(!textures.Dwg2PublishRetainedRGBA8(info.copy_dest_base,width,height,pitch,endian,retained,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,xenos::TextureFormat::k_24_8))return false;
 if(info.IsClearingDepth()){cp.PushTransitionBarrier(target.resource(),target.SetResourceState(D3D12_RESOURCE_STATE_DEPTH_WRITE),D3D12_RESOURCE_STATE_DEPTH_WRITE);cp.SubmitBarriers();cp.GetDeferredCommandList().D3DClearDepthStencilView(target.descriptor_draw().GetHandle(),D3D12_CLEAR_FLAG_DEPTH|D3D12_CLEAR_FLAG_STENCIL,float(info.rb_depth_clear>>8)*(1.f/float(0xFFFFFF)),info.rb_depth_clear&255,1,&rectangle);}
 written_address=info.copy_dest_base;written_length=uint32_t(length);return true;
}
