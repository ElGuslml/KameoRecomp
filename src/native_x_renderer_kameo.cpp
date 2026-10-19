// kameo - NativeXRenderer for Kameo: the game's D3D entry points and its D3D device and object layouts.
//
// The XDK Direct3D library is linked into default.xex (XDK 2.0.2099) at about 0x820BFAA8-0x820DAB50, the draws
// included, with link-time code generation: several calls are inlined into the engine (EndVertices, the typed
// Lock/Unlock thunks, parts of SetVertexShader and SetPixelShader) or specialized for their callers' constant
// arguments. Every address and offset here, with its evidence, is in docs/native-x-renderer-kameo.md. The hooks
// only run the original function unless a NativeXRenderer feature is on (--native_x_renderer_trace,
// --native_x_renderer_verify, --native_x_renderer).

#include "native_x_renderer_kameo.h"

#include <native_x_renderer/hooks.h>

// Draws. The engine calls BeginVertices / BeginIndexedVertices itself, fills the vertices (and indices), then
// commits the packets with an inlined EndVertices ([dev+0] = [dev+13620], 43 sites): there is no EndVertices
// function. The tessellated indexed draw sub_820CF228 (one engine site) has no call of its own yet.
NATIVE_X_RENDERER_HOOK(820CEF88, DrawIndexedVertices)
NATIVE_X_RENDERER_HOOK(820CED48, DrawVertices)
NATIVE_X_RENDERER_HOOK(820CE9D8, DrawVerticesUP)  // calls BeginVertices, inlines EndVertices
NATIVE_X_RENDERER_HOOK(820CE738, BeginVertices)
NATIVE_X_RENDERER_HOOK(820CEA50, BeginIndexedVertices)

// Render targets, clears, resolves, tiling. EndTiling uses only the device in this XDK (it never resolves);
// Clear always passes stencil 0 to ClearF, and BeginTiling always clears with Z 1.0 and stencil 0.
NATIVE_X_RENDERER_HOOK(820C27E0, SetRenderTarget)
NATIVE_X_RENDERER_HOOK(820C2B78, SetDepthStencilSurface)
NATIVE_X_RENDERER_HOOK(820C4E70, Clear)  // calls ClearF
NATIVE_X_RENDERER_HOOK(820C4F20, ClearF)
NATIVE_X_RENDERER_HOOK(820C3460, Resolve)
NATIVE_X_RENDERER_HOOK(820C41C0, BeginTiling)
NATIVE_X_RENDERER_HOOK(820C44E8, EndTiling)
NATIVE_X_RENDERER_HOOK(820C40F0, SetPredication)
NATIVE_X_RENDERER_HOOK(820C24D0, SetViewport)
NATIVE_X_RENDERER_HOOK(820C2408, SetScissorRect)

// Shaders, vertex input, textures, constants. sub_820C8810 is the active vertex shader setter (dev, shader),
// which both the XDK's SetVertexShader (sub_820C8968) and the engine's inlined copies call: hooked, it sees every
// bind. SetPixelShader is also partly inlined in 11 engine functions. The engine writes constants straight into
// the device too (VS in 7 functions, PS in 30), without a call.
NATIVE_X_RENDERER_HOOK(820C8810, SetVertexShader)
NATIVE_X_RENDERER_HOOK(820C84B8, SetPixelShader)
NATIVE_X_RENDERER_HOOK(820C89D0, SetVertexDeclaration)
NATIVE_X_RENDERER_HOOK(820C26C0, SetStreamSource)
NATIVE_X_RENDERER_HOOK(820C2760, SetIndices)
NATIVE_X_RENDERER_HOOK(820C76F0, SetTexture)
NATIVE_X_RENDERER_HOOK(820C7DC0, SetVertexShaderConstantF)
NATIVE_X_RENDERER_HOOK(820C7F08, SetPixelShaderConstantF)

// Resources. The engine inlines the typed Lock/Unlock thunks (it calls the common Lock sub_820D2E10, the common
// Unlock sub_820D30F8 or FlushCpuRange directly); these remain as functions. sub_820C7628 is likely a surface's
// UnlockRect (used by the video helpers).
NATIVE_X_RENDERER_HOOK(820D3508, VertexBufferLock)
NATIVE_X_RENDERER_HOOK(820C7628, TextureUnlock)
NATIVE_X_RENDERER_HOOK(820D6908, FlushCpuRange)

// Command buffer, queries, frame. The kick submits the segment and continues in the same memory; NewSegment
// moves the write pointer to a newly allocated segment. Present (sub_820CF9D8) calls VdSwap with the front buffer
// in r4 and ends the frame; the engine reaches it through the XDK's present helper sub_820D0048 (SetRenderTarget,
// the vblank synchronize sub_820CF868, a resolve into the front buffer, Present).
NATIVE_X_RENDERER_HOOK(820CDC88, KickSegment)
NATIVE_X_RENDERER_HOOK(820CD458, NewSegment)
NATIVE_X_RENDERER_HOOK(820C8B70, SetShaderGPRAllocation)
NATIVE_X_RENDERER_HOOK(820D07E0, QueryIssue)
NATIVE_X_RENDERER_HOOK(820D0968, QueryGetData)
NATIVE_X_RENDERER_HOOK(820CF9D8, Present)

namespace kameo {

const native_x_renderer::Profile& NativeXRendererProfile() {
  static const native_x_renderer::Profile profile = [] {
    native_x_renderer::Profile p;
    p.game = "Kameo: Elements of Power (4D5307D2)";
    p.xdk_version = "2.0.2099";
    p.output_width = 1280;  // the scene and its resolves are 1280x720 (the trace's resolve rectangles)
    p.output_height = 720;
    // The finished scene, before the HUD: the 1280x720 resolve of the bloom composite into 0x82758880, which the
    // last post-processing pass draws onto the screen with the glows before the HUD. Every post-processing pass
    // resolves through the helper library's full-screen pass (return address 0x820DC4B0), called by the
    // post-processing driver (0x8227AE6C); the first pass (the scene with its fog) matches too, so the
    // antialiasing runs twice in play. None of the menus' or videos' resolves come from there.
    p.scene_resolve_caller = 0x820DC4B0;
    p.scene_resolve_outer_caller = 0x8227AE6C;
    native_x_renderer::DeviceLayout& d = p.device;
    // GRAW's (XDK 2417) offsets up to +12964: five dirty masks from +16 (VS constants, PS constants, then fetch
    // slots in the high half, slot 0 in bit 63, bool/loop constants and the 0x2000 group); the register groups;
    // the misc state. The tiling block is smaller than GRAW's (no saved targets).
    d.vs_constants_dirty = 16;
    d.ps_constants_dirty = 24;
    d.fetch_constants_dirty = 32;
    d.fetch_dirty_slot0_bit = 63;
    d.command_buffer_write = 0;
    d.fetch_constants = 1152;
    d.vs_float_constants = 1920;
    d.ps_float_constants = 6016;
    d.bool_loop_constants = 10112;
    d.register_groups = {{
        {11456, 0x2000, 16},
        {11532, 0x2100, 21},
        {11616, 0x2180, 5},
        {11636, 0x2200, 12},
        {11684, 0x2280, 21},
        {11768, 0x2300, 38},
        {11920, 0x2380, 8},
    }};
    d.fence_counter = 10396;
    d.write_back_block = 10384;
    d.flags = 10432;
    d.vertex_declaration = 11408;
    d.index_buffer = 12532;
    d.render_targets = 12536;
    d.depth_stencil = 12552;
    d.vertex_buffers = 12556;  // {offset, buffer} per stream, 8 bytes each
    d.textures = 12704;
    d.viewport = 12808;  // x, y, width, height as integers, then min z, max z as floats
    d.pixel_shader = 12944;
    d.vertex_shader = 12948;
    d.predication_mask = 12960;
    d.tile_count = 12972;  // BeginTiling 0x820C41E8 (GRAW +12996)
    d.tile_rects = 12976;  // BeginTiling 0x820C422C (GRAW +13000)
    // The engine commits BeginVertices' packets itself ([dev+0] = [dev+13620], an inlined EndVertices).
    d.inline_draw_end = 13620;
    native_x_renderer::ObjectLayout& o = p.objects;
    o.texture_fetch = 16;
    o.surface_info = 48;
    o.surface_color_depth_info = 52;
    o.surface_size = 24;  // as a 2D texture's fetch dword 2
    o.surface_width_minus_one = {0, 13};
    o.surface_height_minus_one = {13, 13};
    o.buffer_address = 12;
    o.buffer_size = 16;
    return p;
  }();
  return profile;
}

}  // namespace kameo
