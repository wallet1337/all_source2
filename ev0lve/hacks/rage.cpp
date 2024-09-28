
#include <hacks/rage.h>
#include <sdk/global_vars_base.h>
#include <sdk/client_entity_list.h>
#include <sdk/model_info_client.h>
#include <sdk/engine_client.h>
#include <sdk/client_state.h>
#include <sdk/math.h>
#include <sdk/weapon_system.h>
#include <sdk/debug_overlay.h>
#include <sdk/game_rules.h>
#include <sdk/mdl_cache.h>
#include <base/debug_overlay.h>
#include <detail/aim_helper.h>
#include <detail/custom_tracing.h>
#include <detail/custom_prediction.h>
#include <detail/shot_tracker.h>
#include <hacks/antiaim.h>
#include <hacks/tickbase.h>
#include <hacks/peek_assistant.h>

using namespace sdk;
using namespace detail;
using namespace detail::aim_helper;

namespace sdk
{
	void cs_player_t::modify_eye_position(vec3& pos)
	{
		const auto state = get_anim_state();
		
		if (!state || !hacks::rag.should_recalculate_eye_position())
			return;

		if (state->in_hit_ground || state->duck_amount != 0.f || !get_ground_entity())
		{
			const auto head = lookup_bone(XOR_STR("head_0"));
			auto bone_pos = matrix_get_origin(local_shot_record.mat[head]);

			const auto bone_z = bone_pos.z + 1.7f;
			if (pos.z > bone_z)
			{
				const auto view_modifier = std::clamp((fabsf(pos.z - bone_z) - 4.f) * .16666667f, 0.f, 1.f);
				const auto view_modifier_sqr = view_modifier * view_modifier;
				pos.z += (bone_z - pos.z) * (3.f * view_modifier_sqr - 2.f * view_modifier_sqr * view_modifier);
			}
		}
	}
}

namespace hacks
{
	rage rag{};

	bool rage::on_create_move(sdk::user_cmd* cmd, bool send_packet)
	{
		pred.repredict(cmd);
		update_scan_map();
		
		const auto wpn = reinterpret_cast<weapon_t* const>(game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));
		const auto info = wpn->get_cs_weapon_data();
		
		cock_revolver(cmd);

		auto should_run = !tickbase.to_shift && !tickbase.force_choke && (
			((!aa.is_active() || aa.get_wish_lag() == 0) ? (cmd->command_number % 2 == 0) :
				((game->client_state->choked_commands % 2 == 1 && !aa.is_fakeducking()) || send_packet))
			|| pred.had_attack || pred.had_secondary_attack);

		// prevent us from shooting late into the shift.
		if (!tickbase.to_recharge && tickbase.to_shift > 0)
		{
			cancel_autostop();
			should_run = false;
		}

		// delay the shot if we can peek even further.
		if (cfg.rage.delay_shot.get() && (tickbase.fast_fire || !send_packet) && game->local_player->get_velocity().length() >= 2.f
				&& (tickbase.fast_fire ? (tickbase.lag.first - TIME_TO_TICKS(game->globals->curtime) > 6) : (game->client_state->choked_commands < max_new_cmds - 1))
				&& !wpn->is_knife()
				&& !peek_assistant.pos.has_value()
				&& !aa.is_lag_hittable())
		{
			cancel_autostop();
			should_run = false;
		}
		
		if (!pred.had_attack && !pred.had_secondary_attack && !cfg.rage.auto_engage.get())
			cancel_autostop();
		
		if (!pred.can_shoot && (!info->bfullauto || wpn->get_in_reload() || !wpn->get_clip1()) && cfg.rage.auto_engage.get())
			cancel_autostop();

		if (peek_assistant.has_shot)
			cancel_autostop();
		
		if (should_stop)
			autostop(cmd, true);
		
		// we need to backup this value here, because otherwise the autostop
		// might reset, but we can't scan again next tick. so just keep stopping until
		// the aimbot becomes active again.
		if (should_run)
			stopped_last_interval = has_stopped;
		
		active = pred.can_shoot && (aa.get_shot_cmd() < cmd->command_number - game->client_state->choked_commands || aa.get_shot_cmd() >= cmd->command_number);
		if (!active || !should_run)
			return false;

		auto target_cmd = &game->input->commands[(cmd->command_number - min(sv_maxusrcmdprocessticks / 2 - 1,
			TIME_TO_TICKS(game->globals->curtime - wpn->get_next_primary_attack()))) % input_max];
		if (!aa.is_fakeducking() || target_cmd->command_number < game->client_state->last_command + 1)
			target_cmd = cmd;

		const auto total = target_cmd->command_number - game->client_state->last_command - 1;
		const auto max_autostop = min(total, 2);

		if (cfg.rage.auto_engage.get())
			for (auto i = total - max_autostop; i <= total; i++)
			{
				const auto cur_cmd = &game->input->commands[(game->client_state->last_command + 1 + i) % input_max];
				game->input->verified_commands[(game->client_state->last_command + 1 + i) % input_max].cmd = *cur_cmd;

				if (cfg.rage.auto_scope.get() && wpn->is_scoped_weapon() && wpn->get_zoom_level() == 0
					&& game->local_player->is_on_ground() && !attempted_to_jump && wpn->get_inaccuracy() > 0.f)
					cur_cmd->buttons |= user_cmd::attack2;

				if (!info->bfullauto && peek_assistant.pos.has_value())
					autostop(cur_cmd, true, true, true);
				else
					autostop(cur_cmd, true, true, false);

				pred.repredict(cur_cmd);
			}

		pred.repredict(target_cmd);

		if (!full_scan(target_cmd, send_packet))
		{
			if (cfg.rage.auto_engage.get())
				for (auto i = total - max_autostop; i <= total; i++)
				{
					const auto cur_cmd = &game->input->commands[(game->client_state->last_command + 1 + i) % input_max];
					*cur_cmd = game->input->verified_commands[(game->client_state->last_command + 1 + i) % input_max].cmd;
					pred.repredict(cur_cmd);
				}
		}
		else if (intermediate_shot.has_value())
			return true;
		else if (!info->bfullauto && peek_assistant.pos.has_value())
		{
			if (cfg.rage.auto_engage.get())
				for (auto i = total - max_autostop; i <= total; i++)
				{
					const auto cur_cmd = &game->input->commands[(game->client_state->last_command + 1 + i) % input_max];
					const auto buttons = cur_cmd->buttons & user_cmd::attack2;
					*cur_cmd = game->input->verified_commands[(game->client_state->last_command + 1 + i) % input_max].cmd;
					cur_cmd->buttons |= buttons;
					autostop(cur_cmd, true, true, false);
					pred.repredict(cur_cmd);
				}
		}

		return false;
	}

	void rage::on_finalize_cycle(sdk::user_cmd* const cmd, bool& send_packet)
	{		
		if (!intermediate_shot.has_value())
			return;

		pred.repredict(cmd);
		const auto wpn = reinterpret_cast<weapon_t * const>(game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));

		// recalculate eye position.
		const auto prev_z = intermediate_shot->start.z;
		if (local_shot_record.valid)
		{
			// restore state on the 'current' server frame.
			const auto backup_state = *game->local_player->get_anim_state();
			*game->local_player->get_anim_state() = local_shot_record.state[resolver_networked];

			recalculate_eye_position = true;
			intermediate_shot->start = game->local_player->get_eye_position();
			recalculate_eye_position = false;
			*game->local_player->get_anim_state() = backup_state;
		}

		// recalculate view angles.
		auto viewangles = calc_angle(intermediate_shot->start, intermediate_shot->end);
		viewangles -= game->local_player->get_punch_angle_scaled();

		// check if the shot still is valid.
		if (!intermediate_shot->manual && fabsf(prev_z - intermediate_shot->start.z) > .01f)
		{
			const auto pen = trace.wall_penetration(intermediate_shot->start, intermediate_shot->end, intermediate_shot->record,
					false,intermediate_shot->target_direction, nullptr, true);
			if (!pen.did_hit || (pen.damage < get_mindmg(intermediate_shot->record->player, pen.hitbox)
				&& pen.damage <	get_adjusted_health(intermediate_shot->record->player)))
			{
				tickbase.to_shift = 0;
				peek_assistant.has_shot = false;
				drop_shot();
				aa.on_send_packet(cmd, true);
			}
		}

		pred.repredict(&game->input->commands[(cmd->command_number - 1) % input_max]);

		if (!intermediate_shot.has_value())
		{
			reset();
			return;
		}

		// fixup following commands.
		for (auto i = game->client_state->last_command + 1; i <= cmd->command_number; i++)
		{
			auto& c = game->input->commands[i % input_max];
			auto& verified = game->input->verified_commands[i % input_max];

			auto target = c.viewangles;
			c.viewangles = viewangles;
			normalize(c.viewangles);
			fix_movement(&c, target);
			normalize_move(c.forwardmove, c.sidemove, c.upmove);
			verified.cmd = c;
			verified.crc = c.get_checksum();
		}

		if (wpn && !wpn->is_knife())
		{
			// store off estimated impacts.
			detail::custom_tracing::wall_pen pen{};
			if (!intermediate_shot->manual)
				pen = trace.wall_penetration(intermediate_shot->start, intermediate_shot->end, intermediate_shot->record, false, intermediate_shot->target_direction, nullptr, true);
			else
				pen = trace.wall_penetration(intermediate_shot->start, intermediate_shot->end, nullptr);
			for (auto i = 0; i < pen.impact_count; i++)
				intermediate_shot->impacts.push_back(pen.impacts[i]);

			// draw impacts.
			if (cfg.local_visuals.impacts.get())
				for (const auto& impact : intermediate_shot->impacts)
					game->debug_overlay->box_overlay_2(impact, { -1.25f, -1.25f, -1.25f }, { 1.25f, 1.25f, 1.25f }, sdk::angle(),
							cfg.local_visuals.client_impacts.get(), sdk::color(250, 250, 250,
									static_cast<uint8_t>(.25f * cfg.local_visuals.client_impacts.get().a())), 4.f);
			// update resolver.
			if (!intermediate_shot->manual && !intermediate_shot->record->player->is_fake_player())
			{
				auto& entry = GET_PLAYER_ENTRY(intermediate_shot->record->player);
				auto& resolver = entry.resolver;
				auto& resolver_state = resolver.get_resolver_state(*intermediate_shot->record);
				resolver_state.switch_to_opposite(*intermediate_shot->record, intermediate_shot->record->res_direction);

				if (intermediate_shot->record->res_direction != resolver_state.direction)
				{
					resolver.map_resolver_state(*intermediate_shot->record, resolver_state);
					for (auto& other : entry.records)
					{
						other.res_direction = resolver.get_resolver_state(other).direction;
						if (other.has_matrix)
							memcpy(other.mat, other.res_mat[other.res_direction], sizeof(other.mat));
					}
				}
			}

			// finalize shot.
			shot_track.register_shot(std::move(*intermediate_shot));
		}

		reset();
	}

	void rage::on_manual(sdk::user_cmd* cmd, shot&& s)
	{
		intermediate_cmd = *cmd;
		intermediate_shot = std::move(s);
	}

	bool rage::did_stop() const
	{
		return has_stopped;
	}
	
	bool rage::did_stop_last_interval() const
	{
		return stopped_last_interval;
	}
	
	bool rage::should_recalculate_eye_position() const
	{
		return recalculate_eye_position;
	}
	
	void rage::cock_revolver(sdk::user_cmd* const cmd)
	{
		const auto wpn = reinterpret_cast<weapon_t* const>(game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));

		if (!wpn || wpn->get_item_definition_index() != item_definition_index::weapon_revolver)
			return;
		
		cmd->buttons &= ~user_cmd::attack2;
		
		if (!wpn->get_clip1() || wpn->get_in_reload() || wpn->get_next_primary_attack() <= 0.f
			|| game->local_player->get_next_attack() > game->globals->curtime)
			return;
		
		if (pred.info[cmd->command_number % input_max].post_pone_time >= game->globals->curtime)
			cmd->buttons |= user_cmd::attack;
		else if (game->globals->curtime < wpn->get_next_secondary_attack() - TICKS_TO_TIME(3))
			cmd->buttons |= user_cmd::attack2;
	}

	void rage::prioritize_player(cs_player_t* const player, const uint32_t amount)
	{
		if (is_priority_scan)
			return;

		for (auto i = 0u; i < amount; i++)
			scan_list_priority.push_back(player->index());
	}
	
	bool rage::is_player_in_range(cs_player_t* const player)
	{
		const auto wpn = reinterpret_cast<weapon_t* const>(game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));
		const auto shoot = aa.is_fakeducking() ? aa.get_visual_eye_position() : game->local_player->get_eye_position();

		auto& entry = GET_PLAYER_ENTRY(player);
		if (entry.records.empty() || (player->get_abs_origin() - shoot).length() > wpn->get_cs_weapon_data()->range)
			return false;
		const auto latest = entry.get_record(record_filter::latest);

		lag_record working_copy;
		if (latest)
			working_copy = *latest;
		else
		{
			const auto first = &entry.records.front();
			working_copy = *first;

			const auto rtt = TIME_TO_TICKS(calculate_rtt());
			const auto possible_future_tick = game->client_state->last_server_tick + 1 + rtt + 8;
			const auto max_future_ticks = possible_future_tick - TIME_TO_TICKS(working_copy.sim_time + calculate_lerp());
			const auto lag = max(1, working_copy.lag);

			auto to_server_position = working_copy.is_breaking_lagcomp();
			const auto behind = max(-1, game->client_state->last_server_tick - working_copy.server_tick - 2);

			if (!to_server_position)
			{
				for (auto i = 1; i <= max_future_ticks; i++)
				{
					working_copy.lag = max(0, (i + behind) / lag * lag);
					working_copy.sim_time = first->sim_time + TICKS_TO_TIME(working_copy.lag);
					if (working_copy.is_valid())
						break;
				}

				if (!working_copy.is_valid())
					to_server_position = true;
			}

			if (to_server_position)
			{
				working_copy.lag = max(0, (rtt + behind) / lag * lag);
				working_copy.sim_time += TICKS_TO_TIME(working_copy.lag);
			}

			const auto backup_abs_origin = player->get_abs_origin();
			const auto backup_ground_entity = player->get_ground_entity();
			const auto backup_move_type = player->get_move_type();
			const auto backup_velocity = player->get_velocity();

			user_cmd cmd{};
			const auto speed = working_copy.velocity.length2d();
			for (auto i = 0; i < working_copy.lag; i++)
			{
				const auto p1 = entry.records.size() > 1 ? &entry.records[1] : nullptr;
				const auto p2 = entry.records.size() > 2 ? &entry.records[2] : nullptr;

				vec3 pchange, vchange;
				if (p1 && p2)
				{
					const auto delta = (p1->final_velocity - p2->final_velocity) / p1->lag;
					vchange = (first->final_velocity - p1->final_velocity) / first->lag;
					pchange = vchange - delta;
				}

				angle achange;
				const auto pvel = player->get_velocity() + pchange;
				vector_angles(pvel, achange);
				cmd.viewangles.y = achange.y;
				cmd.forwardmove = speed > 5.f ? forward_bounds : (i % 2 ? 1.01f : -1.01f);
				cmd.sidemove = 0.f;

				if (!(working_copy.flags & cs_player_t::flags::on_ground) && !player->get_ground_entity() && achange.y != 0.f)
				{
					cmd.forwardmove = 0.f;
					cmd.sidemove = achange.y > 0.f ? side_bounds : -side_bounds;
				}
				else if (p1 && speed < p1->velocity.length2d() - 5.f * first->lag)
				{
					auto data = game->cs_game_movement->setup_move(player, &cmd);
					slow_movement(&data, 0.f);
					cmd.forwardmove = data.forward_move;
				}
				else if (p1 && speed < p1->velocity.length2d() + 5.f * first->lag && speed > 5.f)
				{
					auto data = game->cs_game_movement->setup_move(player, &cmd);
					slow_movement(&data, pvel.length2d());
					cmd.forwardmove = data.forward_move;
				}

				auto data = game->cs_game_movement->setup_move(player, &cmd);
				game->cs_game_movement->process_movement(player, &data);

				if (p1)
				{
					if (!(p1->flags & cs_player_t::flags::on_ground) && !(working_copy.flags & cs_player_t::flags::on_ground) && player->get_ground_entity())
						cmd.buttons |= user_cmd::jump;
					else
						cmd.buttons &= ~user_cmd::jump;
				}

				player->set_abs_origin(data.abs_origin);
				player->get_velocity() = data.velocity;

				if (i == working_copy.lag - 1)
					working_copy.origin = data.abs_origin;
			}

			player->get_abs_origin() = backup_abs_origin;
			player->get_ground_entity() = backup_ground_entity;
			player->get_move_type() = backup_move_type;
			player->get_velocity() = backup_velocity;
		}
		
		const auto first_shoot = working_copy.origin + vec3(0, 0, game->rules->data->get_view_vectors()->duck_view.z +
			(game->rules->data->get_view_vectors()->view - game->rules->data->get_view_vectors()->duck_view).z * (1.f - working_copy.duck));
		const auto right = vec3(shoot - first_shoot)
			.normalize().cross({ 0.f, 0.f, 1.f }).normalize();
		
		if (in_range(shoot, first_shoot + right * 22.f)
			|| in_range(shoot, first_shoot - right * 22.f))
			return true;
		
		if (const auto oldest = entry.get_record(record_filter::oldest); oldest != latest)
			return in_range(shoot, oldest->origin + vec3(0, 0, game->rules->data->get_view_vectors()->duck_view.z +
				(game->rules->data->get_view_vectors()->view - game->rules->data->get_view_vectors()->duck_view).z * (1.f - oldest->duck)));

		return false;
	}

	bool rage::did_shoot() const
	{
		return intermediate_shot.has_value();
	}

	void rage::drop_shot()
	{
		intermediate_shot = std::nullopt;
		if (intermediate_cmd.has_value())
		{
			user_cmd cmd{};
			cmd.command_number = 0;
			aa.set_shot_cmd(&cmd);
			game->input->commands[intermediate_cmd->command_number % input_max] = *intermediate_cmd;
		}
		intermediate_cmd = std::nullopt;
	}

	void rage::reset()
	{
		intermediate_shot = std::nullopt;
		intermediate_cmd = std::nullopt;
		active = false;
	}

	bool rage::has_priority_targets() const
	{
		return !scan_list_priority.empty();
	}
	
	bool rage::is_active() const
	{
		return active;
	}

	void rage::set_attempt_jump(bool jump)
	{
		attempted_to_jump = jump;
	}
	
	bool rage::is_player_target(cs_player_t* player) const
	{
		return scan_map[player->index()];
	}

	void rage::update_scan_map()
	{
		// determine if we want to scan this player.
		scan_map.fill(false);
		game->client_entity_list->for_each_player([&](cs_player_t* const player)
		{
			auto& entry = GET_PLAYER_ENTRY(player);
			
			if (!player->is_enemy() || !player->is_valid() || player->get_gun_game_immunity()
				|| player->get_move_type() == movetype_noclip || (!entry.records.empty() && entry.records.front().dormant)
				|| entry.pvs || !is_player_in_range(player))
			{
				entry.last_target = nullptr;
				return;
			}
			
			scan_map[player->index()] = true;
		});

		// remove outdated and priority entries in scan list.
		for (auto it = scan_list_default.begin(); it != scan_list_default.end();)
		{
			const auto player = reinterpret_cast<cs_player_t* const>(game->client_entity_list->get_client_entity(*it));

			if (!player || !scan_map[player->index()])
				it = scan_list_default.erase(it);
			else
				it = std::next(it);
		}

		for (auto it = scan_list_priority.begin(); it != scan_list_priority.end();)
		{
			const auto player = reinterpret_cast<cs_player_t* const>(game->client_entity_list->get_client_entity(*it));

			if (!player || !scan_map[player->index()])
				it = scan_list_priority.erase(it);
			else
				it = std::next(it);
		}

		// populate scan list with new entries.
		game->client_entity_list->for_each_player([&](cs_player_t* const player)
		{
			if (!scan_map[player->index()])
				return;

			if (std::find(scan_list_default.begin(), scan_list_default.end(), player->index()) != scan_list_default.end())
				return;

			scan_list_default.push_front(player->index());
		});

		for (auto i = 0; i < min(scan_list_default.size(), 2); i++)
			player_list.on_target_player(reinterpret_cast<cs_player_t*>(game->client_entity_list->get_client_entity(scan_list_default[i])));
	}

	bool rage::full_scan(sdk::user_cmd* const cmd, bool send_packet)
	{
		const auto wpn = reinterpret_cast<weapon_t* const>(game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));

		is_engaged = false;
		auto target = scan_targets(cmd);

		if (!target.has_value() || !target->pen.did_hit)
			target = scan_dormants(cmd);

		if (!target.has_value() || !target->pen.did_hit)
		{
			had_target = false;
			return is_engaged;
		}

		// delay the shot.
		if (target->record->is_breaking_lagcomp() && target->record->should_delay_shot() && target->record->can_delay_shot())
			return true;

		// make sure to track him regardless of the last scan state.
		is_priority_scan = false;
		prioritize_player(target->record->player, 3);

		// store shot.
		auto s = shot();
		s.damage = target->pen.damage;
		s.start = game->local_player->get_eye_position();
		s.end = target->position;
		s.hitgroup = target->hitgroup;
		s.hitbox = target->pen.hitbox;
		s.time = game->globals->curtime;
		s.manual = false;
		s.alt_attack = target->alt_attack;
		s.server_info.health = target->record->player->get_health();
		s.record = target->record;
		s.target = std::make_shared<rage_target>(*target);
		s.cmd_num = cmd->command_number;
		s.cmd_tick = cmd->tick_count;
		s.server_tick = game->client_state->last_server_tick;
		s.target_tick = cmd->tick_count = TIME_TO_TICKS(target->record->sim_time + calculate_lerp());
		s.target_direction = resolver_networked;

		intermediate_cmd = *cmd;
		aa.set_shot_cmd(cmd);
		auto wish = cmd->viewangles;
		cmd->viewangles = calc_angle(s.start, s.end) - game->local_player->get_punch_angle_scaled();
		normalize(cmd->viewangles);
		fix_movement(cmd, wish);
		cmd->buttons |= s.alt_attack ? user_cmd::attack2 : user_cmd::attack;

		if (!target->record->player->is_fake_player())
			s.target_direction = target->record->res_direction;

		intermediate_shot = s;
		return true;
	}

	std::optional<rage_target> rage::scan_targets(sdk::user_cmd* const cmd)
	{		
		const auto wpn = reinterpret_cast<weapon_t* const>(game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));

		auto current_target = 0;
		is_priority_scan = false;

		if (!scan_list_priority.empty())
		{
			current_target = scan_list_priority.back();
			scan_list_priority.pop_back();
			is_priority_scan = true;
		}
		else if (!scan_list_default.empty())
		{
			current_target = scan_list_default.back();
			scan_list_default.pop_back();
		}

		if (!current_target)
		{
			cancel_autostop();
			return std::nullopt;
		}

		const auto player = reinterpret_cast<cs_player_t*>(game->client_entity_list->get_client_entity(current_target));
		auto& entry = GET_PLAYER_ENTRY(player);
		
		auto hitpoints = scan_target(cmd, player);
		rage_target* best_match = nullptr;

		// find best target spot of all valid spots.
		for (auto& hitpoint : hitpoints)
		{
			auto target = !best_match ? std::nullopt : std::optional(*best_match);
			auto alternative = std::optional(hitpoint);

			if (cfg.rage.fov.get() < 180.f &&
						get_fov(cmd->viewangles, game->local_player->get_eye_position(), alternative.value().position) > cfg.rage.fov.get())
					continue;

			if (should_prefer_record(target, alternative))
				best_match = &hitpoint;
		}

		// stop if no target found.
		if (!best_match)
		{
			cancel_autostop();
			return std::nullopt;
		}

		is_engaged = true;

		// abort here if we are using the knife.
		if (wpn->is_knife() || wpn->get_item_definition_index() == item_definition_index::weapon_shield)
			return *best_match;
		
		// perform final step optimization.
		optimize_cornerpoint(best_match);

		// stop the player, we have targets!
		if (cfg.rage.auto_engage.get())
			autostop(cmd, should_stop = true);

		// calculate hitchance.
		best_match->hc = calculate_hitchance(best_match);

		// abort if damage or hitchance is not suitable.
		const auto target_hitchance = aim_helper::get_hitchance(best_match->record->player);
		if (best_match->hc < target_hitchance && (!has_full_accuracy() || best_match->hc < target_hitchance * .6f))
		{
			// scope the weapon_t if hitchance is not good enough.
			if (cfg.rage.auto_scope.get() && wpn->is_scoped_weapon() && wpn->get_zoom_level() == 0
				&& !attempted_to_jump && wpn->get_inaccuracy() > 0.f)
				cmd->buttons |= user_cmd::attack2;

			return std::nullopt;
		}
		
		return *best_match;
	}

	std::optional<rage_target> rage::scan_dormants(user_cmd* const cmd)
	{
		const auto wpn = reinterpret_cast<weapon_t* const>(game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));
		const auto data = wpn->get_cs_weapon_data();

		// abort here if we are using the knife.
		if (wpn->is_knife() || !cfg.rage.dormant.get())
			return std::nullopt;

		std::vector<rage_target> targets{};

		game->client_entity_list->for_each_player([&targets, &data](cs_player_t* const player)
		{
			if (!player->is_valid(true) || !player->is_dormant() || !player->is_enemy())
				return;

			const auto& player_entry = GET_PLAYER_ENTRY(player);
			if (!player_entry.visuals.has_matrix || player_entry.dormant_miss > 1 || player_entry.visuals.last_update + TIME_TO_TICKS(5.f) < game->client_state->last_server_tick)
				return;

			const auto center = player_entry.visuals.pos.target + vec3(0, 0, game->rules->data->get_view_vectors()->duck_view.z + 1.f);
			const auto record = std::make_shared<lag_record>(player);
			record->has_matrix = true;
			record->dormant = true;
			memcpy(record->mat, player_entry.visuals.matrix, sizeof(lag_record::mat));
			auto start = player_entry.visuals.abs_org;
			move_matrix(record->mat, start, player_entry.visuals.pos.target);
			record->res_direction = resolver_networked;
			memcpy(record->res_mat[resolver_networked], record->mat, sizeof(lag_record::mat));
			rage_target target(center, record, false, center, cs_player_t::hitbox::body, hitgroup_stomach, 0.f);
			target.pen = trace.wall_penetration(game->local_player->get_eye_position(), target.position, record);

			if (!target.pen.did_hit)
				return;

			const auto expected_damage = trace.scale_damage(player, target.pen.damage, data->flarmorratio, hitgroup_stomach);
			if (expected_damage > get_mindmg(player, cs_player_t::hitbox::body) || expected_damage > get_adjusted_health(player) + health_buffer)
			{
				target.pen.secure_point = true;
				target.pen.damage = target.pen.min_damage = expected_damage;
				targets.push_back(target);
			}
		});

		// select best target.
		std::optional<rage_target> best_match{};
		for (auto& target : targets)
		{
			if (cfg.rage.only_visible.get() && target.pen.impact_count > 1)
				continue;

			auto alternative = std::optional(target);

			if (should_prefer_record(best_match, alternative))
				best_match.emplace(target);
		}

		if (best_match)
		{
			is_engaged = true;

			// stop the player, we have targets!
			if (cfg.rage.auto_engage.get())
				autostop(cmd, should_stop = true);

			// calculate hitchance.
			best_match->hc = has_full_accuracy() ? 100.f : calculate_hitchance(&*best_match);

			// abort if damage or hitchance is not suitable.
			const auto target_hitchance = aim_helper::get_hitchance(best_match->record->player);
			if (best_match->hc < target_hitchance)
			{
				// scope the weapon_t if hitchance is not good enough.
				if (cfg.rage.auto_scope.get() && wpn->is_scoped_weapon() && wpn->get_zoom_level() == 0
					&& !attempted_to_jump && wpn->get_inaccuracy() > 0.f)
					cmd->buttons |= user_cmd::attack2;

				return std::nullopt;
			}
		}

		return best_match;
	}

	std::vector<rage_target> rage::scan_target(user_cmd* const cmd, cs_player_t* const player)
	{
		std::vector<rage_target> hitpoints{};

		if (!player || !player->is_enemy() || !player->is_valid())
			return hitpoints;

		auto& entry = GET_PLAYER_ENTRY(player);
		const auto first = !entry.records.empty() ? &entry.records.front() : nullptr;

		if (!first)
			return hitpoints;

		// grab targets to scan.
		std::optional<rage_target> target;
		const auto latest = entry.get_record(record_filter::latest);
		const auto oldest = entry.get_record(record_filter::oldest);
		const auto shot = entry.get_record(record_filter::shot);
		const auto angle_diff = entry.get_record(record_filter::angle_diff);

		// grab last target tick.
		const auto target_time = entry.aimbot.target_time;
		entry.aimbot.target_time = 0.f;
		entry.aimbot.had_secure_point_this_tick = false;

		// function to finalize and store target.
		const auto finalize = [&]()
		{
			entry.hittable = target.has_value();

			if (target.has_value())
			{
				if (target->pen.damage >= get_mindmg(target->record->player, target->pen.hitbox))
					entry.aimbot.target_time = target_time + TICKS_TO_TIME(game->client_state->choked_commands == 1 ? 2 : 1);

				prioritize_player(target->record->player, 2);
				hitpoints.push_back(std::move(*target));
			}
		};

		// need to extrapolate...
		if (!first->dormant && first->valid && ((!latest && !oldest && !shot) || (latest && latest->is_breaking_lagcomp())))
		{
			if (!first->can_delay_shot(true))
			{
				target = scan_record(*first, false, true);
				finalize();
				return hitpoints;
			}

			if (first->should_delay_shot())
				return hitpoints;
		}
		
		// scan latest.
		if (latest)
			target = scan_record(*latest);

		// scan highest angle diff.
		if (angle_diff && angle_diff != latest && angle_diff != shot && (angle_diff != oldest || !angle_diff->is_moving()))
		{
			auto alternative = scan_record(*angle_diff);

			if (should_prefer_record(target, alternative))
				target = std::move(alternative);
		}
		
		// is there a shot that should be used instead?
		if (shot)
		{
			auto alternative = scan_record(*shot);

			if (should_prefer_record(target, alternative))
				target = std::move(alternative);
		}
		// are there two distinct records?
		// and if there are, is the last one moving?
		else if (oldest && (latest == nullptr || latest->recv_time != oldest->recv_time) && oldest->is_moving())
		{
			auto alternative = scan_record(*oldest);

			if (should_prefer_record(target, alternative))
				target = std::move(alternative);
		}
		// is he standing and crouched?
		else if ((!target.has_value() || !target->record->is_moving()) && latest != nullptr && fabsf(latest->duck) > .1f)
		{
			// let's see if he was standing a moment ago...
			const auto uncrouched = entry.get_record(record_filter::uncrouched);

			if (uncrouched)
			{
				auto alternative = scan_record(*uncrouched);

				if (should_prefer_record(target, alternative))
					target = std::move(alternative);
			}
		}
		
		finalize();
		return hitpoints;
	}

	std::optional<rage_target> rage::scan_record(lag_record& record, const bool minimal, bool extrapolate)
	{
		const auto wpn = reinterpret_cast<weapon_t*>(
			game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));

		if (!wpn || (!extrapolate && record.is_breaking_lagcomp() && (!record.can_delay_shot() || record.should_delay_shot())))
			return std::nullopt;

		// create a copy of current information.
		const auto working_copy = std::make_shared<lag_record>(record);
		std::shared_ptr<lag_record> previous;
		if (!extrapolate)
			working_copy->perform_matrix_setup();

		// predict the player ahead for a few ticks...
		if (extrapolate)
		{
			auto& entry = GET_PLAYER_ENTRY(record.player);
			if (entry.records.size() > 1 && entry.records[1].valid)
				previous = std::make_shared<lag_record>(entry.records[1]);

			const auto rtt = TIME_TO_TICKS(calculate_rtt());
			const auto possible_future_tick = game->client_state->last_server_tick + 1 + rtt + 8;
			const auto max_future_ticks = possible_future_tick - TIME_TO_TICKS(working_copy->sim_time + calculate_lerp());
			const auto lag = max(1, working_copy->lag);

			auto to_server_position = working_copy->is_breaking_lagcomp();
			const auto behind = max(-1, game->client_state->last_server_tick - working_copy->server_tick - 2);

			if (!to_server_position)
			{
				for (auto i = 1; i <= max_future_ticks; i++)
				{
					working_copy->lag = max(0, (i + behind) / lag * lag);
					working_copy->sim_time = record.sim_time + TICKS_TO_TIME(working_copy->lag);
					if (working_copy->is_valid())
						break;
				}

				if (!working_copy->is_valid())
					to_server_position = true;
			}

			if (to_server_position)
				working_copy->lag = max(0, (rtt + behind) / lag * lag);

			if (tickbase.limit > 3 && game->local_player->get_tick_base() > tickbase.lag.first && (!tickbase.fast_fire || wpn->get_item_definition_index() != item_definition_index::weapon_revolver))
				working_copy->lag += 2;

			game->mdl_cache->begin_lock();
			working_copy->sim_time = record.sim_time;
			working_copy->velocity = working_copy->final_velocity;

			if (working_copy->lag > 0)
			{
				auto extrapolation_base = std::make_shared<lag_record>(*working_copy);
				working_copy->extrapolated = extrapolation_base->extrapolated = true;
				extrapolation_base->lag = record.lag;

				player_list.perform_animations(working_copy.get(), extrapolation_base.get());

				if (working_copy->lag > max_future_ticks)
					working_copy->lag = max_future_ticks - 2;

				working_copy->sim_time += TICKS_TO_TIME(working_copy->lag);
				working_copy->lag = record.lag;
				working_copy->has_matrix = false;
				working_copy->perform_matrix_setup(true);
			}
			else
			{
				working_copy->lag = record.lag;
				working_copy->perform_matrix_setup();
			}

			if (record.velocity.length() > 2.f || working_copy->lag > 0 || entry.resolver.unreliable_extrapolation)
				working_copy->force_safepoint = true;

			if (previous)
			{
				previous->extrapolated = true;
				previous->origin = working_copy->origin;
				previous->perform_matrix_setup(false, true);
				working_copy->previous = previous.get();
			}

			game->mdl_cache->end_lock();
		}

		if (wpn->is_knife())
			return scan_record_knife(working_copy);

		if (wpn->get_item_definition_index() == item_definition_index::weapon_shield)
			return scan_record_shield(working_copy);

		return scan_record_gun(working_copy, minimal);
	}

	std::optional<rage_target> rage::scan_record_knife(const std::shared_ptr<detail::lag_record>& record)
	{
		const auto wpn = reinterpret_cast<weapon_t*>(
			game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));
		const auto info = wpn->get_cs_weapon_data();
		const auto studio_model = game->model_info_client->get_studio_model(record->player->get_model());

		if (!studio_model)
			return std::nullopt;
		
		auto spot = get_hitbox_position(record->player, cs_player_t::hitbox::upper_chest, record->mat);
		const auto hitbox = studio_model->get_hitbox(static_cast<uint32_t>(cs_player_t::hitbox::upper_chest), record->player->get_hitbox_set());
		
		if (!spot.has_value() || !hitbox)
			return std::nullopt;
		
		const auto start = game->local_player->get_eye_position();
		const auto calc = calc_angle(start, spot.value());
		vec3 forward;
		angle_vectors(calc, forward);
		trace_simple_filter filter(game->local_player);
		const auto first_swing = wpn->get_next_primary_attack() + .4f < game->globals->curtime;
		const auto swing_or_stab = [&](bool stab) -> rage_target
		{
			custom_tracing::wall_pen pen{};
			const auto range = stab ? 32.f : 48.f;
			const auto end = start + forward * range;
			game_trace tr{};
			ray r{};
			r.init(start, end);
			game->engine_trace->trace_ray(r, mask_solid, &filter, &tr);
			
			if (tr.fraction >= 1.f)
			{
				r.init(start, end, { -16, -16, -18 }, { 16, 16, 18 });
				game->engine_trace->trace_ray(r, mask_solid, &filter, &tr);
			}
			
			const auto end_pos = tr.endpos;
			if (tr.fraction >= 1.f || tr.entity != record->player)
				return rage_target { end_pos, record, stab, end_pos, cs_player_t::hitbox::upper_chest, 0, 0.f, std::move(pen) };
			
			auto vec_los = record->origin - game->local_player->get_origin();
			vec_los.z = 0.0f;
			vec3 fwd;
			angle_vectors(record->player->get_abs_angle(), fwd);
			fwd.z = 0.0f;
			const auto back_stab = vec_los.normalize().dot(fwd) > .475f;
			
			auto damage = 0.f;
			if (stab)
				damage = back_stab ? 180.f : 65.f;
			else
				damage = back_stab ? 90.f : (first_swing ? 40.f : 25.f);
			
			pen.damage = trace.scale_damage(record->player, damage, info->flarmorratio, hitgroup_generic);
			pen.did_hit = true;
			pen.secure_point = back_stab;
			return rage_target { end_pos, record, stab, end_pos, cs_player_t::hitbox::upper_chest, 0, 0.f, std::move(pen) };
		};

		const auto backup_abs_origin = record->player->get_abs_origin();
		const auto backup_obb_mins = record->player->get_collideable()->obb_mins();
		const auto backup_obb_maxs = record->player->get_collideable()->obb_maxs();
		record->player->set_abs_origin(record->origin, true);
		record->player->get_collideable()->obb_mins() = record->obb_mins;
		record->player->get_collideable()->obb_maxs() = record->obb_maxs;
		const auto swing = swing_or_stab(false);
		const auto stab = swing_or_stab(true);
		record->player->set_abs_origin(backup_abs_origin, true);
		record->player->get_collideable()->obb_mins() = backup_obb_mins;
		record->player->get_collideable()->obb_maxs() = backup_obb_maxs;
		record->player->get_eflags() |= efl_dirty_abs_transform;
		
		if (get_adjusted_health(record->player) <= swing.pen.damage)
			return swing;
		
		if (get_adjusted_health(record->player) <= stab.pen.damage)
			return stab;
		
		if (swing.pen.damage > 0.f && !swing.pen.secure_point)
			return swing;
		
		return std::nullopt;
	}

	std::optional<rage_target> rage::scan_record_shield(const std::shared_ptr<detail::lag_record>& record)
	{
		const auto wpn = reinterpret_cast<weapon_t*>(
			game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));
		const auto studio_model = game->model_info_client->get_studio_model(record->player->get_model());

		if (!studio_model)
			return std::nullopt;

		auto spot = get_hitbox_position(record->player, cs_player_t::hitbox::upper_chest, record->mat);
		const auto hitbox = studio_model->get_hitbox(static_cast<uint32_t>(cs_player_t::hitbox::upper_chest), record->player->get_hitbox_set());

		if (!spot.has_value() || !hitbox)
			return std::nullopt;

		const auto start = game->local_player->get_eye_position();
		const auto calc = calc_angle(start, spot.value());
		vec3 forward;
		angle_vectors(calc, forward);
		trace_simple_filter filter(game->local_player);
		const auto end = start + forward * 64.f;

		const auto backup_abs_origin = record->player->get_abs_origin();
		const auto backup_obb_mins = record->player->get_collideable()->obb_mins();
		const auto backup_obb_maxs = record->player->get_collideable()->obb_maxs();
		record->player->set_abs_origin(record->origin);
		record->player->get_collideable()->obb_mins() = record->obb_mins;
		record->player->get_collideable()->obb_maxs() = record->obb_maxs;

		game_trace tr{};
		ray r{};
		r.init(start, end);
		game->engine_trace->trace_ray(r, mask_solid, &filter, &tr);

		if (tr.did_hit() && tr.entity && tr.entity == record->player)
		{
			custom_tracing::wall_pen pen{};
			pen.damage = trace.scale_damage(record->player, 90.f, wpn->get_cs_weapon_data()->flarmorratio, tr.hitgroup);
			pen.did_hit = pen.secure_point = true;
			record->player->set_abs_origin(backup_abs_origin);
			record->player->get_collideable()->obb_mins() = backup_obb_mins;
			record->player->get_collideable()->obb_maxs() = backup_obb_maxs;
			return rage_target { end, record, false, end, cs_player_t::hitbox::upper_chest, 0, 0.f, std::move(pen) };
		}

		record->player->set_abs_origin(backup_abs_origin);
		record->player->get_collideable()->obb_mins() = backup_obb_mins;
		record->player->get_collideable()->obb_maxs() = backup_obb_maxs;
		return std::nullopt;
	}

	std::optional<rage_target> rage::scan_record_gun(const std::shared_ptr<detail::lag_record>& record, const bool minimal)
	{
		if (!record->player)
			return std::nullopt;

		auto& entry = GET_PLAYER_ENTRY(record->player);

		std::optional<rage_target> best_match, best_secure;
		const auto unreliable = entry.resolver.unreliable >= 2 && !cfg.rage.hack.secure_point->test(cfg_t::secure_point_disabled);
		auto targets = perform_hitscan(record, minimal, unreliable);
		auto was_hit = false;
		const auto should_force = unreliable || cfg.rage.hack.secure_point.get().test(cfg_t::secure_point_force) || record->force_safepoint;
		const auto health = get_adjusted_health(record->player) + health_buffer;
		const auto is_lethal_in_body = GET_CONVAR_INT("mp_damage_headshot_only") ? false :
				std::find_if(targets.begin(), targets.end(), [&health](rage_target& target)
		{
			return is_body_hitbox(target.hitbox) && target.pen.potential_damage >= health;
		}) != targets.end();

		// perform gun scan.
		for (auto& target : targets)
		{
			if (cfg.rage.only_visible.get() && target.pen.impact_count > 1)
				continue;
			
			if (target.pen.did_hit)
			{
				if (target.pen.min_damage < get_mindmg(target.record->player, target.pen.hitbox)
					&& target.pen.min_damage < health)
					target.pen.secure_point = target.pen.very_secure = false;

				was_hit = true;
			}
			else
				target.pen.secure_point = target.pen.very_secure = false;

			if (!target.pen.very_secure && should_avoid_hitbox(target.pen.hitbox))
				continue;

			if (!target.pen.very_secure && cfg.rage.hack.lethal->test(cfg_t::lethal_secure_points) && is_lethal_in_body)
				continue;

			if (!is_body_hitbox(target.hitbox) && cfg.rage.hack.lethal->test(cfg_t::lethal_body_aim) && is_lethal_in_body)
				continue;

			if (is_limbs_hitbox(target.hitbox) && cfg.rage.hack.lethal->test(cfg_t::lethal_limbs) && is_lethal_in_body)
				continue;
			
			auto alternative = std::optional(target);

			if (should_prefer_record(best_match, alternative))
				best_match.emplace(target);

			if (!minimal && target.pen.secure_point && (!should_force || target.pen.very_secure) && should_prefer_record(best_secure, alternative))
				best_secure.emplace(target);
		}

		const auto should_prefer = best_secure.has_value() && cfg.rage.hack.secure_point.get().test(cfg_t::secure_point_prefer);

		if (was_hit)
			prioritize_player(record->player);

		if (!minimal)
		{
			// update target delay.
			if (best_secure.has_value())
				entry.aimbot.had_secure_point_this_tick = true;

			// no secure aimpoint available.
			if (should_force && !best_secure.has_value())
				return std::nullopt;

			// swap to secure point.
			if (should_prefer || should_force)
				std::swap(best_match, best_secure);

			if (best_match.has_value() && best_match->record->has_visual_matrix && best_match->pen.did_hit)
			{
				const auto visual_pos = get_hitbox_position(record->player, best_match->hitbox, best_match->record->vis_mat);
				if (visual_pos.has_value())
				{
					entry.interpolated_target.update(*visual_pos - best_match->record->abs_origin, .05f);
					if (!entry.last_target)
						entry.interpolated_target.current = entry.interpolated_target.target;
				}
				entry.last_target = std::make_shared<rage_target>(*best_match);
			}
		}

		// make sure the match obeys config values.
		if (!best_match.has_value() || !best_match->pen.did_hit ||
			(best_match->pen.damage < get_mindmg(best_match->record->player, best_match->pen.hitbox)
			&& best_match->pen.damage < health))
			return std::nullopt;

		// report result.
		return best_match;
	}

	void rage::autostop(sdk::user_cmd* const cmd, const bool should_stop, const bool force, bool full)
	{
		if (!force)
		{
			if (!should_stop)
				has_stopped = false;

			if (should_stop || has_stopped)
			{
				has_stopped = true;
				return;
			}
		}
		
		const auto wpn = reinterpret_cast<weapon_t * const>(
			game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));

		if (!wpn)
			return;

		if ((wpn->get_item_definition_index() == item_definition_index::weapon_taser && !cfg.antiaim.slow_walk.get())
			|| !game->local_player->is_on_ground()
			|| cmd->buttons & user_cmd::jump
			|| attempted_to_jump
			|| game->local_player->get_move_type() == movetype_ladder
			|| GET_CONVAR_INT("weapon_accuracy_nospread"))
			return;
		
		// do not use shiftwalk.
		cmd->buttons &= ~user_cmd::speed;

		// determine if we can slide stop.
		const auto should_slide = (cfg.antiaim.slow_walk.get() && (cfg.antiaim.slowwalk_mode.get().test(cfg_t::slowwalk_speed)
			|| aa.determine_maximum_lag() < 8
			|| game->client_state->choked_commands < aa.determine_maximum_lag() - 3)) || !cfg.antiaim.slow_walk.get();

		// calculate new speed.
		const auto slide_speed = (wpn->get_item_definition_index() == item_definition_index::weapon_revolver ? 180.f : wpn->get_max_speed()) / 3.f - 1.f;
		slow_movement(cmd, (should_slide && !full) ? slide_speed : 0.f);
	}
	
	void rage::cancel_autostop()
	{
		should_stop = stopped_last_interval = false;
	}
}
