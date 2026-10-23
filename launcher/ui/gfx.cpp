// The launcher's Direct3D 11 device (gfx.h).

#include "ui/gfx.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#include <iterator>

namespace ui::gfx {
namespace {

ID3D11Device* g_device = nullptr;
ID3D11DeviceContext* g_context = nullptr;
IDXGISwapChain1* g_swap_chain = nullptr;
ID3D11RenderTargetView* g_target = nullptr;

void CreateTarget() {
  ID3D11Texture2D* back_buffer = nullptr;
  if (SUCCEEDED(g_swap_chain->GetBuffer(0, IID_PPV_ARGS(&back_buffer)))) {
    g_device->CreateRenderTargetView(back_buffer, nullptr, &g_target);
    back_buffer->Release();
  }
}

void ReleaseTarget() {
  if (g_target) {
    g_target->Release();
    g_target = nullptr;
  }
}

}  // namespace

bool Init(void* hwnd) {
  const D3D_FEATURE_LEVEL levels[] = {D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1, D3D_FEATURE_LEVEL_10_0};
  // The hardware device, or WARP (software) where there is none: the launcher draws little.
  HRESULT result = E_FAIL;
  for (D3D_DRIVER_TYPE type : {D3D_DRIVER_TYPE_HARDWARE, D3D_DRIVER_TYPE_WARP}) {
    result = D3D11CreateDevice(nullptr, type, nullptr, 0, levels, UINT(std::size(levels)), D3D11_SDK_VERSION,
                               &g_device, nullptr, &g_context);
    if (SUCCEEDED(result)) {
      break;
    }
  }
  if (FAILED(result)) {
    return false;
  }

  IDXGIDevice* dxgi_device = nullptr;
  IDXGIAdapter* adapter = nullptr;
  IDXGIFactory2* factory = nullptr;
  if (SUCCEEDED(g_device->QueryInterface(IID_PPV_ARGS(&dxgi_device))) &&
      SUCCEEDED(dxgi_device->GetAdapter(&adapter)) && SUCCEEDED(adapter->GetParent(IID_PPV_ARGS(&factory)))) {
    DXGI_SWAP_CHAIN_DESC1 desc = {};
    desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    desc.SampleDesc.Count = 1;
    desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    desc.BufferCount = 2;
    desc.Scaling = DXGI_SCALING_NONE;
    desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    result = factory->CreateSwapChainForHwnd(g_device, HWND(hwnd), &desc, nullptr, nullptr, &g_swap_chain);
    if (FAILED(result)) {
      // Before Windows 10 (no FLIP_DISCARD): the sequential flip model, stretched.
      desc.Scaling = DXGI_SCALING_STRETCH;
      desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL;
      result = factory->CreateSwapChainForHwnd(g_device, HWND(hwnd), &desc, nullptr, nullptr, &g_swap_chain);
    }
    // Alt+Enter is the launcher's own (a borderless fullscreen through SDL), not DXGI's.
    factory->MakeWindowAssociation(HWND(hwnd), DXGI_MWA_NO_ALT_ENTER);
  }
  if (factory) factory->Release();
  if (adapter) adapter->Release();
  if (dxgi_device) dxgi_device->Release();
  if (!g_swap_chain) {
    Shutdown();
    return false;
  }
  CreateTarget();
  return true;
}

void Shutdown() {
  ReleaseTarget();
  if (g_swap_chain) {
    g_swap_chain->Release();
    g_swap_chain = nullptr;
  }
  if (g_context) {
    g_context->Release();
    g_context = nullptr;
  }
  if (g_device) {
    g_device->Release();
    g_device = nullptr;
  }
}

ID3D11Device* Device() { return g_device; }
ID3D11DeviceContext* Context() { return g_context; }

void Resize(int width, int height) {
  if (!g_swap_chain || width <= 0 || height <= 0) {
    return;
  }
  ReleaseTarget();
  g_swap_chain->ResizeBuffers(0, UINT(width), UINT(height), DXGI_FORMAT_UNKNOWN, 0);
  CreateTarget();
}

void BeginFrame(ImU32 clear_color) {
  ImVec4 c = ImGui::ColorConvertU32ToFloat4(clear_color);
  const float rgba[4] = {c.x, c.y, c.z, c.w};
  g_context->OMSetRenderTargets(1, &g_target, nullptr);
  g_context->ClearRenderTargetView(g_target, rgba);
}

void Present() { g_swap_chain->Present(1, 0); }

ImTextureID CreateTexture(const uint8_t* rgba, int width, int height) {
  if (!g_device || width <= 0 || height <= 0) {
    return ImTextureID_Invalid;
  }
  D3D11_TEXTURE2D_DESC desc = {};
  desc.Width = UINT(width);
  desc.Height = UINT(height);
  desc.MipLevels = 1;
  desc.ArraySize = 1;
  desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  desc.SampleDesc.Count = 1;
  desc.Usage = D3D11_USAGE_IMMUTABLE;
  desc.BindFlags = D3D11_BIND_SHADER_RESOURCE;
  D3D11_SUBRESOURCE_DATA data = {rgba, UINT(width) * 4, 0};
  ID3D11Texture2D* texture = nullptr;
  if (FAILED(g_device->CreateTexture2D(&desc, &data, &texture))) {
    return ImTextureID_Invalid;
  }
  ID3D11ShaderResourceView* view = nullptr;
  g_device->CreateShaderResourceView(texture, nullptr, &view);
  texture->Release();
  // The DX11 backend takes the shader resource view as the texture id.
  return ImTextureID(view);
}

void DestroyTexture(ImTextureID texture) {
  if (texture != ImTextureID_Invalid) {
    reinterpret_cast<ID3D11ShaderResourceView*>(texture)->Release();
  }
}

}  // namespace ui::gfx
