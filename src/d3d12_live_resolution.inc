// Included inside rex::graphics::d3d12 in our isolated SDK overlay.
namespace {
struct Dwg2ResolutionState {
 dwg2::ResolvedRanges ranges;
 std::filesystem::path cache_root;
 uint32_t title=0;
};
std::unordered_map<const D3D12CommandProcessor*,std::unique_ptr<Dwg2ResolutionState>> dwg2_resolution_states;
Dwg2ResolutionState& Dwg2Info(const D3D12CommandProcessor* processor) {
 auto& p=dwg2_resolution_states[processor];if(!p)p=std::make_unique<Dwg2ResolutionState>();return *p;
}
}
void D3D12CommandProcessor::Dwg2RememberResolve(uint32_t address,uint32_t length) {
 if(length)Dwg2Info(this).ranges.Remember(address,length,
     static_cast<uint32_t>(register_file_->Get<reg::RB_COPY_DEST_INFO>().copy_dest_format));
}
bool D3D12CommandProcessor::Dwg2ChangeResolution(uint32_t scale) {
 if(scale<1 || scale>3 || !texture_cache_ || !render_target_cache_)return false;
 if(texture_cache_->draw_resolution_scale_x()==scale && texture_cache_->draw_resolution_scale_y()==scale)return true;
 uint32_t sx=scale,sy=scale;
 if(!D3D12TextureCache::ClampDrawResolutionScaleToMaxSupported(sx,sy,GetD3D12Provider()) || sx!=scale || sy!=scale)return false;
 if(!AwaitAllQueueOperationsCompletion())return false;
 auto& state=Dwg2Info(this);
 // Preserve resolved textures through the SDK's format-aware downscale/readback
 // path. Never overwrite pages invalidated by guest CPU writes.
 if(texture_cache_->IsDrawResolutionScaled()) {
  // Valid scaled pages are guest-write protected. Hold the same recursive
  // critical region as their invalidation callbacks during validation/copy.
  rex::thread::global_critical_region migration_region;
  auto migration_lock=migration_region.Acquire();
  std::vector<dwg2::ResolvedRanges::Range> plan;
  if(!state.ranges.Plan([&](uint32_t page){return texture_cache_->Dwg2IsRangeScaledResolved(page*4096,4096);},plan))return false;
  for(const auto& range:plan) {
   const auto* info=FormatInfo::Get(range.format);
   if(!info || (info->bits_per_pixel!=8 && info->bits_per_pixel!=16 && info->bits_per_pixel!=32 && info->bits_per_pixel!=64))return false;
   const uint32_t tile_bytes=1024*(info->bits_per_pixel/8);
   if(range.address%tile_bytes || range.length%tile_bytes)return false;
  }
  bool migrated=true;
  for(const auto& range:plan) {
   if(!BeginSubmission(true) || !texture_cache_->MakeScaledResolveRangeCurrent(range.address,range.length) ||
      !Dwg2ReadbackResolvedRange(range.address,range.length,range.format,true)){migrated=false;break;}
  }
  if(frame_open_ && !EndSubmission(true))migrated=false;
  if(!migrated || !AwaitAllQueueOperationsCompletion())return false;
 }
 // Allocate replacements before retiring old resources. Failure keeps the old
 // resource graph usable. Guest CPU state, registers and shared GPU memory stay.
 auto targets=std::make_unique<D3D12RenderTargetCache>(*register_file_,*memory_,scale,scale,*this,bindless_resources_used_);
 if(!targets->Initialize() || targets->GetPath()!=render_target_cache_->GetPath())return false;
 auto textures=D3D12TextureCache::Create(*register_file_,*shared_memory_,scale,scale,*this,bindless_resources_used_);
 if(!textures)return false;
 auto pipelines=std::make_unique<PipelineCache>(*this,*register_file_,*targets,bindless_resources_used_);
 if(!pipelines->Initialize())return false;
 if(!state.cache_root.empty())pipelines->InitializeShaderStorage(state.cache_root/ ("resolution-"+std::to_string(scale)),state.title,true);
 auto reload=[&](Shader* shader)->Shader* {
  if(!shader)return nullptr;
  auto code=shader->ucode_data();for(auto& word:code)word=rex::byte_swap(word);
  return pipelines->LoadShader(shader->type(),code.data(),static_cast<uint32_t>(code.size()));
 };
 auto* vertex=reload(active_vertex_shader_);auto* pixel=reload(active_pixel_shader_);
 auto old_pipelines=std::move(pipeline_cache_);auto old_textures=std::move(texture_cache_);auto old_targets=std::move(render_target_cache_);
 pipeline_cache_=std::move(pipelines);texture_cache_=std::move(textures);render_target_cache_=std::move(targets);
 active_vertex_shader_=vertex;active_pixel_shader_=pixel;
 old_pipelines.reset();old_textures.reset();old_targets.reset();
 // Refresh descriptors that refer to the replaced EDRAM resource.
 const auto& provider=GetD3D12Provider();
 if(bindless_resources_used_) {
  render_target_cache_->WriteEdramRawSRVDescriptor(provider.OffsetViewDescriptor(view_bindless_heap_cpu_start_,uint32_t(SystemBindlessView::kEdramRawSRV)));
  render_target_cache_->WriteEdramRawUAVDescriptor(provider.OffsetViewDescriptor(view_bindless_heap_cpu_start_,uint32_t(SystemBindlessView::kEdramRawUAV)));
  const SystemBindlessView srvs[]={SystemBindlessView::kEdramR32UintSRV,SystemBindlessView::kEdramR32G32UintSRV,SystemBindlessView::kEdramR32G32B32A32UintSRV};
  const SystemBindlessView uavs[]={SystemBindlessView::kEdramR32UintUAV,SystemBindlessView::kEdramR32G32UintUAV,SystemBindlessView::kEdramR32G32B32A32UintUAV};
  for(uint32_t i=0;i<3;++i) {
   render_target_cache_->WriteEdramUintPow2SRVDescriptor(provider.OffsetViewDescriptor(view_bindless_heap_cpu_start_,uint32_t(srvs[i])),i+2);
   render_target_cache_->WriteEdramUintPow2UAVDescriptor(provider.OffsetViewDescriptor(view_bindless_heap_cpu_start_,uint32_t(uavs[i])),i+2);
  }
  texture_cache_bindless_sampler_map_.clear();
 }
 current_guest_pipeline_=nullptr;current_external_pipeline_=nullptr;current_graphics_root_signature_=nullptr;
 InvalidateAllVertexBufferResidency();primitive_processor_->ClearCache();
 state.ranges.Clear();
 REXGPU_INFO("DWG2 live resolution applied: {}x, {}x{} internal/output target; frame resources rebuilt without restarting the title.",scale,1280*scale,720*scale);
 return true;
}
void D3D12CommandProcessor::Dwg2ApplyResolutionRequest() {
 using Take=int(*)();using Complete=void(*)(int,int);
 static auto runtime=GetModuleHandleW(L"rexruntime.dll");
 static auto take=reinterpret_cast<Take>(GetProcAddress(runtime,"Dwg2TakeResolutionRequest"));
 static auto complete=reinterpret_cast<Complete>(GetProcAddress(runtime,"Dwg2CompleteResolution"));
 if(!take || !complete)return;
 int requested=take();if(!requested)return;
 const bool success=Dwg2ChangeResolution(static_cast<uint32_t>(requested));
 complete(requested,success?0:1);
 if(!success)REXGPU_WARN("DWG2 live resolution rejected; old scale retained: request {}",requested);
}
