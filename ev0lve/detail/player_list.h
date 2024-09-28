
#ifndef DETAIL_PLAYER_LIST_H
#define DETAIL_PLAYER_LIST_H

#include <sdk/cs_player.h>
#include <sdk/convar.h>
#include <sdk/client_state.h>
#include <sdk/cutlvector.h>
#include <sdk/prediction.h>
#include <hacks/tickbase.h>
#include <hacks/misc.h>
#include <base/cfg.h>

namespace detail
{
	namespace aim_helper
	{
		std::optional<sdk::vec3> get_hitbox_position(sdk::cs_player_t* player, sdk::cs_player_t::hitbox id, const sdk::mat3x4* bones);
	}

	inline static constexpr auto flip_margin = 30.f;
	inline static constexpr auto teleport_dist = 64 * 64;

	enum class record_filter
	{
		latest,
		oldest,
		uncrouched,
		resolved,
		shot,
		angle_diff
	};

	enum resolver_state
	{
		resolver_default = 0,
		resolver_flip,
		resolver_shot,
		resolver_state_max
	};

	enum resolver_direction
	{
		resolver_networked = 0,
		resolver_max_extra,
		resolver_max_max,
		resolver_max,
		resolver_min,
		resolver_min_min,
		resolver_min_extra,
		resolver_center,
		resolver_direction_max
	};

	__forceinline float calculate_lerp()
	{
		return fmaxf(GET_CONVAR_FLOAT("cl_interp"), GET_CONVAR_FLOAT("cl_interp_ratio") / GET_CONVAR_INT("cl_updaterate"));
	}
	
	__forceinline float calculate_rtt()
	{
		return game->client_state->net_channel->get_avg_latency(sdk::flow_incoming)
			+ game->client_state->net_channel->get_avg_latency(sdk::flow_outgoing);
	}
	
	struct addon_t
	{
		bool in_jump{}, in_rate_limit{}, swing_left = true;
		std::vector<uint16_t> activity_modifiers{};
		float next_lby_update{};
		float adjust_weight{};
		int32_t vm{};
	};

	struct lag_record
	{
		lag_record() = default;
		explicit lag_record(sdk::cs_player_t* const player);
		~lag_record() = default;

		void determine_simulation_ticks(lag_record* last);

		inline bool is_valid(bool visual = false) const
		{
			if (!game->client_state || !game->client_state->net_channel || !valid || !game->local_player || !game->local_player->is_alive())
				return false;

			if (!game->cl_lagcompensation || !game->cl_predict)
				return true;

			const auto adjusted_time = TICKS_TO_TIME(game->local_player->get_tick_base() - 1);
			const auto rtt = calculate_rtt();
			const auto dead_time = static_cast<int32_t>(TICKS_TO_TIME(game->client_state->last_server_tick) + rtt - sdk::sv_max_unlag);
			const auto lerp = calculate_lerp();
			const auto future_tick = game->client_state->last_server_tick + TIME_TO_TICKS(rtt) + sdk::sv_max_usercmd_future_ticks;
			const auto target_sim = sim_time - (visual ? lerp : 0.f);

			if (dead_time >= target_sim || TIME_TO_TICKS(target_sim + lerp) > future_tick)
				return false;

			const auto correct = std::clamp(lerp + rtt, 0.f, sdk::sv_max_unlag);
			const auto dt_start = correct - (adjusted_time - target_sim - game->globals->interval_per_tick);
			const auto dt_end = correct - (adjusted_time - target_sim + game->globals->interval_per_tick);
			return fabsf(dt_start) < sdk::sv_max_unlag && fabsf(dt_end) < sdk::sv_max_unlag;
		}

		inline bool is_moving() const
		{
			return velocity.length() > 5.f;
		}

		inline bool is_moving_on_server() const
		{
			return (layers[6].playback_rate > 0.f && layers[6].weight > 0.f) || !(flags & sdk::cs_player_t::on_ground);
		}

		bool is_breaking_lagcomp() const;
		bool can_delay_shot(bool pure = false) const;
		bool should_delay_shot() const;

		void update_animations(sdk::user_cmd* cmd = nullptr, bool tickbase_drift = false);
		sdk::anim_state predict_animation_state(sdk::user_cmd* cmd = nullptr, sdk::animation_layers* out = nullptr);
		void play_additional_animations(sdk::user_cmd* cmd, const sdk::anim_state& pred_state);
		void build_matrix(resolver_direction direction);
		void perform_matrix_setup(bool extrapolated = false, bool only_max = false);
		float get_resolver_angle(std::optional<resolver_direction> direction_override = std::nullopt);
		float get_resolver_roll(std::optional<resolver_direction> direction_override = std::nullopt);

		bool valid = true, resolved{}, shot{}, tickbase_drift{},
			has_state{}, has_matrix{}, has_visual_matrix{}, force_safepoint{}, do_not_set{};
		int32_t index{};
		addon_t addon{};
		resolver_state res_state{};
		resolver_direction res_direction{};
		bool res_right{};
		sdk::cs_player_t* player{};
		lag_record* previous{};
		alignas(16) sdk::mat3x4 mat[sdk::max_bones]{};
		alignas(16) sdk::mat3x4 vis_mat[sdk::max_bones]{};
		alignas(16) sdk::mat3x4 res_mat[resolver_direction_max][sdk::max_bones]{};

		bool dormant{};
		bool extrapolated{};
		bool strafing{};
		sdk::vec3 velocity{};
		sdk::vec3 final_velocity{};
		sdk::vec3 origin{};
		sdk::vec3 abs_origin{};
		sdk::vec3 obb_mins{};
		sdk::vec3 obb_maxs{};
		sdk::animation_layers layers{};
		sdk::animation_layers custom_layers[resolver_direction_max]{};
		sdk::pose_paramater poses[resolver_direction_max]{};
		sdk::anim_state state[resolver_direction_max]{};
		float sim_time{};
		float recv_time{};
		float delta_time{};
		int32_t server_tick{};
		float duck{};
		float lower_body_yaw_target{};
		float yaw_modifier{};
		float velocity_modifier{};
		sdk::angle eye_ang{}, abs_angle{};
		sdk::angle abs_ang[resolver_direction_max]{};
		int32_t flags{};
		int32_t eflags{};
		int32_t effects{};
		int32_t lag{};
	};

	namespace aim_helper
	{
		struct rage_target;
	}

	struct shot
	{
		std::shared_ptr<lag_record> record;
		sdk::vec3 start{}, end{};
		std::vector<sdk::vec3> impacts{};
		uint32_t hitgroup{};
		sdk::cs_player_t::hitbox hitbox{};
		float time{};
		int32_t damage{};
		bool confirmed{}, impacted{}, skip{}, manual{}, alt_attack{};
		int32_t cmd_num{}, cmd_tick{}, server_tick{}, target_tick{};
		std::shared_ptr<aim_helper::rage_target> target = nullptr;
		resolver_direction target_direction{};
		bool target_right{};

		bool spread_miss{};

		struct
		{
			std::vector<sdk::vec3> impacts{};
			int32_t hitgroup{}, damage{}, health{}, index{};
		} server_info;
	};

	class i_player_list
	{
		struct player_entry
		{
			player_entry() = default;
			explicit player_entry(sdk::cs_player_t* const player);

			lag_record* get_record(record_filter filter = record_filter::latest);
			void get_interpolation_records(lag_record** first, lag_record** second);

			bool is_visually_fakeducking() const;
			bool is_charged() const;
			bool is_exploiting() const;
			bool is_peeking() const;

			sdk::cs_player_t* player;

			struct
			{
				float alpha{}, dormant_alpha{}, previous_health{}, previous_armor{}, previous_ammo{};
				uint32_t last_update{};
				sdk::interpolated_value<sdk::vec3> pos{};
				bool is_visible{}, has_matrix{};
				alignas(16) sdk::mat3x4 matrix[sdk::max_bones]{};
				sdk::vec3 abs_org;

				sdk::vec3 his_org;
				sdk::angle his_ang;
				float his_time;
			} visuals{};

			struct
			{
				struct resolver_state_t
				{
					inline void switch_to_opposite(const lag_record& record, const resolver_direction dir)
					{
						const auto pos_dir = detail::aim_helper::get_hitbox_position(record.player, sdk::cs_player_t::hitbox::head, record.res_mat[dir]);
						if (!pos_dir.has_value())
							return;

						// try to maximize angle delta so that we increase the possibility of hitting next time.
						std::pair<resolver_direction, float> current_delta = { resolver_networked, FLT_EPSILON };
						const auto ang_dir = calc_angle(record.origin, *pos_dir);

						// perform reduction over all open positions.
						for (auto i = 0; i < resolver_direction_max; i++)
						{
							const auto pos = detail::aim_helper::get_hitbox_position(record.player, sdk::cs_player_t::hitbox::head, record.res_mat[i]);
							if (!pos.has_value())
								continue;

							const auto ang = calc_angle(record.origin, *pos);
							const auto delta = fabsf(std::remainderf(ang.y - ang_dir.y, sdk::yaw_bounds));

							if (!eliminated_positions.test(i) && delta > current_delta.second)
								current_delta = { static_cast<resolver_direction>(i), delta };
						}

						// set new resolve direction.
						if (current_delta.second != FLT_EPSILON)
							set_direction(current_delta.first);
					}

					inline void set_direction(resolver_direction dir)
					{
						direction = dir;
						eliminated_positions.reset(dir);
					}

					enum resolver_direction direction = resolver_networked;
					std::bitset<resolver_direction_max> eliminated_positions{};
				} states[2][resolver_state_max]{};

				inline resolver_state_t& get_resolver_state(const lag_record& record)
				{
					return states[record.res_right][record.res_state];
				}
				
				inline void map_resolver_state(const lag_record& record, const resolver_state_t& state)
				{
					// synchronize result to other states.
					if (record.res_state != resolver_shot)
						for (auto i = 0; i < resolver_state_max - 2; i++)
						{
							if (i == record.res_state)
								continue;

							auto& other = states[record.res_right][i];
							auto target = state.direction;

							// should we invert?
							if (i == resolver_shot || steadiness >= .5f)
								switch (state.direction)
								{
								case resolver_max_max:
									target = resolver_min_min;
									break;
								case resolver_max:
								case resolver_max_extra:
									target = resolver_min;
									break;
								case resolver_min:
								case resolver_min_extra:
									target = resolver_max;
									break;
								case resolver_min_min:
									target = resolver_max_max;
									break;
								default:
									target = resolver_networked;
									break;
								}

							if (!other.eliminated_positions.test(target))
								other.direction = target;
						}
				}
				
				enum resolver_state current_state{};
				float steadiness{}, continuous_momentum{}, pitch_timer{}, detected_yaw{};
				bool detected_layer{}, stop_detection{}, right{}, previous_freestanding{}, unreliable_extrapolation{};
				int8_t unreliable{};
			} resolver{};

			struct
			{
				int32_t last_target_tick{};
				float target_time = -1.f;
				bool had_secure_point_this_tick{};
			} aimbot{};

			float spawntime{}, compensation_time{}, dt_interp{}, ground_accel_linear_frac_last_time{};
			int32_t addt{}, ground_accel_linear_frac_last_time_stamp{};
			sdk::base_handle handle{};
			int32_t spread_miss{}, resolver_miss{}, dormant_miss{};
			bool pvs{}, hittable{}, danger{};
			int8_t pitch_alt{};
			uint32_t pitch_prev{};

			float old_maxs_z = FLT_MAX;
			float view_delta = 0.f;
			
			std::deque<lag_record> records;
			sdk::animation_layers server_layers{};
			float lower_body_yaw_target{};
			std::shared_ptr<aim_helper::rage_target> last_target{};
			sdk::interpolated_value<sdk::vec3> interpolated_target{};
		};

	public:
		inline i_player_list::player_entry& get(sdk::cs_player_t* const player)
		{
			if (!player)
				INVALID_ARGUMENT("Player pointer was null.");

			const auto index = player->index();

			if (index < 1 || index > 65)
				RUNTIME_ERROR("Invalid player index");

			auto& entry = entries[index - 1];

			if (entry.handle != player->get_handle())
			{
				const auto first = !entry.handle;

				entry = player_entry(player);

				if (first)
					entry.server_layers = *player->get_animation_layers();
			}
			else if (entry.spawntime != player->get_spawn_time())
			{
				// copy old data.
				const auto server_layers = entry.server_layers;
				const auto resolver = entry.resolver;

				entry = player_entry(player);

				// restore old data.
				entry.server_layers = server_layers;
				entry.resolver = resolver;
				entry.resolver.unreliable = max(entry.resolver.unreliable - 1, 0);
				entry.resolver.unreliable_extrapolation = false;

				// clear positions.
				for (auto i = 0; i < 2; i++)
					for (auto j = 0; j < resolver_state_max; j++)
					{
						auto& state = entry.resolver.states[i][j];
						state.eliminated_positions.reset();
						if (state.direction != resolver_max_max && state.direction != resolver_min_min && state.direction != resolver_min_extra && state.direction != resolver_max_extra)
						{
							state.eliminated_positions.set(resolver_max_max);
							state.eliminated_positions.set(resolver_max_extra);
							state.eliminated_positions.set(resolver_min_min);
							state.eliminated_positions.set(resolver_min_extra);
						}
					}

			}
			else if (entry.player != player)
			{
				entry.player = player;

				for (auto& record : entry.records)
					record.player = player;
			}

			return entry;
		}

		void reset();

		void on_update_end(sdk::cs_player_t* const player);
		void on_shot_resolve(shot& s, const sdk::vec3& end, const bool spread_miss);
		void on_target_player(sdk::cs_player_t* const player);
		void refresh_local();
		void merge_local_animation(sdk::cs_player_t* const player, sdk::user_cmd* cmd);
		void update_addon_bits(sdk::cs_player_t* const player);
		void build_fake_animation(sdk::cs_player_t* const player, sdk::user_cmd* const cmd);
		void build_activity_modifiers(std::vector<uint16_t>& modifiers, bool uh = false) const;
		void perform_animations(lag_record* const current, lag_record* const last = nullptr);

		std::array<player_entry, 64> entries;

	private:
		void update_resolver_state(lag_record* const current, lag_record* const last);
	};

	extern i_player_list player_list;
	extern lag_record local_record, local_shot_record, local_fake_record;
	
	struct animation_copy
	{
		animation_copy() = default;
		
		animation_copy(int32_t sequence, sdk::cs_player_t* player)
		{
			this->sequence = sequence;
			checksum = game->input->verified_commands[sequence % sdk::input_max].crc;
			state = *player->get_anim_state();
			this->addon = local_record.addon;
			layers = visual_layers = *player->get_animation_layers();
			poses = player->get_pose_parameter();
			lower_body_yaw_target = player->get_lower_body_yaw_target();
			eye = *player->eye_angles();
			get_strafing = player->get_strafing();
		}

		void restore(sdk::cs_player_t* player) const
		{
			const auto bak = *player->get_anim_state();
			*player->get_anim_state() = state;
			bak.copy_meta(player->get_anim_state());
			local_record.addon = this->addon;
			*player->get_animation_layers() = layers;
			player->get_pose_parameter() = poses;
			player->get_lower_body_yaw_target() = lower_body_yaw_target;
		}
		
		int32_t sequence = INT_MAX;
		int32_t checksum = INT_MAX;
		sdk::anim_state state{};
		addon_t addon{};
		sdk::animation_layers layers{}, visual_layers{};
		sdk::pose_paramater poses{};
		float lower_body_yaw_target{};
		sdk::angle eye{};
		bool get_strafing{};
	};
}

#define GET_PLAYER_ENTRY(player) detail::player_list.get(player)

#endif // DETAIL_PLAYER_LIST_H
