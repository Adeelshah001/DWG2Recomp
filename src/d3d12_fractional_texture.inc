// Bounded RGBA8 scene/texture paths, also used by the qualified direct resolve
// route. Other formats, partial aliases and full scene integration remain open.
void D3D12TextureCache::Dwg2RetainedWatch(
 const std::unique_lock<std::recursive_mutex>&,void* context,uint32_t first,uint32_t last,bool) {
 static_cast<D3D12TextureCache*>(context)->Dwg2InvalidateRetained(first,last);
}

void D3D12TextureCache::Dwg2InvalidateRetained(uint32_t first,uint32_t last) {
 auto lock=dwg2_retained_lock_.Acquire();
 for(auto& surface:dwg2_retained_surfaces_)
  if(uint64_t(first)<uint64_t(surface.address)+surface.length&&last>=surface.address)surface.valid=false;
}

void D3D12TextureCache::Dwg2PruneRetained() {
 auto lock=dwg2_retained_lock_.Acquire();const auto completed=command_processor_.GetCompletedSubmission();
 auto end=std::remove_if(dwg2_retained_surfaces_.begin(),dwg2_retained_surfaces_.end(),
   [&](const auto& surface){return !surface.valid&&surface.last_use<=completed;});
 dwg2_retained_surfaces_.erase(end,dwg2_retained_surfaces_.end());
}

// Called only after the corresponding native guest write has been queued.
// Publish under the same global critical region used by CPU/GPU memory watches,
// so a concurrent write cannot be missed between invalidation and publication.
bool D3D12TextureCache::Dwg2PublishRetainedRGBA8(uint32_t address,uint32_t width,
 uint32_t height,uint32_t pitch,uint32_t endian,Microsoft::WRL::ComPtr<ID3D12Resource> retained,
 D3D12_RESOURCE_STATES retained_state,xenos::TextureFormat format) {
 if((format!=xenos::TextureFormat::k_8_8_8_8&&format!=xenos::TextureFormat::k_24_8)||dwg2_scene_denominator_!=2||!retained||address%4096||!width||!height||width>8192||height>8192||
    pitch<width||pitch>16384||pitch%32||endian>3)return false;
 const uint64_t length=uint64_t(pitch)*((height+31)/32*32)*4;
 const uint64_t packed=uint64_t(pitch/32)*((height+31)/32)*48*48*4;
 if(uint64_t(address)+length>0x20000000ULL||retained->GetDesc().Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||
    retained->GetDesc().Width<packed)return false;
 auto lock=dwg2_retained_lock_.Acquire();MarkRangeAsResolved(address,uint32_t(length));Dwg2PruneRetained();
 auto existing=dwg2_retained_surfaces_.end();uint64_t used=0;
 for(auto it=dwg2_retained_surfaces_.begin();it!=dwg2_retained_surfaces_.end();++it) {
  used+=it->resource->GetDesc().Width;
  if(it->resource.Get()==retained.Get()&&!it->valid&&(it->address==address||!it->length))existing=it;
 }
 if(existing==dwg2_retained_surfaces_.end())used+=retained->GetDesc().Width;
 if((existing==dwg2_retained_surfaces_.end()&&dwg2_retained_surfaces_.size()>=64)||used>256ULL*1024*1024)return false;
 command_processor_.PushTransitionBarrier(retained.Get(),retained_state,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 command_processor_.SubmitBarriers();
 // RangeWrittenByGpu above invalidates overlapping old retained owners through
 // the registered global watch. Source resources survive until their last GPU
 // use completes, even if a CPU write invalidates them immediately afterward.
 Dwg2RetainedSurface surface{address,uint32_t(length),width,height,pitch,endian,true,
   command_processor_.GetCurrentSubmission(),std::move(retained),uint32_t(format)};
 if(existing!=dwg2_retained_surfaces_.end())*existing=std::move(surface);
 else dwg2_retained_surfaces_.push_back(std::move(surface));
 return true;
}

bool D3D12TextureCache::Dwg2LoadPublishedRGBA8(D3D12Texture& texture) {
 const auto key=texture.key();
 if(!key.scaled_resolve||!key.tiled||key.signed_separate||key.mip_max_level||
    (key.format!=xenos::TextureFormat::k_8_8_8_8&&key.format!=xenos::TextureFormat::k_24_8)||key.dimension!=xenos::DataDimension::k2DOrStacked||
    key.GetDepthOrArraySize()!=1)return false;
 auto lock=dwg2_retained_lock_.Acquire();
 for(auto& surface:dwg2_retained_surfaces_) {
  if(surface.valid&&surface.format==uint32_t(key.format)&&surface.address==(key.base_page<<12)&&surface.width==key.GetWidth()&&
     surface.height==key.GetHeight()&&surface.pitch==texture.guest_layout().base.row_pitch_bytes/4&&
     surface.endian==uint32_t(key.endianness)) {
   surface.last_use=command_processor_.GetCurrentSubmission();
   return Dwg2LoadRetainedRGBA8(texture,surface.resource.Get(),D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
  }
 }
 return false;
}

bool D3D12TextureCache::Dwg2PrepareRetainedPipeline(uint32_t operation) {
 if(operation>4)return false;
 const auto& provider=command_processor_.GetD3D12Provider();
 if(!dwg2_fractional_root_) {
  D3D12_ROOT_PARAMETER params[3]{};
  params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;params[0].Constants={0,0,8};
  params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;params[1].Descriptor={0,0};
  params[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;params[2].Descriptor={0,0};
  D3D12_ROOT_SIGNATURE_DESC desc{3,params,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
  dwg2_fractional_root_.Attach(ui::d3d12::util::CreateRootSignature(provider,desc));
  if(!dwg2_fractional_root_)return false;
 }
 auto& pipeline=operation==4?dwg2_depth_load_:(operation==3?dwg2_depth_pack_:operation==2?dwg2_fractional_readback_:(operation==1?dwg2_fractional_pack_:dwg2_fractional_load_));
 if(!pipeline) {
  const auto* code=operation==4?dwg2::shaders::fractional_depth_load:(operation==3?dwg2::shaders::fractional_depth_pack:operation==2?dwg2::shaders::fractional_texture_readback:(operation==1?dwg2::shaders::fractional_texture_pack:dwg2::shaders::fractional_texture_load));
  const auto size=operation==4?sizeof(dwg2::shaders::fractional_depth_load):(operation==3?sizeof(dwg2::shaders::fractional_depth_pack):operation==2?sizeof(dwg2::shaders::fractional_texture_readback):(operation==1?sizeof(dwg2::shaders::fractional_texture_pack):sizeof(dwg2::shaders::fractional_texture_load)));
  pipeline.Attach(ui::d3d12::util::CreateComputePipeline(provider.GetDevice(),code,size,dwg2_fractional_root_.Get()));
 }
 return pipeline!=nullptr;
}

// Copy an actual 3/2 scene crop into retained tiles. The output is in the SRV
// state and must stay alive until the containing GPU submission has completed.
// This does not yet assign ownership to any guest memory or permit alias reuse.
Microsoft::WRL::ComPtr<ID3D12Resource> D3D12TextureCache::Dwg2CaptureRetainedRGBA8(
 ID3D12Resource* source,D3D12_RESOURCE_STATES source_state,uint32_t guest_width,
 uint32_t guest_height,uint32_t guest_pitch,uint32_t source_left,uint32_t source_top,uint32_t capture_flags) {
 if(dwg2_scene_denominator_!=2||!source||!guest_width||!guest_height||guest_width>8192||
    guest_height>8192||guest_pitch<guest_width||guest_pitch>16384||guest_pitch%32||capture_flags>3)return {};
 const dwg2::RenderScale scale{3,2};const uint32_t width=scale.ResourceExtent(guest_width),height=scale.ResourceExtent(guest_height);
 const auto desc=source->GetDesc();
 if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||desc.SampleDesc.Count!=1||desc.DepthOrArraySize!=1||desc.MipLevels!=1||
    (desc.Format!=DXGI_FORMAT_R8G8B8A8_TYPELESS&&desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM&&desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM_SRGB)||
    uint64_t(source_left)+width>desc.Width||uint64_t(source_top)+height>desc.Height)return {};
 const uint32_t tiles_x=guest_pitch/32,tiles_y=(guest_height+31)/32,row_pitch=(width*4+255)&~255u;
 const uint64_t tile_count=uint64_t(tiles_x)*tiles_y,packed_size=tile_count*48*48*4;
 if(tile_count>65535||packed_size>UINT32_MAX||uint64_t(row_pitch)*height>UINT32_MAX)return {};
 auto ownership_lock=dwg2_retained_lock_.Acquire();Dwg2PruneRetained();
 uint64_t used=packed_size;for(const auto& surface:dwg2_retained_surfaces_)used+=surface.resource->GetDesc().Width;
 if(dwg2_retained_surfaces_.size()>=64||used>256ULL*1024*1024)return {};
 if(!Dwg2PrepareRetainedPipeline(true))return {};
 auto& cp=command_processor_;auto* device=cp.GetD3D12Provider().GetDevice();
 Microsoft::WRL::ComPtr<ID3D12Resource> packed;
 D3D12_RESOURCE_DESC buffer_desc{};ui::d3d12::util::FillBufferResourceDesc(buffer_desc,packed_size,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
 if(FAILED(device->CreateCommittedResource(&ui::d3d12::util::kHeapPropertiesDefault,D3D12_HEAP_FLAG_NONE,
    &buffer_desc,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&packed))))return {};
 auto* linear=cp.RequestScratchGPUBuffer(row_pitch*height,D3D12_RESOURCE_STATE_COPY_DEST);
 if(!linear)return {};
 // Retain an unpublished capture even if a later resolve step fails. It may
 // already be referenced by queued commands when the caller returns failure.
 dwg2_retained_surfaces_.push_back({0,0,0,0,0,0,false,cp.GetCurrentSubmission(),packed});
 auto& list=cp.GetDeferredCommandList();
 cp.PushTransitionBarrier(source,source_state,D3D12_RESOURCE_STATE_COPY_SOURCE);cp.SubmitBarriers();
 D3D12_TEXTURE_COPY_LOCATION src{},dst{};src.pResource=source;src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
 dst.pResource=linear;dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint.Footprint={DXGI_FORMAT_R8G8B8A8_UNORM,width,height,1,row_pitch};
 const D3D12_BOX box{source_left,source_top,0,source_left+width,source_top+height,1};
 list.D3DCopyTextureRegion(&dst,0,0,0,&src,&box);
 cp.PushTransitionBarrier(source,D3D12_RESOURCE_STATE_COPY_SOURCE,source_state);
 cp.PushTransitionBarrier(linear,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);cp.SubmitBarriers();
 const uint32_t constants[8]={guest_width,guest_height,width,height,4,tiles_x,guest_pitch,
   row_pitch|((capture_flags&1u)<<31u)|((capture_flags&2u)<<29u)};
 cp.SetExternalPipeline(dwg2_fractional_pack_.Get());list.D3DSetComputeRootSignature(dwg2_fractional_root_.Get());
 list.D3DSetComputeRoot32BitConstants(0,8,constants,0);list.D3DSetComputeRootShaderResourceView(1,linear->GetGPUVirtualAddress());
 list.D3DSetComputeRootUnorderedAccessView(2,packed->GetGPUVirtualAddress());list.D3DDispatch(6,6,uint32_t(tile_count));
 cp.PushTransitionBarrier(packed.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);cp.SubmitBarriers();
 cp.ReleaseScratchGPUBuffer(linear,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);return packed;
}

// Preserve retained detail while separately producing the native tiled guest
// representation. Only valid texels are written; unrelated destination bytes
// and padding remain owned by their existing producers.
bool D3D12TextureCache::Dwg2ReadbackRetainedRGBA8(ID3D12Resource* retained,
 D3D12_RESOURCE_STATES retained_state,ID3D12Resource* destination,D3D12_RESOURCE_STATES destination_state,
 uint32_t guest_width,uint32_t guest_height,uint32_t guest_pitch,uint32_t endian,
 uint64_t retained_offset,uint64_t destination_offset,uint32_t half_pixel_offset) {
 if(dwg2_scene_denominator_!=2||!retained||!destination||retained==destination||!guest_width||!guest_height||
    guest_width>8192||guest_height>8192||guest_pitch<guest_width||guest_pitch>16384||guest_pitch%32||endian>3||
    retained_offset%4||destination_offset%4||half_pixel_offset>1)return false;
 const uint32_t tiles_x=guest_pitch/32,tiles_y=(guest_height+31)/32;
 const uint64_t needed_retained=uint64_t(tiles_x)*tiles_y*48*48*4,needed_native=uint64_t(tiles_x)*tiles_y*32*32*4;
 const auto src=retained->GetDesc(),dst=destination->GetDesc();
 if(src.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||dst.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||
    !(dst.Flags&D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS)||retained_offset>src.Width||destination_offset>dst.Width||
    needed_retained>src.Width-retained_offset||needed_native>dst.Width-destination_offset)return false;
 if(!Dwg2PrepareRetainedPipeline(2))return false;
 auto& cp=command_processor_;auto& list=cp.GetDeferredCommandList();
 cp.PushTransitionBarrier(retained,retained_state,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);
 cp.PushTransitionBarrier(destination,destination_state,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);cp.SubmitBarriers();
 const uint32_t constants[8]={guest_width,guest_height,(guest_width*3+1)/2,(guest_height*3+1)/2,4,tiles_x,guest_pitch,endian|(half_pixel_offset<<2u)};
 cp.SetExternalPipeline(dwg2_fractional_readback_.Get());list.D3DSetComputeRootSignature(dwg2_fractional_root_.Get());
 list.D3DSetComputeRoot32BitConstants(0,8,constants,0);list.D3DSetComputeRootShaderResourceView(1,retained->GetGPUVirtualAddress()+retained_offset);
 list.D3DSetComputeRootUnorderedAccessView(2,destination->GetGPUVirtualAddress()+destination_offset);
 list.D3DDispatch((guest_width+7)/8,(guest_height+7)/8,1);
 cp.PushTransitionBarrier(retained,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,retained_state);
 if(destination_state==D3D12_RESOURCE_STATE_UNORDERED_ACCESS)cp.PushUAVBarrier(destination);
 else cp.PushTransitionBarrier(destination,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,destination_state);
 cp.SubmitBarriers();return true;
}

bool D3D12TextureCache::Dwg2LoadRetainedRGBA8(D3D12Texture& texture,ID3D12Resource* retained,
    D3D12_RESOURCE_STATES retained_state,uint64_t retained_offset) {
 const auto key=texture.key();
 if(dwg2_scene_denominator_!=2||!key.scaled_resolve||(key.format!=xenos::TextureFormat::k_8_8_8_8&&key.format!=xenos::TextureFormat::k_24_8)||
    key.dimension!=xenos::DataDimension::k2DOrStacked||key.GetDepthOrArraySize()!=1||key.mip_max_level||!retained)return false;
 const dwg2::RenderScale scale{3,2};const uint32_t width=scale.ResourceExtent(key.GetWidth()),height=scale.ResourceExtent(key.GetHeight());
 const uint32_t guest_pitch=texture.guest_layout().base.row_pitch_bytes/4;
 if(guest_pitch<key.GetWidth()||guest_pitch%32)return false;
 const uint32_t tiles_x=guest_pitch/32,tiles_y=(key.GetHeight()+31)/32;
 const auto source_desc=retained->GetDesc();const uint64_t needed=uint64_t(tiles_x)*tiles_y*48*48*4;
 if(source_desc.Dimension!=D3D12_RESOURCE_DIMENSION_BUFFER||retained_offset%4||retained_offset>source_desc.Width||needed>source_desc.Width-retained_offset)return false;
 const uint32_t row_pitch=(width*4+255)&~255u;
 if(uint64_t(row_pitch)*height>UINT32_MAX)return false;
 auto& cp=command_processor_;
 if(!Dwg2PrepareRetainedPipeline(key.format==xenos::TextureFormat::k_24_8?4:0))return false;
 auto* buffer=cp.RequestScratchGPUBuffer(row_pitch*height,D3D12_RESOURCE_STATE_UNORDERED_ACCESS);
 if(!buffer)return false;
 cp.PushTransitionBarrier(retained,retained_state,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);cp.SubmitBarriers();
 auto& list=cp.GetDeferredCommandList();uint32_t constants[8]={key.GetWidth(),key.GetHeight(),width,height,4,tiles_x,guest_pitch,row_pitch};
 list.D3DSetComputeRootSignature(dwg2_fractional_root_.Get());list.D3DSetComputeRoot32BitConstants(0,8,constants,0);
 list.D3DSetComputeRootShaderResourceView(1,retained->GetGPUVirtualAddress()+retained_offset);
 list.D3DSetComputeRootUnorderedAccessView(2,buffer->GetGPUVirtualAddress());cp.SetExternalPipeline(key.format==xenos::TextureFormat::k_24_8?dwg2_depth_load_.Get():dwg2_fractional_load_.Get());list.D3DDispatch((width+7)/8,(height+7)/8,1);
 cp.PushTransitionBarrier(buffer,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);
 cp.PushTransitionBarrier(retained,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,retained_state);
 cp.PushTransitionBarrier(texture.resource(),texture.SetResourceState(D3D12_RESOURCE_STATE_COPY_DEST),D3D12_RESOURCE_STATE_COPY_DEST);cp.SubmitBarriers();
 D3D12_TEXTURE_COPY_LOCATION source{},destination{};source.pResource=buffer;source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;source.PlacedFootprint.Footprint={key.format==xenos::TextureFormat::k_24_8?DXGI_FORMAT_R32_FLOAT:DXGI_FORMAT_R8G8B8A8_UNORM,width,height,1,row_pitch};
 destination.pResource=texture.resource();destination.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;list.D3DCopyTextureRegion(&destination,0,0,0,&source,nullptr);
 cp.ReleaseScratchGPUBuffer(buffer,D3D12_RESOURCE_STATE_COPY_SOURCE);texture.MarkAsUsed();return true;
}

// Capture two/four-sample RGBA8 resources without collapsing retained detail.
Microsoft::WRL::ComPtr<ID3D12Resource> D3D12TextureCache::Dwg2CaptureMSAARGBA8(
 ID3D12Resource* source,D3D12_RESOURCE_STATES state,uint32_t width,uint32_t height,
 uint32_t pitch,uint32_t left,uint32_t top,uint32_t sample,uint32_t flags,uint32_t guest_samples) {
 if(dwg2_scene_denominator_!=2||!source||!width||!height||width>8192||height>8192||pitch<width||pitch>16384||pitch%32||sample>6||flags>3||(guest_samples!=2&&guest_samples!=4)||
   (guest_samples==2&&sample!=0&&sample!=1&&sample!=4))return {};
 const auto desc=source->GetDesc();const uint32_t sw=(width*3+1)/2,sh=(height*3+1)/2;
 if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||(desc.SampleDesc.Count!=2&&desc.SampleDesc.Count!=4)||(guest_samples==4&&desc.SampleDesc.Count!=4)||desc.DepthOrArraySize!=1||desc.MipLevels!=1||
   (desc.Format!=DXGI_FORMAT_R8G8B8A8_TYPELESS&&desc.Format!=DXGI_FORMAT_R8G8B8A8_UNORM)||uint64_t(left)+sw>desc.Width||uint64_t(top)+sh>desc.Height)return {};
 const uint64_t tiles=uint64_t(pitch/32)*((height+31)/32),size=tiles*48*48*4;if(tiles>65535||size>UINT32_MAX)return {};
 auto lock=dwg2_retained_lock_.Acquire();Dwg2PruneRetained();uint64_t used=size;
 for(const auto& record:dwg2_retained_surfaces_)used+=record.resource->GetDesc().Width;
 if(dwg2_retained_surfaces_.size()>=64||used>256ULL*1024*1024)return {};
 auto& cp=command_processor_;const auto& provider=cp.GetD3D12Provider();auto* device=provider.GetDevice();
 if(!dwg2_msaa_root_) {
  D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,1,0,0,0};D3D12_ROOT_PARAMETER parameters[3]{};
  parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;parameters[0].Constants={0,0,8};
  parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[1].DescriptorTable={1,&range};
  parameters[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;parameters[2].Descriptor={0,0};
  D3D12_ROOT_SIGNATURE_DESC signature{3,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
  dwg2_msaa_root_.Attach(ui::d3d12::util::CreateRootSignature(provider,signature));if(!dwg2_msaa_root_)return {};
 }
 if(!dwg2_msaa_pipeline_) {
  dwg2_msaa_pipeline_.Attach(ui::d3d12::util::CreateComputePipeline(device,dwg2::shaders::fractional_msaa_capture,sizeof(dwg2::shaders::fractional_msaa_capture),dwg2_msaa_root_.Get()));if(!dwg2_msaa_pipeline_)return {};
 }
 ui::d3d12::util::DescriptorCpuGpuHandlePair view;if(!cp.RequestOneUseSingleViewDescriptors(1,&view))return {};
 D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R8G8B8A8_UNORM;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DMS;device->CreateShaderResourceView(source,&srv,view.first);
 Microsoft::WRL::ComPtr<ID3D12Resource> packed;D3D12_RESOURCE_DESC bd{};ui::d3d12::util::FillBufferResourceDesc(bd,size,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
 if(FAILED(device->CreateCommittedResource(&ui::d3d12::util::kHeapPropertiesDefault,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&packed))))return {};
 dwg2_retained_surfaces_.push_back({0,0,0,0,0,0,false,cp.GetCurrentSubmission(),packed});
 cp.PushTransitionBarrier(source,state,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);cp.SubmitBarriers();
 auto& list=cp.GetDeferredCommandList();const uint32_t constants[8]={width,height,pitch,left,top,sample,flags,guest_samples==2?(desc.SampleDesc.Count==2?1u:2u):0u};
 cp.SetExternalPipeline(dwg2_msaa_pipeline_.Get());list.D3DSetComputeRootSignature(dwg2_msaa_root_.Get());list.D3DSetComputeRoot32BitConstants(0,8,constants,0);list.D3DSetComputeRootDescriptorTable(1,view.second);list.D3DSetComputeRootUnorderedAccessView(2,packed->GetGPUVirtualAddress());list.D3DDispatch(6,6,uint32_t(tiles));
 cp.PushTransitionBarrier(source,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,state);cp.PushTransitionBarrier(packed.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);cp.SubmitBarriers();return packed;
}

// Individual multisampled D24S8 samples, with separate depth/stencil typed views.
Microsoft::WRL::ComPtr<ID3D12Resource> D3D12TextureCache::Dwg2CaptureMSAADepth24(
 ID3D12Resource* source,D3D12_RESOURCE_STATES state,uint32_t width,uint32_t height,
 uint32_t pitch,uint32_t left,uint32_t top,uint32_t sample,uint32_t flags,uint32_t guest_samples) {
 if(dwg2_scene_denominator_!=2||!source||!width||!height||width>8192||height>8192||pitch<width||pitch>16384||pitch%32||sample>3||flags>1||(guest_samples!=2&&guest_samples!=4)||
   (guest_samples==2&&sample>1))return {};
 const auto desc=source->GetDesc();const uint32_t sw=(width*3+1)/2,sh=(height*3+1)/2;
 if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||(desc.SampleDesc.Count!=2&&desc.SampleDesc.Count!=4)||(guest_samples==4&&desc.SampleDesc.Count!=4)||desc.DepthOrArraySize!=1||desc.MipLevels!=1||
   (desc.Format!=DXGI_FORMAT_R24G8_TYPELESS&&desc.Format!=DXGI_FORMAT_D24_UNORM_S8_UINT)||uint64_t(left)+sw>desc.Width||uint64_t(top)+sh>desc.Height)return {};
 const uint64_t tiles=uint64_t(pitch/32)*((height+31)/32),size=tiles*48*48*4;if(tiles>65535||size>UINT32_MAX)return {};
 auto lock=dwg2_retained_lock_.Acquire();Dwg2PruneRetained();uint64_t used=size;
 for(const auto& record:dwg2_retained_surfaces_)used+=record.resource->GetDesc().Width;
 if(dwg2_retained_surfaces_.size()>=64||used>256ULL*1024*1024)return {};
 auto& cp=command_processor_;const auto& provider=cp.GetD3D12Provider();auto* device=provider.GetDevice();
 if(!dwg2_depth_msaa_root_) {
  D3D12_DESCRIPTOR_RANGE range{D3D12_DESCRIPTOR_RANGE_TYPE_SRV,2,0,0,0};D3D12_ROOT_PARAMETER parameters[3]{};
  parameters[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;parameters[0].Constants={0,0,8};
  parameters[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;parameters[1].DescriptorTable={1,&range};
  parameters[2].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;parameters[2].Descriptor={0,0};
  D3D12_ROOT_SIGNATURE_DESC signature{3,parameters,0,nullptr,D3D12_ROOT_SIGNATURE_FLAG_NONE};
  dwg2_depth_msaa_root_.Attach(ui::d3d12::util::CreateRootSignature(provider,signature));if(!dwg2_depth_msaa_root_)return {};
 }
 if(!dwg2_depth_msaa_pipeline_) {
  dwg2_depth_msaa_pipeline_.Attach(ui::d3d12::util::CreateComputePipeline(device,dwg2::shaders::fractional_depth_msaa_capture,sizeof(dwg2::shaders::fractional_depth_msaa_capture),dwg2_depth_msaa_root_.Get()));if(!dwg2_depth_msaa_pipeline_)return {};
 }
 ui::d3d12::util::DescriptorCpuGpuHandlePair view;if(!cp.RequestOneUseSingleViewDescriptors(2,&view))return {};
 D3D12_SHADER_RESOURCE_VIEW_DESC srv{};srv.Format=DXGI_FORMAT_R24_UNORM_X8_TYPELESS;srv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;srv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DMS;device->CreateShaderResourceView(source,&srv,view.first);
 auto stencil_view=view.first;stencil_view.ptr+=device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);srv.Format=DXGI_FORMAT_X24_TYPELESS_G8_UINT;device->CreateShaderResourceView(source,&srv,stencil_view);
 Microsoft::WRL::ComPtr<ID3D12Resource> packed;D3D12_RESOURCE_DESC bd{};ui::d3d12::util::FillBufferResourceDesc(bd,size,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
 if(FAILED(device->CreateCommittedResource(&ui::d3d12::util::kHeapPropertiesDefault,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&packed))))return {};
 dwg2_retained_surfaces_.push_back({0,0,0,0,0,0,false,cp.GetCurrentSubmission(),packed});
 cp.PushTransitionBarrier(source,state,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);cp.SubmitBarriers();
 auto& list=cp.GetDeferredCommandList();const uint32_t constants[8]={width,height,pitch,left,top,sample,flags,guest_samples==2?(desc.SampleDesc.Count==2?1u:2u):0u};
 cp.SetExternalPipeline(dwg2_depth_msaa_pipeline_.Get());list.D3DSetComputeRootSignature(dwg2_depth_msaa_root_.Get());list.D3DSetComputeRoot32BitConstants(0,8,constants,0);list.D3DSetComputeRootDescriptorTable(1,view.second);list.D3DSetComputeRootUnorderedAccessView(2,packed->GetGPUVirtualAddress());list.D3DDispatch(6,6,uint32_t(tiles));
 cp.PushTransitionBarrier(source,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE,state);cp.PushTransitionBarrier(packed.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);cp.SubmitBarriers();return packed;
}

// Copy complete D24S8 planes (D3D12 requires full depth-subresource copies),
// then retain a bounded high-resolution crop without losing stencil bits.
Microsoft::WRL::ComPtr<ID3D12Resource> D3D12TextureCache::Dwg2CaptureDepth24(
 ID3D12Resource* source,D3D12_RESOURCE_STATES state,uint32_t width,uint32_t height,
 uint32_t pitch,uint32_t left,uint32_t top,uint32_t half) {
 if(dwg2_scene_denominator_!=2||!source||!width||!height||width>8192||height>8192||pitch<width||pitch>16384||pitch%32||half>1)return {};
 const auto desc=source->GetDesc();const uint32_t sw=(width*3+1)/2,sh=(height*3+1)/2;
 if(desc.Dimension!=D3D12_RESOURCE_DIMENSION_TEXTURE2D||desc.SampleDesc.Count!=1||desc.DepthOrArraySize!=1||desc.MipLevels!=1||
   (desc.Format!=DXGI_FORMAT_R24G8_TYPELESS&&desc.Format!=DXGI_FORMAT_D24_UNORM_S8_UINT)||uint64_t(left)+sw>desc.Width||uint64_t(top)+sh>desc.Height)return {};
 const uint64_t tiles=uint64_t(pitch/32)*((height+31)/32),packed_size=tiles*48*48*4;if(tiles>65535||packed_size>UINT32_MAX)return {};
 auto lock=dwg2_retained_lock_.Acquire();Dwg2PruneRetained();uint64_t used=packed_size;for(const auto& record:dwg2_retained_surfaces_)used+=record.resource->GetDesc().Width;
 if(dwg2_retained_surfaces_.size()>=64||used>256ULL*1024*1024||!Dwg2PrepareRetainedPipeline(3))return {};
 auto& cp=command_processor_;auto* device=cp.GetD3D12Provider().GetDevice();D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprints[2]{};UINT rows[2]{};UINT64 row_bytes[2]{},total=0;device->GetCopyableFootprints(&desc,0,2,0,footprints,rows,row_bytes,&total);
 if(total>UINT32_MAX||footprints[1].Offset>UINT32_MAX||footprints[0].Offset||
    row_bytes[0]!=desc.Width*4||row_bytes[1]!=desc.Width||footprints[0].Footprint.RowPitch<desc.Width*4||footprints[1].Footprint.RowPitch<desc.Width||
    footprints[0].Footprint.RowPitch%256||footprints[1].Footprint.RowPitch%256||footprints[0].Footprint.RowPitch>=0x40000000u)return {};
 Microsoft::WRL::ComPtr<ID3D12Resource> packed;D3D12_RESOURCE_DESC bd{};ui::d3d12::util::FillBufferResourceDesc(bd,packed_size,D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS);
 if(FAILED(device->CreateCommittedResource(&ui::d3d12::util::kHeapPropertiesDefault,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&packed))))return {};
 auto* linear=cp.RequestScratchGPUBuffer(uint32_t(total),D3D12_RESOURCE_STATE_COPY_DEST);if(!linear)return {};
 dwg2_retained_surfaces_.push_back({0,0,0,0,0,0,false,cp.GetCurrentSubmission(),packed});
 cp.PushTransitionBarrier(source,state,D3D12_RESOURCE_STATE_COPY_SOURCE);cp.SubmitBarriers();auto& list=cp.GetDeferredCommandList();
 for(uint32_t plane=0;plane<2;++plane) {D3D12_TEXTURE_COPY_LOCATION src{},dst{};src.pResource=source;src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;src.SubresourceIndex=plane;dst.pResource=linear;dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=footprints[plane];list.D3DCopyTextureRegion(&dst,0,0,0,&src,nullptr);}
 cp.PushTransitionBarrier(source,D3D12_RESOURCE_STATE_COPY_SOURCE,state);cp.PushTransitionBarrier(linear,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);cp.SubmitBarriers();
 const uint32_t constants[8]={left,top,sw,sh,footprints[1].Footprint.RowPitch,pitch/32,uint32_t(footprints[1].Offset),footprints[0].Footprint.RowPitch|(half<<31)};
 cp.SetExternalPipeline(dwg2_depth_pack_.Get());list.D3DSetComputeRootSignature(dwg2_fractional_root_.Get());list.D3DSetComputeRoot32BitConstants(0,8,constants,0);list.D3DSetComputeRootShaderResourceView(1,linear->GetGPUVirtualAddress());list.D3DSetComputeRootUnorderedAccessView(2,packed->GetGPUVirtualAddress());list.D3DDispatch(6,6,uint32_t(tiles));
 cp.PushTransitionBarrier(packed.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);cp.SubmitBarriers();cp.ReleaseScratchGPUBuffer(linear,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);return packed;
}
