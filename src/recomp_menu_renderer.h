// D3D12 drawing for the host Recomp Menu. The font is decoded at runtime.
#pragma once

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <string_view>
#include <vector>

#include <rex/logging.h>
#include <rex/ui/d3d12/d3d12_api.h>
#include <rex/ui/d3d12/d3d12_provider.h>
#include <rex/ui/d3d12/d3d12_util.h>
#include <rex/ui/immediate_drawer.h>

#include "game_ui_assets.h"
#include "recomp_menu_layout.h"

namespace dwg2::recomp_menu {

namespace render_detail {

class RecompMenuRenderer {
 public:
  ~RecompMenuRenderer() {
    for (size_t i = 0; i < kVertexBufferSlots; ++i) {
      if (vertex_buffers_[i] && vertex_mapped_[i]) {
        vertex_buffers_[i]->Unmap(0, nullptr);
        vertex_mapped_[i] = nullptr;
      }
    }
  }

  void Draw(const rex::ui::d3d12::D3D12Provider& provider,
            ID3D12GraphicsCommandList* command_list,
            D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle,
            uint32_t width, uint32_t height,
            UINT64 current_submission, UINT64 completed_submission,
            DXGI_FORMAT rtv_format,
            const void* vs_code, size_t vs_size,
            const void* ps_code, size_t ps_size,
            const MenuSnapshot& snap, std::string_view fps_status) {
    if (!command_list || !width || !height || !snap.open) return;
    if (!EnsureInitialized(provider, rtv_format, vs_code, vs_size, ps_code, ps_size)) return;

    if (!textures_uploaded_) {
      std::array<D3D12_RESOURCE_BARRIER, dwg2::ui_assets::kTexCount> barriers{};
      UINT barrier_count = 0;
      for (int t = 0; t < dwg2::ui_assets::kTexCount; ++t) {
        if (!textures_[t] || !upload_buffers_[t]) continue;
        D3D12_RESOURCE_DESC tex_desc = textures_[t]->GetDesc();
        D3D12_TEXTURE_COPY_LOCATION src{}, dst{};
        src.pResource = upload_buffers_[t].Get();
        src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
        provider.GetDevice()->GetCopyableFootprints(&tex_desc, 0, 1, 0, &src.PlacedFootprint,
                                                    nullptr, nullptr, nullptr);
        dst.pResource = textures_[t].Get();
        dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
        dst.SubresourceIndex = 0;
        command_list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

        auto& b = barriers[barrier_count++];
        b.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
        b.Transition.pResource = textures_[t].Get();
        b.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
        b.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
        b.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
      }
      if (barrier_count) command_list->ResourceBarrier(barrier_count, barriers.data());
      textures_uploaded_ = true;
      upload_active_ = true;
      upload_submission_ = current_submission;
    } else if (upload_active_ && completed_submission >= upload_submission_) {
      for (auto& ub : upload_buffers_) ub.Reset();
      for (auto& tex : pack_.textures) std::vector<uint8_t>().swap(tex.rgba);
      upload_active_ = false;
    }

    const size_t slot = size_t(current_submission % kVertexBufferSlots);
    auto* dst = static_cast<rex::ui::ImmediateVertex*>(vertex_mapped_[slot]);
    if (!dst) return;

    const auto viewport = ComputeViewport(float(width), float(height));
    size_t vert_count = 0;
    batches_.clear();
    hits_.clear();

    auto quad = [&](int tex, Box box, float u0, float v0, float u1, float v1, uint32_t color) {
      if (vert_count + 6 > kMaxVerts) return;
      if (batches_.empty() || batches_.back().tex != tex) {
        batches_.push_back({tex, UINT(vert_count), 0});
      }
      const float x0 = viewport.ox + box.x0 * viewport.scale;
      const float x1 = viewport.ox + box.x1 * viewport.scale;
      const float y0 = viewport.oy + box.y0 * viewport.scale;
      const float y1 = viewport.oy + box.y1 * viewport.scale;
      dst[vert_count++] = {x0, y0, u0, v0, color};
      dst[vert_count++] = {x1, y0, u1, v0, color};
      dst[vert_count++] = {x1, y1, u1, v1, color};
      dst[vert_count++] = {x0, y0, u0, v0, color};
      dst[vert_count++] = {x1, y1, u1, v1, color};
      dst[vert_count++] = {x0, y1, u0, v1, color};
      batches_.back().count += 6;
    };
    auto fill = [&](Box box, uint32_t color) {
      quad(dwg2::ui_assets::kTexWhite, box, 0, 0, 1, 1, color);
    };
    const auto& font = pack_.textures[dwg2::ui_assets::kTexFont];
    auto glyph = [&](unsigned char ch) -> const dwg2::ui_assets::GlyphMetric& {
      const auto& ascii = pack_.ascii_glyphs;
      return ch < ascii.size() && ascii[ch].valid ? ascii[ch] : ascii['?'];
    };
    auto text = [&](Box box, std::string_view value, float preferred, uint32_t color, Align align) {
      if (value.empty()) return;
      float advance = 0;
      for (unsigned char ch : value) advance += glyph(ch).advance;
      const auto place = PlaceText(box, advance, preferred, align);
      float x = place.x;
      for (unsigned char ch : value) {
        const auto& g = glyph(ch);
        if (ch != ' ' && g.valid) {
          const float glyph_width = float(g.max_x - g.min_x + 1) * place.scale;
          quad(dwg2::ui_assets::kTexFont,
               {x, place.y, x + glyph_width, place.y + 28 * place.scale},
               float(g.cell_x + g.min_x) / font.width, float(g.cell_y) / font.height,
               float(g.cell_x + g.max_x + 1) / font.width, float(g.cell_y + 28) / font.height,
               color);
        }
        x += g.advance * place.scale;
      }
    };

    DrawMenuLayout(snap, viewport.virt_w, fps_status, fill, text, [&](Box box, int id, int sub) {
      hits_.push_back({box.x0, box.y0, box.x1, box.y1, id, sub});
    });
    UpdateHitRects(hits_, snap.view, snap.wiz_scroll);
    Flush(command_list, rtv_handle, width, height, slot, vert_count, batches_);
  }

 private:
  static constexpr size_t kMaxVerts = 24576;
  static constexpr size_t kVertexBufferSlots = 4;
  static constexpr size_t kVertexBufferBytes = kMaxVerts * sizeof(rex::ui::ImmediateVertex);
  static constexpr uint32_t kHeapDescriptors = 16;

  struct Batch {
    int tex;
    UINT start;
    UINT count;
  };
  std::vector<Batch> batches_;
  std::vector<HitRect> hits_;

  void Flush(ID3D12GraphicsCommandList* command_list, D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle,
             uint32_t width, uint32_t height, size_t slot, size_t vert_count,
             const std::vector<Batch>& batches) {
    if (vert_count == 0 || batches.empty()) return;
    command_list->OMSetRenderTargets(1, &rtv_handle, TRUE, nullptr);
    D3D12_VIEWPORT vp{0.0f, 0.0f, float(width), float(height), 0.0f, 1.0f};
    D3D12_RECT sc{0, 0, LONG(width), LONG(height)};
    command_list->RSSetViewports(1, &vp);
    command_list->RSSetScissorRects(1, &sc);

    ID3D12DescriptorHeap* heaps[] = {srv_heap_.Get()};
    command_list->SetDescriptorHeaps(1, heaps);
    command_list->SetGraphicsRootSignature(root_signature_.Get());
    command_list->SetPipelineState(pso_.Get());
    const float inv[2] = {1.0f / float(width), 1.0f / float(height)};
    command_list->SetGraphicsRoot32BitConstants(1, 2, inv, 0);
    command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D12_VERTEX_BUFFER_VIEW vbv{};
    vbv.BufferLocation = vertex_buffers_[slot]->GetGPUVirtualAddress();
    vbv.SizeInBytes = UINT(vert_count * sizeof(rex::ui::ImmediateVertex));
    vbv.StrideInBytes = UINT(sizeof(rex::ui::ImmediateVertex));
    command_list->IASetVertexBuffers(0, 1, &vbv);

    const D3D12_GPU_DESCRIPTOR_HANDLE gpu_base = srv_heap_->GetGPUDescriptorHandleForHeapStart();
    for (const auto& b : batches) {
      if (b.count == 0) continue;
      D3D12_GPU_DESCRIPTOR_HANDLE h = gpu_base;
      h.ptr += SIZE_T(b.tex) * descriptor_size_;
      command_list->SetGraphicsRootDescriptorTable(0, h);
      command_list->DrawInstanced(b.count, 1, b.start, 0);
    }
  }

  bool EnsureInitialized(const rex::ui::d3d12::D3D12Provider& provider, DXGI_FORMAT rtv_format,
                         const void* vs_code, size_t vs_size,
                         const void* ps_code, size_t ps_size) {
    if (initialized_) return true;
    if (failed_) return false;
    ID3D12Device* device = provider.GetDevice();
    if (!device) return false;

    D3D12_DESCRIPTOR_RANGE srv_range{};
    srv_range.RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;
    srv_range.NumDescriptors = 1;
    srv_range.BaseShaderRegister = 0;

    D3D12_ROOT_PARAMETER params[2]{};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[0].DescriptorTable.NumDescriptorRanges = 1;
    params[0].DescriptorTable.pDescriptorRanges = &srv_range;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[1].Constants.ShaderRegister = 0;
    params[1].Constants.Num32BitValues = 2;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    D3D12_ROOT_SIGNATURE_DESC rs_desc{};
    rs_desc.NumParameters = 2;
    rs_desc.pParameters = params;
    rs_desc.NumStaticSamplers = 1;
    rs_desc.pStaticSamplers = &sampler;
    rs_desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    *(root_signature_.ReleaseAndGetAddressOf()) =
        rex::ui::d3d12::util::CreateRootSignature(provider, rs_desc);
    if (!root_signature_) {
      REXLOG_ERROR("DWG2 RecompMenu: failed to create root signature");
      failed_ = true;
      return false;
    }

    D3D12_INPUT_ELEMENT_DESC input_elements[3]{};
    input_elements[0].SemanticName = "POSITION";
    input_elements[0].Format = DXGI_FORMAT_R32G32_FLOAT;
    input_elements[0].AlignedByteOffset = offsetof(rex::ui::ImmediateVertex, x);
    input_elements[1].SemanticName = "TEXCOORD";
    input_elements[1].Format = DXGI_FORMAT_R32G32_FLOAT;
    input_elements[1].AlignedByteOffset = offsetof(rex::ui::ImmediateVertex, u);
    input_elements[2].SemanticName = "COLOR";
    input_elements[2].Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    input_elements[2].AlignedByteOffset = offsetof(rex::ui::ImmediateVertex, color);

    D3D12_GRAPHICS_PIPELINE_STATE_DESC pso_desc{};
    pso_desc.pRootSignature = root_signature_.Get();
    pso_desc.VS = {vs_code, vs_size};
    pso_desc.PS = {ps_code, ps_size};
    auto& blend = pso_desc.BlendState.RenderTarget[0];
    blend.BlendEnable = TRUE;
    blend.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blend.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOp = D3D12_BLEND_OP_ADD;
    blend.SrcBlendAlpha = D3D12_BLEND_ONE;
    blend.DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
    blend.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blend.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
    pso_desc.SampleMask = UINT_MAX;
    pso_desc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
    pso_desc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
    pso_desc.RasterizerState.DepthClipEnable = TRUE;
    pso_desc.InputLayout = {input_elements, 3};
    pso_desc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    pso_desc.NumRenderTargets = 1;
    pso_desc.RTVFormats[0] = rtv_format;
    pso_desc.SampleDesc.Count = 1;
    if (FAILED(device->CreateGraphicsPipelineState(&pso_desc, IID_PPV_ARGS(&pso_)))) {
      REXLOG_ERROR("DWG2 RecompMenu: failed to create PSO");
      failed_ = true;
      return false;
    }

    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_desc.NumDescriptors = kHeapDescriptors;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&srv_heap_)))) {
      REXLOG_ERROR("DWG2 RecompMenu: failed to create SRV heap");
      failed_ = true;
      return false;
    }
    descriptor_size_ = device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    pack_ = dwg2::ui_assets::LoadPack();
    const D3D12_CPU_DESCRIPTOR_HANDLE cpu_base = srv_heap_->GetCPUDescriptorHandleForHeapStart();
    const D3D12_RANGE read_none{0, 0};

    for (int t : {dwg2::ui_assets::kTexFont, dwg2::ui_assets::kTexWhite}) {
      auto& img = pack_.textures[t];
      if (img.width == 0 || img.height == 0 || img.rgba.size() < size_t(img.width) * img.height * 4u) {
        img = {4, 4, std::vector<uint8_t>(64, 255)};
      }
      D3D12_RESOURCE_DESC tex_desc{};
      tex_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
      tex_desc.Width = img.width;
      tex_desc.Height = img.height;
      tex_desc.DepthOrArraySize = 1;
      tex_desc.MipLevels = 1;
      tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      tex_desc.SampleDesc.Count = 1;
      if (FAILED(device->CreateCommittedResource(
              &rex::ui::d3d12::util::kHeapPropertiesDefault,
              provider.GetHeapFlagCreateNotZeroed(), &tex_desc,
              D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&textures_[t])))) {
        failed_ = true;
        return false;
      }

      D3D12_PLACED_SUBRESOURCE_FOOTPRINT footprint{};
      UINT64 upload_bytes = 0;
      device->GetCopyableFootprints(&tex_desc, 0, 1, 0, &footprint, nullptr, nullptr, &upload_bytes);
      D3D12_RESOURCE_DESC up_desc{};
      rex::ui::d3d12::util::FillBufferResourceDesc(up_desc, upload_bytes, D3D12_RESOURCE_FLAG_NONE);
      if (FAILED(device->CreateCommittedResource(
              &rex::ui::d3d12::util::kHeapPropertiesUpload,
              provider.GetHeapFlagCreateNotZeroed(), &up_desc,
              D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&upload_buffers_[t])))) {
        failed_ = true;
        return false;
      }

      void* mapped = nullptr;
      if (FAILED(upload_buffers_[t]->Map(0, &read_none, &mapped))) {
        failed_ = true;
        return false;
      }
      const size_t row_bytes = size_t(img.width) * 4u;
      for (uint32_t y = 0; y < img.height; ++y) {
        auto* dst_row = reinterpret_cast<uint8_t*>(mapped) + footprint.Offset +
                        size_t(footprint.Footprint.RowPitch) * y;
        std::memcpy(dst_row, &img.rgba[size_t(y) * row_bytes], row_bytes);
      }
      upload_buffers_[t]->Unmap(0, nullptr);

      D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
      srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
      srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
      srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      srv_desc.Texture2D.MipLevels = 1;
      D3D12_CPU_DESCRIPTOR_HANDLE h = cpu_base;
      h.ptr += SIZE_T(t) * descriptor_size_;
      device->CreateShaderResourceView(textures_[t].Get(), &srv_desc, h);
    }

    D3D12_RESOURCE_DESC vb_desc{};
    rex::ui::d3d12::util::FillBufferResourceDesc(vb_desc, kVertexBufferBytes, D3D12_RESOURCE_FLAG_NONE);
    for (size_t i = 0; i < kVertexBufferSlots; ++i) {
      if (FAILED(device->CreateCommittedResource(
              &rex::ui::d3d12::util::kHeapPropertiesUpload,
              provider.GetHeapFlagCreateNotZeroed(), &vb_desc,
              D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&vertex_buffers_[i])))) {
        failed_ = true;
        return false;
      }
      if (FAILED(vertex_buffers_[i]->Map(0, &read_none, &vertex_mapped_[i]))) {
        failed_ = true;
        return false;
      }
    }

    REXLOG_INFO("DWG2 RecompMenu initialized (from_game_data={})", pack_.from_game_data);
    initialized_ = true;
    return true;
  }

  bool initialized_ = false;
  bool failed_ = false;
  bool textures_uploaded_ = false;
  bool upload_active_ = false;
  UINT64 upload_submission_ = 0;
  UINT descriptor_size_ = 0;
  dwg2::ui_assets::AssetPack pack_;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srv_heap_;
  std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, dwg2::ui_assets::kTexCount> textures_;
  std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, dwg2::ui_assets::kTexCount> upload_buffers_;
  std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kVertexBufferSlots> vertex_buffers_;
  std::array<void*, kVertexBufferSlots> vertex_mapped_{};
};

struct OverlaySlot {
  const void* key = nullptr;
  std::unique_ptr<RecompMenuRenderer> renderer;
};
inline std::mutex& OverlayMutex() {
  static std::mutex m;
  return m;
}
inline std::array<OverlaySlot, 4>& OverlaySlots() {
  static std::array<OverlaySlot, 4> slots;
  return slots;
}

}  // namespace render_detail

inline void DrawRecompMenuOverlay(const void* owner_key,
                                  const rex::ui::d3d12::D3D12Provider& provider,
                                  ID3D12GraphicsCommandList* command_list,
                                  D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle,
                                  uint32_t width, uint32_t height,
                                  UINT64 current_submission, UINT64 completed_submission,
                                  DXGI_FORMAT rtv_format,
                                  const void* vs_code, size_t vs_size,
                                  const void* ps_code, size_t ps_size,
                                  const MenuSnapshot& snap, std::string_view fps_status) {
  std::lock_guard<std::mutex> lock(render_detail::OverlayMutex());
  auto& slots = render_detail::OverlaySlots();
  render_detail::OverlaySlot* free_slot = nullptr;
  for (auto& s : slots) {
    if (s.key == owner_key) {
      s.renderer->Draw(provider, command_list, rtv_handle, width, height,
                       current_submission, completed_submission, rtv_format,
                       vs_code, vs_size, ps_code, ps_size, snap, fps_status);
      return;
    }
    if (!s.key && !free_slot) free_slot = &s;
  }
  if (!free_slot) return;
  free_slot->key = owner_key;
  free_slot->renderer = std::make_unique<render_detail::RecompMenuRenderer>();
  free_slot->renderer->Draw(provider, command_list, rtv_handle, width, height,
                            current_submission, completed_submission, rtv_format,
                            vs_code, vs_size, ps_code, ps_size, snap, fps_status);
}

inline void DestroyRecompMenuOverlay(const void* owner_key) {
  std::lock_guard<std::mutex> lock(render_detail::OverlayMutex());
  for (auto& s : render_detail::OverlaySlots()) {
    if (s.key == owner_key) {
      s.renderer.reset();
      s.key = nullptr;
    }
  }
}

}  // namespace dwg2::recomp_menu
