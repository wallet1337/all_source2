
#ifndef HACKS_ANTIAIM_H
#define HACKS_ANTIAIM_H

#include <sdk/input.h>
#include <base/cfg.h>
#include <detail/player_list.h>

namespace hacks
{
	class antiaim
	{
		static constexpr auto jitter_width = 20.f;
		static constexpr auto flip_range = -152.f;
		static constexpr auto flop_tolerance = 6.f;
		
	public:
		void on_send_packet(sdk::user_cmd* cmd, bool fixup = false);
		void on_send_command(sdk::user_cmd* cmd);

		bool calculate_lag(sdk::user_cmd* cmd);
		bool should_shift();
		void prepare_on_peek();
		int32_t get_shot_cmd() const;
		bool is_manual_shot() const;

		void set_shot_cmd(sdk::user_cmd* cmd, bool manual = false);
		int32_t determine_maximum_lag() const;

		bool is_active() const;
		bool is_visually_fakeducking() const;
		bool is_fakeducking() const;
		void set_highest_z(float z);
		sdk::vec3 get_visual_eye_position() const;
		bool is_peeking() const;
		bool is_lag_hittable() const;
		int32_t get_wish_lag() const;

		void clear_times();
	private:
		void calculate_freestanding();
		void determine_fake_limit(sdk::user_cmd* const cmd);
		static bool expected_to_be_in_range(sdk::cs_player_t* const player, std::shared_ptr<detail::lag_record> peek, sdk::vec3& start);

		float get_desired_yaw(sdk::user_cmd* const cmd);
		float get_correction_yaw(float body_yaw);
		float get_modifier_yaw(sdk::user_cmd* const cmd);
		float get_desired_pitch(sdk::user_cmd* const cmd);
		float get_target_delta(sdk::user_cmd* const cmd, float ideal_yaw);
		float get_body_lean(sdk::user_cmd* const cmd);

		std::optional<sdk::angle> get_bomb_angle();
		std::optional<sdk::angle> get_knife_angle();

		float last_fakeduck_time{}, highest_z{}, optimal_yaw{}, target_delta{}, at_target{}, freestand_yaw{}, freestand_body_yaw{};
		int32_t shot_nr{}, choke_buffer{}, target_choke{}, wish_lag{}, last_lag{}, in_on_peek{};
		uint32_t step{};
		sdk::vec3 last_velocity{}, last_origin{};
		bool in_fakeduck{}, has_target{}, had_target{}, had_action{}, jitter{}, manual_shot{},
			desync_jitter{}, body_lean_jitter{}, enemies_alive{}, lag_hittable{}, protected_by_shield{};
		std::optional<sdk::angle> bomb_angle{}, knife_angle{};
	};

	extern antiaim aa;
}

#endif // HACKS_ANTIAIM_H
