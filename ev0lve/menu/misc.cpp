
#include <menu/menu.h>
#include <menu/macros.h>
#include <base/cfg.h>
#include <gui/helpers.h>
#include <filesystem>
#include <network/helpers.h>
#include <gui/popups/about_popup.h>
#include <util/fnv1a.h>
#include <shellapi.h>

#ifdef CSGO_LUA
#include <lua/engine.h>
#endif

#ifndef CSGO_SECURE
#include <base/game.h>
#endif

USE_NAMESPACES;

void unload();

void menu::group::misc_general(std::shared_ptr<layout> &e)
{
	ADD("Bunny hop", "misc.general.bunny_hop", checkbox, cfg.misc.bhop);

	const auto auto_strafe = MAKE("misc.general.auto_strafe", combo_box, cfg.misc.autostrafe);
	auto_strafe->add({
		MAKE("misc.general.auto_strafe.disabled", selectable, XOR("Disabled")),
		MAKE("misc.general.auto_strafe.view_direction", selectable, XOR("View direction")),
		MAKE("misc.general.auto_strafe.movement_input", selectable, XOR("Movement input")),
	});

	ADD_INLINE("Auto strafe", auto_strafe);
	ADD("Air strafe smoothing", "misc.general.auto_strafe_turn_speed", slider<float>, 0.f, 100.f, cfg.misc.autostrafe_turn_speed, XOR("%.0f%%"));

	ADD_TOOLTIP("Nade assistant", "misc.general.nade_assistant", "Adjusts grenade throw angle to maximize distance while moving", checkbox, cfg.misc.nade_assistant);

	const auto peek_assistant = MAKE("misc.general.peek_assistant_mode", combo_box, cfg.misc.peek_assistant_mode);
	peek_assistant->add({
			MAKE("misc.general.peek_assistant_mode.free_roam", selectable, XOR("Free roaming")),
			MAKE("misc.general.peek_assistant_mode.retreat_early", selectable, XOR("Retreat early"))
	});
	ADD_INLINE("Peek assistant", MAKE("misc.general.peek_assistant", checkbox, cfg.misc.peek_assistant));
	ADD_LINE("misc.general.peek_assistant", peek_assistant, MAKE("misc.general.peek_assistant_color", color_picker, cfg.misc.peek_assistant_color));

	ADD("Reveal ranks", "misc.general.reveal_ranks", checkbox, cfg.misc.reveal_ranks);
	ADD("Auto accept", "misc.general.auto_accept", checkbox, cfg.misc.auto_accept);
	ADD("Clantag changer", "misc.general.clantag_changer", checkbox, cfg.misc.clantag_changer);
	ADD("Preserve killfeed", "misc.general.preserve_killfeed", checkbox, cfg.misc.preserve_killfeed);

#ifndef CSGO_SECURE
	const auto btn = MAKE("misc.general.unload", button, XOR("Unload"));
	btn->callback = unload;

	e->add(btn);
#endif
}

void menu::group::misc_settings(std::shared_ptr<layout> &e)
{
	const auto menu_color = MAKE("misc.settings.menu_color", color_picker, cfg.misc.menu_color, false);
	menu_color->callback = [](color c) {
		colors.accent = c;
	};

	ADD_INLINE("Menu color", menu_color);
	ADD("Enable menu sounds", "misc.settings.enable_tick_sound", checkbox, cfg.misc.enable_tick_sound);

	const auto about = MAKE("misc.settings.about", button, XOR("About"));
	about->size.x = 164.f;
	about->margin.mins.y += 20.f;
	about->margin.maxs.y += 20.f;
	about->callback = []() {
		const auto abt_popup = MAKE("ev0lve.about", ::gui::popups::about_popup);
		abt_popup->open();
	};
	
	e->add(about);
}

void menu::group::misc_helpers(std::shared_ptr<layout> &e)
{
	const auto event_log_triggers = MAKE("misc.helpers.event_log_triggers", combo_box, cfg.misc.event_triggers);
	event_log_triggers->allow_multiple = true;
	event_log_triggers->add({
		MAKE("misc.helpers.event_log_triggers.shot_info", selectable, XOR("Shot info")),
		MAKE("misc.helpers.event_log_triggers.damage_taken", selectable, XOR("Damage taken")),
		MAKE("misc.helpers.event_log_triggers.purchase", selectable, XOR("Purchase")),
	});

	const auto event_log_output = MAKE("misc.helpers.event_log_output", combo_box, cfg.misc.eventlog);
	event_log_output->allow_multiple = true;
	event_log_output->add({
	    MAKE("misc.helpers.event_log_output.console", selectable, XOR("Console")),
	    MAKE("misc.helpers.event_log_output.chat", selectable, XOR("Chat")),
	    MAKE("misc.helpers.event_log_output.top_left", selectable, XOR("Top left")),
	});

	ADD_INLINE("Event log triggers", event_log_triggers);
	ADD_INLINE("Event log output", event_log_output);

	ADD("Penetration crosshair", "misc.helpers.penetration_crosshair", checkbox, cfg.misc.penetration_crosshair);
}

void menu::group::misc_configs(std::shared_ptr<layout> &e)
{
	static value_param<bits> selected{};

	const auto cfg_refresh = MAKE("misc.configs.refresh", button, XOR("Refresh"));
	cfg_refresh->size.x = 165.f;
	cfg_refresh->callback = []() {
		if (cfg.is_loading)
			return;
		
		const auto l = ctx->find<list>(GUI_HASH("misc.configs.list"));
		l->clear();

		for (const auto& f : std::filesystem::directory_iterator(XOR("ev0lve")))
		{
			const auto &path = f.path();
			if (path.extension() != XOR(".cfg"))
				continue;

			const auto sel = std::make_shared<selectable>(util::fnv1a(path.string().c_str()),
														  path.filename().replace_extension("").string());

			if (cfg.last_changed_config == XOR("ev0lve/") + sel->text + XOR(".cfg"))
				sel->is_loaded = true;

			if (cfg.last_changed_config == XOR("ev0lve/") + sel->text + XOR(".cfg"))
				sel->select();

			l->add(sel);
		}
	};

	const auto cfg_load =  MAKE("misc.configs.load", button, XOR("Load"));
	cfg_load->size.x = 165.f;
	cfg_load->callback = []() {
		if (cfg.is_loading)
			return;

		const auto cfg_list = ctx->find<list>(GUI_HASH("misc.configs.list"));
		const auto s = cfg_list->at(selected.get().first_set_bit().value_or(0))->as<selectable>();
		cfg.load(XOR("ev0lve/") + s->text + XOR(".cfg"));

		cfg_list->for_each_control([&](std::shared_ptr<control>& c){
			const auto sel = c->as<selectable>();
			if (cfg.last_changed_config == XOR("ev0lve/") + sel->text + XOR(".cfg"))
				sel->is_loaded = true;
			else
				sel->is_loaded = false;

			sel->reset();
		});
	};

	const auto cfg_save =  MAKE("misc.configs.save", button, XOR("Save"));
	cfg_save->size.x = 165.f;
	cfg_save->callback = []() {
		if (cfg.is_loading)
			return;

		const auto cfg_list = ctx->find<list>(GUI_HASH("misc.configs.list"));
		const auto s = cfg_list->at(selected.get().first_set_bit().value_or(0))->as<selectable>();
		const auto file = XOR("ev0lve/") + s->text + XOR(".cfg");

		// open dialog if we are unsure
		if (cfg.is_dangerous || cfg.last_changed_config != file)
		{
			cfg.want_to_change = file;

			const auto dlg = std::make_shared<dialog_box>(GUI_HASH("config.save"), XOR("Warning"), XOR("Are you sure you want to overwrite this config?"), db_yes_no);
			dlg->callback = [](dialog_result r) {
				if (r == dr_affirmative)
					cfg.save(cfg.want_to_change);
			};

			dlg->open();
		} else
			cfg.save(file);

		cfg_list->for_each_control([&](std::shared_ptr<control>& c){
			const auto sel = c->as<selectable>();
			if (file == XOR("ev0lve/") + sel->text + XOR(".cfg"))
				sel->is_loaded = true;
			else
				sel->is_loaded = false;

			sel->reset();
		});
	};

	const auto cfg_reset =  MAKE("misc.configs.reset", button, XOR("Reset"));
	cfg_reset->size.x = 165.f;
	cfg_reset->callback = []() {
		if (cfg.is_loading)
			return;
		
		cfg.drop();
	};

	const auto cfg_create = MAKE("misc.configs.create", button, XOR("Create"));
	cfg_create->size.x = 165.f;
	cfg_create->callback = []() {
		if (cfg.is_loading)
			return;
		
		const auto inp = ctx->find<text_input>(GUI_HASH("misc.configs.name"));
		const auto refresh = ctx->find<button>(GUI_HASH("misc.configs.refresh"));

		std::ofstream _file(XOR("ev0lve/") + inp->value + XOR(".cfg"));
		_file.close();

		cfg.last_changed_config = XOR("ev0lve/") + inp->value + XOR(".cfg");

		refresh->callback();
	};

	const auto cfg_delete =  MAKE("misc.configs.delete", button, XOR("Delete"));
	cfg_delete->size.x = 165.f;
	cfg_delete->callback = []() {
		if (cfg.is_loading)
			return;
		
		const auto s = ctx->find<list>(GUI_HASH("misc.configs.list"))->at(selected.get().first_set_bit().value_or(0))->as<selectable>();
		const auto file = XOR("ev0lve/") + s->text + XOR(".cfg");

		cfg.want_to_change = file;

		const auto dlg = std::make_shared<dialog_box>(GUI_HASH("config.delete"), XOR("Warning"), XOR("Are you sure you want to delete this config?"), db_yes_no);
		dlg->callback = [](dialog_result r) {
			if (r == dr_affirmative)
			{
				std::filesystem::remove(cfg.want_to_change);
				ctx->find<button>(GUI_HASH("misc.configs.refresh"))->callback();
			}
		};

		dlg->open();
	};

	const auto cfg_open_folder = MAKE("misc.configs.open_folder", button, XOR("Open Folder"));
	cfg_open_folder->size.x = 165.f;
	cfg_open_folder->callback = []() {
		std::thread([&] {
			char path[MAX_PATH];
			GetCurrentDirectoryA(MAX_PATH, path);

			ShellExecuteA(nullptr, nullptr, (std::string(path) + XOR("/ev0lve/")).c_str(), nullptr, nullptr, SW_SHOWNORMAL);
		}).detach();
	};

	e->add(std::make_shared<list>(GUI_HASH("misc.configs.list"), selected, vec2(), vec2(165.f, 120.f))->adjust_margin({2.f, 2.f, 2.f, 2.f}));
	e->add(cfg_open_folder);
	e->add(cfg_refresh);
	e->add(cfg_load);
	e->add(cfg_save);
	e->add(cfg_reset);
	e->add(MAKE("misc.configs.name", text_input, vec2{}, vec2{165.f, 24.f}));
	e->add(cfg_create);
	e->add(cfg_delete);
}

void unload()
{
#ifndef CSGO_SECURE
	std::thread([]() {
		game->deinit();
		game = nullptr;
	}).detach();
#endif
}