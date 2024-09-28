
#ifndef BASE_CFG_H
#define BASE_CFG_H

#include <fstream>
#include <sdk/color.h>
#include <util/misc.h>

#include <gui/values.h>
#include <ren/types.h>
#include <sdk/surface.h>

namespace sdk
{
	class weapon_t;
}

enum weapon_groups
{
	grp_general,
	grp_pistol,
	grp_heavypistol,
	grp_automatic,
	grp_awp,
	grp_scout,
	grp_autosniper,
	grp_zeus,
	grp_max
};

template<typename T>
using setting = evo::gui::value_param<T>;

struct cfg_t
{
	static weapon_groups get_group(sdk::weapon_t* wpn);
	void on_create_move(sdk::weapon_t* wpn);

	enum aimbot_mode
	{
		mode_onkey,
		mode_onfire,
		mode_max
	};

	enum clantag_style
	{
		clantag_off,
		clantag_build,
		clantag_scroll,
		clantag_static,
		clantag_max
	};

	enum esp_style
	{
		esp_box,
		esp_text,
		esp_health_bar,
		esp_health_text,
		esp_armor,
		esp_ammo,
		esp_weapon,
		esp_flags,
		esp_distance,
		esp_expiry,
		esp_warn_zeus,
		esp_warn_fastfire,
		esp_max
	};

	enum grenade_warning
	{
		grenade_warning_only_near,
		grenade_warning_only_hostile,
		grenade_warning_show_type,
		grenade_warning_max
	};

	enum aa_indicator
	{
		aa_real,
		aa_desync,
		aa_max
	};

	enum pitch_antiaim
	{
		pitch_none,
		pitch_down,
		pitch_up,
		pitch_max
	};

	enum yaw_antiaim
	{
		yaw_viewangle,
		yaw_at_target,
		yaw_freestanding,
		yaw_max
	};

	enum yaw_override_antiaim
	{
		yaw_none,
		yaw_left,
		yaw_right,
		yaw_back,
		yaw_forward,
		yaw_override_max
	};

	enum yaw_modifier_antiaim
	{
		yaw_modifier_none,
		yaw_modifier_directional,
		yaw_modifier_jitter,
		yaw_modifier_half_jitter,
		yaw_modifier_random,
		yaw_modifier_max
	};

	enum desync_antiaim
	{
		desync_none,
		desync_static,
		desync_opposite,
		desync_jitter,
		desync_random,
		desync_freestanding,
		desync_max
	};

	enum body_lean_antiaim
	{
		body_lean_none,
		body_lean_static,
		body_lean_jitter,
		body_lean_random,
		body_lean_freestanding,
		body_lean_max
	};

	enum antiaim_disabler
	{
		antiaim_disabler_round_end,
		antiaim_disabler_knife,
		antiaim_disabler_defuse,
		antiaim_disabler_shield,
		antiaim_disabler_max
	};

	enum events
	{
		event_shot_info,
		event_dmg_taken,
		event_purchase,
		event_max
	};

	enum eventlog_mode
	{
		eventlog_console,
		eventlog_chat,
		eventlog_top_left,
		eventlog_max
	};

	enum rage_mode
	{
		rage_just_in_time,
		rage_ahead_of_time,
		rage_ahead_of_time_fallback,
		rage_max
	};

	enum aim
	{
		aim_head,
		aim_arms,
		aim_upper_body,
		aim_lower_body,
		aim_legs,
		aim_max
	};

	enum secure_point
	{
		secure_point_default,
		secure_point_prefer,
		secure_point_force,
		secure_point_disabled,
		secure_point_max
	};

	enum lethal
	{
		lethal_secure_points,
		lethal_body_aim,
		lethal_limbs
	};

	enum chams_mode
	{
		chams_disabled,
		chams_default,
		chams_flat,
		chams_glow,
		chams_pulse,
		chams_acryl,
		chams_max
	};

	enum hit_indicators
	{
		hit_on_hit,
		hit_on_kill,
		hit_max
	};

	enum autostrafe_mode
	{
		autostrafe_disabled,
		autostrafe_view,
		autostrafe_move,
		autostrafe_max
	};

	enum slowwalk_mode
	{
		slowwalk_optimal,
		slowwalk_speed,
		slowwalk_max
	};

	enum movement_mode
	{
		movement_default,
		movement_skate,
		movement_walk,
		movement_opposite,
		movement_max
	};

	enum fast_fire_mode
	{
		fast_fire_default,
		fast_fire_peek,
		fast_fire_max
	};

	enum peek_assistant_mode
	{
		peek_assistant_free_roam,
		peek_assistant_retreat_early,
		peek_assistant_max
	};

	enum legit_triggers
	{
		legit_visible,
		legit_spotted,
		legit_sound,
		legit_max
	};

	enum scope_mode
	{
		scope_default,
		scope_static,
		scope_dynamic,
		scope_small,
		scope_small_dynamic,
		scope_max
	};

	enum hitmarker_style
	{
		hitmarker_default,
		hitmarker_animated,
		hitmarker_max,
	};

	using bitset = evo::gui::bits;
	using color = evo::ren::color;

	struct chams
	{
		chams(bitset m, color p, color s)
		{
			mode = setting<bitset>(m);
			primary = setting<color>(p);
			secondary = setting<color>(s);
		}

		setting<bitset> mode;
		setting<color> primary, secondary;
	};

	struct player_esp_s
	{
		setting<bitset> enemy{ };
		setting<bitset> team{ };
		setting<bool> enemy_skeleton{ }, team_skeleton{ }, vis_check{ };
		setting<color> enemy_color{color::white()}, team_color{color::white()};
		setting<color> enemy_skeleton_color{color::white()}, team_skeleton_color{color::white()};
		setting<color> enemy_color_visible{color::white()}, team_color_visible{color::white()};

		setting<bool> prefer_icons{}, apply_on_death{};

		setting<bool> glow_enemy{ }, glow_team{ };
		setting<color> enemy_color_glow{color(255, 0, 255)}, team_color_glow{color::white()};

		chams enemy_chams = chams({ 1 << chams_disabled, color(250, 250, 250, 125), color(59, 59, 59, 250) });
		chams enemy_attachment_chams = chams({ 1 << chams_disabled, color(33, 33, 33, 170), color(250, 250, 250, 125) });
		chams enemy_chams_visible = chams({ 1 << chams_disabled, color(250, 250, 250, 125), color(59, 59, 59, 250) });
		chams enemy_attachment_chams_visible = chams({ 1 << chams_disabled, color(33, 33, 33, 170), color(250, 250, 250, 125) });
		chams enemy_chams_history = chams({ 1 << chams_disabled, color(250, 250, 250, 125), color(59, 59, 59, 250) });
		chams team_chams = chams({ 1 << chams_disabled, color(33, 33, 33, 250), color(250, 250, 250, 250) });
		chams team_attachment_chams = chams({ 1 << chams_disabled,  color(250, 250, 250, 125), color(250, 250, 250, 250) });
		chams team_chams_visible = chams({ 1 << chams_disabled, color(33, 33, 33, 250), color(250, 250, 250, 250) });
		chams team_attachment_chams_visible = chams({ 1 << chams_disabled, color(250, 250, 250, 125), color(250, 250, 250, 250) });

		setting<bool> offscreen_esp{};
		setting<float> offscreen_radius{150.f};

		setting<bool> legit_esp{};
		setting<bitset> legit_triggers{};

		setting<bool> target_scan { };
		setting<color> target_scan_secure{color(0, 180, 0)};
		setting<color> target_scan_aim{color(180, 0, 0)};
	} player_esp{};

	struct local_visuals_s
	{
		setting<bool> glow_local{ };
		setting<bool> fov_changer{ };
		setting<float> fov{}, fov2{};
		setting<bool> skip_first_scope{};
		setting<bool> viewmodel_in_scope{};
		setting<bool> thirdperson{ };
		setting<bool> thirdperson_dead{ };
		setting<float> thirdperson_distance{50.f};
		setting<bool> alpha_blend{};
		setting<bool> hitmarker{ };
		setting<bitset> hitmarker_style{1<<hitmarker_default};

		setting<color> local_color_glow{color(0, 255, 0)};
		chams local_chams = chams({ 1 << chams_disabled, color(250, 250, 250, 175), color(30, 48, 48, 230) });
		chams local_attachment_chams = chams({ 1 << chams_disabled, color(63, 132, 242, 150), color(30, 48, 48, 230) });
		chams fake_chams = chams({ 1 << chams_disabled, color(250, 250, 250, 175), color(30, 48, 48, 230) });
		chams weapon_chams = chams({ 1 << chams_disabled, color(63, 132, 242, 150), color(56, 35, 25, 230) });
		chams arms_chams = chams({ 1 << chams_disabled, color(250, 250, 250, 175), color(30, 48, 48, 230) });

		setting<bool> impacts{ };
		setting<color> server_impacts{color(63, 132, 242, 150)};
		setting<color> client_impacts{color(245, 99, 66, 150)};
		setting<bitset> hit_indicator { };
		setting<color> hit_color{color(0, 255, 0, 130)};

		setting<bitset> aa_indicator{};
		setting<color> clr_aa_real{color(245, 99, 66)};
		setting<color> clr_aa_desync{color(133, 203, 93)};

		setting<bool> night_mode{ }, disable_post_processing{ };
		setting<float> night_mode_value{};
		setting<color> night_color{color(0, 0, 0, 60)};
		setting<float> transparent_prop_value{0.f};

		setting<bool> aspect_changer{ };
		setting<float> aspect{};
	} local_visuals{};

	struct world_esp_s
	{
		setting<bitset> weapon{ };
		setting<bitset> objective{ };
		setting<bitset> nades{ };
		setting<bool> visualize_nade_path { };

		setting<color> nade_path_color{color(245, 99, 66)};
		setting<color> objective_color{color::white()}, weapon_color{color::white()};
		setting<color> fire_color{color(245, 99, 66)},
			smoke_color{color::white()},
			flash_color{color(255, 225, 60)};

		setting<bool> grenade_warning{ };
		setting<bitset> grenade_warning_options{ };
		setting<color> grenade_warning_progress{ color(sdk::color::attention().red(), sdk::color::attention().green(), sdk::color::attention().blue()) };
		setting<color> grenade_warning_damage{ color(sdk::color::attention().red(), sdk::color::attention().green(), sdk::color::attention().blue()) };
	} world_esp{};

	struct misc_s
	{
		setting<bool> bhop{}, nade_assistant{}, peek_assistant{}, preserve_killfeed{}, penetration_crosshair{};
		setting<color> peek_assistant_color{color(0, 120, 255)};
		setting<bitset> peek_assistant_mode{1 << peek_assistant_free_roam};
		setting<bitset> eventlog{};
		setting<bitset> event_triggers{};
		setting<bitset> autostrafe{ 1 << autostrafe_disabled};
		setting<bool> clantag_changer{};
		setting<bool> auto_accept{};
		setting<bool> reveal_ranks{};
		setting<float> autostrafe_turn_speed{70.f};

		setting<bool> enable_tick_sound{true};
		setting<color> menu_color{color(0, 172, 245)};
	} misc{};

	struct legit_cfg
	{
		setting<bool> backtrack{};
		setting<float> backtrack_range{};
		setting<bitset> hitbox{};
		setting<bool> distance_fov{};
		setting<float> fov{1.f};
		setting<float> aimtime{251.f};
		setting<float> smoothfactor{};
		setting<bool> silent_aim{},
			no_snap{},
			smokecheck{},
			recoil_control{};
		setting<float> recoil_control_x{};
		setting<float> recoil_control_y{};
	};

	struct legit_s
	{
		setting<bool> enable{ false };
		setting<bitset> aimmode{ 1 << mode_onfire };
		setting<int> aimkey{0 };

		std::array<legit_cfg, grp_max> weapon_cfgs{};
		legit_cfg hack{};
	} legit{};

	struct trigger_cfg
	{
		setting<bool> backtrack{};
		setting<float> backtrack_range{};
		setting<bitset> hitbox{};
		setting<float> afterburst{};
		setting<float> delay{};
		setting<bool> magnet{},
			smokecheck{},
			ignore_flash_local{},
			ignore_jump_local{},
			ignore_jump_enemy{};
	};

	struct trigger_s
	{
		setting<bool> enable{};
		setting<bitset> mode{ 1 << mode_onkey };
		setting<int> key = 0;

		std::array<trigger_cfg, grp_max> weapon_cfgs{};

		// actual hack settings - gets filled from cfg_container
		trigger_cfg hack{};
	} trigger{};

	struct rage_cfg
	{
		setting<bitset> secure_point{1 << secure_point_default};
		setting<bool> secure_fast_fire{};
		setting<float> hitchance_value{};
		setting<bool> dynamic_min_dmg{};
		setting<float> min_dmg{};
		setting<bitset> hitbox{};
		setting<bitset> avoid_hitbox{};
		setting<bitset> lethal{};
		setting<float> multipoint{50.f};

		bool was_anything_changed()
		{
			return secure_point.was_changed() || secure_fast_fire.was_changed() || hitchance_value.was_changed()
				|| dynamic_min_dmg.was_changed() || min_dmg.was_changed() ||  hitbox.was_changed()
				|| avoid_hitbox.was_changed() || lethal.was_changed() || multipoint.was_changed();
		}

		std::function<void(bool)> on_anything_changed{};
		bool previous_anything_changed{};

		void handle_on_anything_changed(bool force = false)
		{
			if (!on_anything_changed)
				return;

			const auto anything_changed = was_anything_changed();
			if (anything_changed != previous_anything_changed || force)
			{
				previous_anything_changed = anything_changed;
				on_anything_changed(anything_changed);
			}
		}
	};

	struct rage_s
	{
		setting<bool> enable{};
		setting<bitset> mode{ (1 << rage_just_in_time)};
		setting<float> fov{};
		setting<bool> auto_scope{}, only_visible{}, auto_engage{}, enable_fast_fire{}, delay_shot{}, dormant{};

		setting<bitset> fast_fire{1 << fast_fire_default};

		std::array<rage_cfg, grp_max> weapon_cfgs{};

		// actual hack settings - gets filled from cfg_container
		rage_cfg hack{};
	} rage{};

	struct antiaim_s
	{
		setting<bool> enable{};
		setting<bitset> disablers{};
		setting<bitset> pitch{ 1 << pitch_none };
		setting<bitset> yaw{ 1 << yaw_none };
		setting<bitset> yaw_override{ 1 << yaw_none };
		setting<bitset> yaw_modifier{ 1 << yaw_modifier_none };
		setting<bitset> desync{ 1 << desync_none };
		setting<bitset> body_lean{ 1 << body_lean_none };
		setting<bool> ensure_body_lean{};

		setting<float> desync_amount{0.f}, body_lean_amount{ 0.f }, yaw_modifier_amount{0.f};

		setting<bool> align_fake{}, desync_flip{}, body_lean_flip{}, hide_shot{}, slow_walk{}, fake_duck{}, translate_jump{};
		setting<bitset> slowwalk_mode {1 << slowwalk_speed};
		setting<bitset> movement_mode {1 << movement_walk};
	} antiaim{};

	struct fakelag_s
	{
		setting<float> lag_amount{0.f};
		setting<float> variance{0.f};
	} fakelag{};

	struct removals_s
	{
		setting<bool> no_visual_recoil{}, no_flash{}, no_smoke{};
		setting<bitset> scope { 1 << scope_default };
		setting<color> scope_color{color::white()};
	} removals{};

	struct lua_s
	{
		setting<bool> allow_insecure{}, allow_pcall{}, allow_dynamic_load{};
	} lua{};

	struct skins_s
	{
		setting<bitset> selected_type{};
		setting<bitset> selected_entity{};
		setting<bitset> selected_skin{};
		setting<bool> sort_by_item{true};
		setting<float> wear{0.f};

		setting<bitset> stattrak_type{};
		setting<bool> custom_seed{};
	} skins{};

	const char* cfg_magic 	= "EV04";
	const char* cfg_game 	= "CSGO";
	const int   cfg_version = 4;

	void init();
	void save(const std::string& file);
	bool load(const std::string& file, bool welcome = false);
	void drop();

	bool register_lua_bool_entry(uint32_t id);
	bool register_lua_float_entry(uint32_t id);
	bool register_lua_bit_entry(uint32_t id);
	bool register_lua_color_entry(uint32_t id);

	std::unordered_map<uint32_t , evo::gui::value_entry*> entries{}; // NOLINT(cert-err58-cpp)

	// unknown values which might be used by lua
	std::unordered_map<uint32_t, setting<bool>> unk_bool_entries{};
	std::unordered_map<uint32_t, setting<float>> unk_float_entries{};
	std::unordered_map<uint32_t, setting<bitset>> unk_bit_entries{};
	std::unordered_map<uint32_t, setting<color>> unk_color_entries{};

	std::string last_changed_config{};
	std::string want_to_change{};
	bool is_dangerous{};
	bool is_loading{};

private:
	void build_rage_config(weapon_groups grp);
};

extern cfg_t cfg;

#endif // BASE_CFG_H
