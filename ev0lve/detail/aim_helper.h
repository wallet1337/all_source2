
#ifndef DETAIL_AIM_HELPER_H
#define DETAIL_AIM_HELPER_H

#include <detail/player_list.h>
#include <base/cfg.h>
#include <detail/dispatch_queue.h>
#include <sdk/input.h>
#include <hacks/tickbase.h>

namespace detail
{
namespace aim_helper
{
	inline static constexpr auto total_seeds = 64;
	inline static constexpr auto multipoint_steps = 4;
	inline static constexpr auto scout_air_accuracy = .009f;
	inline static constexpr auto health_buffer = 6;
	inline static constexpr auto freestand_width = 25.f;
	
	inline static constexpr auto capsule_vert_size = 26;
	inline static constexpr float capsule_verts[capsule_vert_size][3] = {
		{ -0.01f, -0.01f, 1.0f } /*0*/, { 0.0f, 0.0f, -1.0f } /*73*/,
		{ 0.51f, 0.0f, 0.86f } /*1*/, { 0.86f, 0.0f, 0.51f } /*13*/, { 1.0f, 0.0f, 0.01f } /*25*/, { 1.0f, 0.0f, -0.02f } /*37*/, { 0.86f, 0.0f, -0.51f } /*49*/, { 0.51f, 0.0f, -0.87f } /*64*/,
		{ -0.01f, 0.51f, 0.86f } /*4*/, { -0.01f, 0.86f, 0.51f } /*16*/, { -0.01f, 1.0f, 0.01f } /*28*/, { -0.01f, 1.0f, -0.02f } /*40*/, { -0.01f, 0.86f, -0.51f } /*52*/, { -0.01f, 0.51f, -0.87f } /*64*/,
		{ -0.51f, 0.0f, 0.86f } /*7*/, { -0.87f, 0.0f, 0.51f } /*19*/, { -1.0f, 0.0f, 0.01f } /*31*/, { -1.0f, 0.0f, -0.02f } /*43*/, { -0.87f, 0.0f, -0.51f } /*55*/, { -0.51f, 0.0f, -0.87f } /*67*/,
		{ -0.01f, -0.51f, 0.86f } /*10*/, { -0.01f, -0.87f, 0.51f } /*22*/, { -0.01f, -1.0f, 0.01f } /*34*/, { -0.01f, -1.0f, -0.02f } /*46*/, { -0.01f, -0.87f, -0.51f } /*58*/, { -0.01f, -0.51f, -0.87f } /*70*/,
	};
	inline static constexpr auto capsule_line_size = 8;
	inline static constexpr int8_t capsule_lines[capsule_line_size] = { 4, 5, 10, 11, 16, 17, 22, 23 };

	struct legit_target
	{
		float fov{};
		std::shared_ptr<lag_record> record = nullptr;
		sdk::vec3 pos{};
	};
	
	struct rage_target
	{
		rage_target(const sdk::vec3 position, const std::shared_ptr<lag_record> record, const bool alt_attack, const sdk::vec3 center,
			const sdk::cs_player_t::hitbox hitbox, const int32_t hitgroup, const float hc = 0.f, const custom_tracing::wall_pen pen = {})
			: position(position), record(record), alt_attack(alt_attack), center(center), hitbox(hitbox), hitgroup(hitgroup),
				hc(hc), pen(pen) { }

		rage_target() = default;

		inline int32_t get_shots_to_kill();

		sdk::vec3 position{};
		std::shared_ptr<lag_record> record = nullptr;
		bool alt_attack{};
		sdk::vec3 center{};
		sdk::cs_player_t::hitbox hitbox{};
		int32_t hitgroup{};
		float hc{}, dist{}, likelihood{}, likelihood_roll{};
		custom_tracing::wall_pen pen{};
	};

	struct seed
	{
		float inaccuracy, sin_inaccuracy, cos_inaccuracy,
			spread, sin_spread, cos_spread;
	};

	int32_t hitbox_to_hitgroup(sdk::cs_player_t::hitbox box);
	bool is_limbs_hitbox(sdk::cs_player_t::hitbox box);
	bool is_body_hitbox(sdk::cs_player_t::hitbox box);
	bool is_multipoint_hitbox(sdk::cs_player_t::hitbox box, bool potential = false);
	bool is_aim_hitbox_legit(sdk::cs_player_t::hitbox box);
	bool is_aim_hitbox_rage(sdk::cs_player_t::hitbox box, bool potential = false);
	bool should_avoid_hitbox(sdk::cs_player_t::hitbox box, bool potential = false);

	bool is_shooting(sdk::user_cmd* cmd);
	float get_hitchance(sdk::cs_player_t* player);
	int32_t get_adjusted_health(sdk::cs_player_t* player);
	float get_mindmg(sdk::cs_player_t* player, std::optional<sdk::cs_player_t::hitbox> hitbox, bool approach = false);

	sdk::cs_player_t* get_closest_target();
	bool in_range(sdk::vec3 start, sdk::vec3 end);
	std::optional<sdk::vec3> get_hitbox_position(sdk::cs_player_t* player, sdk::cs_player_t::hitbox id, const sdk::mat3x4* bones);

	std::vector<rage_target> perform_hitscan(const std::shared_ptr<lag_record>& record, const bool minimal = false, const bool everything = false);
	void calculate_multipoint(std::deque<rage_target>& targets, const std::shared_ptr<lag_record>& record, sdk::cs_player_t::hitbox box,
		sdk::cs_player_t* override_player = nullptr, bool potential = false, float adjustment = 0.f);
	void optimize_cornerpoint(rage_target* target);

	std::pair<float, float> calculate_likelihood(rage_target* target);
	float calculate_hitchance(rage_target* target);
	float get_lowest_inaccuray();
	bool has_full_accuracy();
	bool should_prefer_record(std::optional<rage_target>& target, std::optional<rage_target>& alternative, bool compare_hc = false);

	bool is_attackable();
	bool holds_tick_base_weapon();
	bool should_fast_fire();

	void fix_movement(sdk::user_cmd* const cmd, sdk::angle& wishangle);
	void slow_movement(sdk::user_cmd* const cmd, float target_speed);
	bool slow_movement(sdk::move_data* const data, float target_speed);
	
	std::pair<float, float> perform_freestanding(const sdk::vec3& from, const sdk::vec3& to, bool* previous = nullptr);

	void build_seed_table();
	bool is_visible(const sdk::vec3& pos);
}
}

#endif // DETAIL_AIM_HELPER_H
