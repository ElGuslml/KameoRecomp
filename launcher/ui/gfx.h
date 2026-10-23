// The launcher's Direct3D 11 device and swap chain, and the textures its pictures use. Dear ImGui draws
// through its DX11 backend: the SDK builds SDL3 without SDL_render, and D3D11 is part of Windows (and of
// Proton, through DXVK, on the Steam Deck).

#pragma once

#include <imgui.h>

#include <cstdint>

struct ID3D11Device;
struct ID3D11DeviceContext;

namespace ui::gfx {

// Creates the device and a flip-model swap chain for the window. False if Direct3D 11 is not available.
bool Init(void* hwnd);
void Shutdown();
ID3D11Device* Device();
ID3D11DeviceContext* Context();

// After the window's size in pixels changed.
void Resize(int width, int height);

// Clears the back buffer and binds it, then presents what ImGui drew (on the next vblank).
void BeginFrame(ImU32 clear_color);
void Present();

// RGBA pixels (straight alpha) as a texture ImGui can draw; 0 if it could not be created.
ImTextureID CreateTexture(const uint8_t* rgba, int width, int height);
void DestroyTexture(ImTextureID texture);

}  // namespace ui::gfx
