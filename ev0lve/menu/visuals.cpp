
#include <menu/menu.h>
#include <menu/macros.h>
#include <base/cfg.h>
#include <gui/helpers.h>

USE_NAMESPACES;

#define MANAGE_CHAMS_COLORS_CB \
const auto manage_chams_colors_cb = [&](const std::string& key, bits& value) { \
	const auto primary_color = ctx->find<color_picker>(gui::hash(XOR("visuals.players.") + key + XOR(".primary_color"))); \
	const auto secondary_color = ctx->find<color_picker>(gui::hash(XOR("visuals.players.") + key + XOR(".secondary_color"))); \
	\
	if (primary_color) primary_color->set_visible(value.first_set_bit().value_or(0) > 0); \
	if (secondary_color) \
	{ \
		const auto val = value.first_set_bit().value_or(0); \
		secondary_color->set_visible(val == 3 || val == 4); \
	} \
};

void menu::group::visuals_players(std::shared_ptr<layout> &e)
{
	MANAGE_CHAMS_COLORS_CB;

	const auto enemy_esp = MAKE("visuals.players.enemy_esp", combo_box, cfg.player_esp.enemy);
	enemy_esp->allow_multiple = true;
	enemy_esp->add({
		MAKE("visuals.players.enemy_esp.box", selectable, XOR("Box")),
		MAKE("visuals.players.enemy_esp.text", selectable, XOR("Name")),
		MAKE("visuals.players.enemy_esp.health_bar", selectable, XOR("Health bar")),
		MAKE("visuals.players.enemy_esp.health_text", selectable, XOR("Health text")),
		MAKE("visuals.players.enemy_esp.armor", selectable, XOR("Armor")),
		MAKE("visuals.players.enemy_esp.ammo", selectable, XOR("Ammo")),
		MAKE("visuals.players.enemy_esp.weapon", selectable, XOR("Weapon")),
		MAKE("visuals.players.enemy_esp.flags", selectable, XOR("Flags")),
		MAKE("visuals.players.enemy_esp.distance", selectable, XOR("Distance")),
		MAKE("visuals.players.enemy_esp.warn_zeus", selectable, XOR("Zeus warning"), cfg_t::esp_warn_zeus),
		MAKE("visuals.players.enemy_esp.warn_fastfire", selectable, XOR("Fast fire warning"), cfg_t::esp_warn_fastfire)
	});

	const auto enemy_visible_color = MAKE("visuals.players.enemy_color_visible", color_picker, cfg.player_esp.enemy_color_visible)->make_tooltip(XOR_STR("Visible"));

	ADD_INLINE("Enemy ESP", enemy_esp, MAKE("visuals.players.enemy_esp_color", color_picker, cfg.player_esp.enemy_color)->make_tooltip(XOR_STR("Invisible")), enemy_visible_color);
	ADD_INLINE("Enemy skeleton", MAKE("visuals.players.enemy_skeleton", checkbox, cfg.player_esp.enemy_skeleton), MAKE("visuals.players.enemy_skeleton_color", color_picker, cfg.player_esp.enemy_skeleton_color));

	const auto team_esp = MAKE("visuals.players.team_esp", combo_box, cfg.player_esp.team);
	team_esp->allow_multiple = true;
	team_esp->add({
		MAKE("visuals.players.team_esp.box", selectable, XOR("Box")),
		MAKE("visuals.players.team_esp.text", selectable, XOR("Name")),
		MAKE("visuals.players.team_esp.health_bar", selectable, XOR("Health bar")),
		MAKE("visuals.players.team_esp.health_text", selectable, XOR("Health text")),
		MAKE("visuals.players.team_esp.armor", selectable, XOR("Armor")),
		MAKE("visuals.players.team_esp.ammo", selectable, XOR("Ammo")),
		MAKE("visuals.players.team_esp.weapon", selectable, XOR("Weapon")),
		MAKE("visuals.players.team_esp.flags", selectable, XOR("Flags")),
		MAKE("visuals.players.team_esp.distance", selectable, XOR("Distance")),
		MAKE("visuals.players.team_esp.warn_zeus", selectable, XOR("Zeus warning"), cfg_t::esp_warn_zeus),
		MAKE("visuals.players.team_esp.warn_fastfire", selectable, XOR("Fast fire warning"), cfg_t::esp_warn_fastfire)
	});

	const auto team_visible_color = MAKE("visuals.players.team_color_visible", color_picker, cfg.player_esp.team_color_visible)->make_tooltip(XOR_STR("Visible"));

	ADD_INLINE("Team ESP", team_esp, MAKE("visuals.players.team_esp_color", color_picker, cfg.player_esp.team_color)->make_tooltip(XOR_STR("Invisible")), team_visible_color);
	ADD_INLINE("Team skeleton", MAKE("visuals.players.team_skeleton", checkbox, cfg.player_esp.team_skeleton), MAKE("visuals.players.team_skeleton_color", color_picker, cfg.player_esp.team_skeleton_color));

	/*const auto legit_esp = MAKE("visuals.players.legit_esp_triggers", combo_box, cfg.player_esp.legit_triggers);
	legit_esp->allow_multiple = true;
	legit_esp->add({
		MAKE("visuals.players.legit_esp_triggers.visible", selectable, XOR("Visible")),
		MAKE("visuals.players.legit_esp_triggers.spotted", selectable, XOR("Spotted")),
		MAKE("visuals.players.legit_esp_triggers.sound", selectable, XOR("Sound")),
	});*/

	//ADD_INLINE("Legit ESP", legit_esp, MAKE("visuals.players.legit_esp", checkbox, cfg.player_esp.legit_esp));

	const auto check_visible = MAKE("visuals.players.check_visibility", checkbox, cfg.player_esp.vis_check);
	check_visible->callback = [&](bool value) {
		const auto enemy_color = ctx->find<color_picker>(gui::hash(XOR("visuals.players.enemy_color_visible")));
		const auto team_color = ctx->find<color_picker>(gui::hash(XOR("visuals.players.team_color_visible")));

		if (enemy_color) enemy_color->set_visible(value);
		if (team_color) team_color->set_visible(value);
	};

	ADD_INLINE("Check visibility", check_visible);
	ADD("Prefer icons", "visuals.players.prefer_icons", checkbox, cfg.player_esp.prefer_icons);

	ADD_INLINE("Offscreen ESP",
			   MAKE("visuals.players.offscreen_esp", checkbox, cfg.player_esp.offscreen_esp),
			   MAKE("visuals.players.offscreen_esp_radius", slider<float>, 60.f, 600.f, cfg.player_esp.offscreen_radius, XOR("%.0fpx")));

	const auto enemy_chams = MAKE("visuals.players.enemy_chams", combo_box, cfg.player_esp.enemy_chams.mode);
	enemy_chams->add({
		MAKE("visuals.players.enemy_chams.disabled", selectable, XOR("Disabled")),
		MAKE("visuals.players.enemy_chams.default", selectable, XOR("Default")),
		MAKE("visuals.players.enemy_chams.flat", selectable, XOR("Flat")),
		MAKE("visuals.players.enemy_chams.glow", selectable, XOR("Glow")),
		MAKE("visuals.players.enemy_chams.pulse", selectable, XOR("Pulse")),
		MAKE("visuals.players.enemy_chams.acryl", selectable, XOR("Acryl"))
	});

	enemy_chams->callback = [&](bits& value) { manage_chams_colors_cb(XOR_STR("enemy_chams"), value); };

	ADD_INLINE("Enemy chams");
	ADD_LINE("visuals.players.enemy_chams", enemy_chams,
			 MAKE("visuals.players.enemy_chams.primary_color", color_picker, cfg.player_esp.enemy_chams.primary)->make_tooltip(XOR("Primary color"))->make_invisible(),
			 MAKE("visuals.players.enemy_chams.secondary_color", color_picker, cfg.player_esp.enemy_chams.secondary)->make_tooltip(XOR("Secondary color"))->make_invisible());

	const auto enemy_attachment_chams = MAKE("visuals.players.enemy_attachment_chams", combo_box, cfg.player_esp.enemy_attachment_chams.mode);
	enemy_attachment_chams->add({
			MAKE("visuals.players.enemy_attachment_chams.disabled", selectable, XOR("Disabled")),
			MAKE("visuals.players.enemy_attachment_chams.default", selectable, XOR("Default")),
			MAKE("visuals.players.enemy_attachment_chams.flat", selectable, XOR("Flat")),
			MAKE("visuals.players.enemy_attachment_chams.glow", selectable, XOR("Glow")),
			MAKE("visuals.players.enemy_attachment_chams.pulse", selectable, XOR("Pulse")),
			MAKE("visuals.players.enemy_attachment_chams.acryl", selectable, XOR("Acryl"))
	});

	enemy_attachment_chams->callback = [&](bits& value) { manage_chams_colors_cb(XOR_STR("enemy_attachment_chams"), value); };

	ADD_INLINE("Enemy attachment chams");
	ADD_LINE("visuals.players.enemy_attachment_chams", enemy_attachment_chams,
			MAKE("visuals.players.enemy_attachment_chams.primary_color", color_picker, cfg.player_esp.enemy_attachment_chams.primary)->make_tooltip(XOR("Primary color"))->make_invisible(),
			MAKE("visuals.players.enemy_attachment_chams.secondary_color", color_picker, cfg.player_esp.enemy_attachment_chams.secondary)->make_tooltip(XOR("Secondary color"))->make_invisible());

	const auto enemy_chams_vis = MAKE("visuals.players.enemy_chams_visible", combo_box, cfg.player_esp.enemy_chams_visible.mode);
	enemy_chams_vis->add({
		MAKE("visuals.players.enemy_chams_visible.disabled", selectable, XOR("Disabled")),
		MAKE("visuals.players.enemy_chams_visible.default", selectable, XOR("Default")),
		MAKE("visuals.players.enemy_chams_visible.flat", selectable, XOR("Flat")),
		MAKE("visuals.players.enemy_chams_visible.glow", selectable, XOR("Glow")),
		MAKE("visuals.players.enemy_chams_visible.pulse", selectable, XOR("Pulse")),
		MAKE("visuals.players.enemy_chams_visible.acryl", selectable, XOR("Acryl"))
	});

	enemy_chams_vis->callback = [&](bits& value) { manage_chams_colors_cb(XOR_STR("enemy_chams_visible"), value); };

	ADD_INLINE("Enemy chams (vis.)");
	ADD_LINE("visuals.players.enemy_chams_visible", enemy_chams_vis,
			   MAKE("visuals.players.enemy_chams_visible.primary_color", color_picker, cfg.player_esp.enemy_chams_visible.primary)->make_tooltip(XOR("Primary color"))->make_invisible(),
			   MAKE("visuals.players.enemy_chams_visible.secondary_color", color_picker, cfg.player_esp.enemy_chams_visible.secondary)->make_tooltip(XOR("Secondary color"))->make_invisible());

	const auto enemy_attachment_chams_vis = MAKE("visuals.players.enemy_chams_visible", combo_box, cfg.player_esp.enemy_attachment_chams_visible.mode);
	enemy_attachment_chams_vis->add({
			MAKE("visuals.players.enemy_attachment_chams_visible.disabled", selectable, XOR("Disabled")),
			MAKE("visuals.players.enemy_attachment_chams_visible.default", selectable, XOR("Default")),
			MAKE("visuals.players.enemy_attachment_chams_visible.flat", selectable, XOR("Flat")),
			MAKE("visuals.players.enemy_attachment_chams_visible.glow", selectable, XOR("Glow")),
			MAKE("visuals.players.enemy_attachment_chams_visible.pulse", selectable, XOR("Pulse")),
			MAKE("visuals.players.enemy_attachment_chams_visible.acryl", selectable, XOR("Acryl"))
	});

	enemy_attachment_chams_vis->callback = [&](bits& value) { manage_chams_colors_cb(XOR_STR("enemy_attachment_chams_visible"), value); };

	ADD_INLINE("Enemy attachment chams (vis.)");
	ADD_LINE("visuals.players.enemy_attachment_chams_visible", enemy_attachment_chams_vis,
			MAKE("visuals.players.enemy_attachment_chams_visible.primary_color", color_picker, cfg.player_esp.enemy_attachment_chams_visible.primary)->make_tooltip(XOR("Primary color"))->make_invisible(),
			MAKE("visuals.players.enemy_attachment_chams_visible.secondary_color", color_picker, cfg.player_esp.enemy_attachment_chams_visible.secondary)->make_tooltip(XOR("Secondary color"))->make_invisible());

	const auto enemy_chams_his = MAKE("visuals.players.enemy_chams_history", combo_box, cfg.player_esp.enemy_chams_history.mode);
	enemy_chams_his->add({
			MAKE("visuals.players.enemy_chams_history.disabled", selectable, XOR("Disabled")),
			MAKE("visuals.players.enemy_chams_history.default", selectable, XOR("Default")),
			MAKE("visuals.players.enemy_chams_history.flat", selectable, XOR("Flat")),
			MAKE("visuals.players.enemy_chams_history.glow", selectable, XOR("Glow")),
			MAKE("visuals.players.enemy_chams_history.pulse", selectable, XOR("Pulse")),
			MAKE("visuals.players.enemy_chams_history.acryl", selectable, XOR("Acryl"))
	});

	ADD_INLINE("Enemy chams (his.)");
	ADD_LINE("visuals.players.enemy_chams_history", enemy_chams_his,
			MAKE("visuals.players.enemy_chams_history.primary_color", color_picker, cfg.player_esp.enemy_chams_history.primary)->make_tooltip(XOR("Primary color")),
			MAKE("visuals.players.enemy_chams_history.secondary_color", color_picker, cfg.player_esp.enemy_chams_history.secondary)->make_tooltip(XOR("Secondary color")));

	enemy_chams_his->callback = [&](bits& value) { manage_chams_colors_cb(XOR_STR("enemy_chams_history"), value); };
	
	const auto team_chams = MAKE("visuals.players.team_chams", combo_box, cfg.player_esp.team_chams.mode);
	team_chams->add({
		MAKE("visuals.players.team_chams.disabled", selectable, XOR("Disabled")),
		MAKE("visuals.players.team_chams.default", selectable, XOR("Default")),
		MAKE("visuals.players.team_chams.flat", selectable, XOR("Flat")),
		MAKE("visuals.players.team_chams.glow", selectable, XOR("Glow")),
		MAKE("visuals.players.team_chams.pulse", selectable, XOR("Pulse")),
		MAKE("visuals.players.team_chams.acryl", selectable, XOR("Acryl"))
	});

	team_chams->callback = [&](bits& value) { manage_chams_colors_cb(XOR_STR("team_chams"), value); };

	ADD_INLINE("Team chams");
	ADD_LINE("visuals.players.team_chams", team_chams,
			   MAKE("visuals.players.team_chams.primary_color", color_picker, cfg.player_esp.team_chams.primary)->make_tooltip(XOR("Primary color"))->make_invisible(),
			   MAKE("visuals.players.team_chams.secondary_color", color_picker, cfg.player_esp.team_chams.secondary)->make_tooltip(XOR("Secondary color"))->make_invisible());

	const auto team_attachment_chams = MAKE("visuals.players.team_attachment_chams", combo_box, cfg.player_esp.team_attachment_chams.mode);
	team_attachment_chams->add({
			MAKE("visuals.players.team_attachment_chams.disabled", selectable, XOR("Disabled")),
			MAKE("visuals.players.team_attachment_chams.default", selectable, XOR("Default")),
			MAKE("visuals.players.team_attachment_chams.flat", selectable, XOR("Flat")),
			MAKE("visuals.players.team_attachment_chams.glow", selectable, XOR("Glow")),
			MAKE("visuals.players.team_attachment_chams.pulse", selectable, XOR("Pulse")),
			MAKE("visuals.players.team_attachment_chams.acryl", selectable, XOR("Acryl"))
	});

	team_attachment_chams->callback = [&](bits& value) { manage_chams_colors_cb(XOR_STR("team_attachment_chams"), value); };

	ADD_INLINE("Team attachment chams");
	ADD_LINE("visuals.players.team_attachment_chams", team_attachment_chams,
			MAKE("visuals.players.team_attachment_chams.primary_color", color_picker, cfg.player_esp.team_attachment_chams.primary)->make_tooltip(XOR("Primary color"))->make_invisible(),
			MAKE("visuals.players.team_attachment_chams.secondary_color", color_picker, cfg.player_esp.team_attachment_chams.secondary)->make_tooltip(XOR("Secondary color"))->make_invisible());

	const auto team_chams_vis = MAKE("visuals.players.team_chams_visible", combo_box, cfg.player_esp.team_chams_visible.mode);
	team_chams_vis->add({
		MAKE("visuals.players.team_chams_visible.disabled", selectable, XOR("Disabled")),
		MAKE("visuals.players.team_chams_visible.default", selectable, XOR("Default")),
		MAKE("visuals.players.team_chams_visible.flat", selectable, XOR("Flat")),
		MAKE("visuals.players.team_chams_visible.glow", selectable, XOR("Glow")),
		MAKE("visuals.players.team_chams_visible.pulse", selectable, XOR("Pulse")),
		MAKE("visuals.players.team_chams_visible.acryl", selectable, XOR("Acryl"))
	});

	team_chams_vis->callback = [&](bits& value) { manage_chams_colors_cb(XOR_STR("team_chams_visible"), value); };

	ADD_INLINE("Team chams (vis.)");
	ADD_LINE("visuals.players.team_chams_visible", team_chams_vis,
			   MAKE("visuals.players.team_chams_visible.primary_color", color_picker, cfg.player_esp.team_chams_visible.primary)->make_tooltip(XOR("Primary color"))->make_invisible(),
			   MAKE("visuals.players.team_chams_visible.secondary_color", color_picker, cfg.player_esp.team_chams_visible.secondary)->make_tooltip(XOR("Secondary color"))->make_invisible());

	const auto team_attachment_chams_vis = MAKE("visuals.players.team_attachment_chams_visible", combo_box, cfg.player_esp.team_attachment_chams_visible.mode);
	team_attachment_chams_vis->add({
			MAKE("visuals.players.team_attachment_chams_visible.disabled", selectable, XOR("Disabled")),
			MAKE("visuals.players.team_attachment_chams_visible.default", selectable, XOR("Default")),
			MAKE("visuals.players.team_attachment_chams_visible.flat", selectable, XOR("Flat")),
			MAKE("visuals.players.team_attachment_chams_visible.glow", selectable, XOR("Glow")),
			MAKE("visuals.players.team_attachment_chams_visible.pulse", selectable, XOR("Pulse")),
			MAKE("visuals.players.team_attachment_chams_visible.acryl", selectable, XOR("Acryl"))
	});

	team_attachment_chams_vis->callback = [&](bits& value) { manage_chams_colors_cb(XOR_STR("team_attachment_chams_visible"), value); };

	ADD_INLINE("Team attachment chams (vis.)");
	ADD_LINE("visuals.players.team_attachment_chams_visible", team_attachment_chams_vis,
			MAKE("visuals.players.team_attachment_chams_visible.primary_color", color_picker, cfg.player_esp.team_attachment_chams_visible.primary)->make_tooltip(XOR("Primary color"))->make_invisible(),
			MAKE("visuals.players.team_attachment_chams_visible.secondary_color", color_picker, cfg.player_esp.team_attachment_chams_visible.secondary)->make_tooltip(XOR("Secondary color"))->make_invisible());

	ADD("Apply on ragdolls", "visuals.players.apply_on_ragdolls", checkbox, cfg.player_esp.apply_on_death);

	ADD_INLINE("Glow",
			   MAKE("visuals.players.glow_enemy", checkbox, cfg.player_esp.glow_enemy)->make_tooltip(XOR("Enemy")),
			   MAKE("visuals.players.glow_enemy_color", color_picker, cfg.player_esp.enemy_color_glow)->make_tooltip(XOR("Enemy")),
			   MAKE("visuals.players.glow_team", checkbox, cfg.player_esp.glow_team)->make_tooltip(XOR("Team")),
			   MAKE("visuals.players.glow_team_color", color_picker, cfg.player_esp.team_color_glow)->make_tooltip(XOR("Team")));

	ADD_INLINE("Visualize target scan",
			   MAKE("visuals.players.visualize_target_scan", checkbox, cfg.player_esp.target_scan),
			   MAKE("visuals.players.visualize_target_scan_secure", color_picker, cfg.player_esp.target_scan_secure, false)->make_tooltip(XOR("Secure")),
			   MAKE("visuals.players.visualize_target_scan_aim", color_picker, cfg.player_esp.target_scan_aim, false)->make_tooltip(XOR("Aim")));
}

void menu::group::visuals_local(std::shared_ptr<layout> &e)
{
	MANAGE_CHAMS_COLORS_CB;

	const auto local_chams = MAKE("visuals.local.chams", combo_box, cfg.local_visuals.local_chams.mode);
	local_chams->add({
		MAKE("visuals.local.chams.disabled", selectable, XOR("Disabled")),
		MAKE("visuals.local.chams.default", selectable, XOR("Default")),
		MAKE("visuals.local.chams.flat", selectable, XOR("Flat")),
		MAKE("visuals.local.chams.glow", selectable, XOR("Glow")),
		MAKE("visuals.local.chams.pulse", selectable, XOR("Pulse")),
		MAKE("visuals.local.chams.acryl", selectable, XOR("Acryl"))
	});

	local_chams->callback = [&](bits& value) { manage_chams_colors_cb(XOR_STR("chams"), value); };

	ADD_INLINE("Player chams");
	ADD_LINE("visuals.players.chams", local_chams,
			 MAKE("visuals.players.chams.primary_color", color_picker, cfg.local_visuals.local_chams.primary)->make_tooltip(XOR("Primary color")),
			 MAKE("visuals.players.chams.secondary_color", color_picker, cfg.local_visuals.local_chams.secondary)->make_tooltip(XOR("Secondary color")));

	const auto local_attachment_chams = MAKE("visuals.local.attachment.chams", combo_box, cfg.local_visuals.local_attachment_chams.mode);
	local_attachment_chams->add({
			MAKE("visuals.local.attachment.chams.disabled", selectable, XOR("Disabled")),
			MAKE("visuals.local.attachment.chams.default", selectable, XOR("Default")),
			MAKE("visuals.local.attachment.chams.flat", selectable, XOR("Flat")),
			MAKE("visuals.local.attachment.chams.glow", selectable, XOR("Glow")),
			MAKE("visuals.local.attachment.chams.pulse", selectable, XOR("Pulse")),
			MAKE("visuals.local.attachment.chams.acryl", selectable, XOR("Acryl"))
	});

	local_attachment_chams->callback = [&](bits& value) { manage_chams_colors_cb(XOR_STR("attachment_chams"), value); };

	ADD_INLINE("Attachment chams");
	ADD_LINE("visuals.players.attachment_chams", local_attachment_chams,
			MAKE("visuals.players.attachment_chams.primary_color", color_picker, cfg.local_visuals.local_attachment_chams.primary)->make_tooltip(XOR("Primary color")),
			MAKE("visuals.players.attachment_chams.secondary_color", color_picker, cfg.local_visuals.local_attachment_chams.secondary)->make_tooltip(XOR("Secondary color")));

	const auto fake_chams = MAKE("visuals.local.fake_chams", combo_box, cfg.local_visuals.fake_chams.mode);
	fake_chams->add({
		MAKE("visuals.local.fake_chams.disabled", selectable, XOR("Disabled")),
		MAKE("visuals.local.fake_chams.default", selectable, XOR("Default")),
		MAKE("visuals.local.fake_chams.flat", selectable, XOR("Flat")),
		MAKE("visuals.local.fake_chams.glow", selectable, XOR("Glow")),
		MAKE("visuals.local.fake_chams.pulse", selectable, XOR("Pulse")),
		MAKE("visuals.local.fake_chams.acryl", selectable, XOR("Acryl"))
	});

	fake_chams->callback = [&](bits& value) { manage_chams_colors_cb(XOR_STR("fake_chams"), value); };

	ADD_INLINE("Fake chams");
	ADD_LINE("visuals.players.fake_chams", fake_chams,
			 MAKE("visuals.players.fake_chams.primary_color", color_picker, cfg.local_visuals.fake_chams.primary)->make_tooltip(XOR("Primary color")),
			 MAKE("visuals.players.fake_chams.secondary_color", color_picker, cfg.local_visuals.fake_chams.secondary)->make_tooltip(XOR("Secondary color")));

	const auto weapon_chams = MAKE("visuals.local.weapon_chams", combo_box, cfg.local_visuals.weapon_chams.mode);
	weapon_chams->add({
		MAKE("visuals.local.weapon_chams.disabled", selectable, XOR("Disabled")),
		MAKE("visuals.local.weapon_chams.default", selectable, XOR("Default")),
		MAKE("visuals.local.weapon_chams.flat", selectable, XOR("Flat")),
		MAKE("visuals.local.weapon_chams.glow", selectable, XOR("Glow")),
		MAKE("visuals.local.weapon_chams.pulse", selectable, XOR("Pulse")),
		MAKE("visuals.local.weapon_chams.acryl", selectable, XOR("Acryl"))
	});

	weapon_chams->callback = [&](bits& value) { manage_chams_colors_cb(XOR_STR("weapon_chams"), value); };

	ADD_INLINE("Weapon chams");
	ADD_LINE("visuals.players.weapon_chams", weapon_chams,
			 MAKE("visuals.players.weapon_chams.primary_color", color_picker, cfg.local_visuals.weapon_chams.primary)->make_tooltip(XOR("Primary color")),
			 MAKE("visuals.players.weapon_chams.secondary_color", color_picker, cfg.local_visuals.weapon_chams.secondary)->make_tooltip(XOR("Secondary color")));

	const auto arms_chams = MAKE("visuals.local.arms_chams", combo_box, cfg.local_visuals.arms_chams.mode);
	arms_chams->add({
		MAKE("visuals.local.arms_chams.disabled", selectable, XOR("Disabled")),
		MAKE("visuals.local.arms_chams.default", selectable, XOR("Default")),
		MAKE("visuals.local.arms_chams.flat", selectable, XOR("Flat")),
		MAKE("visuals.local.arms_chams.glow", selectable, XOR("Glow")),
		MAKE("visuals.local.arms_chams.pulse", selectable, XOR("Pulse")),
		MAKE("visuals.local.arms_chams.acryl", selectable, XOR("Acryl"))
	});

	arms_chams->callback = [&](bits& value) { manage_chams_colors_cb(XOR_STR("arms_chams"), value); };

	ADD_INLINE("Arms chams");
	ADD_LINE("visuals.players.arms_chams", arms_chams,
			 MAKE("visuals.players.arms_chams.primary_color", color_picker, cfg.local_visuals.arms_chams.primary)->make_tooltip(XOR("Primary color")),
			 MAKE("visuals.players.arms_chams.secondary_color", color_picker, cfg.local_visuals.arms_chams.secondary)->make_tooltip(XOR("Secondary color")));

	ADD_INLINE("Glow",
			   MAKE("visuals.local.glow", checkbox, cfg.local_visuals.glow_local),
			   MAKE("visuals.local.glow_color", color_picker, cfg.local_visuals.local_color_glow));

	ADD_INLINE("Thirdperson",
			   MAKE("visuals.local.thirdperson", checkbox, cfg.local_visuals.thirdperson),
			   MAKE("visuals.local.thirdperson_distance", slider<float>, 50.f, 200.f, cfg.local_visuals.thirdperson_distance, XOR("%.0fu")));

	ADD("Thirdperson while dead", "visuals.local.thirdperson_dead", checkbox, cfg.local_visuals.thirdperson_dead);

	ADD("Blend alpha in thirdperson", "visuals.local.alpha_blend", checkbox, cfg.local_visuals.alpha_blend);

	const auto hitmarker_style = MAKE("visuals.local.hitmarker_style", combo_box, cfg.local_visuals.hitmarker_style);
	hitmarker_style->add({
		MAKE("visuals.local.hitmarker_style.default", selectable, XOR("Default")),
		MAKE("visuals.local.hitmarker_style.animated", selectable, XOR("Animated")),
	});

	ADD_INLINE("Hitmarker", MAKE("visuals.local.hitmarker", checkbox, cfg.local_visuals.hitmarker), hitmarker_style);

	ADD_INLINE("FOV", MAKE("visuals.local.fov", checkbox, cfg.local_visuals.fov_changer));
	ADD_LINE("visuals.local.fov", MAKE("visuals.local.fov_value", slider<float>, -40.f, 40.f, cfg.local_visuals.fov, XOR("%.0f°"))->make_tooltip(XOR("Normal FOV override")),
			MAKE("visuals.local.fov2_value", slider<float>, -40.f, 40.f, cfg.local_visuals.fov2, XOR("%.0f°"))->make_tooltip(XOR("Scoped FOV override")));
	ADD("Skip first zoom level", "visuals.local.skip_first_scope", checkbox, cfg.local_visuals.skip_first_scope);
	ADD("Render viewmodel in zoom", "visuals.local.viewmodel_in_scope", checkbox, cfg.local_visuals.viewmodel_in_scope);

	ADD_INLINE("Bullet impacts",
			   MAKE("visuals.local.bullet_impacts", checkbox, cfg.local_visuals.impacts),
			   MAKE("visuals.local.bullet_impacts_server", color_picker, cfg.local_visuals.server_impacts)->make_tooltip(XOR("Server")),
			   MAKE("visuals.local.bullet_impacts_client", color_picker, cfg.local_visuals.client_impacts)->make_tooltip(XOR("Client")));

	const auto hit_indicator = MAKE("visuals.local.hit_indicator", combo_box, cfg.local_visuals.hit_indicator);
	hit_indicator->allow_multiple = true;
	hit_indicator->add({
		MAKE("visuals.local.hit_indicator.on_hit", selectable, XOR("On hit")),
		MAKE("visuals.local.hit_indicator.on_kill", selectable, XOR("On kill")),
	});

	ADD_INLINE("Hit indicator", hit_indicator,
			   MAKE("visuals.local.hit_indicator_color", color_picker, cfg.local_visuals.hit_color));

	const auto aa_indicator = MAKE("visuals.local.antiaim_indicator", combo_box, cfg.local_visuals.aa_indicator);
	aa_indicator->allow_multiple = true;
	aa_indicator->add({
		MAKE("visuals.local.antiaim_indicator.real", selectable, XOR("Real")),
		MAKE("visuals.local.antiaim_indicator.desync", selectable, XOR("Desync")),
	});

	ADD_INLINE("AA", aa_indicator,
			   MAKE("visuals.local.antiaim_indicator_real", color_picker, cfg.local_visuals.clr_aa_real)->make_tooltip(XOR("Real")),
			   MAKE("visuals.local.antiaim_indicator_desync", color_picker, cfg.local_visuals.clr_aa_desync)->make_tooltip(XOR("Desync")));

	ADD_INLINE("Visualize grenade path",
			   MAKE("visuals.local.visualize_grenade_path", checkbox, cfg.world_esp.visualize_nade_path),
			   MAKE("visuals.local.visualize_grenade_path_color", color_picker, cfg.world_esp.nade_path_color));

	ADD_INLINE("Aspect ratio",
			   MAKE("visuals.local.aspect_ratio", checkbox, cfg.local_visuals.aspect_changer),
			   MAKE("visuals.local.aspect_ratio_value", slider<float>, 0.f, 100.f, cfg.local_visuals.aspect, XOR("%.0f%%")));
}

void menu::group::visuals_world(std::shared_ptr<layout> &e)
{
	const auto weapons_esp = MAKE("visuals.world.weapons", combo_box, cfg.world_esp.weapon);
	weapons_esp->allow_multiple = true;
	weapons_esp->add({
		MAKE("visuals.world.weapons.text", selectable, XOR("Text"), cfg_t::esp_text),
		MAKE("visuals.world.weapons.ammo", selectable, XOR("Ammo"), cfg_t::esp_ammo),
		MAKE("visuals.world.weapons.distance", selectable, XOR("Distance"), cfg_t::esp_distance)
	});

	ADD_INLINE("Weapon", weapons_esp,
			   MAKE("visuals.world.weapons_color", color_picker, cfg.world_esp.weapon_color));

	const auto objective_esp = MAKE("visuals.world.objective", combo_box, cfg.world_esp.objective);
	objective_esp->allow_multiple = true;
	objective_esp->add({
		MAKE("visuals.world.objective.text", selectable, XOR("Text"), cfg_t::esp_text),
		MAKE("visuals.world.objective.distance", selectable, XOR("Distance"), cfg_t::esp_distance)
	});

	ADD_INLINE("Objective", objective_esp,
			   MAKE("visuals.world.objective_color", color_picker, cfg.world_esp.objective_color));

	const auto nades_esp = MAKE("visuals.world.nades", combo_box, cfg.world_esp.nades);
	nades_esp->allow_multiple = true;
	nades_esp->size.x = 80.f;
	nades_esp->add({
		MAKE("visuals.world.nades.text", selectable, XOR("Text"), cfg_t::esp_text),
		MAKE("visuals.world.nades.expiry", selectable, XOR("Expiry"), cfg_t::esp_expiry)
	});

	ADD_INLINE("Nades", nades_esp,
			   MAKE("visuals.world.nades_fire_color", color_picker, cfg.world_esp.fire_color)->make_tooltip(XOR("Fire")),
			   MAKE("visuals.world.nades_smoke_color", color_picker, cfg.world_esp.smoke_color)->make_tooltip(XOR("Smoke")),
			   MAKE("visuals.world.nades_flash_color", color_picker, cfg.world_esp.flash_color)->make_tooltip(XOR("Flash")));

	const auto nade_warning = MAKE("visuals.world.nade_warning_options", combo_box, cfg.world_esp.grenade_warning_options);
	nade_warning->allow_multiple = true;
	nade_warning->size.x = 60.f;
	nade_warning->add({
		MAKE("visuals.world.nade_warning.only_near", selectable, XOR("Only on close impact"), cfg_t::grenade_warning_only_near),
		MAKE("visuals.world.nade_warning.only_hostile", selectable, XOR("Only for hostiles"), cfg_t::grenade_warning_only_hostile),
		MAKE("visuals.world.nade_warning.show_type", selectable, XOR("Display type"), cfg_t::grenade_warning_show_type),
	});

	ADD_INLINE("Nade warning", MAKE("visuals.world.nade_warning", checkbox, cfg.world_esp.grenade_warning), nade_warning,
			MAKE("visuals.world.nade_warning.progress", color_picker, cfg.world_esp.grenade_warning_progress)->make_tooltip(XOR("Progress")),
			MAKE("visuals.world.nade_warning.damage", color_picker, cfg.world_esp.grenade_warning_damage)->make_tooltip(XOR("Damage")));

	ADD_INLINE("Night mode",
			   MAKE("visuals.world.night_mode", checkbox, cfg.local_visuals.night_mode),
			   MAKE("visuals.world.night_mode_value", slider<float>, 0.f, 100.f, cfg.local_visuals.night_mode_value, XOR("%.0f%%")),
			   MAKE("visuals.world.night_mode_color", color_picker, cfg.local_visuals.night_color, false));

	ADD_INLINE("Transparent props",
			   MAKE("visuals.world.transparent_props", slider<float>, 0.f, 100.f, cfg.local_visuals.transparent_prop_value, XOR("%.0f%%")));

	ADD("Skip post processing", "visuals.world.skip_post_processing", checkbox, cfg.local_visuals.disable_post_processing);
}

void menu::group::visuals_removals(std::shared_ptr<layout> &e)
{
	ADD("Disable visual recoil", "visuals.removals.disable_visual_recoil", checkbox, cfg.removals.no_visual_recoil);
	ADD("Disable flashbang effect", "visuals.removals.disable_flashbang_effect", checkbox, cfg.removals.no_flash);
	ADD("Disable smoke effect", "visuals.removals.disable_smoke_effect", checkbox, cfg.removals.no_smoke);

	const auto scope_color = MAKE("visuals.removals.override_scope_color", color_picker, cfg.removals.scope_color);

	const auto scope = MAKE("visuals.removals.override_scope", combo_box, cfg.removals.scope);
	scope->add({
			MAKE("visuals.removals.override_scope.default", selectable, XOR("Default"), cfg_t::scope_default),
			MAKE("visuals.removals.override_scope.static", selectable, XOR("Static"), cfg_t::scope_static),
			MAKE("visuals.removals.override_scope.dynamic", selectable, XOR("Dynamic"), cfg_t::scope_dynamic),
			MAKE("visuals.removals.override_scope.small", selectable, XOR("Small"), cfg_t::scope_small),
			MAKE("visuals.removals.override_scope.small_dynamic", selectable, XOR("Small (dynamic)"), cfg_t::scope_small_dynamic)
	});

	scope->callback = [scope_color, scope](bits& v) {
		scope_color->set_visible(v.test(cfg_t::scope_small) || v.test(cfg_t::scope_small_dynamic));
	};

	ADD_INLINE("Override scope", scope, scope_color);
}