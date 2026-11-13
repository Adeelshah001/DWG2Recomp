// Included inside rex::graphics::d3d12. Guest memory is never patched.
namespace {
int Dwg2PromptStyle() {
  using Query=int(*)();
  static auto query=reinterpret_cast<Query>(GetProcAddress(GetModuleHandleW(L"rexruntime.dll"),"Dwg2PromptStyle"));
  return query?query():1;
}
int Dwg2ControllerStyle() {
  using Query=int(*)();
  static auto query=reinterpret_cast<Query>(GetProcAddress(GetModuleHandleW(L"rexruntime.dll"),"Dwg2ControllerStyle"));
  return query?query():1;
}
bool Dwg2HudCorrection() {
  using Query=int(*)();
  static auto query=reinterpret_cast<Query>(GetProcAddress(GetModuleHandleW(L"rexruntime.dll"),"Dwg2HudCorrection"));
  return query&&query()!=0;
}
}
void D3D12TextureCache::Dwg2RecognizeAtlas(D3D12Texture& texture) {
  texture.dwg2_atlas_kind=0;texture.dwg2_prompt_style=1;texture.dwg2_original.clear();
  const auto key=texture.key();
  const uint32_t width=key.GetWidth(),height=key.GetHeight();
  // The options controller image is BC3, not one of the raw HUD atlases.
  // Hash canonical GPU blocks, handling linear and tiled guest images and all
  // Xenos byte orders. Retain the original blocks for exact Xbox restoration.
  if(width==512&&height==256&&!key.scaled_resolve&&
     key.format==xenos::TextureFormat::k_DXT4_5&&
     key.dimension==xenos::DataDimension::k2DOrStacked&&key.GetDepthOrArraySize()==1&&!key.mip_max_level) {
    const auto& layout=texture.guest_layout().base;const uint32_t address=key.base_page<<12;
    if(!address||uint64_t(address)+texture.GetGuestBaseSize()>0x20000000ULL)return;
    std::vector<uint8_t> blocks(131072);
    rex::thread::global_critical_region region;auto lock=region.Acquire();
    const uint8_t* data=command_processor_.Dwg2Memory().TranslatePhysical(address);
    for(uint32_t y=0;y<64;++y)for(uint32_t x=0;x<128;++x) {
      uint32_t at=key.tiled?uint32_t(texture_util::GetTiledOffset2D(x,y,layout.row_pitch_bytes/16,4)):y*layout.row_pitch_bytes+x*16;
      if(uint64_t(at)+16>texture.GetGuestBaseSize())return;
      for(uint32_t word=0;word<4;++word) {
        uint32_t value;std::memcpy(&value,data+at+word*4,4);value=xenos::GpuSwap(value,key.endianness);
        std::memcpy(blocks.data()+(y*128+x)*16+word*4,&value,4);
      }
    }
    const auto hash=dwg2::atlas::Hash(blocks);
    if(hash.a==0x44a946c9ce7324edULL&&hash.b==0xd764f0715b061c72ULL) {
      texture.dwg2_atlas_kind=5;texture.dwg2_original=std::move(blocks);
      REXGPU_INFO("DWG2 identified options controller diagram");
    }
    return;
  }
  bool possible=false;
  for(const auto& id:dwg2::atlas::fingerprints)possible|=id.width==int(width)&&id.height==int(height);
  if(!possible||key.scaled_resolve||key.format!=xenos::TextureFormat::k_8_8_8_8||
     key.dimension!=xenos::DataDimension::k2DOrStacked||key.GetDepthOrArraySize()!=1||key.mip_max_level)return;
  const auto& layout=texture.guest_layout().base;
  const uint32_t address=key.base_page<<12;
  if(!address||uint64_t(address)+texture.GetGuestBaseSize()>0x20000000ULL)return;
  std::vector<uint8_t> pixels(size_t(width)*height*4);
  {
    rex::thread::global_critical_region region;auto lock=region.Acquire();
    const uint8_t* data=command_processor_.Dwg2Memory().TranslatePhysical(address);
    for(uint32_t y=0;y<height;++y)for(uint32_t x=0;x<width;++x) {
      uint32_t at=key.tiled?uint32_t(texture_util::GetTiledOffset2D(x,y,layout.row_pitch_bytes/4,2)):
                           y*layout.row_pitch_bytes+x*4;
      if(uint64_t(at)+4>texture.GetGuestBaseSize())return;
      uint32_t value;std::memcpy(&value,data+at,4);
      value=xenos::GpuSwap(value,key.endianness);
      std::memcpy(pixels.data()+(size_t(y)*width+x)*4,&value,4);
    }
  }
  texture.dwg2_atlas_kind=dwg2::atlas::Identify(pixels,width,height);
  if(texture.dwg2_atlas_kind>=2)texture.dwg2_original=std::move(pixels);
  if(texture.dwg2_atlas_kind)REXGPU_INFO("DWG2 identified HUD/prompt atlas: {}x{}, kind {}",width,height,texture.dwg2_atlas_kind);
}
void D3D12TextureCache::Dwg2ApplyAtlas(D3D12Texture& texture) {
  if(texture.dwg2_original.empty())return;
  const bool controller=texture.dwg2_atlas_kind==5;
  const int style=controller?Dwg2ControllerStyle():Dwg2PromptStyle();
  if(style==texture.dwg2_prompt_style)return;
  const auto key=texture.key();const uint32_t width=key.GetWidth(),height=key.GetHeight();
  auto pixels=texture.dwg2_original;
  if(style==2) {
    if(controller)pixels=dwg2::art::Controller();
    else if(!dwg2::atlas::PlayStation(pixels,width,height,texture.dwg2_atlas_kind))return;
  }
  // These qualified atlases have power-of-two widths >= 512, so each row is
  // already aligned to D3D12's 256-byte texture-copy pitch requirement.
  ID3D12Resource* buffer=nullptr;size_t offset=0;
  auto* upload=command_processor_.GetConstantBufferPool().Request(
      command_processor_.GetCurrentSubmission(),pixels.size(),D3D12_TEXTURE_DATA_PLACEMENT_ALIGNMENT,
      &buffer,&offset,nullptr);
  if(!upload)return;
  std::memcpy(upload,pixels.data(),pixels.size());
  command_processor_.PushTransitionBarrier(texture.resource(),texture.SetResourceState(D3D12_RESOURCE_STATE_COPY_DEST),D3D12_RESOURCE_STATE_COPY_DEST);
  command_processor_.SubmitBarriers();
  D3D12_TEXTURE_COPY_LOCATION source{},dest{};
  source.pResource=buffer;source.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
  source.PlacedFootprint.Offset=offset;
  source.PlacedFootprint.Footprint={controller?DXGI_FORMAT_BC3_UNORM:DXGI_FORMAT_R8G8B8A8_UNORM,width,height,1,width*4};
  dest.pResource=texture.resource();dest.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
  command_processor_.GetDeferredCommandList().D3DCopyTextureRegion(&dest,0,0,0,&source,nullptr);
  texture.MarkAsUsed();texture.dwg2_prompt_style=style;
}
