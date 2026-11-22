#pragma once
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <vector>

#include <rex/logging.h>
#include <rex/ui/d3d12/d3d12_api.h>
#include <rex/ui/d3d12/d3d12_provider.h>
#include <rex/ui/d3d12/d3d12_util.h>
#include <rex/ui/immediate_drawer.h>

namespace dwg2::overlay {

class FpsOverlayRenderer {
 public:
  ~FpsOverlayRenderer() {
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
            const char* text) {
    if (!command_list || !width || !height || !text || !*text) return;
    if (!EnsureInitialized(provider, rtv_format, vs_code, vs_size, ps_code, ps_size)) return;

    if (!font_uploaded_) {
      D3D12_RESOURCE_DESC tex_desc = font_texture_->GetDesc();
      D3D12_TEXTURE_COPY_LOCATION src{}, dst{};
      src.pResource = font_upload_buffer_.Get();
      src.Type = D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;
      provider.GetDevice()->GetCopyableFootprints(&tex_desc, 0, 1, 0, &src.PlacedFootprint, nullptr, nullptr, nullptr);
      dst.pResource = font_texture_.Get();
      dst.Type = D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;
      dst.SubresourceIndex = 0;
      command_list->CopyTextureRegion(&dst, 0, 0, 0, &src, nullptr);

      D3D12_RESOURCE_BARRIER barrier{};
      barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
      barrier.Transition.pResource = font_texture_.Get();
      barrier.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
      barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_COPY_DEST;
      barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE;
      command_list->ResourceBarrier(1, &barrier);

      font_uploaded_ = true;
      font_upload_submission_ = current_submission;
    } else if (font_upload_buffer_ && completed_submission >= font_upload_submission_) {
      font_upload_buffer_.Reset();
    }

    const size_t len = std::min<size_t>(std::strlen(text), kMaxChars);
    const int scale = std::clamp(int((height + 180) / 360), 2, 6);
    const int glyph_w = 5 * scale;
    const int glyph_h = 7 * scale;
    const int advance = 6 * scale;
    const int pad_x = 5 * scale;
    const int pad_y = 4 * scale;
    const int margin = 6 * scale;
    const int text_w = int(len) * advance - scale;
    const int box_w = text_w + 2 * pad_x;
    const int box_h = glyph_h + 2 * pad_y;
    const float box_x = float(int(width) - box_w - margin);
    const float box_y = float(margin);
    if (box_x < 0.0f || box_y + float(box_h) > float(height)) return;

    const size_t slot = size_t(current_submission % kVertexBufferSlots);
    auto* dst = static_cast<rex::ui::ImmediateVertex*>(vertex_mapped_[slot]);
    if (!dst) return;

    size_t vert_count = 0;
    // Solid white block at (2, 58) inside the 128x64 atlas for untextured quads.
    constexpr float kSolidU = 2.5f / float(kAtlasWidth);
    constexpr float kSolidV = 58.5f / float(kAtlasHeight);
    auto push_quad = [&](float x0, float y0, float x1, float y1,
                         float u0, float v0, float u1, float v1, uint32_t col) {
      if (vert_count + 6 > kMaxVerts) return;
      dst[vert_count++] = {x0, y0, u0, v0, col};
      dst[vert_count++] = {x1, y0, u1, v0, col};
      dst[vert_count++] = {x1, y1, u1, v1, col};
      dst[vert_count++] = {x0, y0, u0, v0, col};
      dst[vert_count++] = {x1, y1, u1, v1, col};
      dst[vert_count++] = {x0, y1, u0, v1, col};
    };

    // Dark translucent backdrop + crisp subtle border (R8G8B8A8 little-endian: 0xAABBGGRR).
    constexpr uint32_t kBgColor = 0xCC161210u;
    constexpr uint32_t kBorderColor = 0x9968A868u;
    constexpr uint32_t kTextColor = 0xFF98FF98u;
    const float b = float(std::max(1, scale / 2));
    push_quad(box_x, box_y, box_x + float(box_w), box_y + float(box_h),
              kSolidU, kSolidV, kSolidU, kSolidV, kBgColor);
    push_quad(box_x, box_y, box_x + float(box_w), box_y + b,
              kSolidU, kSolidV, kSolidU, kSolidV, kBorderColor);
    push_quad(box_x, box_y + float(box_h) - b, box_x + float(box_w), box_y + float(box_h),
              kSolidU, kSolidV, kSolidU, kSolidV, kBorderColor);
    push_quad(box_x, box_y + b, box_x + b, box_y + float(box_h) - b,
              kSolidU, kSolidV, kSolidU, kSolidV, kBorderColor);
    push_quad(box_x + float(box_w) - b, box_y + b, box_x + float(box_w), box_y + float(box_h) - b,
              kSolidU, kSolidV, kSolidU, kSolidV, kBorderColor);

    for (size_t i = 0; i < len; ++i) {
      const int idx = AtlasIndex(text[i]);
      if (idx <= 0) continue;  // Skip spaces.
      const int col_idx = idx % 16;
      const int row_idx = idx / 16;
      const float u0 = float(col_idx * 8 + 1) / float(kAtlasWidth);
      const float v0 = float(row_idx * 8) / float(kAtlasHeight);
      const float u1 = float(col_idx * 8 + 6) / float(kAtlasWidth);
      const float v1 = float(row_idx * 8 + 7) / float(kAtlasHeight);
      const float gx = box_x + float(pad_x + int(i) * advance);
      const float gy = box_y + float(pad_y);
      push_quad(gx, gy, gx + float(glyph_w), gy + float(glyph_h), u0, v0, u1, v1, kTextColor);
    }

    const size_t byte_size = vert_count * sizeof(rex::ui::ImmediateVertex);

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
    command_list->SetGraphicsRootDescriptorTable(0, srv_heap_->GetGPUDescriptorHandleForHeapStart());
    command_list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

    D3D12_VERTEX_BUFFER_VIEW vbv{};
    vbv.BufferLocation = vertex_buffers_[slot]->GetGPUVirtualAddress();
    vbv.SizeInBytes = UINT(byte_size);
    vbv.StrideInBytes = UINT(sizeof(rex::ui::ImmediateVertex));
    command_list->IASetVertexBuffers(0, 1, &vbv);
    command_list->DrawInstanced(UINT(vert_count), 1, 0, 0);
  }

 private:
  static constexpr uint32_t kAtlasWidth = 128;
  static constexpr uint32_t kAtlasHeight = 64;
  static constexpr size_t kMaxChars = 64;
  static constexpr size_t kMaxVerts = (kMaxChars + 8) * 6;
  static constexpr size_t kVertexBufferSlots = 4;
  static constexpr size_t kVertexBufferBytes = kMaxVerts * sizeof(rex::ui::ImmediateVertex);

  static int AtlasIndex(char c) {
    const unsigned char uc = static_cast<unsigned char>(c);
    if (uc >= 0x20 && uc <= 0x7E) return int(uc - 0x20);
    return 0;
  }

  static std::array<uint8_t, 7> GlyphRows(char c) {
    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    switch (c) {
      case '0': return {0x0E, 0x11, 0x13, 0x15, 0x19, 0x11, 0x0E};
      case '1': return {0x04, 0x0C, 0x04, 0x04, 0x04, 0x04, 0x0E};
      case '2': return {0x0E, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1F};
      case '3': return {0x1F, 0x02, 0x04, 0x02, 0x01, 0x11, 0x0E};
      case '4': return {0x02, 0x06, 0x0A, 0x12, 0x1F, 0x02, 0x02};
      case '5': return {0x1F, 0x10, 0x1E, 0x01, 0x01, 0x11, 0x0E};
      case '6': return {0x06, 0x08, 0x10, 0x1E, 0x11, 0x11, 0x0E};
      case '7': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x08, 0x08};
      case '8': return {0x0E, 0x11, 0x11, 0x0E, 0x11, 0x11, 0x0E};
      case '9': return {0x0E, 0x11, 0x11, 0x0F, 0x01, 0x02, 0x0C};
      case 'A': return {0x0E, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
      case 'B': return {0x1E, 0x11, 0x11, 0x1E, 0x11, 0x11, 0x1E};
      case 'C': return {0x0E, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0E};
      case 'D': return {0x1C, 0x12, 0x11, 0x11, 0x11, 0x12, 0x1C};
      case 'E': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x1F};
      case 'F': return {0x1F, 0x10, 0x10, 0x1E, 0x10, 0x10, 0x10};
      case 'G': return {0x0E, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0F};
      case 'H': return {0x11, 0x11, 0x11, 0x1F, 0x11, 0x11, 0x11};
      case 'I': return {0x0E, 0x04, 0x04, 0x04, 0x04, 0x04, 0x0E};
      case 'J': return {0x07, 0x02, 0x02, 0x02, 0x02, 0x12, 0x0C};
      case 'K': return {0x11, 0x12, 0x14, 0x18, 0x14, 0x12, 0x11};
      case 'L': return {0x10, 0x10, 0x10, 0x10, 0x10, 0x10, 0x1F};
      case 'M': return {0x11, 0x1B, 0x15, 0x15, 0x11, 0x11, 0x11};
      case 'N': return {0x11, 0x11, 0x19, 0x15, 0x13, 0x11, 0x11};
      case 'O': return {0x0E, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
      case 'P': return {0x1E, 0x11, 0x11, 0x1E, 0x10, 0x10, 0x10};
      case 'Q': return {0x0E, 0x11, 0x11, 0x11, 0x15, 0x12, 0x0D};
      case 'R': return {0x1E, 0x11, 0x11, 0x1E, 0x14, 0x12, 0x11};
      case 'S': return {0x0E, 0x11, 0x10, 0x0E, 0x01, 0x11, 0x0E};
      case 'T': return {0x1F, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
      case 'U': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x11, 0x0E};
      case 'V': return {0x11, 0x11, 0x11, 0x11, 0x11, 0x0A, 0x04};
      case 'W': return {0x11, 0x11, 0x11, 0x15, 0x15, 0x1B, 0x11};
      case 'X': return {0x11, 0x11, 0x0A, 0x04, 0x0A, 0x11, 0x11};
      case 'Y': return {0x11, 0x11, 0x0A, 0x04, 0x04, 0x04, 0x04};
      case 'Z': return {0x1F, 0x01, 0x02, 0x04, 0x08, 0x10, 0x1F};
      case ':': return {0x00, 0x04, 0x04, 0x00, 0x04, 0x04, 0x00};
      case '.': return {0x00, 0x00, 0x00, 0x00, 0x00, 0x0C, 0x0C};
      case '/': return {0x01, 0x01, 0x02, 0x04, 0x08, 0x10, 0x10};
      case '|': return {0x04, 0x04, 0x04, 0x04, 0x04, 0x04, 0x04};
      case '(': return {0x02, 0x04, 0x08, 0x08, 0x08, 0x04, 0x02};
      case ')': return {0x08, 0x04, 0x02, 0x02, 0x02, 0x04, 0x08};
      case '+': return {0x00, 0x04, 0x04, 0x1F, 0x04, 0x04, 0x00};
      case '-': return {0x00, 0x00, 0x00, 0x1F, 0x00, 0x00, 0x00};
      case '%': return {0x18, 0x19, 0x02, 0x04, 0x08, 0x13, 0x03};
      default:  return {0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
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
    srv_range.RegisterSpace = 0;
    srv_range.OffsetInDescriptorsFromTableStart = 0;

    D3D12_ROOT_PARAMETER params[2]{};
    params[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
    params[0].DescriptorTable.NumDescriptorRanges = 1;
    params[0].DescriptorTable.pDescriptorRanges = &srv_range;
    params[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;

    params[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;
    params[1].Constants.ShaderRegister = 0;
    params[1].Constants.RegisterSpace = 0;
    params[1].Constants.Num32BitValues = 2;
    params[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX;

    D3D12_STATIC_SAMPLER_DESC sampler{};
    sampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
    sampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
    sampler.MaxAnisotropy = 1;
    sampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
    sampler.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;
    sampler.ShaderRegister = 0;
    sampler.RegisterSpace = 0;
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
      REXLOG_ERROR("DWG2 FpsOverlay: failed to create root signature");
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
      REXLOG_ERROR("DWG2 FpsOverlay: failed to create PSO");
      failed_ = true;
      return false;
    }

    D3D12_DESCRIPTOR_HEAP_DESC heap_desc{};
    heap_desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    heap_desc.NumDescriptors = 1;
    heap_desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    if (FAILED(device->CreateDescriptorHeap(&heap_desc, IID_PPV_ARGS(&srv_heap_)))) {
      REXLOG_ERROR("DWG2 FpsOverlay: failed to create SRV heap");
      failed_ = true;
      return false;
    }

    D3D12_RESOURCE_DESC tex_desc{};
    tex_desc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    tex_desc.Width = kAtlasWidth;
    tex_desc.Height = kAtlasHeight;
    tex_desc.DepthOrArraySize = 1;
    tex_desc.MipLevels = 1;
    tex_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    tex_desc.SampleDesc.Count = 1;
    if (FAILED(device->CreateCommittedResource(
            &rex::ui::d3d12::util::kHeapPropertiesDefault,
            provider.GetHeapFlagCreateNotZeroed(), &tex_desc,
            D3D12_RESOURCE_STATE_COPY_DEST, nullptr, IID_PPV_ARGS(&font_texture_)))) {
      REXLOG_ERROR("DWG2 FpsOverlay: failed to create font texture");
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
            D3D12_RESOURCE_STATE_GENERIC_READ, nullptr, IID_PPV_ARGS(&font_upload_buffer_)))) {
      REXLOG_ERROR("DWG2 FpsOverlay: failed to create font upload buffer");
      failed_ = true;
      return false;
    }

    std::vector<uint32_t> atlas(kAtlasWidth * kAtlasHeight, 0u);
    for (int idx = 0; idx < 95; ++idx) {
      const char ch = char(0x20 + idx);
      const auto rows = GlyphRows(ch);
      const int col_idx = idx % 16;
      const int row_idx = idx / 16;
      for (int r = 0; r < 7; ++r) {
        for (int c = 0; c < 5; ++c) {
          if (rows[r] & (1u << (4 - c))) {
            atlas[size_t(row_idx * 8 + r) * kAtlasWidth + size_t(col_idx * 8 + 1 + c)] = 0xFFFFFFFFu;
          }
        }
      }
    }
    // 4x4 solid white block for backdrop/border quads at (1..4, 57..60).
    for (int y = 57; y <= 60; ++y) {
      for (int x = 1; x <= 4; ++x) {
        atlas[size_t(y) * kAtlasWidth + size_t(x)] = 0xFFFFFFFFu;
      }
    }

    void* mapped = nullptr;
    D3D12_RANGE read_none{0, 0};
    if (FAILED(font_upload_buffer_->Map(0, &read_none, &mapped))) {
      failed_ = true;
      return false;
    }
    for (uint32_t y = 0; y < kAtlasHeight; ++y) {
      auto* dst_row = reinterpret_cast<uint8_t*>(mapped) + footprint.Offset +
                      size_t(footprint.Footprint.RowPitch) * y;
      std::memcpy(dst_row, &atlas[size_t(y) * kAtlasWidth], kAtlasWidth * sizeof(uint32_t));
    }
    font_upload_buffer_->Unmap(0, nullptr);

    D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc{};
    srv_desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    srv_desc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srv_desc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srv_desc.Texture2D.MipLevels = 1;
    device->CreateShaderResourceView(font_texture_.Get(), &srv_desc,
                                     srv_heap_->GetCPUDescriptorHandleForHeapStart());

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

    initialized_ = true;
    return true;
  }

  bool initialized_ = false;
  bool failed_ = false;
  bool font_uploaded_ = false;
  UINT64 font_upload_submission_ = 0;
  Microsoft::WRL::ComPtr<ID3D12RootSignature> root_signature_;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> pso_;
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> srv_heap_;
  Microsoft::WRL::ComPtr<ID3D12Resource> font_texture_;
  Microsoft::WRL::ComPtr<ID3D12Resource> font_upload_buffer_;
  std::array<Microsoft::WRL::ComPtr<ID3D12Resource>, kVertexBufferSlots> vertex_buffers_;
  std::array<void*, kVertexBufferSlots> vertex_mapped_{};
};

struct OverlaySlot {
  const void* key = nullptr;
  std::unique_ptr<FpsOverlayRenderer> renderer;
};
inline std::mutex& OverlayMutex() {
  static std::mutex m;
  return m;
}
inline std::array<OverlaySlot, 4>& OverlaySlots() {
  static std::array<OverlaySlot, 4> slots;
  return slots;
}

inline void DrawFpsOverlay(const void* owner_key,
                           const rex::ui::d3d12::D3D12Provider& provider,
                           ID3D12GraphicsCommandList* command_list,
                           D3D12_CPU_DESCRIPTOR_HANDLE rtv_handle,
                           uint32_t width, uint32_t height,
                           UINT64 current_submission, UINT64 completed_submission,
                           DXGI_FORMAT rtv_format,
                           const void* vs_code, size_t vs_size,
                           const void* ps_code, size_t ps_size,
                           const char* text) {
  std::lock_guard<std::mutex> lock(OverlayMutex());
  auto& slots = OverlaySlots();
  OverlaySlot* free_slot = nullptr;
  for (auto& s : slots) {
    if (s.key == owner_key) {
      s.renderer->Draw(provider, command_list, rtv_handle, width, height,
                       current_submission, completed_submission, rtv_format,
                       vs_code, vs_size, ps_code, ps_size, text);
      return;
    }
    if (!s.key && !free_slot) free_slot = &s;
  }
  if (!free_slot) return;
  free_slot->key = owner_key;
  free_slot->renderer = std::make_unique<FpsOverlayRenderer>();
  free_slot->renderer->Draw(provider, command_list, rtv_handle, width, height,
                            current_submission, completed_submission, rtv_format,
                            vs_code, vs_size, ps_code, ps_size, text);
}

inline void DestroyFpsOverlay(const void* owner_key) {
  std::lock_guard<std::mutex> lock(OverlayMutex());
  for (auto& s : OverlaySlots()) {
    if (s.key == owner_key) {
      s.renderer.reset();
      s.key = nullptr;
    }
  }
}

}  // namespace dwg2::overlay
