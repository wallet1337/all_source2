
#include <gui/public_software_item.h>
#include <gui/gui.h>
#include <shellapi.h>

using namespace evo::gui;
using namespace evo::ren;
using namespace gui;

void public_software_item::init()
{
	draw.manage(id, std::make_shared<anim_color>(colors.text, 0.15f));
}

void public_software_item::render()
{
	const auto r = area_abs();
	
	auto& d = draw.get_layer(ctx->content_layer);
	d.add_text(draw.get_font(GUI_HASH("gui_bold")), r.tl() + vec2{ 20.f, 5.f }, name, draw.get_anim<anim_color>(id)->value);
	d.add_text(draw.get_font(GUI_HASH("gui_main")), r.tl() + vec2{ 20.f, 20.f }, license, colors.text_mid);
}

void public_software_item::on_mouse_enter()
{
	draw.get_anim<anim_color>(id)->direct(colors.accent);
}

void public_software_item::on_mouse_leave()
{
	draw.get_anim<anim_color>(id)->direct(colors.text);
}

void public_software_item::on_mouse_down(char key)
{
	ctx->do_tick_sound();
	if (key == m_left)
		std::thread([&]{ ShellExecuteA(nullptr, nullptr, url.c_str(), nullptr, nullptr, SW_SHOWNORMAL); }).detach();
}
