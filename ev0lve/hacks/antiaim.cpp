
#include <hacks/antiaim.h>
#include <detail/aim_helper.h>
#include <detail/custom_prediction.h>
#include <base/debug_overlay.h>
#include <detail/events.h>
#include <hacks/rage.h>
#include <sdk/debug_overlay.h>

using namespace sdk;
using namespace detail;
using namespace detail::aim_helper;
using namespace util;

namespace hacks
{
	antiaim aa;

	void antiaim::on_send_packet(user_cmd* const cmd, bool fixup)
	{
		if (!fixup)
		{
			local_record.yaw_modifier = 0.f;
			in_on_peek = max(0, in_on_peek - 1);
		}

		if (!is_active() || !cfg.antiaim.enable.get())
			return;

		if (!fixup)
		{
			static const auto wpn_shield = XOR_STR_STORE("weapon_shield");
			XOR_STR_STACK(shield, wpn_shield);
			const auto wpn = reinterpret_cast<weapon_t *>(
					game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));

			bomb_angle = get_bomb_angle();
			knife_angle = get_knife_angle();
			protected_by_shield = wpn->get_item_definition_index() != item_definition_index::weapon_shield &&
					game->local_player->owns_this_type(shield);

			calculate_freestanding();
		}

		optimal_yaw = (shot_nr > game->client_state->last_command && shot_nr <= cmd->command_number) ?
			game->input->commands[shot_nr % input_max].viewangles.y :
			std::remainderf(get_desired_yaw(cmd) + get_modifier_yaw(cmd), yaw_bounds);
		target_delta = get_target_delta(cmd, optimal_yaw);
	}
	
	bool antiaim::should_shift()
	{
		const auto sequence = game->client_state->last_command + game->client_state->choked_commands + 1;
		const auto& info = pred.info[sequence % input_max].animation;
		
		if (!is_active() || info.sequence != sequence)
			return false;

		const auto wpn = reinterpret_cast<weapon_t* const>(game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));
		return wpn->get_item_definition_index() != item_definition_index::weapon_revolver && cfg.antiaim.translate_jump.get() && info.state.air_time > 0.f && info.state.air_time < .8f;
	}
	
	void antiaim::on_send_command(user_cmd* const cmd)
	{
		if (!is_active())
			return;
		
		const auto move_type = pred.info[cmd->command_number % input_max].move_type;
		
		if (move_type == movetype_noclip
			|| move_type == movetype_ladder)
			return;

		const auto s = game->local_player->get_anim_state();
		
		if (cfg.antiaim.movement_mode.get().test(cfg_t::movement_opposite))
			target_delta = float(signum(target_delta));
			
		if ((!(cmd->buttons & user_cmd::use) || bomb_angle.has_value())	&& (cfg.rage.enable.get() || !(cmd->buttons & user_cmd::attack)))
		{
			determine_fake_limit(cmd);
			const auto min_max = target_delta > 0.f ? s->aim_yaw_max : s->aim_yaw_min;
			const auto min_max_inv = target_delta <= 0.f ? s->aim_yaw_max : s->aim_yaw_min;
			const auto body_yaw = min_max * local_record.yaw_modifier * fabsf(target_delta);
			
			const auto i = cmd->command_number - game->client_state->last_command - 1;
			const auto ideal_pitch = get_desired_pitch(cmd);
			const auto ideal_yaw = std::remainderf(optimal_yaw + get_correction_yaw(body_yaw), yaw_bounds);
			auto target = cmd->viewangles;
			
			if (cmd->command_number != shot_nr)
				cmd->viewangles = { ideal_pitch, ideal_yaw, 0.f };

			if (bomb_angle.has_value() && (cmd->buttons & user_cmd::use) && (cmd->viewangles.x < 50.f || bomb_angle->x < 50.f))
				cmd->viewangles = *bomb_angle;
			
			if (i != game->client_state->choked_commands && target_delta != 0.f && (!bomb_angle.has_value() || !(cmd->buttons & user_cmd::use)))
			{
				auto intermediate = 0.f;
				
				if (fabsf(target_delta) <= .999f && fabsf(min_max * local_record.yaw_modifier) + fabsf(body_yaw) + fabsf(min_max_inv * local_record.yaw_modifier) < 180.f)
				{
					intermediate = body_yaw;
					
					const auto delta = angle_diff(std::remainderf(cmd->viewangles.y + body_yaw, yaw_bounds),
						game->local_player->get_anim_state()->foot_yaw);
					
					if (fabsf(delta) >= 2.5f)
						intermediate += (delta > 0.f ? s->aim_yaw_max : s->aim_yaw_min) * local_record.yaw_modifier;
				}
				else if (cmd->command_number % 2 != (game->client_state->last_command + game->client_state->choked_commands + 1) % 2)
				{
					const auto opposite = std::remainderf(flip_range - fabsf(min_max_inv), yaw_bounds);
					intermediate += opposite + (fabsf(min_max_inv) - fabsf(body_yaw)) * .75f;
					const auto delta = angle_diff(std::remainderf(cmd->viewangles.y +
						intermediate * -signum(target_delta), yaw_bounds),
						game->local_player->get_anim_state()->foot_yaw);
					
					if (fabsf(delta) <= opposite)
						intermediate = 180.f + -signum(target_delta) * (fabsf(min_max_inv) + flop_tolerance);
					else
						intermediate *= -signum(target_delta);
				}
				
				if (shot_nr > game->client_state->last_command && shot_nr <= cmd->command_number)
					intermediate = 0.f;
				
				cmd->viewangles.y = std::remainderf(cmd->viewangles.y + intermediate, yaw_bounds);
			}

			fix_movement(cmd, target);

			const auto lean = get_body_lean(cmd);
			if (i == game->client_state->choked_commands && fabsf(lean) >= .01f)
			{
				auto data = game->cs_game_movement->setup_move(game->local_player, cmd);
				game->cs_game_movement->process_movement(game->local_player, &data);
				const auto vel = data.velocity;
				const auto start = *cmd;
				cmd->viewangles.z = lean * roll_bounds;
				fix_movement(cmd, target);
				data = game->cs_game_movement->setup_move(game->local_player, cmd);
				game->cs_game_movement->process_movement(game->local_player, &data);
				if ((data.velocity - vel).length() > .1f && !cfg.antiaim.ensure_body_lean.get())
					*cmd = start;
			}
		}

		game->prediction->set_local_view_angles(cmd->viewangles);
		const auto p = local_record.predict_animation_state();
		vec3 forward, right, up;
		angle_vectors(angle(0.f, p.foot_yaw, 0.f), forward, right, up);
		right.normalize();
		const auto to_forward_dot = p.last_accel_speed.dot(forward);
		const auto to_right_dot = p.last_accel_speed.dot(right);
		
		auto movement_mode = cfg.antiaim.movement_mode.get();
		
		if (movement_mode.test(cfg_t::movement_walk) && !game->rules->data->get_is_valve_ds()
			&& cmd->command_number % 2 != (game->client_state->last_command + game->client_state->choked_commands + 1) % 2)
			cmd->buttons |= user_cmd::run;
		
		if (movement_mode.test(cfg_t::movement_opposite))
		{
			movement_mode = evo::gui::bits();
			
			if (cmd->command_number % 2 != (game->client_state->last_command + game->client_state->choked_commands + 1) % 2)
				movement_mode.set(cfg_t::movement_skate);
			else
				movement_mode.set(cfg_t::movement_walk);
		}
		
		if (movement_mode.test(cfg_t::movement_walk) || movement_mode.test(cfg_t::movement_skate))
			cmd->buttons &= ~(user_cmd::back | user_cmd::forward | user_cmd::move_left | user_cmd::move_right);
		
		if (movement_mode.test(cfg_t::movement_skate) && (s->strafe_sequence != -1 || s->strafe_change_weight <= 0.f))
		{
			if (to_forward_dot > .55f)
				cmd->buttons |= user_cmd::back;
			else if (to_forward_dot < -.55f)
				cmd->buttons |= user_cmd::forward;
			if (to_right_dot > .63f)
				cmd->buttons |= user_cmd::move_left;
			else if (to_right_dot < -.63f)
				cmd->buttons |= user_cmd::move_right;
		}
	}

	bool antiaim::calculate_lag(user_cmd* const cmd)
	{
		enemies_alive = false;

		game->client_entity_list->for_each_player([this](cs_player_t* const player)
		{
			if (player->is_enemy() && player->is_alive())
				enemies_alive = true;
		});
		
		if (!is_active())
			return true;
		
		const auto wpn = reinterpret_cast<weapon_t*>(
			game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));

		// determine if we should fakeduck.
		if (!game->client_state->choked_commands)
		{
			in_fakeduck = !game->rules->data->get_is_valve_ds() && cfg.antiaim.fake_duck.get() && game->local_player->get_flags() & cs_player_t::on_ground;
			
			if (in_fakeduck)
				last_fakeduck_time = game->globals->curtime;
		}

		auto buffer = choke_buffer;
		const auto max_lag = determine_maximum_lag();
		const auto velocity = game->local_player->get_velocity();
		const auto moving = velocity.length2d() >= 2.f;
		auto min_lag = 0;
		auto skip_anims = false;

		if (cfg.antiaim.enable.get() && !cfg.antiaim.desync.get().test(cfg_t::desync_none))
			min_lag = std::clamp(min_lag, 1, max_lag);
		
		wish_lag = static_cast<int32_t>(cfg.fakelag.lag_amount.get());
		wish_lag += random_int(0, static_cast<int32_t>(cfg.fakelag.variance.get()));
		wish_lag = std::clamp(wish_lag, min_lag, max_lag);
		
		target_choke = min_lag;			

		if (!game->local_player->is_on_ground())
			target_choke = wish_lag;
		
		if ((velocity * game->globals->interval_per_tick * max_lag).length2d_sqr() > teleport_dist)
			target_choke = max_lag;

		if ((game->local_player->get_abs_origin() - last_origin).length2d_sqr() > teleport_dist)
		{
  			target_choke = min_lag;
			choke_buffer = 0;
			skip_anims = true;
		}
		
		target_choke = min(max(target_choke, choke_buffer), max_lag);

		if (moving && wpn && !skip_anims)
		{			
			pred.restore_animation(cmd->command_number);
			const auto& c = *game->local_player->get_anim_state();
			const auto slow = c.speed < wpn->get_max_speed() / 3.f;	
			const auto dot = vec3(velocity).normalize().dot(vec3(last_velocity).normalize());
			auto can_skip_animation = dot > .1f && dot < .9995f;
			
			if (cfg.antiaim.enable.get() && !cfg.antiaim.desync.get().test(cfg_t::desync_none))
			{
				can_skip_animation |= c.speed > wpn->get_max_speed() - 2.5f;
				can_skip_animation |= c.is_accelerating && slow;
			}
			
			if (!choke_buffer)
			{
				if (!can_skip_animation)
				{
					lag_record tmp(game->local_player);
					animation_layers next_layers;
					const auto backup_curtime = game->globals->curtime;
					game->globals->curtime += game->globals->interval_per_tick;
					tmp.predict_animation_state(cmd, &next_layers);
					game->globals->curtime = backup_curtime;
					for (auto i = 0u; i < game->local_player->get_animation_layers()->size(); i++)
					{
						const auto& current = game->local_player->get_animation_layers()->at(i);
						const auto& next = next_layers.at(i);

						if (current.sequence != next.sequence
							|| (!current.has_animation_completed() && next.has_animation_completed()))
						{
							can_skip_animation = true;
							break;
						}
					}
				}
				
				if (can_skip_animation)
					target_choke = choke_buffer = buffer = wish_lag;
			}
			
			pred.restore_animation(tickbase.lag.second);
			game->prediction->set_local_view_angles(cmd->viewangles);
		}
		
		if ((game->input->commands[(cmd->command_number - 1) % input_max].buttons & user_cmd::duck) &&
			!(cmd->buttons & user_cmd::duck))
			choke_buffer = wish_lag;

		if (cfg.antiaim.slowwalk_mode.get().test(cfg_t::slowwalk_optimal) && cfg.antiaim.slow_walk.get())
			target_choke = max_lag;
		
		if (in_on_peek == 2)
		{
			target_choke = 0;
			choke_buffer = max_lag;
		}

		auto send_packet = true;

		if (in_fakeduck)
		{
			// remove duck flag.
			cmd->buttons &= ~(user_cmd::duck);

			// set duck flag accordingly.
			if (game->client_state->choked_commands > max_lag / 2)
				cmd->buttons |= user_cmd::duck;

			// repredict.
			pred.repredict(cmd);
			
			// align cycle.
			target_choke = max_lag;
			choke_buffer = buffer;
		}
		
		// update send packet.
		send_packet = game->client_state->choked_commands >= target_choke;
		last_velocity = velocity;

		if (send_packet && buffer == choke_buffer)
			choke_buffer = 0;
		
		if (send_packet)
		{
			last_lag = game->client_state->choked_commands;
			last_origin = game->local_player->get_abs_origin();
		}
		
		return send_packet;
	}

	int32_t antiaim::get_shot_cmd() const
	{
		return shot_nr;
	}

	bool antiaim::is_manual_shot() const
	{
		return manual_shot;
	}

	void antiaim::set_shot_cmd(user_cmd* const cmd, bool manual_shot)
	{
		shot_nr = cmd->command_number;
		this->manual_shot = manual_shot;
	}

	int32_t antiaim::determine_maximum_lag() const
	{		
		return std::clamp(sv_maxusrcmdprocessticks - tickbase.limit - 1, 0, sv_maxusrcmdprocessticks);
	}

	bool antiaim::is_active() const
	{
		const auto should_skip = (evnt.is_round_end && !enemies_alive && cfg.antiaim.disablers->test(cfg_t::antiaim_disabler_round_end))
				|| game->rules->data->get_freeze_period();
		return cfg.antiaim.enable.get() && !should_skip;
	}

	bool antiaim::is_visually_fakeducking() const
	{
		return last_fakeduck_time > game->globals->curtime - .5f;
	}

	bool antiaim::is_fakeducking() const
	{
		return in_fakeduck;
	}
	
	void antiaim::set_highest_z(float z)
	{
		if (!is_visually_fakeducking() && game->rules->data)
			highest_z = game->rules->data->get_view_vectors()->view.z;
		
		if (in_fakeduck)
			highest_z = z;
	}
	
	vec3 antiaim::get_visual_eye_position() const
	{
		if (is_visually_fakeducking())
			return game->local_player->get_abs_origin() + vec3 { 0.f, 0.f, highest_z };
			
		return game->local_player->get_eye_position();
	}
	
	bool antiaim::is_peeking() const
	{
		return in_on_peek == 2;
	}

	bool antiaim::is_lag_hittable() const
	{
		return (game->local_player->get_tick_base() > tickbase.lag.first && in_on_peek <= 0) || lag_hittable;
	}
	
	int32_t antiaim::get_wish_lag() const
	{
		return wish_lag;
	}

	float antiaim::get_desired_pitch(user_cmd* const cmd)
	{
		const auto wpn = reinterpret_cast<weapon_t*>(
				game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));
		const auto target = aim_helper::get_closest_target();
		if (wpn && wpn->get_item_definition_index() == item_definition_index::weapon_shield && target && cfg.antiaim.disablers->test(cfg_t::antiaim_disabler_shield))
			return std::clamp(calc_angle(game->local_player->get_eye_position(), target->get_eye_position()).x, -pitch_bounds, pitch_bounds);

		if (knife_angle.has_value())
			return std::clamp(knife_angle->x, -pitch_bounds, pitch_bounds);

		if (cfg.antiaim.pitch.get().test(cfg_t::pitch_down))
			return protected_by_shield ? -89.f : 89.f;

		if (cfg.antiaim.pitch.get().test(cfg_t::pitch_up))
			return -89.f;

		return cmd->viewangles.x;
	}

	void antiaim::calculate_freestanding()
	{
		if (!cfg.antiaim.yaw.get().test(cfg_t::yaw_freestanding)
			&& !cfg.antiaim.desync.get().test(cfg_t::desync_freestanding)
			&& !cfg.antiaim.body_lean.get().test(cfg_t::body_lean_freestanding))
			return;
		
		const auto target = aim_helper::get_closest_target();
		
		vec3 from = game->local_player->get_eye_position(), to;
		if (target)
		{
			to = target->get_eye_position();
			if (target->is_dormant())
				to = to - target->get_abs_origin() + GET_PLAYER_ENTRY(target).visuals.pos.target;
		}
		else
		{
			const auto ang = angle { 0.f, game->engine_client->get_view_angles().y, 0.f };
			angle_vectors(ang, to);
			to *= 750.f;
			to += from;
		}

		std::tie(freestand_yaw, freestand_body_yaw) = perform_freestanding(from, to);
	}

	float antiaim::get_desired_yaw(user_cmd* const cmd)
	{
		if (knife_angle.has_value())
			return std::remainderf(knife_angle->y, yaw_bounds);

		const auto wpn = reinterpret_cast<weapon_t*>(
			game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));
		const auto target = aim_helper::get_closest_target();
		if (wpn && wpn->get_item_definition_index() == item_definition_index::weapon_shield && target && cfg.antiaim.disablers->test(cfg_t::antiaim_disabler_shield))
			return calc_angle(game->local_player->get_eye_position(), target->get_eye_position()).y;

		at_target = std::remainderf(game->engine_client->get_view_angles().y - 180.f, yaw_bounds);

		if (target && !cfg.antiaim.yaw.get().test(cfg_t::yaw_viewangle))
			at_target = calc_angle(target->get_eye_position(), game->local_player->get_eye_position()).y;

		if (!bomb_angle.has_value() && !protected_by_shield)
		{
			if (cfg.antiaim.yaw_override.get().test(cfg_t::yaw_left))
				return std::remainderf(at_target - 90.f, yaw_bounds);

			if (cfg.antiaim.yaw_override.get().test(cfg_t::yaw_right))
				return std::remainderf(at_target + 90.f, yaw_bounds);

			if (cfg.antiaim.yaw_override.get().test(cfg_t::yaw_forward))
				return std::remainderf(at_target + 180.f, yaw_bounds);

			if (cfg.antiaim.yaw_override.get().test(cfg_t::yaw_back))
				return std::remainderf(at_target, yaw_bounds);

			if (cfg.antiaim.yaw.get().test(cfg_t::yaw_freestanding))
				return std::remainderf(freestand_yaw, yaw_bounds);
		}

		return std::remainderf(at_target, yaw_bounds);
	}

	float antiaim::get_correction_yaw(const float body_yaw)
	{		
		if ((shot_nr > game->client_state->last_command && shot_nr <= game->client_state->last_command + game->client_state->choked_commands + 1)
			|| !cfg.antiaim.align_fake.get()
			|| bomb_angle.has_value())
			return 0.f;

		return body_yaw > 0.f ? 0.f : body_yaw * -.5f;
	}

	float antiaim::get_modifier_yaw(sdk::user_cmd* const cmd)
	{
		const auto wpn = reinterpret_cast<weapon_t*>(
			game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));
		if (wpn && wpn->get_item_definition_index() == item_definition_index::weapon_shield && cfg.antiaim.disablers->test(cfg_t::antiaim_disabler_shield))
			return 0.f;

		if (bomb_angle.has_value() || knife_angle.has_value() || protected_by_shield)
			return 0.f;

		const float yaw_modifier = abs(cfg.antiaim.yaw_modifier_amount.get());

		if (cfg.antiaim.yaw_modifier.get().test(cfg_t::yaw_modifier_jitter))
		{
			jitter = !jitter;
			return (jitter ? yaw_modifier : -yaw_modifier) / 2.f;
		}
		else if (cfg.antiaim.yaw_modifier.get().test(cfg_t::yaw_modifier_half_jitter))
		{
			if (jitter)
			{
				if (++step > ceil(yaw_modifier / jitter_width))
					jitter = false;

				return (-yaw_modifier / 2.f) + jitter_width * (step - 1);
			}
			else
			{
				step = 0;
				jitter = !jitter;
				return -yaw_modifier / 2.f;
			}
		}
		else
			jitter = false;

		if (cfg.antiaim.yaw_modifier.get().test(cfg_t::yaw_modifier_random))
			return random_float(-yaw_modifier, yaw_modifier);

		if (cfg.antiaim.yaw_modifier.get().test(cfg_t::yaw_modifier_directional))
			return cfg.antiaim.yaw_modifier_amount.get();

		return 0.f;
	}

	float antiaim::get_target_delta(user_cmd* const cmd, const float ideal_yaw)
	{
		if (cfg.antiaim.desync.get().test(cfg_t::desync_none))
			return 0.f;

		// difference to last one.
		const auto diff = std::remainderf(ideal_yaw - game->input->commands[game->client_state->last_command % input_max].viewangles.y, yaw_bounds);
		
		// go to opposite on shot.
		if (shot_nr > game->client_state->last_command && shot_nr <= cmd->command_number)
			return diff <= 0.f ? -1.f : 1.f;

		auto amount = cfg.antiaim.desync_amount.get() / 100.f;

		if (cfg.antiaim.desync.get().test(cfg_t::desync_static))
		{
			if (cfg.antiaim.desync_flip.get())
				amount *= -1.f;
			return -amount;
		}
		
		if (cfg.antiaim.desync.get().test(cfg_t::desync_opposite))
		{
			if (cfg.antiaim.desync_flip.get())
				amount *= -1.f;
			return diff <= 0.f ? -amount : amount;
		}

		if (cfg.antiaim.desync.get().test(cfg_t::desync_jitter))
		{
			desync_jitter = !desync_jitter;
			return desync_jitter ? -amount : amount;
		}

		if (cfg.antiaim.desync.get().test(cfg_t::desync_random))
			return random_int(0, 1) ? -amount : amount;

		if (cfg.antiaim.desync.get().test(cfg_t::desync_freestanding))
		{
			amount = fabsf(amount);

			if (cfg.antiaim.desync_flip.get())
				amount *= -1.f;

			if (std::remainderf(freestand_body_yaw - optimal_yaw, yaw_bounds) < -2.5f)
				amount *= -1.f;
			
			return amount;
		}

		return amount;
	}

	float antiaim::get_body_lean(user_cmd* const cmd)
	{
		if (cfg.antiaim.body_lean.get().test(cfg_t::body_lean_none) || shot_nr == cmd->command_number)
			return 0.f;

		auto amount = -cfg.antiaim.body_lean_amount.get() / 100.f;

		if (cfg.antiaim.body_lean.get().test(cfg_t::body_lean_static))
		{
			if (cfg.antiaim.body_lean_flip.get())
				amount *= -1.f;
			return -amount;
		}

		if (cfg.antiaim.body_lean.get().test(cfg_t::body_lean_jitter))
		{
			body_lean_jitter = !body_lean_jitter;
			return body_lean_jitter ? -amount : amount;
		}

		if (cfg.antiaim.body_lean.get().test(cfg_t::body_lean_random))
			return random_int(0, 1) ? -amount : amount;

		if (cfg.antiaim.body_lean.get().test(cfg_t::body_lean_freestanding))
		{
			amount = -fabsf(amount);

			if (cfg.antiaim.body_lean_flip.get())
				amount *= -1.f;

			if (std::remainderf(freestand_body_yaw - optimal_yaw, yaw_bounds) < -2.5f)
				amount *= -1.f;

			return amount;
		}

		return amount;
	}

	void antiaim::prepare_on_peek()
	{
		game->client_entity_list->for_each_player([](cs_player_t* const player)
		{
			auto& player_entry = GET_PLAYER_ENTRY(player);
			player_entry.danger = false;
			
			if (!rag.is_active() || !rag.is_player_target(player))
				player_entry.hittable = false;
		});

		lag_hittable = false;

		if (!game->local_player || !game->local_player->is_alive() || !game->local_player->get_anim_state())
		{
			had_target = has_target = false;
			return;
		}

		if (in_on_peek <= 0 && game->local_player->get_tick_base() > tickbase.lag.first)
		{
			had_target = has_target;
			has_target = false;
		}
		
		if (!cfg.antiaim.enable.get() && !cfg.rage.enable.get())
			return;

		auto peek_record = std::make_shared<lag_record>(local_record);
		const auto view = *game->local_player->eye_angles();
		const auto offset = game->local_player->get_view_offset().z;
		const auto obb_maxs = game->local_player->get_collideable()->obb_maxs();

		if (game->local_player->get_tick_base() > tickbase.lag.first && in_on_peek <= 0 && (tickbase.to_recharge <= 0 || game->client_state->choked_commands > 0))
		{
			const auto wpn = reinterpret_cast<weapon_t*>(
				game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));

			pred.restore_animation(tickbase.lag.second);

			// extrapolate the player a small bit.
			const auto backup_frametime = game->globals->frametime;
			const auto backup_curtime = game->globals->curtime;
			const auto backup_abs_origin = game->local_player->get_abs_origin();
			const auto backup_velocity = game->local_player->get_velocity();
			const auto last_cmd = &game->input->commands[tickbase.lag.second % input_max];
			const auto initial_yaw = game->local_player->get_anim_state()->foot_yaw;
			move_data data;
			const auto shifting = tickbase.fast_fire && cfg.rage.fast_fire.get().test(cfg_t::fast_fire_peek) && wpn && wpn->get_item_definition_index() != item_definition_index::weapon_revolver;
			if (shifting)
				game->globals->curtime += TICKS_TO_TIME(tickbase.limit);
			for (auto i = 0; i < game->client_state->choked_commands + 3; i++)
			{
				if (i <= game->client_state->choked_commands)
				{
					auto& p = pred.info[(game->client_state->last_command + 1 + i) % input_max];
					if (p.sequence == game->client_state->last_command + 1 + i)
					{
						game->local_player->set_abs_origin(p.origin);
						game->local_player->get_velocity() = p.velocity;
					}
					data = game->cs_game_movement->setup_move(game->local_player, last_cmd);
				}
				else
				{
					game->cs_game_movement->process_movement(game->local_player, &data);
					game->local_player->set_abs_origin(data.abs_origin);
					game->local_player->get_velocity() = data.velocity;
				}

				if (game->local_player->get_anim_state()->last_framecount >= game->globals->framecount)
					game->local_player->get_anim_state()->last_framecount = game->globals->framecount - 1;
				game->local_player->get_anim_state()->update(last_cmd->viewangles.y, last_cmd->viewangles.x);
				game->local_player->get_anim_state()->foot_yaw = initial_yaw;
				if (!shifting)
					game->local_player->get_animation_layers()->at(7) = local_record.layers[7];
				game->local_player->set_abs_angle({ 0.f, initial_yaw, 0.f });
				game->globals->curtime += game->globals->interval_per_tick;
			}
			game->globals->frametime = backup_frametime;
			game->globals->curtime = backup_curtime;
			peek_record = std::make_shared<lag_record>(game->local_player);
			peek_record->origin = game->local_player->get_abs_origin();
			peek_record->obb_mins = game->local_player->get_collideable()->obb_mins();
			peek_record->obb_maxs = game->local_player->get_collideable()->obb_maxs();
			game->local_player->get_abs_origin() = backup_abs_origin;
			game->local_player->get_velocity() = backup_velocity;
		}

		game->prediction->set_local_view_angles(game->input->commands[tickbase.lag.second % input_max].viewangles);
		peek_record->perform_matrix_setup();
		game->prediction->set_local_view_angles(view);
		game->local_player->get_view_offset().z = offset;
		game->local_player->get_collideable()->obb_maxs() = obb_maxs;

		std::vector<dispatch_queue::fn> calls{};
		std::deque<rage_target> targets{};

		game->client_entity_list->for_each_player([&peek_record, &targets](cs_player_t* const player)
		{
			if (!player->is_valid(true) || !player->is_enemy())
				return;

			const auto& player_entry = GET_PLAYER_ENTRY(player);
			if (player_entry.visuals.last_update + TIME_TO_TICKS(5.f) < game->client_state->last_server_tick)
				return;

			auto eye = player->get_eye_position();
			if (player->is_dormant())
				eye = player_entry.visuals.pos.target + vec3(0, 0, game->rules->data->get_view_vectors()->duck_view.z +
					(game->rules->data->get_view_vectors()->view - game->rules->data->get_view_vectors()->duck_view).z * (1.f - player->get_duck_amount()));

			if (!expected_to_be_in_range(player, peek_record, eye))
				return;

			const auto start = targets.end();
			for (auto hitbox : cs_player_t::local_scan_hitboxes)
				calculate_multipoint(targets, peek_record, hitbox, player);
			
			for (auto it = start; it != targets.end(); it++)
			{
				it->dist = *reinterpret_cast<const float*>(&player);
				it->pen.end = eye;
			}
		});

		auto hittable = false;
		for (const auto& target : targets)
			calls.push_back([&peek_record, &target, &hittable]()
			{
				const auto player = *reinterpret_cast<cs_player_t*const*>(&target.dist);
				auto from = target.pen.end;
				if (player->get_velocity().length2d() > 40.f)
					from += player->get_velocity() * TICKS_TO_TIME(4);
				
				if (trace.wall_penetration(from, target.position, peek_record, false, std::nullopt, player).did_hit)
					GET_PLAYER_ENTRY(player).danger = hittable = true;
			});
			
		dispatch.evaluate(calls);

		if (game->local_player->get_tick_base() > tickbase.lag.first && in_on_peek <= 0)
		{
			has_target = hittable;
			if (has_target && !had_target)
				in_on_peek = 2;
		}
		else
			lag_hittable = hittable;
	}

	bool antiaim::expected_to_be_in_range(cs_player_t* const player, std::shared_ptr<detail::lag_record> peek, sdk::vec3& start)
	{
		const auto wpn = reinterpret_cast<weapon_t* const>(game->client_entity_list->get_client_entity_from_handle(player->get_active_weapon()));
		if (!wpn)
			return false;

		auto shoot = start;
		if (player->get_velocity().length2d() > 40.f)
			shoot += player->get_velocity() * TICKS_TO_TIME(4);

		if ((game->local_player->get_origin() - start).length() > wpn->get_cs_weapon_data()->range)
			return false;

		const auto first_shoot = peek->origin + vec3(0, 0, game->rules->data->get_view_vectors()->duck_view.z +
			(game->rules->data->get_view_vectors()->view - game->rules->data->get_view_vectors()->duck_view).z * (1.f - peek->duck));
		const auto right = vec3(shoot - first_shoot).normalize().cross({ 0.f, 0.f, 1.f }).normalize();

		return in_range(shoot, first_shoot + right * 22.f)
			|| in_range(shoot, first_shoot - right * 22.f);
	}

	void antiaim::determine_fake_limit(user_cmd* const cmd)
	{
		// grab maximum desync.
		const auto pred = local_record.predict_animation_state();
		local_record.velocity = pred.velocity;
		local_record.duck = pred.duck_amount;
		local_record.get_resolver_angle();
	}

	void antiaim::clear_times()
	{
		last_fakeduck_time = 0.f;
		optimal_yaw = 0.f;
		target_delta = 0.f;
	}

	std::optional<angle> antiaim::get_bomb_angle()
	{
		if (!cfg.antiaim.disablers->test(cfg_t::antiaim_disabler_defuse))
			return std::nullopt;

		if (game->local_player->get_team_num() != 3)
			return std::nullopt;

		entity_t* bomb = nullptr;
		game->client_entity_list->for_each([&bomb](client_entity* const ent) -> void
		{
			if (!ent->is_entity())
				return;

			const auto ent_ = reinterpret_cast<entity_t*>(ent);
			if (ent_->get_class_id() == class_id::planted_c4 && !ent_->get_bomb_defused())
				bomb = ent_;
		});

		if (!bomb || (game->local_player->get_origin() - bomb->get_origin()).length() > 70.f)
			return std::nullopt;

		return calc_angle(game->local_player->get_eye_position(), bomb->get_origin());
	}

	std::optional<angle> antiaim::get_knife_angle()
	{
		if (!cfg.antiaim.disablers->test(cfg_t::antiaim_disabler_knife))
			return std::nullopt;

		const auto wpn = reinterpret_cast<weapon_t*>(
			game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));

		auto best_match = std::make_pair(FLT_MAX, angle());
		auto knife_round = false;
		game->client_entity_list->for_each_player([wpn, &best_match, &knife_round](cs_player_t* const player)
		{
			if (!player->is_valid() || !rag.is_player_target(player))
				return;

			const auto dist = (player->get_origin() - game->local_player->get_origin()).length();
			if (dist > 600.f)
				return;

			if (!(wpn && wpn->is_knife()) && dist > 200.f)
				return;

			const auto other_wpn = reinterpret_cast<weapon_t*>(
				game->client_entity_list->get_client_entity_from_handle(player->get_active_weapon()));
			if (other_wpn && !other_wpn->is_knife())
			{
				knife_round = false;
				return;
			}

			knife_round = true;

			if (best_match.first > dist)
				best_match = std::make_pair(dist, calc_angle(game->local_player->get_eye_position(), player->world_space_center()));
		});

		if (best_match.first == FLT_MAX || !knife_round)
			return std::nullopt;

		return best_match.second;
	}
}
