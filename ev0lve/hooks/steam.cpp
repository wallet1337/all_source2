
#include <base/hook_manager.h>
#include <menu/menu.h>
#include <detail/dx_adapter.h>
#include <sdk/global_vars_base.h>
#include <sdk/input_system.h>

#include <ren/renderer.h>
#include <ren/adapter_dx9.h>
#include <gui/renderer/bitfont.h>

#ifdef CSGO_LUA
#include <lua/engine.h>
#endif

using namespace evo;
using namespace ren;
using namespace hooks;
using namespace menu;
using namespace detail;

struct dx_backup
{
	explicit dx_backup(IDirect3DDevice9* device)
	{
		device->CreateStateBlock(D3DSBT_PIXELSTATE, &pixel_state);
		pixel_state->Capture();

		device->GetRenderState(D3DRS_SRGBWRITEENABLE, &srgb);
		device->GetRenderState(D3DRS_SRGBWRITEENABLE, &color);
		device->GetFVF(&fvf);

		device->GetStreamSource(0, &stream_data, &stream_offset, &stream_stride);

		device->GetTexture(0, &texture);
		device->GetRenderTarget(0, &target);
		device->GetIndices(&indices);
		device->GetVertexDeclaration(&vertex_declaration);
		device->GetPixelShader(&pixel_shader);
		device->GetVertexShader(&vertex_shader);
	}

	~dx_backup()
	{
		if (stream_data)
			stream_data->Release();
		if (texture)
			texture->Release();
		if (target)
			target->Release();
		if (indices)
			indices->Release();
		if (vertex_declaration)
			vertex_declaration->Release();
		if (pixel_shader)
			pixel_shader->Release();
		if (vertex_shader)
			vertex_shader->Release();
		if (pixel_state)
			pixel_state->Release();
	}

	void apply(IDirect3DDevice9* device)
	{
		device->SetRenderState(D3DRS_SRGBWRITEENABLE, srgb);
		device->SetRenderState(D3DRS_SRGBWRITEENABLE, color);
		device->SetFVF(fvf);

		device->SetStreamSource(0, stream_data, stream_offset, stream_stride);

		device->SetTexture(0, texture);
		device->SetRenderTarget(0, target);
		device->SetIndices(indices);
		device->SetVertexDeclaration(vertex_declaration);
		device->SetPixelShader(pixel_shader);
		device->SetVertexShader(vertex_shader);
		pixel_state->Apply();
	}

	DWORD srgb{}, color{}, fvf{};
	IDirect3DVertexBuffer9* stream_data = nullptr;
	UINT stream_offset{};
	UINT stream_stride{};
	IDirect3DBaseTexture9* texture = nullptr;
	IDirect3DSurface9* target = nullptr;
	IDirect3DIndexBuffer9* indices = nullptr;
	IDirect3DVertexDeclaration9* vertex_declaration = nullptr;
	IDirect3DPixelShader9* pixel_shader = nullptr;
	IDirect3DVertexShader9* vertex_shader = nullptr;
	IDirect3DStateBlock9* pixel_state = nullptr;
};

HRESULT __stdcall steam::present(IDirect3DDevice9* dev, RECT* a, RECT* b, HWND c, RGNDATA* d)
{
	dx_backup bak(dev);

	if (!adapter)
	{
		adapter = std::make_shared<adapter_dx9>(dev, game->input_system->get_attached_window());
		evo::gui::ctx = std::make_unique<evo::gui::context>();

		char win_dir_arr[256]{};
		GetSystemWindowsDirectoryA(win_dir_arr, 256);
		
		std::string win_dir{win_dir_arr};
		draw.manage(FNV1A("segoeui13"), std::make_shared<font>(win_dir + XOR("/fonts/segoeui.ttf"), 13.f, ff_shadow, 0, 0x45F));
		draw.manage(FNV1A("small8"), std::make_shared<::gui::bitfont>( "Small Fonts", 8.f, ff_outline, 0, 0x45F));
		draw.manage(FNV1A("segoeuib13"), std::make_shared<font>(win_dir + XOR("/fonts/segoeuib.ttf"), 13.f, ff_shadow, 0, 0x45F));
		draw.manage(FNV1A("segoeuib28"), std::make_shared<font>(win_dir + XOR("/fonts/segoeuib.ttf"), 28.f, ff_shadow, 0, 0x45F));
		
		menu::menu.create();
		adapter->create_objects();
	}

	draw.set_frame_time(game->globals->frametime);
	adapter->refresh();

	dx_adapter::mtx.lock();
	if (dx_adapter::ready)
	{
		adapter->flush();
		dx_adapter::swap_buffers();
		dx_adapter::ready = false;
	}
	dx_adapter::mtx.unlock();

	evo::gui::ctx->render();
	adapter->render();
	adapter->flush();

	menu::menu.finalize();

	bak.apply(dev);
	return hook_manager.present->call(dev, a, b, c, d);
}

HRESULT __stdcall steam::reset(IDirect3DDevice9* dev, D3DPRESENT_PARAMETERS* pp)
{
	if (!adapter)
		return hook_manager.reset->call(dev, pp);

	adapter->destroy_objects();
	const auto r = hook_manager.reset->call(dev, pp);
	adapter->create_objects();
	return r;
}