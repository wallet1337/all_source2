
#ifdef CSGO_LUA

#include <gui/selectable_script.h>

using namespace evo::gui;
using namespace evo::ren;

void gui::selectable_script::render()
{
	control::render();
	if (!is_visible)
		return;

	const auto r = area_abs();
	const auto a = draw.get_anim<anim_float_color>(id);

	auto& l = draw.get_layer(ctx->content_layer);
	l.add_rect_filled(r, is_odd ? colors.bg_odd : colors.bg_even);
	l.add_rect_filled(r.width(a->value.f), colors.accent);

	std::shared_ptr<texture> tex{};
	switch (file.type)
	{
		case lua::st_script:
			tex = draw.get_texture(GUI_HASH("icon_file"));
			break;
		case lua::st_remote:
			tex = draw.get_texture(GUI_HASH("icon_cloud"));
			break;
		default: break;
	}

	if (tex)
	{
		l.push_texture(tex);
		l.add_rect_filled(rect(r.tl(), r.tl() + vec2{12.f, 12.f}).translate({a->value.f + 4.f, 5.f}), a->value.c);
		l.pop_texture();
	}

	l.add_text(is_highlighted
			   ? draw.get_font(GUI_HASH("gui_bold"))
			   : draw.get_font(GUI_HASH("gui_main")), r.tl() + vec2(a->value.f + (tex ? 20.f : 4.f), 2.f), text, is_loaded ? colors.accent : a->value.c);
}

#endif
