
#include <menu/menu.h>
#include <menu/macros.h>
#include <base/cfg.h>

#include <gui/helpers.h>

USE_NAMESPACES;

void menu::group::rage_tab(std::shared_ptr<layout>& e)
{
	const auto side_bar = MAKE("rage.sidebar", tabs_layout, vec2{-10.f, -10.f}, vec2{200.f, 440.f}, td_vertical, true);
	side_bar->add(MAKE("rage.sidebar.general", child_tab, XOR("General"), GUI_HASH("rage.weapon_tab.general"))->make_selected());
	side_bar->add(MAKE("rage.sidebar.pistol", child_tab, XOR("Pistols"), GUI_HASH("rage.weapon_tab.pistol")));
	side_bar->add(MAKE("rage.sidebar.heavy_pistol", child_tab, XOR("Heavy pistols"), GUI_HASH("rage.weapon_tab.heavy_pistol")));
	side_bar->add(MAKE("rage.sidebar.automatic", child_tab, XOR("Automatic"), GUI_HASH("rage.weapon_tab.automatic")));
	side_bar->add(MAKE("rage.sidebar.awp", child_tab, XOR("AWP"), GUI_HASH("rage.weapon_tab.awp")));
	side_bar->add(MAKE("rage.sidebar.scout", child_tab, XOR("SSG-08"), GUI_HASH("rage.weapon_tab.scout")));
	side_bar->add(MAKE("rage.sidebar.auto_sniper", child_tab, XOR("Auto snipers"), GUI_HASH("rage.weapon_tab.auto_sniper")));

	side_bar->process_queues();
	side_bar->for_each_control([](std::shared_ptr<control>& c) {
		c->as<child_tab>()->make_vertical();
	});

	const vec2 group_sz{284.f, 430.f};
	const vec2 group_half_sz = group_sz * vec2{1.f, 0.5f} - vec2{0.f, 5.f};

	const auto weapon_tabs = MAKE("rage.weapon_tabs", layout, vec2{}, group_half_sz, s_justify);
	weapon_tabs->make_not_clip();
	weapon_tabs->add(rage_weapon(XOR("general"), grp_general, group_half_sz, true));
	weapon_tabs->add(rage_weapon(XOR("pistol"), grp_pistol, group_half_sz));
	weapon_tabs->add(rage_weapon(XOR("heavy_pistol"), grp_heavypistol, group_half_sz));
	weapon_tabs->add(rage_weapon(XOR("automatic"), grp_automatic, group_half_sz));
	weapon_tabs->add(rage_weapon(XOR("awp"), grp_awp, group_half_sz));
	weapon_tabs->add(rage_weapon(XOR("scout"), grp_scout, group_half_sz));
	weapon_tabs->add(rage_weapon(XOR("auto_sniper"), grp_autosniper, group_half_sz));

	const auto groups = std::make_shared<layout>(GUI_HASH("rage.groups"), vec2{200.f, 0.f}, vec2{580.f, 430.f}, s_justify);
	groups->add(tools::make_stacked_groupboxes(GUI_HASH("rage.groups.general_weapon"), group_sz, {
			make_groupbox(XOR("rage.general"), XOR("General"), group_half_sz, rage_general),
			weapon_tabs
	}));

	groups->add(tools::make_stacked_groupboxes(GUI_HASH("rage.groups.antiaim_fakelag_desync"), group_sz, {
		make_groupbox(XOR("rage.antiaim"), XOR("Anti-aim"), vec2{group_sz.x, 150.f}, rage_antiaim),
		make_groupbox(XOR("rage.desync"), XOR("Desync"), vec2{group_sz.x, 165.f}, rage_desync),
		make_groupbox(XOR("rage.fakelag"), XOR("Fakelag"), vec2{group_sz.x, 90.f}, rage_fakelag)
	}));

	e->add(side_bar);
	e->add(groups);
}

void menu::group::rage_general(std::shared_ptr<layout> &e)
{
	const auto mode = MAKE("rage.general.mode", combo_box, cfg.rage.mode);
	mode->add({
		MAKE("rage.general.mode.just_in_time", selectable, XOR("Just in time")),
		MAKE("rage.general.mode.ahead_of_time", selectable, XOR("Ahead of time"))->make_tooltip(XOR("Increases chance of hitting a running player but disables backtracking")),
		//MAKE("rage.general.mode.ahead_of_time_fallback", selectable, XOR("Ahead of time (fallback)")),
	});

	ADD_INLINE("Mode", mode, MAKE("rage.general.enable", checkbox, cfg.rage.enable));
	ADD("FOV", "rage.general.fov", slider<float>, 0.f, 180.f, cfg.rage.fov, XOR("%.0f°"));
	ADD("Target visible only", "rage.general.only_visible", checkbox, cfg.rage.only_visible);

	ADD("Auto scope", "rage.general.auto_scope", checkbox, cfg.rage.auto_scope);
	ADD_TOOLTIP("Auto engage", "rage.general.auto_engage", "Enables automatic fire and stop", checkbox, cfg.rage.auto_engage);

	const auto fast_fire_mode = MAKE("rage.general.fast_fire_mode", combo_box, cfg.rage.fast_fire);
	fast_fire_mode->add({
		MAKE("rage.general.fast_fire_mode.default", selectable, XOR("Default")),
		MAKE("rage.general.fast_fire_mode.peek", selectable, XOR("Peek")),
	});

	ADD_INLINE("Fast fire", fast_fire_mode, MAKE("rage.general.fast_fire", checkbox, cfg.rage.enable_fast_fire));
	ADD_TOOLTIP("Delay shot", "rage.general.delay_shot", "Delays shot if it could be possible to deal more damage", checkbox, cfg.rage.delay_shot);
	ADD("Target dormant", "rage.general.dormant", checkbox, cfg.rage.dormant);
}

void menu::group::rage_antiaim(std::shared_ptr<layout> &e)
{
	const auto disablers = MAKE("rage.antiaim.disablers", combo_box, cfg.antiaim.disablers);
	disablers->add({
			MAKE("rage.antiaim.disablers.round_end", selectable, XOR("Disable at round end")),
			MAKE("rage.antiaim.disablers.knife", selectable, XOR("Disable against knives")),
			MAKE("rage.antiaim.disablers.defuse", selectable, XOR("Disable when defusing")),
			MAKE("rage.antiaim.disablers.shield", selectable, XOR("Disable with riot shield"))
	});
	disablers->allow_multiple = true;
	ADD_INLINE("Mode", disablers, MAKE("rage.antiaim.enable", checkbox, cfg.antiaim.enable));

	const auto pitch = MAKE("rage.antiaim.pitch", combo_box, cfg.antiaim.pitch);
	pitch->add({
		MAKE("rage.antiaim.pitch.none", selectable, XOR("None")),
		MAKE("rage.antiaim.pitch.down", selectable, XOR("Down")),
		MAKE("rage.antiaim.pitch.up", selectable, XOR("Up"))
	});

	ADD_INLINE("Pitch", pitch);

	const auto yaw = MAKE("rage.antiaim.yaw", combo_box, cfg.antiaim.yaw);
	yaw->add({
		MAKE("rage.antiaim.yaw.view_angle", selectable, XOR("View angle")),
		MAKE("rage.antiaim.yaw.at_target", selectable, XOR("At target")),
		MAKE("rage.antiaim.yaw.freestanding", selectable, XOR("Freestanding"))
	});

	ADD_INLINE("Yaw", yaw);

	const auto yaw_override = MAKE("rage.antiaim.yaw_override", combo_box, cfg.antiaim.yaw_override);
	yaw_override->add({
		MAKE("rage.antiaim.yaw_override.none", selectable, XOR("None")),
		MAKE("rage.antiaim.yaw_override.left", selectable, XOR("Left")),
		MAKE("rage.antiaim.yaw_override.right", selectable, XOR("Right")),
		MAKE("rage.antiaim.yaw_override.back", selectable, XOR("Back")),
		MAKE("rage.antiaim.yaw_override.forward", selectable, XOR("Forward")),
	});

	ADD_INLINE("Yaw override", yaw_override);

	const auto yaw_modifier = MAKE("rage.antiaim.yaw_modifier", combo_box, cfg.antiaim.yaw_modifier);
	yaw_modifier->add({
		MAKE("rage.antiaim.yaw_modifier.none", selectable, XOR("None")),
		MAKE("rage.antiaim.yaw_modifier.directional", selectable, XOR("Directional")),
		MAKE("rage.antiaim.yaw_modifier.jitter", selectable, XOR("Jitter")),
		MAKE("rage.antiaim.yaw_modifier.half_jitter", selectable, XOR("Half jitter")),
		MAKE("rage.antiaim.yaw_modifier.random", selectable, XOR("Random")),
	});

	ADD_INLINE("Yaw modifier", yaw_modifier);

	ADD("Yaw modifier amount", "rage.antiaim.yaw_modifier_amount", slider<float>, -180.f, 180.f, cfg.antiaim.yaw_modifier_amount, XOR("%.0f°"));

	const auto movement_mode = MAKE("rage.antiaim.movement_mode", combo_box, cfg.antiaim.movement_mode);
	movement_mode->add({
		MAKE("rage.antiaim.movement_mode.default", selectable, XOR("Default")),
		MAKE("rage.antiaim.movement_mode.skate", selectable, XOR("Skate")),
		MAKE("rage.antiaim.movement_mode.walk", selectable, XOR("Walk")),
		MAKE("rage.antiaim.movement_mode.opposite", selectable, XOR("Opposite")),
	});

	ADD_INLINE("Movement mode", movement_mode);
}

void menu::group::rage_fakelag(std::shared_ptr<layout> &e)
{
	ADD("Amount", "rage.fakelag.amount", slider<float>, 0.f, 14.f, cfg.fakelag.lag_amount, XOR("%.0f ticks"));
	ADD("Variance", "rage.fakelag.variance", slider<float>, 0.f, 14.f, cfg.fakelag.variance, XOR("%.0f ticks"));
}

void menu::group::rage_desync(std::shared_ptr<layout> &e)
{
	const auto desync = MAKE("rage.desync.desync", combo_box, cfg.antiaim.desync);
	desync->add({
		MAKE("rage.desync.desync.none", selectable, XOR("None")),
		MAKE("rage.desync.desync.static", selectable, XOR("Static")),
		MAKE("rage.desync.desync.opposite", selectable, XOR("Opposite")),
		MAKE("rage.desync.desync.jitter", selectable, XOR("Jitter")),
		MAKE("rage.desync.desync.random", selectable, XOR("Random")),
		MAKE("rage.desync.desync.freestanding", selectable, XOR("Freestanding")),
	});

	ADD_INLINE("Desync", desync, MAKE("rage.desync.flip", checkbox, cfg.antiaim.desync_flip)->make_tooltip(XOR("Flip")));
	ADD("Desync amount", "rage.desync.amount", slider<float>, 0.f, 100.f, cfg.antiaim.desync_amount, XOR("%.0f%%"));

	const auto body_lean = MAKE("rage.desync.body_lean", combo_box, cfg.antiaim.body_lean);
	body_lean->add({
			MAKE("rage.desync.body_lean.none", selectable, XOR("None")),
			MAKE("rage.desync.body_lean.static", selectable, XOR("Static")),
			MAKE("rage.desync.body_lean.jitter", selectable, XOR("Jitter")),
			MAKE("rage.desync.body_lean.random", selectable, XOR("Random")),
			MAKE("rage.desync.body_lean.freestanding", selectable, XOR("Freestanding"))
	});
	ADD_INLINE("Body lean", body_lean, MAKE("rage.desync.body_lean_flip", checkbox, cfg.antiaim.body_lean_flip)->make_tooltip(XOR("Flip")));
	ADD("Body lean amount", "rage.desync.body_lean_amount", slider<float>, 0.f, 100.f, cfg.antiaim.body_lean_amount, XOR("%.0f%%"));
	ADD("Ensure body lean", "rage.desync.ensure_body_lean", checkbox, cfg.antiaim.ensure_body_lean);

	ADD("Align fake", "rage.desync.align_fake", checkbox, cfg.antiaim.align_fake);
	ADD("Hide shot", "rage.desync.hide_shot", checkbox, cfg.antiaim.hide_shot);
	ADD("Fake duck", "rage.desync.fake_duck", checkbox, cfg.antiaim.fake_duck);

	const auto slow_walk = MAKE("rage.desync.slow_walk", combo_box, cfg.antiaim.slowwalk_mode);
	slow_walk->add({
		MAKE("rage.desync.slow_walk.optimal", selectable, XOR("Prefer optimal")),
		MAKE("rage.desync.slow_walk.prefer_speed", selectable, XOR("Prefer speed")),
	});

	ADD_TOOLTIP("Translate jump impulse", "rage.desync.translate_jump_impulse", "Breaks in-air animations. Works best with fast fire enabled", checkbox, cfg.antiaim.translate_jump);
	ADD_INLINE("Slow walk", slow_walk, MAKE("rage.desync.slow_walk_enable", checkbox, cfg.antiaim.slow_walk));
}

std::shared_ptr<layout> menu::group::rage_weapon(const std::string& group, int num, const vec2& size, bool vis)
{
	auto group_container = MAKE_RUNTIME(XOR("rage.weapon_tab.") + group, layout, vec2{}, size, s_justify);
	group_container->make_not_clip();
	group_container->is_visible = vis;
	group_container->add(make_groupbox(XOR("rage.weapon.") + group, XOR("Weapon"), size - vec2{0.f, 4.f}, [num, group](std::shared_ptr<layout>& e) {
		const auto group_n = XOR("rage.weapon.") + group + XOR(".");

		const auto secure_point = MAKE_RUNTIME(group_n + XOR("secure_point"), combo_box, cfg.rage.weapon_cfgs[num].secure_point);
		secure_point->add({
			MAKE_RUNTIME(group_n + XOR("secure_point.disabled"), selectable, XOR("Disabled"), cfg_t::secure_point_disabled),
			MAKE_RUNTIME(group_n + XOR("secure_point.default"), selectable, XOR("Default"), cfg_t::secure_point_default),
			MAKE_RUNTIME(group_n + XOR("secure_point.prefer"), selectable, XOR("Prefer"), cfg_t::secure_point_prefer),
			MAKE_RUNTIME(group_n + XOR("secure_point.force"), selectable, XOR("Force"), cfg_t::secure_point_force),
		});

		const auto secure_point_enable = MAKE_RUNTIME(group_n + XOR("secure_fast_fire"), checkbox, cfg.rage.weapon_cfgs[num].secure_fast_fire);
		secure_point_enable->make_tooltip(XOR("Secure fast fire"));
		secure_point_enable->hotkey_type = hkt_none;

		ADD_INLINE("Secure point", secure_point, secure_point_enable);

		const auto dynamic = MAKE_RUNTIME(group_n + XOR("dynamic"), checkbox, cfg.rage.weapon_cfgs[num].dynamic_min_dmg);
		dynamic->make_tooltip(XOR("Dynamic mode. Attempts to deal as much damage as possible and lowers to set value over time"));
		dynamic->hotkey_type = hkt_none;

		ADD_RUNTIME("Hit chance", group_n + XOR("hit_chance"), slider<float>, 0.f, 100.f, cfg.rage.weapon_cfgs[num].hitchance_value, XOR("%.0f%%"));
		ADD_INLINE("Minimal damage",
				   MAKE_RUNTIME(group_n + XOR("minimal_damage"), slider<float>, 0.f, 100.f, cfg.rage.weapon_cfgs[num].min_dmg, XOR("%.0fhp")),
				   dynamic);

		const auto hitboxes = MAKE_RUNTIME(group_n + XOR("hitboxes"), combo_box, cfg.rage.weapon_cfgs[num].hitbox);
		hitboxes->allow_multiple = true;
		hitboxes->add({
				MAKE_RUNTIME(group_n + XOR("hitboxes.head"), selectable, XOR("Head")),
				MAKE_RUNTIME(group_n + XOR("hitboxes.arms"), selectable, XOR("Arms")),
				MAKE_RUNTIME(group_n + XOR("hitboxes.upper_body"), selectable, XOR("Upper body")),
				MAKE_RUNTIME(group_n + XOR("hitboxes.lower_body"), selectable, XOR("Lower body")),
				MAKE_RUNTIME(group_n + XOR("hitboxes.legs"), selectable, XOR("Legs")),
		});

		ADD_INLINE("Hitboxes", hitboxes);
		const auto avoid_hitboxes = MAKE_RUNTIME(group_n + XOR("avoid_hitboxes"), combo_box, cfg.rage.weapon_cfgs[num].avoid_hitbox);
		avoid_hitboxes->allow_multiple = true;
		avoid_hitboxes->add({
				MAKE_RUNTIME(group_n + XOR("avoid_hitboxes.head"), selectable, XOR("Head")),
				MAKE_RUNTIME(group_n + XOR("avoid_hitboxes.arms"), selectable, XOR("Arms")),
				MAKE_RUNTIME(group_n + XOR("avoid_hitboxes.upper_body"), selectable, XOR("Upper body")),
				MAKE_RUNTIME(group_n + XOR("avoid_hitboxes.lower_body"), selectable, XOR("Lower body")),
				MAKE_RUNTIME(group_n + XOR("avoid_hitboxes.legs"), selectable, XOR("Legs")),
		});

		ADD_INLINE("Avoid insecure hitboxes", avoid_hitboxes);

		const auto lethal = MAKE_RUNTIME(group_n + XOR("lethal"), combo_box, cfg.rage.weapon_cfgs[num].lethal);
		lethal->allow_multiple = true;
		lethal->add({
				MAKE_RUNTIME(group_n + XOR("lethal.secure_points"), selectable, XOR("Force secure points")),
				MAKE_RUNTIME(group_n + XOR("lethal.body_aim"), selectable, XOR("Force body aim")),
				MAKE_RUNTIME(group_n + XOR("lethal.limbs"), selectable, XOR("Avoid limbs"))
		});

		ADD_INLINE("When lethal", lethal);

		ADD_RUNTIME("Multipoint", group_n + XOR("multipoint"), slider<float>, 0.f, 100.f, cfg.rage.weapon_cfgs[num].multipoint, XOR("%.0f%%"));

		if (num == grp_general)
			return;

		cfg.rage.weapon_cfgs[num].on_anything_changed = [group](bool status) {
			const auto box_deco = ctx->find<gui::group>(hash(XOR("rage.weapon.") + group));
			if (!box_deco)
				return;

			if (!status)
				box_deco->warning = XOR("Not configured. General config applies!");
			else
				box_deco->warning.clear();
		};
	}));

	return group_container;
}