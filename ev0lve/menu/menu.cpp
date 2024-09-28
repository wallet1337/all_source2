
#include <gui/helpers.h>
#include <gui/values.h>

#include <menu/macros.h>
#include <menu/menu.h>
#include <base/cfg.h>
#include <base/game.h>
#include <sdk/input_system.h>
#include <hacks/skinchanger.h>

#ifdef CSGO_SECURE
#include <network/helpers.h>
#include <VirtualizerSDK.h>
#endif

#include <resources/icon_up.h>
#include <resources/icon_down.h>
#include <resources/icon_clear.h>
#include <resources/icon_rage.h>
#include <resources/icon_legit.h>
#include <resources/icon_visuals.h>
#include <resources/icon_misc.h>
#include <resources/icon_scripts.h>
#include <resources/icon_copy.h>
#include <resources/icon_paste.h>
#include <resources/icon_cloud.h>
#include <resources/icon_file.h>
#include <resources/icon_skins.h>
#include <resources/icon_alert.h>
#include <resources/logo_head.h>
#include <resources/logo_stripes.h>
#include <resources/pattern.h>
#include <resources/pattern_group.h>
#include <resources/cursor.h>

#ifdef CSGO_LUA
#include <lua/engine.h>
#endif

using namespace menu;
using namespace evo;
using namespace gui;
using namespace ren;
using namespace helpers;

namespace menu::tools
{
	std::shared_ptr<control> make_stacked_groupboxes(uint32_t id, const vec2& size, const std::vector<std::shared_ptr<control>>& groups)
	{
		const auto l = std::make_shared<layout>(id, vec2(), size, s_vertical);
		l->make_not_clip()->margin = rect();

		std::shared_ptr<control> last{};
		for (const auto& c : groups)
		{
			c->margin = rect(0.f, 0.f, 0.f, 10.f);
			l->add(c);
			last = c;
		}

		last->size.y -= 2.f;
		last->margin = rect();
		return l;
	}
}

void menu_manager::create()
{
#ifdef CSGO_SECURE
	VIRTUALIZER_SHARK_WHITE_START;
#endif
	
	// gui resources
	ctx->res.general.logo_head = std::make_shared<texture>(data::logo_head, sizeof(data::logo_head));
	ctx->res.general.logo_stripes = std::make_shared<texture>(data::logo_stripes, sizeof(data::logo_stripes));
	ctx->res.general.pattern = std::make_shared<texture>(data::pattern, sizeof(data::pattern));
	ctx->res.general.pattern_group = std::make_shared<texture>(data::pattern_group, sizeof(data::pattern_group));
	ctx->res.general.cursor = std::make_shared<texture>(data::cursor, sizeof(data::cursor));

	draw.manage(GUI_HASH("gui_logo_head"), ctx->res.general.logo_head);
	draw.manage(GUI_HASH("gui_logo_stripes"), ctx->res.general.logo_stripes);
	draw.manage(GUI_HASH("gui_pattern"), ctx->res.general.pattern);
	draw.manage(GUI_HASH("gui_pattern_group"), ctx->res.general.pattern_group);
	draw.manage(GUI_HASH("gui_cursor"), ctx->res.general.cursor);

	ctx->res.icons.up = std::make_shared<texture>(data::icon_up, sizeof(data::icon_up));
	ctx->res.icons.down = std::make_shared<texture>(data::icon_down, sizeof(data::icon_down));
	ctx->res.icons.notify_clear = std::make_shared<texture>(data::icon_clear, sizeof(data::icon_clear));
	ctx->res.icons.copy = std::make_shared<texture>(data::icon_copy, sizeof(data::icon_copy));
	ctx->res.icons.paste = std::make_shared<texture>(data::icon_paste, sizeof(data::icon_paste));
	ctx->res.icons.alert = std::make_shared<texture>(data::icon_alert, sizeof(data::icon_alert));

	draw.manage(GUI_HASH("gui_icon_up"), ctx->res.icons.up);
	draw.manage(GUI_HASH("gui_icon_down"), ctx->res.icons.down);
	draw.manage(GUI_HASH("gui_icon_clear"), ctx->res.icons.notify_clear);
	draw.manage(GUI_HASH("gui_icon_copy"), ctx->res.icons.copy);
	draw.manage(GUI_HASH("gui_icon_paste"), ctx->res.icons.paste);
	draw.manage(GUI_HASH("gui_icon_alert"), ctx->res.icons.alert);

	// menu resources
	draw.manage(GUI_HASH("icon_rage"), std::make_shared<texture>(data::icon_rage, sizeof(data::icon_rage)));
	draw.manage(GUI_HASH("icon_legit"), std::make_shared<texture>(data::icon_legit, sizeof(data::icon_legit)));
	draw.manage(GUI_HASH("icon_visuals"), std::make_shared<texture>(data::icon_visuals, sizeof(data::icon_visuals)));
	draw.manage(GUI_HASH("icon_misc"), std::make_shared<texture>(data::icon_misc, sizeof(data::icon_misc)));
	draw.manage(GUI_HASH("icon_scripts"), std::make_shared<texture>(data::icon_scripts, sizeof(data::icon_scripts)));
	draw.manage(GUI_HASH("icon_skins"), std::make_shared<texture>(data::icon_skins, sizeof(data::icon_skins)));

	// hack resources
	//draw.manage(GUI_HASH("icon_wep_general"), create_in_game_weapon_texture(XOR("glock")));
	draw.manage(GUI_HASH("icon_cloud"), std::make_shared<texture>(data::icon_cloud, sizeof(data::icon_cloud)));
	draw.manage(GUI_HASH("icon_file"), std::make_shared<texture>(data::icon_file, sizeof(data::icon_file)));

	ctx->tick_sound_callback = []()
	{
		if (!cfg.misc.enable_tick_sound)
			return;
		
		EMIT_TICK_SOUND();
	};
	
	// user
	ctx->user.username = XOR("developer");
	ctx->user.active_until = XOR("Never");

#ifdef CSGO_SECURE
	const auto ud = network::get_user_data();
	ctx->user.username = ud.username;
	ctx->user.active_until = ud.expiry;
	
	if (ud.avatar_size)
	{
		auto dec_avatar = network::get_decoded_avatar();
		ctx->user.avatar = std::make_shared<texture>(reinterpret_cast<void*>(dec_avatar.data()), ud.avatar_size);
		
		draw.manage(GUI_HASH("user_avatar"), ctx->user.avatar);
	}
#endif

	const auto window_sz = vec2{800.f, 500.f};
	const auto group_sz = window_size_to_groupbox_size(window_sz.x, window_sz.y);
	const auto group_half_sz = group_sz * vec2{1.f, .5f} - vec2{0.f, 5.f};

	const auto wnd = make_window(XOR("wnd_main"), {50.f, 50.f}, window_sz, [](std::shared_ptr<layout>& e) {
		e->add(MAKE_TAB("rage", "RAGE")->make_selected());
		e->add(MAKE_TAB("visuals", "VISUALS"));
		e->add(MAKE_TAB("misc", "MISC"));
        e->add(MAKE_TAB("skins", "SKINS"));
#ifdef CSGO_LUA
		e->add(MAKE_TAB("scripts", "SCRIPTS"));
#endif
	}, [&](std::shared_ptr<layout>& e) {
		e->add(make_tab_area(GUI_HASH("rage_container"), true, s_none, group::rage_tab));

		e->add(make_tab_area(GUI_HASH("visuals_container"), false, [&](std::shared_ptr<layout>& e) {
			e->add(make_groupbox(XOR("visuals.players"), XOR("Players"), group_sz, group::visuals_players));
			e->add(make_groupbox(XOR("visuals.local"), XOR("Local"), group_sz, group::visuals_local));
			e->add(tools::make_stacked_groupboxes(GUI_HASH("visuals.world_removals"), group_sz, {
				make_groupbox(XOR("visuals.world"), XOR("World"), group_half_sz, group::visuals_world),
				make_groupbox(XOR("visuals.removals"), XOR("Removals"), group_half_sz, group::visuals_removals),
			}));
		}));

		e->add(make_tab_area(GUI_HASH("misc_container"), false, [&](std::shared_ptr<layout>& e) {
			e->add(make_groupbox(XOR("misc.general"), XOR("General"), group_sz, group::misc_general));
			e->add(tools::make_stacked_groupboxes(GUI_HASH("misc.settings_helpers"), group_sz, {
					make_groupbox(XOR("misc.settings"), XOR("Settings"), group_half_sz, group::misc_settings),
					make_groupbox(XOR("misc.helpers"), XOR("Helpers"), group_half_sz, group::misc_helpers),
			}));
			e->add(make_groupbox(XOR("misc.configs"), XOR("Configs"), group_sz, group::misc_configs));
		}));

        e->add(make_tab_area(GUI_HASH("skins_container"), false, [&](std::shared_ptr<layout>& e) {
            e->add(make_groupbox(XOR("skins.select"), XOR("Select"), group_sz, group::skinchanger_select));
            e->add(make_groupbox(XOR("skins.settings"), XOR("Settings"), group_sz, group::skinchanger_settings));
            // TODO: e->add(make_groupbox(XOR("skins.preview"), XOR("Preview"), group_sz, group::skinchanger_preview));
        }));

#ifdef CSGO_LUA
		e->add(make_tab_area(GUI_HASH("scripts_container"), false, [&](std::shared_ptr<layout>& e) {
			e->add(make_groupbox(XOR("scripts.general"), XOR("General"), group_sz, group::scripts_general));
			e->add(make_groupbox(XOR("scripts.elements_a"), XOR("Elements A"), group_sz, group::scripts_elements));
			e->add(make_groupbox(XOR("scripts.elements_b"), XOR("Elements B"), group_sz, group::scripts_elements));
		}));
#endif
	});

	wnd->is_visible = wnd->is_taking_input = ctx->should_render_cursor = false;
	ctx->add(wnd);
	main_wnd = wnd;

#ifdef CSGO_SECURE
	VIRTUALIZER_SHARK_WHITE_END;
#endif
}

void menu_manager::toggle()
{
	if (const auto wnd = main_wnd.lock(); wnd)
	{
		wnd->is_visible = !wnd->is_visible;
		wnd->is_taking_input = wnd->is_visible;

		if (!wnd->is_visible)
		{
			ctx->active = nullptr;
			if (ctx->active_popup)
				ctx->active_popup->close();
		}

		ctx->should_render_cursor = wnd->is_visible;

		game->input_system->enable_input(!wnd->is_visible);
		game->input_system->reset_input_state();
	}
}

void menu_manager::finalize()
{
	if (did_finalize)
		return;

#ifdef CSGO_SECURE
	VIRTUALIZER_SHARK_WHITE_START;
#endif

#ifdef CSGO_LUA
	// refresh available scripts
	ctx->find<button>(GUI_HASH("scripts.general.refresh"))->callback();
#endif

	// load default config
	cfg.load(XOR("ev0lve/default.cfg"), true);

	// refresh available configs
	ctx->find<button>(GUI_HASH("misc.configs.refresh"))->callback();

	hacks::skin_changer->fill_weapon_list();

#ifdef CSGO_LUA
	lua::api.run_autoload();
#endif
	
	did_finalize = true;

#ifdef CSGO_SECURE
	VIRTUALIZER_SHARK_WHITE_END;
#endif
}

bool menu_manager::is_open()
{
	const auto wnd = main_wnd.lock();
	return wnd && wnd->is_visible;
}

std::shared_ptr<evo::ren::texture> menu_manager::create_in_game_texture(const char* name)
{
#if 0
	// load file
	size_t size{};
	const auto file = sdk::load_text_file(name, &size);
	if (!file)
		RUNTIME_ERROR("Texture file not found");

	std::vector<uint32_t> raw_tex(0xFFFFFF);

	// create svg texture
	sdk::image_data img(raw_tex);
	uint32_t w{}, h{};
	if (!img.load_svg(file, size, &w, &h))
		RUNTIME_ERROR("Could not load SVG");

	// create texture
	auto tex = std::make_shared<texture>(raw_tex.data(), w, h, w);
	sdk::mem_alloc->free(file);

	return tex;
#endif
	return nullptr;
}

std::shared_ptr<evo::ren::texture> menu_manager::create_in_game_weapon_texture(const char *name)
{
	static const auto path = XOR_STR_STORE("materials/panorama/images/icons/equipment/%s.svg");

	// build full path
	char p[MAX_PATH];
	XOR_STR_STACK(pa, path);
	sprintf(p, pa, name);

	return create_in_game_texture(p);
}
