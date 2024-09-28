
#include <gui/popups/about_popup.h>
#include <gui/gui.h>
#include <gui/controls.h>
#include <menu/macros.h>
#include <gui/popups/public_software_popup.h>

using namespace evo::gui;
using namespace evo::ren;
using namespace gui::popups;

void about_popup::on_first_render_call()
{
	const auto test_username = [](const std::tuple<std::string, std::string>& t) {
		const auto& [user, _] = t;
		return user == ctx->user.username;
	};
	
	const auto credits_check = std::find_if(credits.begin(), credits.end(), test_username) != credits.end();
	const auto special_thanks_check = std::find_if(special_thanks.begin(), special_thanks.end(), test_username) != special_thanks.end();
	
	if (!credits_check && !special_thanks_check)
		special_thanks.emplace_back(ctx->user.username, XOR("Supporting ev0lve"));
	
	size = { 260.f, 220.f + credits.size() * 20.f + special_thanks.size() * 20.f };
	pos = adapter->display * 0.5f - size * 0.5f;
	
	popup::on_first_render_call();
	
	const auto pub_soft = MAKE("ev0lve.about.ps", button, XOR("Public Software"));
	pub_soft->pos = { size.x * 0.5f - pub_soft->size.x - 2.f, size.y - 40.f };
	pub_soft->callback = [&]() {
		const auto ps_popup = MAKE("ev0lve.ps", gui::popups::public_software_popup);
		ps_popup->open();
	};
	
	const auto close_btn = MAKE("ev0lve.about.close", button, XOR("Close"));
	close_btn->pos = { size.x * 0.5f + 2.f, size.y - 40.f };
	close_btn->callback = [&]() {
		close();
	};
	
	add(pub_soft);
	add(close_btn);
}

void about_popup::render()
{
	if (!is_visible)
		return;
	
	begin_render();
	
	const auto area = area_abs();
	const auto header = area_abs().height(24.f);
	const auto fnt = draw.get_font(GUI_HASH("gui_main"));
	const auto fnt_bold = draw.get_font(GUI_HASH("gui_bold"));
	
	const auto i_rect = rect(area.tl() + vec2{ 10.f, 30.f }).size({50.f, 50.f}).shrink(4.f);
	
	auto& d = draw.get_layer(ctx->content_layer);
	d.add_shadow(area, 12.f, 0.25f, true);
	d.push_aa(true);
	d.add_rect_filled_rounded(area, colors.bg_bottom, 4.f);
	d.add_rect_filled_rounded(header, colors.bg_block, 4.f, layer::rnd_t);
	d.pop_aa();
	
	d.add_text(fnt_bold, header.center(), XOR("About"), colors.text, layer::align_center, layer::align_center);
	d.add_line(header.bl(), header.br(), colors.accent);
	
	// head
	d.push_texture(ctx->res.general.logo_head);
	d.add_rect_filled(i_rect, color::white());
	d.pop_texture();
	
	// stripes
	d.push_texture(ctx->res.general.logo_stripes);
	d.add_rect_filled(i_rect, colors.accent);
	d.pop_texture();
	
	// ev0lve Digital
	const auto ev0_size = text_size(fnt_bold, XOR("ev0"));
	const auto txt_pos = i_rect.tr() + vec2{ 10.f, 5.f };
	
	const auto time = std::time(nullptr);
	
	std::tm tm{};
	localtime_s(&tm, &time);
	
	d.add_text(fnt_bold, txt_pos, XOR("ev0"), colors.accent);
	d.add_text(fnt_bold, txt_pos + vec2{ ev0_size.x, 0.f }, XOR("lve Digital"), colors.text);
	d.add_text(fnt, txt_pos + vec2{ 0.f, 16.f }, tfm::format(XOR("© 2015-%i. All rights reserved."), tm.tm_year + 1900), colors.text_mid);
	
	// separator
	d.add_line(i_rect.bl() - vec2{ 14.f, -10.f}, i_rect.bl() + vec2{ size.x - 14.f, 10.f }, colors.outline);
	
	// credits
	auto offset = i_rect.bl() + vec2{ 6.f, 20.f };
	d.add_text(fnt_bold, vec2{ area.tl().x + size.x * 0.5f, offset.y }, XOR("Credits"), colors.text, layer::align_center);
	
	for (const auto& [user, desc] : credits)
	{
		offset.y += 20.f;
		d.add_text(fnt, offset, user, colors.text);
		d.add_text(fnt, offset + vec2{ size.x - 40.f, 0.f }, desc, colors.text_mid, layer::align_right);
	}
	
	d.add_line(offset - vec2{ 20.f, -30.f }, offset + vec2{ size.x - 20.f, 30.f }, colors.outline);
	offset.y += 40.f;
	
	// special thanks
	d.add_text(fnt_bold, vec2{ area.tl().x + size.x * 0.5f, offset.y }, XOR("Special Thanks"), colors.text, layer::align_center);
	
	for (const auto& [user, desc] : special_thanks)
	{
		offset.y += 20.f;
		d.add_text(fnt, offset, user, colors.text);
		d.add_text(fnt, offset + vec2{ size.x - 40.f, 0.f }, desc, colors.text_mid, layer::align_right);
	}
	
	d.add_line(offset - vec2{ 20.f, -30.f }, offset + vec2{ size.x - 20.f, 30.f }, colors.outline);
	offset.y += 40.f;
	
	popup::render();
}