
#include <hacks/tickbase.h>
#include <hacks/antiaim.h>
#include <sdk/client_state.h>
#include <base/debug_overlay.h>
#include <base/cfg.h>
#include <detail/aim_helper.h>
#include <detail/custom_prediction.h>
#include <hacks/rage.h>
#include <hacks/peek_assistant.h>

using namespace sdk;
using namespace detail;
using namespace detail::aim_helper;
using namespace hooks;

namespace hacks
{
	tickbase_t tickbase{};
	
	void tickbase_t::reset()
	{
		limit = to_recharge = to_shift = to_adjust = clock_drift = last_drift = 0;
		force_choke = force_drop = skip_next_adjust = fast_fire = hide_shot = false;
		lag = { 0, 0 };
	}
	
	void tickbase_t::adjust_shift(int32_t cmd, int32_t ticks)
	{
		auto& entry = pred.info[cmd % input_max];
		
		if (cmd != entry.sequence)
		{
			entry.sequence = cmd;
			entry.shift = entry.ev = 0;
		}
		
		entry.shift += ticks;
	}
	
	void tickbase_t::adjust_limit_dynamic(bool finalize)
	{		
		const auto ready = !to_recharge && !to_shift;
		const auto attackable = is_attackable();
		
		if (attackable)
			to_recharge = 0;

		if (!game->local_player)
			return;

		const auto wpn = reinterpret_cast<weapon_t* const>(game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));
		if (!wpn || !ready)
			return;
				
		if (fabsf(wpn->get_last_shot_time() - game->globals->curtime) < max(.3f, .5f * wpn->get_cs_weapon_data()->cycle_time))
			return;
		
		if (peek_assistant.pos.has_value() && game->local_player->get_velocity().length() > 5.f)
			return;

		const auto clock_correction = game->cl_predict ? TIME_TO_TICKS(.03f) : 0;
		if (game->client_state->last_command_ack > last_drift + TIME_TO_TICKS(1.f)
			&& game->client_state->last_command + game->client_state->choked_commands + 1 > last_drift + 2
			&& abs(clock_drift - limit) > clock_correction + 2)
			limit = clock_drift;
	
		const auto diff = determine_optimal_limit() - limit;
		if (diff > 0 && !attackable)
			to_recharge = diff;
		else if (diff < 0 && !finalize)
		{
			to_shift = -diff;
			force_choke = true;
		}
	}
	
	bool tickbase_t::attempt_shift_back(bool& send_packet)
	{
		const auto wpn = reinterpret_cast<weapon_t* const>(game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));
		
		if ((!holds_tick_base_weapon() && (!wpn->is_grenade() || !peek_assistant.pos.has_value())) || to_shift > 0 || (!fast_fire && !hide_shot))
			return true;
		
		const auto optimal = determine_optimal_shift();

		if (limit > 3 && game->local_player->get_tick_base() > lag.first && (!fast_fire || wpn->get_item_definition_index() != item_definition_index::weapon_revolver))
		{
			const auto predicted_time = game->globals->curtime + TICKS_TO_TIME(limit);
			const auto release_tick = TIME_TO_TICKS(wpn->get_next_secondary_attack() - predicted_time);
			skip_next_adjust = wpn->get_item_definition_index() != item_definition_index::weapon_revolver
					|| (release_tick > 0 && release_tick < TIME_TO_TICKS(post_delay) - 3 - game->client_state->choked_commands);

			if (skip_next_adjust)
				send_packet = true;
			return false;
		}

		if (fast_fire)
			to_shift = optimal;

		send_packet = true;
		return true;
	}
	
	void tickbase_t::prepare_cycle()
	{
		const auto wpn = reinterpret_cast<weapon_t* const>(game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));
		
		if (aa.should_shift())
			skip_next_adjust = true;
		
		if (fast_fire && cfg.rage.fast_fire.get().test(cfg_t::fast_fire_peek) && wpn->get_item_definition_index() != item_definition_index::weapon_revolver && aa.is_peeking())
			skip_next_adjust = true;
		
		if (game->local_player->get_tick_base() < lag.first || wpn->is_grenade())
			skip_next_adjust = false;
		
		to_adjust = limit;
		if (skip_next_adjust)
		{
			to_adjust = 0;
			last_drift = game->client_state->last_command + game->client_state->choked_commands + 1;
			
			// add intermediate correction term during prediction.
			adjust_shift(game->client_state->last_command + 1, limit);
			adjust_shift(game->client_state->last_command + game->client_state->choked_commands + 2, -limit);
			game->prediction->get_predicted_commands() = std::clamp(game->client_state->last_command - game->client_state->last_command_ack, 0, game->prediction->get_predicted_commands());
		}
	}
	
	void tickbase_t::on_finalize_cycle(bool& send_packet)
	{
		// buffer the option swap so that the previous correction had the chance to apply on recharge.
		apply_static_configuration();
		adjust_limit_dynamic(true);
		
		// finally, adjust the tickbase.
		send_packet = skip_next_adjust = false;
		for (auto i = 0; i < to_adjust; i++)
		{
			game->client_state->choked_commands++;
			const auto sequence = game->client_state->last_command + game->client_state->choked_commands + 1;
			auto cmd = &game->input->commands[sequence % input_max];
			*cmd = game->input->commands[(sequence - 1) % input_max];
			cmd->command_number = sequence;
			cmd->buttons &= ~(user_cmd::attack | user_cmd::attack2);
			cmd->tick_count = INT_MAX;
			auto& verified = game->input->verified_commands[cmd->command_number % input_max];
			verified.cmd = *cmd;
			verified.crc = cmd->get_checksum();
		}
		send_packet = true;
	}
	
	void tickbase_t::apply_static_configuration()
	{
		const auto wpn = reinterpret_cast<weapon_t* const>(game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));
		
		if (cfg.antiaim.fake_duck.get())
		{
			fast_fire = hide_shot = false;
			return;
		}

        fast_fire = cfg.rage.enable_fast_fire.get();
		hide_shot = !fast_fire && cfg.antiaim.hide_shot.get();
	}
	
	int32_t tickbase_t::determine_optimal_shift() const
	{
		const auto wpn = reinterpret_cast<weapon_t* const>(game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));
		if (!wpn)
			return 0;
		
		if (wpn->is_grenade())
			return limit;

		return min(limit, TIME_TO_TICKS(wpn->get_cs_weapon_data()->cycle_time) - 1);
	}
	
	int32_t tickbase_t::determine_optimal_limit() const
	{		
		if (fast_fire)
			return max_new_cmds;
		
		if (hide_shot)
			return 5;
		
		return 0;
	}
}
