
#include <detail/custom_prediction.h>
#include <sdk/global_vars_base.h>
#include <sdk/client_entity_list.h>
#include <sdk/math.h>
#include <sdk/game_rules.h>
#include <detail/player_list.h>
#include <detail/events.h>
#include <hacks/antiaim.h>
#include <detail/aim_helper.h>

using namespace sdk;
using namespace hacks;
using namespace detail::aim_helper;

namespace detail
{
	custom_prediction pred{};

	void custom_prediction::initial(user_cmd* const cmd)
	{		
		// backup globals.
		backup_curtime = game->globals->curtime;
		backup_frametime = game->globals->frametime;
		
		// backup original data.
		const auto wpn = reinterpret_cast<weapon_t*>(
			game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));
		original_cmd = *cmd;

		// start prediction.
		predicting = true;
		
		// predict post pone time.
		predict_post_pone_time(cmd);
		
		// predict the local player to see if he fired.
		predict_can_fire(cmd);
		
		// predict the local player for real.
		repredict(cmd);

		// store off target prediction.
		backup_predicted = game->prediction->get_predicted_commands();
	}

	void custom_prediction::repredict(user_cmd* const cmd)
	{
		if (game->client_state->delta_tick <= 0 || game->client_state->signon_state != 6
			|| cmd->command_number - last_sequence > input_max || last_sequence == -1)
			return;
		
		// no sound when we run the predictor.
		for (auto i = game->client_state->last_command + 1; i <= cmd->command_number; i++)
			game->input->commands[i % input_max].predicted = true;

		// store old flags.
		auto& entry = info[cmd->command_number % input_max];
		entry.prev_flags = game->local_player->get_flags();

		// run computation.
		computing = true;
		const auto check = cmd->get_checksum();
		if (entry.checksum != check || game->input->verified_commands[cmd->command_number % input_max].crc == *reinterpret_cast<int32_t*>(&game->globals->interval_per_tick))
		{
			game->prediction->get_predicted_commands() = std::clamp(cmd->command_number - game->client_state->last_command_ack,
					0, game->prediction->get_predicted_commands());
			entry.checksum = check;
		}
		game->prediction->update(game->client_state->delta_tick, true, game->client_state->last_command_ack, cmd->command_number);
		computing = false;

		game->globals->curtime = TICKS_TO_TIME(game->local_player->get_tick_base() - 1);
		game->globals->frametime = game->globals->interval_per_tick;

		// store new flags.
		entry.flags = game->local_player->get_flags();
	}

	void custom_prediction::restore(user_cmd* const cmd, bool send_packet)
	{
		// stop predicting.
		predicting = false;
		
		// restore globals.
		game->globals->curtime = backup_curtime;
		game->globals->frametime = backup_frametime;
		
		// let the game do it's own prediction.
		auto last = game->client_state->last_command_ack + backup_predicted;

		game->input->commands[cmd->command_number % input_max].predicted = false;

		if (send_packet && aa.get_shot_cmd() > game->client_state->last_command && aa.get_shot_cmd() < cmd->command_number && !aa.is_manual_shot())
		{
			game->input->commands[aa.get_shot_cmd() % input_max].predicted = false;
			last = aa.get_shot_cmd();
		}
		if (can_shoot && (had_attack || had_secondary_attack))
		{
			last = min(last, cmd->command_number);
			if (had_attack)
				cmd->buttons |= user_cmd::attack;
			if (had_secondary_attack)
				cmd->buttons |= user_cmd::attack2;
			game->input->verified_commands[cmd->command_number % input_max] = { *cmd, cmd->get_checksum() };
		}
		game->prediction->get_predicted_commands() = std::clamp(last - game->client_state->last_command_ack - 1, 0, backup_predicted);
	}

	bool custom_prediction::is_predicting() const
	{
		return predicting;
	}
	
	bool custom_prediction::is_computing() const
	{
		return computing;
	}

	void custom_prediction::predict_post_pone_time(sdk::user_cmd* const cmd)
	{
		const auto wpn = reinterpret_cast<weapon_t* const>(game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));

		if (wpn && cmd->command_number > 0 && cmd->command_number - last_sequence <= input_max)
		{
			const auto last_cmd = &game->input->commands[(cmd->command_number - 1) % input_max];
			repredict(last_cmd);
			const auto last_tick = game->local_player->get_tick_base() - 1;
			
			wpn->get_postpone_fire_ready_time() = pred.info[(cmd->command_number - 1) % input_max].post_pone_time;

			if (wpn->get_item_definition_index() != item_definition_index::weapon_revolver ||
				wpn->get_in_reload() ||
				game->local_player->get_next_attack() > TICKS_TO_TIME(last_tick) ||
				wpn->get_next_primary_attack() > TICKS_TO_TIME(last_tick) ||
				(!(last_cmd->buttons & user_cmd::attack) && !(last_cmd->buttons & user_cmd::attack2)))
			{
				wpn->get_postpone_fire_ready_time() = FLT_MAX;
				prone_delay = -1;
			}
			else if (last_cmd->buttons & user_cmd::attack)
			{
				if (prone_delay == -1)
					prone_delay = last_tick + TIME_TO_TICKS(prone_time);

				if (prone_delay <= last_tick)
					wpn->get_postpone_fire_ready_time() = TICKS_TO_TIME(prone_delay) + post_delay;
			}

			auto& entry = pred.info[cmd->command_number % input_max];
			if (cmd->command_number != entry.sequence)
			{
				entry.sequence = cmd->command_number;
				entry.shift = entry.ev = 0;
			}
			entry.post_pone_time = wpn->get_postpone_fire_ready_time();
		}
	}
	
	void custom_prediction::predict_can_fire(user_cmd* const cmd)
	{
		can_shoot = had_attack = had_secondary_attack = false;
				
		const auto wpn = reinterpret_cast<weapon_t*>(
			game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));
		if (!wpn || wpn->is_grenade() || wpn->get_item_definition_index() == item_definition_index::weapon_healthshot)
			return;

		const auto last_shot_time = wpn->get_last_shot_time();
		const auto last_secondary_attack = wpn->get_next_secondary_attack();
		const auto clip = wpn->get_clip1();
		const auto attack_flag = wpn->get_item_definition_index() == item_definition_index::weapon_shield ? user_cmd::attack2 : user_cmd::attack;
		had_attack = !!(cmd->buttons & user_cmd::attack);
		had_secondary_attack = !!(cmd->buttons & user_cmd::attack2);
		const auto engage =(wpn->is_shootable_weapon() && clip > 0) || wpn->is_knife() || wpn->get_item_definition_index() == item_definition_index::weapon_shield;
		if (cfg.rage.auto_engage.get() && engage)
			cmd->buttons |= attack_flag;
		repredict(cmd);

		if (wpn == reinterpret_cast<weapon_t*>(game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon())))
			can_shoot = (wpn->get_item_definition_index() == item_definition_index::weapon_shield) ?
					(wpn->get_next_secondary_attack() != last_secondary_attack) :
					(last_shot_time != wpn->get_last_shot_time() && (!wpn->is_shootable_weapon() || clip > 0));
		if (engage || had_attack)
			cmd->buttons &= ~attack_flag;
	}
	
	void custom_prediction::restore_animation(const int32_t sequence, const bool env)
	{
		auto& log = pred.info[sequence % input_max];
		const auto local_player = game->local_player;
		if (!local_player || log.animation.sequence != sequence)
			return;
		
		const auto bak = *local_player->get_anim_state();
		*local_player->get_anim_state() = log.animation.state;
		bak.copy_meta(local_player->get_anim_state());
		*local_player->get_animation_layers() = env ? log.animation.visual_layers : log.animation.layers;
		local_player->get_pose_parameter() = log.animation.poses;

		if (log.sequence == sequence)
		{
			local_player->get_view_height() = log.view_height;
			local_player->get_bounds_change_time() = log.bounds_change_time;
			local_player->get_view_offset().z = log.view_offset;
			local_player->get_collideable()->obb_maxs() = log.obb_maxs;
			local_player->get_ground_entity() = log.ground_entity;
			if (env)
				local_player->get_view_height() = local_player->get_abs_origin().z + log.view_delta;
		}

		game->prediction->set_local_view_angles(log.animation.eye);
	}
}

move_data cs_game_movement_t::setup_move(cs_player_t* const player, user_cmd* cmd)
{
	move_data data{};

	data.first_run_of_functions = false;
	data.game_code_moved_player = false;
	data.player_handle = player->get_handle();

	data.velocity = player->get_velocity();
	data.abs_origin = player->get_abs_origin();
	data.client_max_speed = player->get_maxspeed();

	data.angles = data.view_angles = data.abs_view_angles = cmd->viewangles;
	data.impulse_command = cmd->impulse;
	data.buttons = cmd->buttons;

	data.forward_move = cmd->forwardmove;
	data.side_move = cmd->sidemove;
	data.up_move = cmd->upmove;

	data.constraint_center = vec3();
	data.constraint_radius = 0.f;
	data.constraint_speed_factor = 0.f;

	setup_movement_bounds(&data);

	return data;
}

move_delta_t cs_game_movement_t::process_movement(cs_player_t* const player, move_data* const data)
{
	move_delta_t backup;
	backup.store(player);

	is_processing = true;
	process_movement_int(player, data);
	is_processing = false;

	move_delta_t ret;
	ret.store(player);
	backup.restore(player);
	return ret;
}
