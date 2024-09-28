
#include <gui/popups/public_software_popup.h>
#include <gui/gui.h>
#include <gui/controls.h>
#include <menu/macros.h>
#include <gui/public_software_item.h>

using namespace evo::gui;
using namespace evo::ren;
using namespace gui::popups;

void public_software_popup::on_first_render_call()
{
	size = { 260.f, 80.f + links.size() * 40.f };
	pos = adapter->display * 0.5f - size * 0.5f;
	
	popup::on_first_render_call();
	
	auto offset = 30.f;
	for (const auto& [name, license, url] : links)
	{
		const auto ctrl = MAKE_RUNTIME("ev0lve.ps." + name, gui::public_software_item, name, license, url);
		ctrl->pos = vec2 { 0.f, offset };
		add(ctrl);
		
		offset += 40.f;
	}
	
	const auto close_btn = MAKE("ev0lve.ps.close", button, XOR("Close"));
	close_btn->pos = { size.x * 0.5f - close_btn->size.x * 0.5f, size.y - 40.f };
	close_btn->callback = [&]() {
		close();
	};
	
	add(close_btn);
}

void public_software_popup::render()
{
	if (!is_visible)
		return;
	
	begin_render();
	
	const auto area = area_abs();
	const auto header = area_abs().height(24.f);
	const auto fnt = draw.get_font(GUI_HASH("gui_main"));
	const auto fnt_bold = draw.get_font(GUI_HASH("gui_bold"));
	
	auto& d = draw.get_layer(ctx->content_layer);
	d.add_shadow(area, 12.f, 0.25f, true);
	d.push_aa(true);
	d.add_rect_filled_rounded(area, colors.bg_bottom, 4.f);
	d.add_rect_filled_rounded(header, colors.bg_block, 4.f, layer::rnd_t);
	d.pop_aa();
	
	d.add_text(fnt_bold, header.center(), XOR("Public Software"), colors.text, layer::align_center, layer::align_center);
	d.add_line(header.bl(), header.br(), colors.accent);
	
	popup::render();
}