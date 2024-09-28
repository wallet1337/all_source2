#include <lua/engine.h>
#include <lua/helpers.h>
#include <base/event_log.h>
#include <lua/api_def.h>
#include <gui/gui.h>
#include <base/cfg.h>

using namespace evo::ren;

void lua::script::initialize()
{
	// set panic function
	l.set_panic_func(api_def::panic);

	// hide poor globals first
	hide_globals();

	// register global functions
	register_globals();

	// register namespaces
	register_namespaces();

	// register types
	register_types();

	// attempt loading file
	if (!l.load_file(file.c_str()))
	{
		lua::helpers::error(XOR_STR("syntax_error"), l.get_last_error_description().c_str());

		throw std::runtime_error(XOR_STR("Unable to initialize the script.\nSee console for more details."));
	}
}

void lua::script::unload()
{
	using namespace evo::gui;

	// resolve parent and erase element, freeing it
	for (const auto& e : gui_elements)
	{
		// remove from parent
		if (const auto& c = ctx->find(e))
		{
			if (const auto p = c->parent.lock(); p && p->is_container)
				p->as<control_container>()->remove(e);
			else
				ctx->remove(e);
		}

		// remove from hotkey list
		if (ctx->hotkey_list.find(e) != ctx->hotkey_list.end())
			ctx->hotkey_list.erase(e);
	}
	
	// erase callbacks
	for (const auto& e : gui_callbacks)
	{
		if (const auto& c = ctx->find(e))
		{
			if (c->universal_callbacks.find(id) != c->universal_callbacks.end())
				c->universal_callbacks[id].clear();
		}
	}
	
	for (const auto& f : fonts)
	{
		if (draw.has_font(f))
			draw.forget_font(f, true);
	}
	
	for (const auto& t : textures)
	{
		if (draw.has_texture(t))
			draw.forget_texture(t, true);
	}
	
	for (const auto& a : animators)
	{
		if (draw.has_anim(a))
			draw.forget_anim(a);
	}
	
	for (const auto& s : libs)
		s->unlock();
	
	libs.clear();
	fonts.clear();
	textures.clear();
	gui_elements.clear();
}

void lua::script::use_library(uint32_t n)
{
	const auto lib = std::static_pointer_cast<library>(api.find_by_id(n));
	if (!lib)
		return;
	
	lib->lock();
	libs.emplace_back(lib);
	
	// create wrapper and push it to the stack
	std::weak_ptr<state_bus> bus{lib->get_bus()};
	l.create_user_object(XOR_STR("bus"), &bus);
}

void lua::script::call_main()
{
	// taking off!
	if (!l.run())
	{
		lua::helpers::error(XOR_STR("runtime_error"), l.get_last_error_description().c_str());
		unload();
		throw std::runtime_error(XOR_STR("Unable to run the script.       \nSee console for more details."));
	}

	// find all forwards
	find_forwards();
}

void lua::script::find_forwards()
{
	// declare list of all possible forwards
	const std::vector<std::string> forward_list = {
		XOR_STR("on_paint"),
		XOR_STR("on_frame_stage_notify"),
		XOR_STR("on_setup_move"),
		XOR_STR("on_run_command"),
		XOR_STR("on_create_move"),
		XOR_STR("on_level_init"),
		XOR_STR("on_do_post_screen_space_events"),
		XOR_STR("on_input"),
		XOR_STR("on_game_event"),
		XOR_STR("on_shutdown"),
		XOR_STR("on_config_load"),
		XOR_STR("on_config_save"),
		XOR_STR("on_shot_fired")
	};

	// iterate through the list and check if we find any functions
	for (const auto& f : forward_list)
	{
		l.get_global(f.c_str());
		if (l.is_function(-1))
			forwards[util::fnv1a(f.c_str())] = f;

		l.pop(1);
	}
}

void lua::script::create_forward(const char *n)
{
	forwards[util::fnv1a(n)] = n;
}

bool lua::script::has_forward(uint32_t hash)
{
	return forwards.find(hash) != forwards.end();
}

void lua::script::call_forward(uint32_t hash, const std::function<int(state &)> &arg_callback)
{
	// check if forward exists
	if (!is_running || !has_forward(hash))
		return;

	// call the function
	l.get_global(forwards[hash].c_str());
	if (l.is_nil(-1))
	{
		l.pop(1);
		return;
	}
	
	if (!l.call(arg_callback ? arg_callback(l) : 0, 0))
	{
		// print error
		lua::helpers::error(XOR_STR("runtime_error"), l.get_last_error_description().c_str());

		// disable script
		is_running = false;

		unload();
	}
}

void lua::script::hide_globals()
{
	l.unset_global("getfenv");			// allows getting environment table

	if (!cfg.lua.allow_pcall.get())
	{
		l.unset_global("pcall");            // allows raw calling
		l.unset_global("xpcall");            // ^
	}

	if (!cfg.lua.allow_dynamic_load.get())
	{
		l.unset_global("loadfile");            // allows direct file load
		l.unset_global("load");                // allows direct load
		l.unset_global("loadstring");        // allows direct string load
		l.unset_global("dofile");            // same as loadfile but also executes it
	}

	l.unset_global("gcinfo");			// garbage collector info
	l.unset_global("collectgarbage");	// forces GC to collect
	l.unset_global("newproxy");			// proxy functions
	l.unset_global("coroutine");		// allows managing coroutines
	l.unset_global("setfenv");			// allows replacing environment table

	if (!cfg.lua.allow_insecure.get())
	{
		l.unset_global("rawget");            // bypasses metatables
		l.unset_global("rawset");            // ^
		l.unset_global("rawequal");            // ^
	}

	l.unset_global("_G");				// disable global table loop

	l.unset_in_table("ffi", "C");		// useless without load()
	l.unset_in_table("ffi", "load");	// allows loading DLLs
	l.unset_in_table("ffi", "gc");		// forces GC to collect
	l.unset_in_table("ffi", "fill");	// memory setting

	l.unset_in_table("string", "dump");	// useless without load()
}

void lua::script::register_namespaces()
{
	l.create_namespace(XOR_STR("event_log"), {
		{ XOR_STR("add"), api_def::event_log::add },
		{ XOR_STR("output"), api_def::event_log::output },
	});

	l.set_integers_for_table(XOR_STR("event_log"), {
		{ XOR_STR("white"), 1 },
		{ XOR_STR("red"), 2 },
		{ XOR_STR("purple"), 3 },
		{ XOR_STR("green"), 4 },
		{ XOR_STR("light_green"), 5 },
		{ XOR_STR("lime"), 6 },
		{ XOR_STR("light_red"), 7 },
		{ XOR_STR("gray"), 8 },
		{ XOR_STR("light_yellow"), 9 },
		{ XOR_STR("sky"), 10 },
		{ XOR_STR("light_blue"), 11 },
		{ XOR_STR("blue"), 12 },
		{ XOR_STR("gray_blue"), 13 },
		{ XOR_STR("pink"), 14 },
		{ XOR_STR("soft_red"), 15 },
		{ XOR_STR("yellow"), 16 }
	});

	l.create_namespace(XOR_STR("render"), {
		{ XOR_STR("color"), api_def::render::color },
		{ XOR_STR("rect_filled"), api_def::render::rect_filled },
		{ XOR_STR("rect"), api_def::render::rect },
		{ XOR_STR("rect_filled_rounded"), api_def::render::rect_filled_rounded },
		{ XOR_STR("text"), api_def::render::text },
		{ XOR_STR("get_screen_size"), api_def::render::get_screen_size },
		{ XOR_STR("create_font"), api_def::render::create_font },
		{ XOR_STR("create_font_gdi"), api_def::render::create_font_gdi },
		{ XOR_STR("get_text_size"), api_def::render::get_text_size },
		{ XOR_STR("circle"), api_def::render::circle },
		{ XOR_STR("circle_filled"), api_def::render::circle_filled },
		{ XOR_STR("create_texture"), api_def::render::create_texture },
		{ XOR_STR("create_texture_bytes"), api_def::render::create_texture_bytes },
		{ XOR_STR("create_texture_rgba"), api_def::render::create_texture_rgba },
		{ XOR_STR("push_texture"), api_def::render::push_texture },
		{ XOR_STR("pop_texture"), api_def::render::pop_texture },
		{ XOR_STR("get_texture_size"), api_def::render::get_texture_size },
		{ XOR_STR("create_animator_float"), api_def::render::create_animator_float },
		{ XOR_STR("create_animator_color"), api_def::render::create_animator_color },
		{ XOR_STR("push_clip_rect"), api_def::render::push_clip_rect },
		{ XOR_STR("pop_clip_rect"), api_def::render::pop_clip_rect },
		{ XOR_STR("push_uv"), api_def::render::push_uv },
		{ XOR_STR("pop_uv"), api_def::render::pop_uv },
		{ XOR_STR("rect_filled_multicolor"), api_def::render::rect_filled_multicolor },
		{ XOR_STR("line"), api_def::render::line },
		{ XOR_STR("line_multicolor"), api_def::render::line_multicolor },
		{ XOR_STR("triangle"), api_def::render::triangle },
		{ XOR_STR("triangle_filled"), api_def::render::triangle_filled },
		{ XOR_STR("triangle_filled_multicolor"), api_def::render::triangle_filled_multicolor },
	});

	l.set_integers_for_table(XOR_STR("render"), {
		// alignment
		{ XOR_STR("align_left"), 0 },
		{ XOR_STR("align_top"), 0 },
		{ XOR_STR("align_center"), 1 },
		{ XOR_STR("align_right"), 2 },
		{ XOR_STR("align_bottom"), 2 },
		{ XOR_STR("top_left"), 1 },
		{ XOR_STR("top_right"), 2 },
		{ XOR_STR("bottom_left"), 4 },
		{ XOR_STR("bottom_right"), 8 },
		{ XOR_STR("top"), 1 | 2 },
		{ XOR_STR("bottom"), 4 | 8 },
		{ XOR_STR("left"), 1 | 4 },
		{ XOR_STR("right"), 2 | 8 },
		{ XOR_STR("all"), 1 | 2 | 4 | 8 },

		// font flags
		{ XOR_STR("font_flag_shadow"), 1 },
		{ XOR_STR("font_flag_outline"), 2 },

		// default fonts
		{ XOR_STR("font_main"), GUI_HASH("gui_main") },
		{ XOR_STR("font_bold"), GUI_HASH("gui_bold") },
		{ XOR_STR("font_small"), FNV1A("small8") },

		// easing
		{ XOR_STR("linear"), 0 },
		{ XOR_STR("ease_in"), 1 },
		{ XOR_STR("ease_out"), 2 },
		{ XOR_STR("ease_in_out"), 3 },
		{ XOR_STR("elastic_in"), 4 },
		{ XOR_STR("elastic_out"), 5 },
		{ XOR_STR("elastic_in_out"), 6 },
		{ XOR_STR("bounce_in"), 7 },
		{ XOR_STR("bounce_out"), 8 },
		{ XOR_STR("bounce_in_out"), 9 },

		// default textures
		{ XOR_STR("texture_logo_head"), GUI_HASH("gui_logo_head") },
		{ XOR_STR("texture_logo_stripes"), GUI_HASH("gui_logo_stripes") },
		{ XOR_STR("texture_pattern"), GUI_HASH("gui_pattern") },
		{ XOR_STR("texture_pattern_group"), GUI_HASH("gui_pattern_group") },
		{ XOR_STR("texture_cursor"), GUI_HASH("gui_cursor") },
		{ XOR_STR("texture_icon_up"), GUI_HASH("gui_icon_up") },
		{ XOR_STR("texture_icon_down"), GUI_HASH("gui_icon_down") },
		{ XOR_STR("texture_icon_clear"), GUI_HASH("gui_icon_clear") },
		{ XOR_STR("texture_icon_copy"), GUI_HASH("gui_icon_copy") },
		{ XOR_STR("texture_icon_paste"), GUI_HASH("gui_icon_paste") },
		{ XOR_STR("texture_icon_alert"), GUI_HASH("gui_icon_alert") },
		{ XOR_STR("texture_icon_rage"), GUI_HASH("gui_icon_rage") },
		{ XOR_STR("texture_icon_legit"), GUI_HASH("gui_icon_legit") },
		{ XOR_STR("texture_icon_visuals"), GUI_HASH("gui_icon_visuals") },
		{ XOR_STR("texture_icon_misc"), GUI_HASH("gui_icon_misc") },
		{ XOR_STR("texture_icon_scripts"), GUI_HASH("gui_icon_scripts") },
		{ XOR_STR("texture_icon_skins"), GUI_HASH("gui_icon_skins") },
		{ XOR_STR("texture_icon_cloud"), GUI_HASH("gui_icon_cloud") },
		{ XOR_STR("texture_icon_file"), GUI_HASH("gui_icon_file") },
		{ XOR_STR("texture_avatar"), GUI_HASH("user_avatar") },
	});

	l.create_namespace(XOR_STR("utils"), {
		{ XOR_STR("random_int"), api_def::utils::random_int },
		{ XOR_STR("random_float"), api_def::utils::random_float },
		{ XOR_STR("flags"), api_def::utils::flags },
		{ XOR_STR("find_interface"), api_def::utils::find_interface },
		{ XOR_STR("find_pattern"), api_def::utils::find_pattern },
		{ XOR_STR("get_weapon_info"), api_def::utils::get_weapon_info },
		{ XOR_STR("new_timer"), api_def::utils::new_timer },
		{ XOR_STR("run_delayed"), api_def::utils::run_delayed },
		{ XOR_STR("world_to_screen"), api_def::utils::world_to_screen },
		{ XOR_STR("get_rtt"), api_def::utils::get_rtt },
		{ XOR_STR("get_time"), api_def::utils::get_time },
		{ XOR_STR("http_get"), api_def::utils::http_get },
		{ XOR_STR("http_post"), api_def::utils::http_post },
		{ XOR_STR("json_encode"), api_def::utils::json_encode },
		{ XOR_STR("json_decode"), api_def::utils::json_decode },
		{ XOR_STR("set_clan_tag"), api_def::utils::set_clan_tag },
	});

	l.create_namespace(XOR_STR("gui"), {
		{ XOR_STR("textbox"), api_def::gui::textbox_construct },
		{ XOR_STR("checkbox"), api_def::gui::checkbox_construct },
		{ XOR_STR("slider"), api_def::gui::slider_construct },
		{ XOR_STR("get_checkbox"), api_def::gui::get_checkbox },
		{ XOR_STR("get_slider"), api_def::gui::get_slider },
		{ XOR_STR("add_notification"), api_def::gui::add_notification },
		{ XOR_STR("for_each_hotkey"), api_def::gui::for_each_hotkey },
		{ XOR_STR("is_menu_open"), api_def::gui::is_menu_open },
		{ XOR_STR("get_combobox"), api_def::gui::get_combobox },
		{ XOR_STR("combobox"), api_def::gui::combobox_construct },
		{ XOR_STR("label"), api_def::gui::label_construct },
		{ XOR_STR("button"), api_def::gui::button_construct },
		{ XOR_STR("color_picker"), api_def::gui::color_picker_construct },
		{ XOR_STR("get_color_picker"), api_def::gui::get_color_picker },
		{ XOR_STR("show_message"), api_def::gui::show_message },
		{ XOR_STR("show_dialog"), api_def::gui::show_dialog },
		{ XOR_STR("get_menu_rect"), api_def::gui::get_menu_rect },
	});
	
	l.set_integers_for_table(XOR_STR("gui"), {
		{ XOR_STR("hotkey_toggle"), 0 },
		{ XOR_STR("hotkey_hold"), 1 },
		{ XOR_STR("dialog_buttons_ok_cancel"), 0 },
		{ XOR_STR("dialog_buttons_yes_no"), 1 },
		{ XOR_STR("dialog_buttons_yes_no_cancel"), 2 },
		{ XOR_STR("dialog_result_affirmative"), 0 },
		{ XOR_STR("dialog_result_negative"), 1 },
	});

	l.create_namespace(XOR_STR("input"), {
		{ XOR_STR("is_key_down"), api_def::input::is_key_down },
		{ XOR_STR("is_mouse_down"), api_def::input::is_mouse_down },
		{ XOR_STR("get_cursor_pos"), api_def::input::get_cursor_pos },
	});

	l.set_integers_for_table(XOR_STR("input"), {
		{ XOR_STR("mouse_left"), 0 },
		{ XOR_STR("mouse_right"), 1 },
		{ XOR_STR("mouse_middle"), 2 },
		{ XOR_STR("mouse_back"), 3 },
		{ XOR_STR("mouse_forward"), 4 },
	});

	l.extend_namespace(XOR_STR("math"), {
		{ XOR_STR("vec3"), api_def::math::vec3_new }
	});
}

void lua::script::register_globals()
{
	l.set_global_function(XOR_STR("print"), api_def::print);
	l.set_global_function(XOR_STR("require"), api_def::require);

	l.create_table();
	{
		l.set_field(XOR_STR("save"), api_def::database::save);
		l.set_field(XOR_STR("load"), api_def::database::load);

		l.create_metatable(XOR_STR("database"));
		l.set_field(XOR_STR("__newindex"), api_def::stub_new_index);
		l.set_field(XOR_STR("__index"), api_def::stub_index);
		l.set_metatable();

	}
	l.set_global(XOR_STR("database"));

	l.create_table();
	{
		l.set_field(XOR_STR("is_in_game"), api_def::engine::is_in_game);
		l.set_field(XOR_STR("exec"), api_def::engine::exec);
		l.set_field(XOR_STR("get_local_player"), api_def::engine::get_local_player);
		l.set_field(XOR_STR("get_view_angles"), api_def::engine::get_view_angles);
		l.set_field(XOR_STR("get_player_for_user_id"), api_def::engine::get_player_for_user_id);
		l.set_field(XOR_STR("get_player_info"), api_def::engine::get_player_info);
		l.set_field(XOR_STR("set_view_angles"), api_def::engine::set_view_angles);

		l.create_metatable(XOR_STR("engine"));
		l.set_field(XOR_STR("__newindex"), api_def::stub_new_index);
		l.set_field(XOR_STR("__index"), api_def::stub_index);
		l.set_metatable();

	}
	l.set_global(XOR_STR("engine"));

	l.create_table();
	{
		l.set_field(XOR_STR("get_entity"), api_def::entities::get_entity);
		l.set_field(XOR_STR("get_entity_from_handle"), api_def::entities::get_entity_from_handle);
		l.set_field(XOR_STR("for_each"), api_def::entities::for_each);
		l.set_field(XOR_STR("for_each_z"), api_def::entities::for_each_z);
		l.set_field(XOR_STR("for_each_player"), api_def::entities::for_each_player);

		l.create_metatable(XOR_STR("entities"));
		l.set_field(XOR_STR("__newindex"), api_def::stub_new_index);
		l.set_field(XOR_STR("__index"), api_def::entities::index);
		l.set_metatable();

	}
	l.set_global(XOR_STR("entities"));

	l.create_table();
	{
		l.create_table();
		{
			l.create_metatable(XOR_STR("ev0lve"));
			l.set_field(XOR_STR("__newindex"), api_def::stub_new_index);
			l.set_field(XOR_STR("__index"), api_def::ev0lve_index);
			l.set_metatable();
		}
		l.set_field(XOR_STR("ev0lve"));

		l.create_table();
		{
			l.create_metatable(XOR_STR("server"));
			l.set_field(XOR_STR("__newindex"), api_def::stub_new_index);
			l.set_field(XOR_STR("__index"), api_def::ev0lve_index);
			l.set_metatable();
		}
		l.set_field(XOR_STR("server"));
	}
	l.set_global(XOR_STR("info"));
	
	l.create_table();
	{
		l.create_metatable(XOR_STR("cvar"));
		l.set_field(XOR_STR("__newindex"), api_def::stub_new_index);
		l.set_field(XOR_STR("__index"), api_def::cvar::index);
		l.set_metatable();
	}
	l.set_global(XOR_STR("cvar"));

	l.create_table();
	{
		l.create_metatable(XOR_STR("global_vars"));
		l.set_field(XOR_STR("__newindex"), api_def::stub_new_index);
		l.set_field(XOR_STR("__index"), api_def::global_vars_index);
		l.set_metatable();
	}
	l.set_global(XOR_STR("global_vars"));
	
	l.create_table();
	{
		l.create_metatable(XOR_STR("game_rules"));
		l.set_field(XOR_STR("__newindex"), api_def::stub_new_index);
		l.set_field(XOR_STR("__index"), api_def::game_rules_index);
		l.set_metatable();
	}
	l.set_global(XOR_STR("game_rules"));
	
	l.create_table();
	{
		l.set_field(XOR_STR("frame_undefined"), -1);
		l.set_field(XOR_STR("frame_start"), 0);
		l.set_field(XOR_STR("frame_net_update_start"), 1);
		l.set_field(XOR_STR("frame_net_update_postdataupdate_start"), 2);
		l.set_field(XOR_STR("frame_net_update_postdataupdate_end"), 3);
		l.set_field(XOR_STR("frame_net_update_end"), 4);
		l.set_field(XOR_STR("frame_render_start"), 5);
		l.set_field(XOR_STR("frame_render_end"), 6);
		l.set_field(XOR_STR("in_attack"), sdk::user_cmd::attack);
		l.set_field(XOR_STR("in_jump"), sdk::user_cmd::jump);
		l.set_field(XOR_STR("in_duck"), sdk::user_cmd::duck);
		l.set_field(XOR_STR("in_forward"), sdk::user_cmd::forward);
		l.set_field(XOR_STR("in_back"), sdk::user_cmd::back);
		l.set_field(XOR_STR("in_use"), sdk::user_cmd::use);
		l.set_field(XOR_STR("in_left"), sdk::user_cmd::left);
		l.set_field(XOR_STR("in_move_left"), sdk::user_cmd::move_left);
		l.set_field(XOR_STR("in_right"), sdk::user_cmd::right);
		l.set_field(XOR_STR("in_move_right"), sdk::user_cmd::move_right);
		l.set_field(XOR_STR("in_attack2"), sdk::user_cmd::attack2);
		l.set_field(XOR_STR("in_score"), sdk::user_cmd::score);
	}
	l.set_global(XOR_STR("csgo"));
}

void lua::script::register_types()
{
	l.create_type(XOR_STR("gui.textbox"), {
		{ XOR_STR("get_value"), api_def::gui::textbox_get_value },
		{ XOR_STR("set_value"), api_def::gui::textbox_set_value },
		{ XOR_STR("set_tooltip"), api_def::gui::control_set_tooltip },
		{ XOR_STR("set_visible"), api_def::gui::control_set_visible },
		{ XOR_STR("add_callback"), api_def::gui::control_add_callback },
	});

	l.create_type(XOR_STR("gui.checkbox"), {
		{ XOR_STR("get_value"), api_def::gui::checkbox_get_value },
		{ XOR_STR("set_value"), api_def::gui::checkbox_set_value },
		{ XOR_STR("set_tooltip"), api_def::gui::control_set_tooltip },
		{ XOR_STR("set_visible"), api_def::gui::control_set_visible },
		{ XOR_STR("add_callback"), api_def::gui::control_add_callback },
	});
	
	l.create_type(XOR_STR("gui.slider"), {
		{ XOR_STR("get_value"), api_def::gui::slider_get_value },
		{ XOR_STR("set_value"), api_def::gui::slider_set_value },
		{ XOR_STR("set_tooltip"), api_def::gui::control_set_tooltip },
		{ XOR_STR("set_visible"), api_def::gui::control_set_visible },
		{ XOR_STR("add_callback"), api_def::gui::control_add_callback },
	});
	
	l.create_type(XOR_STR("gui.combobox"), {
		{ XOR_STR("get_value"), api_def::gui::combobox_get_value },
		{ XOR_STR("set_value"), api_def::gui::combobox_set_value },
		{ XOR_STR("set_tooltip"), api_def::gui::control_set_tooltip },
		{ XOR_STR("set_visible"), api_def::gui::control_set_visible },
		{ XOR_STR("add_callback"), api_def::gui::control_add_callback },
	});
	
	l.create_type(XOR_STR("gui.color_picker"), {
		{ XOR_STR("get_value"), api_def::gui::color_picker_get_value },
		{ XOR_STR("set_value"), api_def::gui::color_picker_set_value },
		{ XOR_STR("set_tooltip"), api_def::gui::control_set_tooltip },
		{ XOR_STR("set_visible"), api_def::gui::control_set_visible },
		{ XOR_STR("add_callback"), api_def::gui::control_add_callback },
	});

	l.create_type(XOR_STR("gui.label"), {
		{ XOR_STR("set_tooltip"), api_def::gui::control_set_tooltip },
		{ XOR_STR("set_visible"), api_def::gui::control_set_visible },
		{ XOR_STR("add_callback"), api_def::gui::control_add_callback },
	});

	l.create_type(XOR_STR("gui.button"), {
		{ XOR_STR("set_tooltip"), api_def::gui::control_set_tooltip },
		{ XOR_STR("set_visible"), api_def::gui::control_set_visible },
		{ XOR_STR("add_callback"), api_def::gui::control_add_callback },
	});
	
	l.create_type(XOR_STR("render.anim_float"), {
		{ XOR_STR("direct"), api_def::render::anim_float_direct },
		{ XOR_STR("get_value"), api_def::render::anim_float_get_value },
	});
	
	l.create_type(XOR_STR("render.anim_color"), {
		{ XOR_STR("direct"), api_def::render::anim_color_direct },
		{ XOR_STR("get_value"), api_def::render::anim_color_get_value },
	});

	l.create_type(XOR_STR("csgo.entity"), {
			{XOR_STR("get_index"),           api_def::entity::get_index},
			{XOR_STR("is_valid"),            api_def::entity::is_valid},
			{XOR_STR("is_dormant"),          api_def::entity::is_dormant},
			{XOR_STR("is_player"),           api_def::entity::is_player},
			{XOR_STR("is_enemy"),            api_def::entity::is_enemy},
			{XOR_STR("get_class"),           api_def::entity::get_class},
			{XOR_STR("get_prop"),            api_def::entity::get_prop},
			{XOR_STR("set_prop"),            api_def::entity::set_prop},
			{XOR_STR("get_hitbox_position"), api_def::entity::get_hitbox_position},
			{XOR_STR("get_eye_position"),    api_def::entity::get_eye_position},
			{XOR_STR("get_player_info"),     api_def::entity::get_player_info},
			{XOR_STR("get_move_type"),       api_def::entity::get_move_type},
	});

	l.create_type(XOR_STR("csgo.cvar"), {
		{ XOR_STR("get_int"), api_def::cvar::get_int },
		{ XOR_STR("get_float"), api_def::cvar::get_float },
		{ XOR_STR("set_int"), api_def::cvar::set_int },
		{ XOR_STR("set_float"), api_def::cvar::set_float },
		{ XOR_STR("get_string"), api_def::cvar::get_string },
		{ XOR_STR("set_string"), api_def::cvar::set_string },
	});
	
	l.create_type(XOR_STR("csgo.event"), {
		{ XOR_STR("get_name"), api_def::event::get_name },
		{ XOR_STR("get_bool"), api_def::event::get_bool },
		{ XOR_STR("get_int"), api_def::event::get_int },
		{ XOR_STR("get_float"), api_def::event::get_float },
		{ XOR_STR("get_string"), api_def::event::get_string },
	});
	
	l.create_type(XOR_STR("csgo.user_cmd"), {
		{ XOR_STR("get_command_number"), api_def::user_cmd::get_command_number },
		{ XOR_STR("get_view_angles"), api_def::user_cmd::get_view_angles },
		{ XOR_STR("get_move"), api_def::user_cmd::get_move },
		{ XOR_STR("get_buttons"), api_def::user_cmd::get_buttons },
		{ XOR_STR("set_view_angles"), api_def::user_cmd::set_view_angles },
		{ XOR_STR("set_move"), api_def::user_cmd::set_move },
		{ XOR_STR("set_buttons"), api_def::user_cmd::set_buttons },
	});

	l.create_type(XOR_STR("utils.timer"), {
		{ XOR_STR("start"), api_def::timer::start },
		{ XOR_STR("stop"), api_def::timer::stop },
		{ XOR_STR("run_once"), api_def::timer::run_once },
		{ XOR_STR("set_delay"), api_def::timer::set_delay },
		{ XOR_STR("is_active"), api_def::timer::is_active }
	});

	l.create_type(XOR_STR("bus"), {
		{ XOR_STR("__index"), api_def::bus::index }
	});
	
	l.create_type(XOR_STR("bus_object"), {
		{ XOR_STR("__call"), api_def::bus::invoke }
	});

	l.create_type(XOR_STR("vec3"), {
		{ XOR_STR("__index"), api_def::math::vec3_index },
		{ XOR_STR("__newindex"), api_def::math::vec3_new_index },
		{ XOR_STR("__add"), api_def::math::vec3_add },
		{ XOR_STR("__sub"), api_def::math::vec3_sub },
		{ XOR_STR("__mul"), api_def::math::vec3_mul },
		{ XOR_STR("__div"), api_def::math::vec3_div },
	});
}

void lua::script::add_gui_element(uint32_t e_id)
{
	gui_elements.emplace_back(e_id);
}

void lua::script::add_gui_element_with_callback(uint32_t e_id)
{
	if (std::find(gui_callbacks.begin(), gui_callbacks.end(), e_id) == gui_callbacks.end())
		gui_callbacks.emplace_back(e_id);
}

void lua::script::add_font(uint32_t _id)
{
	fonts.emplace_back(_id);
}

void lua::script::add_texture(uint32_t _id)
{
	textures.emplace_back(_id);
}

void lua::script::add_animator(uint32_t _id)
{
	animators.emplace_back(_id);
}

void lua::script::run_timers() const
{
	if (!is_running || lua::helpers::timers[id].empty())
		return;

	auto& my_timers = lua::helpers::timers[id];

	// loop through all existing callbacks
	for (auto it = my_timers.begin(); it != my_timers.end();)
	{
		// get current callback
		const auto timer = *it;

		// skip if inactive
		if (!timer->is_active())
		{
			++it;
			continue;
		}

		// we waited long enough
		if (timer->cycle_completed())
		{
			// run callback
			const auto cbk = timer->get_callback();

			if (cbk)
				cbk();
		}

		// should be deleted
		if (timer->should_delete())
		{
			// erase callback
			it = my_timers.erase(it);
		}
		else
			++it;
	}
}