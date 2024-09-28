
#include <menu/menu.h>
#include <menu/macros.h>
#include <base/cfg.h>
#include <gui/helpers.h>
#include <gui/selectable_script.h>
#include <shellapi.h>

#ifdef CSGO_LUA
#include <lua/engine.h>
using namespace gui;
#endif

USE_NAMESPACES;

void menu::group::scripts_general(std::shared_ptr<layout> &e)
{
#ifdef CSGO_LUA
	static value_param<bits> script_select{};
	static value_param<bool> autoload{};
	
	const auto refresh = MAKE("scripts.general.refresh", button, XOR_STR("Refresh"), nullptr, vec2{}, vec2{165.f, 20.f});
	refresh->callback = []() {
		if (cfg.is_loading)
			return;
		
		const auto scripts = ctx->find<list>(GUI_HASH("scripts.general.list"));
		scripts->clear();
		
		bool is_first{true};
		
		lua::api.refresh_scripts();
		lua::api.for_each_script_name([scripts, &is_first](const lua::script_file& f) {
			const auto s = std::make_shared<selectable_script>(f.make_id(), f);
			if (is_first)
			{
				s->select();
				is_first = false;
			}
			
			scripts->add(s);
		});
		
		scripts->process_queues();
		
		const auto is_empty = scripts->empty();
		ctx->find(GUI_HASH("scripts.general.toggle"))->set_visible(!is_empty);
		ctx->find(GUI_HASH("scripts.general.autoload"))->parent.lock()->set_visible(!is_empty);
		
		if (!is_empty)
			scripts->callback(script_select.get());

		scripts->for_each_control([&](std::shared_ptr<control>& c){
			const auto sel = c->as<selectable>();
			if (lua::api.exists(sel->id))
				sel->is_loaded = true;
			else
				sel->is_loaded = false;

			sel->reset();
		});
	};

	const auto toggle = MAKE("scripts.general.toggle", button, XOR_STR("Load"), nullptr, vec2{}, vec2{165.f, 20.f});
	toggle->callback = [toggle]() {
		if (cfg.is_loading)
			return;
		
		const auto scripts = ctx->find<list>(GUI_HASH("scripts.general.list"));
		const auto sel = scripts->at(script_select.get())->as<selectable_script>();
		
		if (lua::api.exists(sel->id))
			sel->file.should_unload = true;
		else
			sel->file.should_load = true;

		sel->reset();
	};
	
	const auto autoload_toggle = MAKE("scripts.general.autoload", checkbox, autoload);
	autoload_toggle->callback = [](bool v) {
		// don't want to react to cfg load calls
		if (cfg.is_loading)
			return;
		
		const auto scripts = ctx->find<list>(GUI_HASH("scripts.general.list"));
		const auto sel = scripts->at(script_select.get())->as<selectable_script>();
		
		v ? lua::api.enable_autoload(sel->file) : lua::api.disable_autoload(sel->file);
	};
	
	const auto scripts = MAKE("scripts.general.list", list, script_select, vec2{}, vec2{165.f, 200.f});
	scripts->legacy_mode = true;
	scripts->callback = [scripts, toggle, autoload_toggle](bits& v) {
		if (cfg.is_loading)
			return;
		
		if (scripts->empty())
		{
			toggle->set_visible(false);
			autoload_toggle->set_visible(false);
		} else
		{
			toggle->set_visible(true);
			autoload_toggle->set_visible(true);
		}
		
		const auto sel = scripts->at(v)->as<selectable_script>();
		toggle->text = lua::api.exists(sel->id) ? XOR_STR("Unload") : XOR_STR("Load");
		autoload.set(lua::api.is_autoload_enabled(sel->file));
		autoload_toggle->reset_internal();
	};

	const auto open_folder = MAKE("scripts.general.open_folder", button, XOR_STR("Open Folder"), nullptr, vec2{}, vec2{165.f, 20.f});
	open_folder->callback = []() {
		std::thread([&] {
			char path[MAX_PATH];
			GetCurrentDirectoryA(MAX_PATH, path);

			ShellExecuteA(nullptr, nullptr, (std::string(path) + XOR("/ev0lve/scripts")).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
		}).detach();
	};

	ADD_LINE("Script list", scripts->adjust_margin({2.f, 2.f, 2.f, 2.f}));
	ADD_LINE("Open Folder", open_folder);
	ADD_LINE("Refresh", refresh);
	ADD_LINE("Load", toggle);

	// HACK: appending dontuse so it will be harder for them to figure out how to get those from the script
	ADD("Allow insecure", "scripts.general.allow_insecure.dontuse", checkbox, cfg.lua.allow_insecure);
	ADD_TOOLTIP("Allow pcall", "scripts.general.allow_pcall.dontuse", "Allows pcall() function. Enable this feature only if you are using trusted scripts.", checkbox, cfg.lua.allow_pcall);
	ADD_TOOLTIP("Allow dynamic load", "scripts.general.allow_dynamic_load.dontuse", "Allows dynamic script load. Enable this feature only if you are using trusted scripts.", checkbox, cfg.lua.allow_dynamic_load);

	ADD_INLINE("Autoload", autoload_toggle);
#endif
}

void menu::group::scripts_elements(std::shared_ptr<layout>& e)
{
	// empty
}