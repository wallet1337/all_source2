
#include <detail/player_list.h>
#include <sdk/client_entity_list.h>
#include <sdk/weapon.h>
#include <sdk/weapon_system.h>
#include <sdk/client_state.h>
#include <sdk/engine_client.h>
#include <sdk/math.h>
#include <sdk/game_rules.h>
#include <sdk/debug_overlay.h>
#include <sdk/mdl_cache.h>
#include <detail/custom_tracing.h>
#include <detail/aim_helper.h>
#include <base/cfg.h>
#include <detail/shot_tracker.h>
#include <base/debug_overlay.h>
#include <hooks/hooks.h>
#include <detail/custom_prediction.h>
#include <detail/events.h>
#include <hacks/antiaim.h>
#include <hacks/rage.h>
#include <hacks/tickbase.h>
#include <hacks/skinchanger.h>
#include <base/hook_manager.h>

using namespace sdk;
using namespace detail::aim_helper;
using namespace hacks;
using namespace hacks::misc;

namespace detail
{
	i_player_list player_list = i_player_list();
	lag_record local_record = lag_record(), local_shot_record = lag_record(), local_fake_record = lag_record();

	void i_player_list::reset()
	{
		for (auto& entry : entries)
			entry = player_entry();
	}

	void i_player_list::on_update_end(sdk::cs_player_t* const player)
	{
		auto& player_entry = GET_PLAYER_ENTRY(player);

		if (!player->is_valid() || player->index() == game->engine_client->get_local_player())
		{
			const auto local = player->index() == game->engine_client->get_local_player();
			
			if (local)
			{
				player_entry.records.clear();
				player_entry.compensation_time = 0.f;
			}

			return;
		}

		// erase records out-of-range.
		for (auto it = player_entry.records.rbegin(); it != player_entry.records.rend();)
		{
			if (game->globals->curtime - it->sim_time > 1.2f)
				it = decltype(it) { player_entry.records.erase(next(it).base()) };
			else
				it = next(it);
		}
		
		// failsafe against respawn.
		if (!player->get_tick_base())
			player_entry.pvs = false;
		
		// move pvs indicator forwards.
		if (player_entry.pvs)
		{
			// update player with stale data.
			const auto state = player->get_anim_state();
			player->get_simulation_time() = state->last_update = TICKS_TO_TIME(player->get_tick_base());
			state->velocity = player->get_velocity();

			state->velocity.z = 0.f;
			state->feet_cycle = player_entry.server_layers[6].cycle;
			state->feet_weight = player_entry.server_layers[6].weight;
			state->strafe_sequence = player_entry.server_layers[7].sequence;
			state->strafe_change_cycle = player_entry.server_layers[7].cycle;
			state->strafe_change_weight = player_entry.server_layers[7].weight;
			state->acceleration_weight = player_entry.server_layers[12].weight;
			state->on_ground = !!(player->get_flags() & cs_player_t::on_ground);
			state->on_ladder = player->get_move_type() == movetype_ladder;
			if (!state->on_ground)
				state->air_time = player_entry.server_layers[4].cycle / player_entry.server_layers[4].playback_rate;

			player_entry.pvs = false;
			auto dormant = lag_record(player);
			dormant.layers = player_entry.server_layers;
			dormant.valid = dormant.dormant = false;
			player_entry.records.push_front(dormant);

			player->get_eflags() |= efl_dirty_abs_transform | efl_dirty_abs_velocity;
			return;
		}
		
		// determine if simulation has occured.
		auto last_record = !player_entry.records.empty() ? &player_entry.records.front() : nullptr;
		auto matching = last_record != nullptr;
		if (last_record)
			for (auto i = 0; i < last_record->layers.size(); i++)
				if (last_record->layers[i] != player_entry.server_layers[i])
				{
					matching = false;
					break;
				}
		if (matching)
			return;
		
		// detect simulation amount.
		lag_record record(player);
		record.layers = player_entry.server_layers;
		record.determine_simulation_ticks(last_record);

		// update bounds.
		if (player_entry.old_maxs_z != FLT_MAX && cfg.rage.enable.get())
		{
			player->get_view_height() = player->get_origin().z + player_entry.old_maxs_z;
			player->get_bounds_change_time() = record.sim_time;
			player_entry.view_delta = player->get_view_height() - player->get_origin().z;
			player_entry.old_maxs_z = FLT_MAX;
		}
		
		// continue the interpolation if the update has been postponed.
		if (player->get_simulation_time() != record.sim_time)
		{
			player->on_simulation_time_changing(player->get_simulation_time(), record.sim_time);
			player->get_simulation_time() = record.sim_time;
		}

		// have we already seen this update?
		if (record.lag <= 0 || record.lag > TIME_TO_TICKS(1.f))
			return;

		// count jittering pitch for dumb reasons.
		if (player_entry.pitch_prev == XOR_32(0x42b1fa80) && *reinterpret_cast<uint32_t*>(&player->get_eye_angles().x) == XOR_32(0x42b1fd50))
			player_entry.pitch_alt = max(player_entry.pitch_alt + 1, 4);
		else if (player_entry.pitch_prev != XOR_32(0x42b1fd50) || *reinterpret_cast<uint32_t*>(&player->get_eye_angles().x) != XOR_32(0x42b1fa80))
			player_entry.pitch_alt = 0;
		player_entry.pitch_prev = *reinterpret_cast<uint32_t*>(&player->get_eye_angles().x);
		if (player_entry.pitch_alt >= 4)
			record.do_not_set = true;

		// place the record in the list.
		game->mdl_cache->begin_lock();
		player_entry.records.push_front(std::move(record));
		auto& rec = player_entry.records.front();

		// animate the record.
		if (cfg.rage.enable.get())
		{
			perform_animations(&rec, last_record);
			player->get_eflags() |= efl_dirty_abs_transform | efl_dirty_abs_velocity;
		}

		// update the resolver.
		if (last_record && !last_record->extrapolated)
			update_resolver_state(&rec, last_record);

		if (cfg.rage.enable.get())
		{
			// restore everything.
			player->get_lower_body_yaw_target() = rec.lower_body_yaw_target;
			*player->get_animation_layers() = rec.custom_layers[rec.res_direction];
			*player->get_anim_state() = rec.state[rec.res_direction];
			player->get_pose_parameter() = rec.poses[rec.res_direction];
			hook_manager.set_abs_angles->call(player, 0, &rec.abs_ang[rec.res_direction]);

			// hans, get us new blending.
			player->get_last_non_skipped_frame() = 0;
		}

		// tickbase drifting records are invalid.
		if (rec.sim_time <= player_entry.compensation_time)
		{
			if (game->cl_predict && game->cl_lagcompensation)
				rec.valid = false;
		}
		// set lagcompensation time.
		else
			player_entry.compensation_time = player->get_simulation_time();

		// fixing llama's dumb exploit.
		if (!player_entry.addt && last_record && TIME_TO_TICKS(last_record->delta_time) < 6)
			rec.valid = false;

		player_entry.dormant_miss = 0;
		game->mdl_cache->end_lock();
	}
	
	void i_player_list::on_shot_resolve(shot& s, const vec3& end, const bool spread_miss)
	{
		if (!cfg.rage.enable.get())
			return;

		auto& record = s.record;
		const auto player = record->player;
		auto& entry = GET_PLAYER_ENTRY(player);
		auto& resolver = entry.resolver;
		auto& resolver_state = resolver.get_resolver_state(*record);
		auto& positions = resolver_state.eliminated_positions;

		if (player->is_fake_player())
			return;

		// killed?
		if (!player->is_alive())
		{
			resolver_state.set_direction(record->res_direction);

			if (record->res_direction != resolver_state.direction)
			{
				resolver.map_resolver_state(*record, resolver_state);
				for (auto& other : entry.records)
				{
					other.res_direction = resolver.get_resolver_state(other).direction;
					if (other.has_matrix)
						memcpy(other.mat, other.res_mat[other.res_direction], sizeof(other.mat));
				}
			}

			return;
		}

		// missed on server.
		if (s.server_info.damage <= 0)
		{
			std::bitset<resolver_direction_max> alt_positions{};

			// eliminate resolver directions.
			for (auto i = 0; i < resolver_direction_max; i++)
			{
				const auto pen = trace.wall_penetration(s.start, end, s.record, false, static_cast<resolver_direction>(i));

				if (pen.did_hit)
				{
					positions.set(i);
					alt_positions.set(i);
				}
			}

			// did we go through all the values?
			if (positions.all())
				resolver.unreliable = min(resolver.unreliable + 1, 2);

			// update brute entries.
			if (positions.all() && !alt_positions.all())
				positions = alt_positions;
			else if (positions.all())
				positions.reset();

			// go to opposite.
			resolver_state.switch_to_opposite(*record, record->res_direction);

			// increase resolver misses.
			if (!spread_miss)
			{
				// stop the detection.
				resolver.stop_detection = true;
				entry.resolver_miss = std::clamp(entry.resolver_miss + 1, 0, 2);
			}
		}
		// did we hit the enemy?
		else
		{
			// alternative brute list.
			std::bitset<resolver_direction_max> alt_positions{};

			// setup distance vector.
			std::array<float, resolver_direction_max> distances;
			distances.fill(FLT_MAX);

			// perform reduction over all positions.
			for (auto i = 0; i < resolver_direction_max; i++)
			{
				const auto pen = trace.wall_penetration(s.start, end, s.record, false, static_cast<resolver_direction>(i));

				if (!pen.did_hit || pen.hitgroup != s.server_info.hitgroup)
				{
					positions.set(i);
					alt_positions.set(i);
					continue;
				}

				for (auto& impact : s.server_info.impacts)
				{
					const auto length_remaining = (impact - pen.end).length();

					if (length_remaining < distances[i])
						distances[i] = length_remaining;
				}
			}

			// update reliability state.
			if (positions.all())
				resolver.unreliable = min(resolver.unreliable + 1, 2);
			
			// update brute entries.
			if (positions.all() && !alt_positions.all())
				positions = alt_positions;
			else if (positions.all())
				positions.reset();

			// try to minimize distance delta so that we increase the possibility of hitting next time.
			std::pair<resolver_direction, float> current_delta = { record->res_direction, FLT_MAX };
			if (distances[record->res_direction] > .1f)
				for (auto i = 0; i < resolver_direction_max; i++)
					if (distances[i] < current_delta.second)
						current_delta = { static_cast<resolver_direction>(i), distances[i] };

			resolver_state.set_direction(current_delta.first);
			
			if (record->res_direction != resolver_state.direction)
				resolver.stop_detection = true;
		}

		// did resolver state change?
		if (record->res_direction != resolver_state.direction)
		{
			resolver.map_resolver_state(*record, resolver_state);
			
			// find all records and update matrices.
			for (auto& other : entry.records)
			{
				other.res_direction = resolver.get_resolver_state(other).direction;
				if (other.has_matrix)
					memcpy(other.mat, other.res_mat[other.res_direction], sizeof(other.mat));
			}
		}
	}
	
	void i_player_list::on_target_player(cs_player_t* const player)
	{
		if (!cfg.rage.enable.get() || player->is_fake_player())
			return;
		
		auto& entry = GET_PLAYER_ENTRY(player);
		auto& resolver = entry.resolver;
		const auto latest = entry.get_record();
		
		if (!latest)
			return;

		// run detection step.
		const auto from = player->get_eye_position();
		const auto to = game->local_player->get_eye_position();
		const auto at_target = calc_angle(from, to);
		resolver.detected_yaw = perform_freestanding(from, to, &resolver.previous_freestanding).first;
		resolver.right = std::remainderf(resolver.detected_yaw - at_target.y, yaw_bounds) > 0.f;

		const auto pitch = latest->eye_ang.x - floorf(latest->eye_ang.x / 360.f + .5f) * 360.f;
		if (fabsf(pitch) <= 50.f)
			return;

		if (latest->res_state == resolver_shot)
			return;

		if (entry.resolver.detected_layer && latest->is_moving())
			return;

		// perform reduction over all open positions.
		if (!entry.resolver.stop_detection)
		{
			const auto working_copy = std::make_shared<lag_record>(*latest);
			working_copy->res_right = resolver.right;
			auto& resolver_state = resolver.get_resolver_state(*working_copy);

			std::pair<resolver_direction, float> current_delta = {resolver_networked, FLT_MAX};
			for (auto i = 0; i < resolver_direction_max; i++)
			{
				const auto pos = get_hitbox_position(player, cs_player_t::hitbox::head, working_copy->res_mat[i]);
				if (!pos.has_value())
					continue;

				const auto ang = calc_angle(working_copy->origin, *pos);
				const auto delta = fabsf(std::remainderf(ang.y - resolver.detected_yaw, yaw_bounds));

				if (!resolver_state.eliminated_positions.test(i) && delta < current_delta.second)
					current_delta = {static_cast<resolver_direction>(i), delta};
			}

			if (current_delta.first == resolver_networked)
				return;

			// did resolver state change?
			resolver_state.direction = current_delta.first;
			if (working_copy->res_direction != resolver_state.direction)
			{
				resolver.map_resolver_state(*working_copy, resolver_state);

				// find all records and update matrices.
				for (auto& other: entry.records) {
					other.res_direction = resolver.get_resolver_state(other).direction;
					if (other.has_matrix)
						memcpy(other.mat, other.res_mat[other.res_direction], sizeof(other.mat));
				}
			}

			// apply also to the other side.
			working_copy->res_right = !working_copy->res_right;
			auto& other_state = resolver.get_resolver_state(*working_copy);
			other_state.set_direction(current_delta.first);
			resolver.map_resolver_state(*working_copy, resolver_state);

			// find all records and update matrices.
			for (auto& other: entry.records)
			{
				other.res_direction = resolver.get_resolver_state(other).direction;
				if (other.has_matrix)
					memcpy(other.mat, other.res_mat[other.res_direction], sizeof(other.mat));
			}
		}
	}
	
	void i_player_list::refresh_local()
	{
		const auto old = game->local_player;

		if (!game->engine_client->is_ingame() || !game->engine_client->is_connected() ||
			(game->local_player && (game->local_player != reinterpret_cast<cs_player_t*>(game->client_entity_list->get_client_entity(
				game->engine_client->get_local_player())) || !game->local_player->is_alive())))
			game->local_player = nullptr;

		const auto out = !game->local_player && old;
		if (out)
		{
			skin_changer->remove_glove();
			skin_changer->clear_sequence_table();
			local_record = local_shot_record = local_fake_record = lag_record();
			evnt.is_game_end = false;
			for (auto& p : pred.info)
				p.post_pone_time = FLT_MAX;
			rag.reset();
		}

		const auto reset = game->local_player && game->local_player->get_spawn_time() != local_record.recv_time;
		if (reset)
		{
			local_record = local_shot_record = local_fake_record = lag_record(game->local_player);
			local_record.recv_time = game->local_player->get_spawn_time();
			evnt.is_game_end = false;
			for (auto& p : pred.info)
				p.post_pone_time = FLT_MAX;

			auto death_notice = find_hud_element(XOR_STR("CCSGO_HudDeathNotice"));

			if (death_notice - 0x14)
				reinterpret_cast<void (__thiscall *)(uintptr_t)>(game->client.at(functions::clear_death_notices))(death_notice - 0x14);
		}
		
		if (evnt.is_freezetime || reset || out)
			for (auto& entry : player_list.entries)
			{
				entry.visuals.last_update = 0;
				entry.visuals.has_matrix = false;
				entry.visuals.alpha = 0.f;
				entry.dormant_miss = entry.spread_miss = entry.resolver_miss = 0;
			}
	}

	void i_player_list::merge_local_animation(cs_player_t* const player, user_cmd* cmd)
	{
		game->mdl_cache->begin_lock();
		local_record.player = player;
		
		auto& crc = game->input->verified_commands[cmd->command_number % input_max].crc;
		const auto initial = crc == *reinterpret_cast<int32_t*>(&game->globals->interval_per_tick);

		if (!initial && cmd->command_number <= game->client_state->last_command && player->get_tick_base() > tickbase.lag.first)
		{
			player->get_last_non_skipped_frame() = 0;
			local_record.has_visual_matrix = false;
			tickbase.lag = std::make_pair(player->get_tick_base(), game->client_state->last_command);
		}
		
		auto& current = pred.info[cmd->command_number % input_max].animation;
		auto& before = pred.info[(cmd->command_number - 1) % input_max].animation;
		
		if (current.sequence != cmd->command_number || crc != current.checksum)
		{
			const auto in = &game->input->commands[cmd->command_number % input_max];
			
			if (in->command_number == cmd->command_number && before.sequence == cmd->command_number - 1)
			{
				before.restore(player);
				local_record.update_animations(in);
			}
			else if (current.sequence == cmd->command_number)
			{
				current.restore(player);
				player->get_strafing() = current.get_strafing;
			}
			else
			{
				player->get_lower_body_yaw_target() = 0.f;
				for (auto& layer : *player->get_animation_layers())
					layer = animation_layer();
				player->get_anim_state()->player = player;
				player->get_anim_state()->reset();
				local_record = lag_record();
				local_record.player = player;
				local_record.update_animations(in);
				local_record.addon.vm = XOR_32(183);
				before = animation_copy(cmd->command_number - 1, player);
				local_record.update_animations(in);
			}

			current = animation_copy(cmd->command_number, player);
		}
		else
			player->get_strafing() = current.get_strafing;

		game->mdl_cache->end_lock();
	}
	
	void i_player_list::update_addon_bits(cs_player_t* const player)
	{
		static const auto wpn_c4 = XOR_STR_STORE("weapon_c4");
		static const auto wpn_shield = XOR_STR_STORE("weapon_shield");
		XOR_STR_STACK(c4, wpn_c4);
		XOR_STR_STACK(shield, wpn_shield);
		
		const auto wpn = reinterpret_cast<weapon_t * const>(
			game->client_entity_list->get_client_entity_from_handle(player->get_active_weapon()));
		const auto item_definition = wpn ? wpn->get_item_definition_index() : item_definition_index(-1);
		const auto world_model = wpn ? reinterpret_cast<entity_t* const>(
			game->client_entity_list->get_client_entity_from_handle(wpn->get_weapon_world_model())) : nullptr;
		
		int32_t new_bits = 0;
		auto weapon_visible = world_model ? !(world_model->get_effects() & ef_nodraw): true;
		
		auto flash_bang = player->get_ammo_count(FNV1A("AMMO_TYPE_FLASHBANG"));
		if (weapon_visible && item_definition == item_definition_index::weapon_flashbang)
			flash_bang--;
		
		if (flash_bang >= 1)
			new_bits |= 0x1;
		
		if (flash_bang >= 2)
			new_bits |= 0x2;
		
		if (player->get_ammo_count(FNV1A("AMMO_TYPE_HEGRENADE"))
			&& (item_definition != item_definition_index::weapon_hegrenade || !weapon_visible))
			new_bits |= 0x4;
			
		if (player->get_ammo_count(FNV1A("AMMO_TYPE_SMOKEGRENADE"))
			&& (item_definition != item_definition_index::weapon_smokegrenade || !weapon_visible))
			new_bits |= 0x8;
		
		if (player->get_ammo_count(FNV1A("AMMO_TYPE_DECOY"))
			&& (item_definition != item_definition_index::weapon_decoy || !weapon_visible))
			new_bits |= 0x200;
			
		if (player->get_ammo_count(FNV1A("AMMO_TYPE_TAGRENADE"))
			&& (item_definition != item_definition_index::weapon_tagrenade || !weapon_visible))
			new_bits |= 0x1000;
			
		if (player->owns_this_type(c4)
			&& (item_definition != item_definition_index::weapon_c4 || !weapon_visible))
			new_bits |= 0x10;
		
		if (player->get_player_defuser())
			new_bits |= 0x20;
		
		const auto rifle = player->get_slot(0);
		if (rifle && (rifle != wpn || !weapon_visible))
		{
			new_bits |= 0x40;
			player->get_primary_addon() = int32_t(rifle->get_item_definition_index());
		}
		else
			player->get_primary_addon() = 0;
		
		const auto pistol = player->get_slot(1);
		if (pistol && (pistol != wpn || !weapon_visible))
		{
			new_bits |= 0x80;
			if (pistol->get_item_definition_index() == item_definition_index::weapon_elite)
				new_bits |= 0x100;			
			player->get_secondary_addon() = int32_t(pistol->get_item_definition_index());
		}
		else if (pistol && pistol->get_item_definition_index() == item_definition_index::weapon_elite)
		{
			new_bits |= 0x100;			
			player->get_secondary_addon() = int32_t(pistol->get_item_definition_index());
		}
		else
			player->get_secondary_addon() = 0;

		const auto knife = player->get_slot(2);
		if (knife && (knife != wpn || !weapon_visible))
			new_bits |= 0x400;

		if (player->owns_this_type(shield)
			&& (item_definition != item_definition_index::weapon_shield || !weapon_visible))
			new_bits |= 0x2000;

		if (player->index() == game->engine_client->get_local_player())
			skin_changer->on_addon_bits(player, new_bits);
		
		player->get_addon_bits() = new_bits;
	}
	
	void i_player_list::build_fake_animation(cs_player_t* const player, user_cmd* const cmd)
	{
		// store off real animation.
		game->mdl_cache->begin_lock();
		pred.repredict(cmd);
		auto& log = pred.info[tickbase.lag.second % input_max].animation;
		if (log.sequence == tickbase.lag.second)
			log.restore(game->local_player);
		const auto real_layers = *player->get_animation_layers();
		const auto buttons = cmd->buttons;
		
		// restore buttons.
		for (auto i = 0; i < game->client_state->choked_commands; i++)
			cmd->buttons |= game->input->commands[(cmd->command_number - i - 1) % input_max].buttons & user_cmd::jump;
		
		// emplace last fake.
		if (local_fake_record.has_state)
		{
			const auto bak = *player->get_anim_state();
			*player->get_anim_state() = local_fake_record.state[resolver_networked];
			bak.copy_meta(player->get_anim_state());
			*player->get_animation_layers() = local_fake_record.custom_layers[resolver_networked];
			player->get_pose_parameter() = local_fake_record.poses[resolver_networked];
			player->get_anim_state()->feet_cycle = player->get_animation_layers()->at(6).cycle;
			player->get_anim_state()->feet_weight = player->get_animation_layers()->at(6).weight;
		}

		// animate fake.
		local_fake_record.update_animations(cmd);
		
		// selectively merge in the server layers now.
		const auto& entry = GET_PLAYER_ENTRY(game->local_player);
		for (auto i = 0u; i < game->local_player->get_animation_layers()->size(); i++)
		{
			auto& client = game->local_player->get_animation_layers()->at(i);
			auto& real = real_layers[i];
			
			client.order = real.order;

			if (i == 1 || (i > 7 && i < 12))
				client = real;
		}

		// store fake record.
		cmd->buttons = buttons;
		local_fake_record.has_state = true;
		local_fake_record.state[resolver_networked] = *player->get_anim_state();
		local_fake_record.layers = local_fake_record.custom_layers[resolver_networked] = *player->get_animation_layers();
		local_fake_record.poses[resolver_networked] = player->get_pose_parameter();
		local_fake_record.abs_ang[resolver_networked] = { 0.f, player->get_anim_state()->foot_yaw, 0.f };
		local_fake_record.origin = player->get_origin();
		local_fake_record.abs_origin = player->get_abs_origin();
		local_fake_record.eye_ang = *player->eye_angles();
		local_fake_record.has_matrix = false;
		if (game->input->camera_in_third_person && !cfg.local_visuals.fake_chams.mode.get().test(cfg_t::chams_disabled))
		{
			const auto view = *game->local_player->eye_angles();
			game->prediction->set_local_view_angles({ cmd->viewangles.x, cmd->viewangles.y, 0.f });
			local_fake_record.perform_matrix_setup();
			game->prediction->set_local_view_angles(view);
		}

		game->mdl_cache->end_lock();
	}

	void i_player_list::perform_animations(lag_record* const current, lag_record* const last)
	{
		const auto player = current->player;
		const auto state = player->get_anim_state();
		auto& entry = GET_PLAYER_ENTRY(player);
		auto target_record = last ? last : current;
		const auto is_onground = current->flags & cs_player_t::on_ground;
		const auto is_fake = player->is_fake_player();
		const auto full_setup = !is_fake && player->is_enemy();

		const auto wpn = reinterpret_cast<weapon_t*>(game->client_entity_list->get_client_entity_from_handle(
				current->player->get_active_weapon()));

		// did the player shoot in this frame chunk?
		current->shot = last && !last->extrapolated && wpn && wpn->is_shootable_weapon()
			&& wpn->get_last_shot_time() > last->sim_time && wpn->get_last_shot_time() <= current->sim_time;

		// reset spread misses, if possible.
		if ((!last || !last->extrapolated) && current->velocity.length() > 200.f && current->flags & cs_player_t::flags::on_ground)
			entry.spread_miss = 0;

		// correct velocity when standing.
		if (!current->is_moving_on_server() && is_onground)
			current->velocity = vec3();

		// intermediate helper record.
		auto intermediate = lag_record(player);
		if (last)
			intermediate.addon = last->addon;

		// steal the velocity from the alive loop.
		if (last && !last->extrapolated && current->layers[11].playback_rate == last->layers[11].playback_rate
				&& wpn == state->weapon && current->lag > 1)
		{
			const auto speed = min(current->velocity.length2d(), max_cs_player_move_speed);
			const auto max_movement_speed = wpn ? max(wpn->get_max_speed(), .001f) : max_cs_player_move_speed;
			const auto portion = std::clamp(1.f - current->layers[11].weight, 0.f, 1.f);
			const auto vel = (portion * .35f + .55f) * max_movement_speed;
			if ((portion > 0.f && portion < 1.f)
					|| (portion == 0.f && speed > vel)
					|| (portion == 1.f && speed < vel))
			{
				auto v1 = player->get_velocity();
				v1.x = v1.x / speed * vel;
				v1.y = v1.y / speed * vel;
				if (v1.is_valid())
					player->get_velocity() = v1;
			}
		}

		// backup stuff.
		const auto backup_poses = player->get_pose_parameter();
		const auto backup_layers = *player->get_animation_layers();
		const auto backup_state = *state;
		const auto backup_abs_origin = player->get_abs_origin();
		const auto backup_abs_angle = player->get_abs_angle();
		const auto backup_lby = player->get_lower_body_yaw_target();
		const auto backup_flags = player->get_flags();
		const auto backup_strafing = player->get_strafing();
		const auto backup_ground_entity = player->get_ground_entity();
		const auto backup_velocity = player->get_velocity();
		const auto backup_velocity_modifier = player->get_velocity_modifier();
		const auto backup_eye_angles = player->get_eye_angles();
		const auto backup_curtime = game->globals->curtime;
		const auto backup_frametime = game->globals->frametime;

		// set simulation start.
		player->set_abs_origin(target_record->origin);
		player->get_flags() = target_record->flags;
		player->get_velocity_modifier() = target_record->velocity_modifier;
		if (!target_record->velocity.is_valid())
			target_record->velocity = { 0.f, 0.f, 0.f };
		player->get_velocity() = target_record->velocity;

		if (target_record->flags & cs_player_t::flags::on_ground)
			player->get_ground_entity() = game->client_entity_list->get_client_entity(0)->get_handle();
		else
			player->get_ground_entity() = 0;

		// run simulation.
		user_cmd cmd{};
		cmd.buttons |= user_cmd::bull_rush;
		float lower_body_yaw[resolver_direction_max];
		for (auto j = 0; j < current->lag; j++)
		{
			const auto lerp = static_cast<float>(j + 1) / static_cast<float>(current->lag);

			// setup animation time.
			intermediate.sim_time = game->globals->curtime = current->sim_time - TICKS_TO_TIME(current->lag - j - 1);
			cmd.command_number = TIME_TO_TICKS(intermediate.sim_time);
			game->globals->frametime = game->globals->interval_per_tick;

			// intermediate movement.
			const auto speed = current->velocity.length2d();
			if (current->lag > 1 && last && !last->extrapolated && full_setup)
			{
				cmd.viewangles.y = calc_angle(player->get_abs_origin(), current->origin).y;
				cmd.forwardmove = speed;
				cmd.sidemove = 0.f;

				if (speed < last->velocity.length2d())
				{
					auto data = game->cs_game_movement->setup_move(player, &cmd);
					slow_movement(&data, speed);
					cmd.forwardmove = data.forward_move;
				}

				if (!(last->flags & cs_player_t::flags::on_ground) && !(current->flags & cs_player_t::flags::on_ground) && player->get_ground_entity())
					cmd.buttons |= user_cmd::jump;
				else
					cmd.buttons &= ~user_cmd::jump;

				auto data = game->cs_game_movement->setup_move(player, &cmd);
				const auto delta = game->cs_game_movement->process_movement(player, &data);
				player->get_ground_entity() = delta.ground_entity;
				player->get_velocity_modifier() = delta.velocity_modifier;

				player->set_abs_origin(data.abs_origin);
				player->get_velocity() = data.velocity;

				if (player->get_ground_entity())
					player->get_flags() |= cs_player_t::flags::on_ground;
				else
					player->get_flags() &= ~cs_player_t::flags::on_ground;

				// steal the velocity from the alive loop.
				if (current->layers[11].playback_rate == last->layers[11].playback_rate && wpn == state->weapon)
				{
					const auto speed = min(current->velocity.length2d(), max_cs_player_move_speed);
					const auto max_movement_speed = wpn ? max(wpn->get_max_speed(), .001f) : max_cs_player_move_speed;
					const auto portion = std::clamp(1.f - current->layers[11].weight, 0.f, 1.f);
					const auto vel = (portion * .35f + .55f) * max_movement_speed;
					if ((portion > 0.f && portion < 1.f)
							|| (portion == 0.f && speed > vel)
							|| (portion == 1.f && speed < vel))
					{
						auto v1 = player->get_velocity();
						v1.x = v1.x / speed * vel;
						v1.y = v1.y / speed * vel;
						if (v1.is_valid())
							player->get_velocity() = v1;
					}
				}
			}
			else if (last && last->extrapolated)
			{
				const auto p1 = entry.records.size() > 1 ? &entry.records[1] : nullptr;
				const auto p2 = entry.records.size() > 2 ? &entry.records[2] : nullptr;

				vec3 pchange, vchange;
				if (p1 && p2)
				{
					const auto delta = (p1->final_velocity - p2->final_velocity) / p1->lag;
					vchange = (last->final_velocity - p1->final_velocity) / last->lag;
					pchange = vchange - delta;
				}

				angle achange;
				const auto pvel = player->get_velocity() + pchange;
				vector_angles(pvel, achange);
				cmd.viewangles.y = achange.y;
				cmd.forwardmove = speed > 5.f ? forward_bounds : (j % 2 ? 1.01f : -1.01f);
				cmd.sidemove = 0.f;

				if (!(current->flags & cs_player_t::flags::on_ground) && !player->get_ground_entity() && achange.y != 0.f && !is_fake)
				{
					cmd.forwardmove = 0.f;
					cmd.sidemove = achange.y > 0.f ? side_bounds : -side_bounds;
				}
				else if (p1 && (speed < p1->velocity.length2d() - 5.f * last->lag || speed <= 5.f) && !is_fake)
				{
					auto data = game->cs_game_movement->setup_move(player, &cmd);
					slow_movement(&data, 1.01f);
					cmd.forwardmove = data.forward_move;
				}
				else if (p1 && speed < p1->velocity.length2d() + 5.f * last->lag && (speed > 5.f || is_fake))
				{
					auto data = game->cs_game_movement->setup_move(player, &cmd);
					slow_movement(&data, pvel.length2d());
					cmd.forwardmove = data.forward_move;
				}

				auto data = game->cs_game_movement->setup_move(player, &cmd);
				const auto delta = game->cs_game_movement->process_movement(player, &data);
				player->get_ground_entity() = delta.ground_entity;
				player->get_velocity_modifier() = delta.velocity_modifier;

				if (p1)
				{
					if (!(p1->flags & cs_player_t::flags::on_ground) && !(current->flags & cs_player_t::flags::on_ground) && player->get_ground_entity())
						cmd.buttons |= user_cmd::jump;
					else
						cmd.buttons &= ~user_cmd::jump;
				}

				player->set_abs_origin(data.abs_origin);
				player->get_velocity() = data.velocity;

				if (player->get_ground_entity())
					player->get_flags() |= cs_player_t::flags::on_ground;
				else
					player->get_flags() &= ~cs_player_t::flags::on_ground;

				if (j == current->lag - 1)
					current->origin = data.abs_origin;

				player->get_eye_angles() = current->eye_ang;
			}
			else
				player->get_velocity() = current->velocity;

			// set correct velocity for the hook.
			if (j == current->lag - 1 && current->layers[6].playback_rate <= 0.f && is_onground)
				player->get_velocity() = vec3();

			// check if game movement screwed up.
			if (!player->get_velocity().is_valid())
				player->get_velocity() = current->velocity;

			// set final values.
			if (j == current->lag - 1 && (!last || !last->extrapolated))
			{
				player->set_abs_origin(current->origin);
				player->get_flags() = current->flags;
			}

			// interpolate duck amount.
			if (last)
				player->get_duck_amount() = interpolate(last->duck, current->duck, lerp);

			// perform all possible animations.
			for (auto i = 0; i < resolver_direction_max; i++)
			{
				// bots and teammates only get the networked angle animated.
				if (!full_setup && i != resolver_networked)
					continue;

				if (i == resolver_max_max || i == resolver_min_min || i == resolver_min_extra || i == resolver_max_extra)
					continue;

				// copy back pre-animation data.
				if (j == 0)
				{
					player->get_pose_parameter() = target_record->poses[i];
					*player->get_animation_layers() = target_record->layers;
					player->get_strafing() = target_record->strafing;
					*state = target_record->state[i];
					intermediate.state[i].copy_meta(state);
					state->feet_cycle = target_record->layers[6].cycle;
					state->feet_weight = target_record->layers[6].weight;
					state->strafe_sequence = target_record->layers[7].sequence;
					state->strafe_change_cycle = target_record->layers[7].cycle;
					state->strafe_change_weight = target_record->layers[7].weight;
					state->acceleration_weight = target_record->layers[12].weight;
					player->get_lower_body_yaw_target() = target_record->lower_body_yaw_target;
					player->set_abs_angle(target_record->abs_ang[i]);
				}
				else
				{
					*player->get_animation_layers() = current->custom_layers[i];
					*state = current->state[i];
					intermediate.state[i].copy_meta(state);
					player->get_pose_parameter() = current->poses[i];
					player->get_lower_body_yaw_target() = lower_body_yaw[i];
					player->set_abs_angle(current->abs_ang[i]);
				}

				// resolve intermediate animations.
				if (!is_fake)
				{
					player->get_eye_angles().y = current->get_resolver_angle(static_cast<resolver_direction>(i));

					if (i == resolver_center)
						player->get_anim_state()->foot_yaw = player->get_eye_angles().y;
				}

				// already shot?
				if (current->shot && TIME_TO_TICKS(wpn->get_last_shot_time()) <= TIME_TO_TICKS(intermediate.sim_time))
					player->get_eye_angles() = current->eye_ang;

				// set final values.
				if (j == current->lag - 1)
				{
					player->get_eye_angles() = current->eye_ang;

					if (!last || !last->extrapolated)
						player->get_strafing() = current->strafing;
				}

				// perform animation.
				intermediate.update_animations((last && last->extrapolated) ? &cmd : nullptr,
						current->tickbase_drift && !current->extrapolated && j == 0);

				// set lby.
				if (i == resolver_networked)
					player->get_lower_body_yaw_target() = current->lower_body_yaw_target;

				// force strafe direction to opposite of current move yaw.
				if (i != resolver_networked && last && (!player->get_strafing() ||
						(fabsf(last->layers[7].weight - current->layers[7].weight) < .1f && current->layers[7].weight > 0.f && current->layers[7].weight < 1.f)))
					player->get_pose_parameter()[cs_player_t::strafe_yaw] = std::remainderf(state->move_yaw + .5f * yaw_bounds, yaw_bounds) / yaw_bounds + .5f;

				// store current pose parameters.
				current->abs_ang[i] = { 0.f, player->get_anim_state()->foot_yaw, 0.f };
				current->poses[i] = player->get_pose_parameter();
				current->state[i] = *player->get_anim_state();
				current->custom_layers[i] = *player->get_animation_layers();
				lower_body_yaw[i] = player->get_lower_body_yaw_target();
			}
		}

		// store final predicted velocity.
		current->final_velocity = player->get_velocity();

		// restore stuff.
		player->get_pose_parameter() = backup_poses;
		*player->get_animation_layers() = backup_layers;
		*state = backup_state;
		player->set_abs_origin(backup_abs_origin);
		player->set_abs_angle(backup_abs_angle);
		player->get_lower_body_yaw_target() = backup_lby;
		player->get_flags() = backup_flags;
		player->get_strafing() = backup_strafing;
		player->get_ground_entity() = backup_ground_entity;
		player->get_velocity() = backup_velocity;
		player->get_velocity_modifier() = backup_velocity_modifier;
		player->get_eye_angles() = backup_eye_angles;
		game->globals->curtime = backup_curtime;
		game->globals->frametime = backup_frametime;

		// set roll resolver directions.
		current->abs_ang[resolver_max_max] = current->abs_ang[resolver_max];
		current->abs_ang[resolver_max_extra] = current->abs_ang[resolver_max];
		current->abs_ang[resolver_min_min] = current->abs_ang[resolver_min];
		current->abs_ang[resolver_min_extra] = current->abs_ang[resolver_min];
		current->poses[resolver_max_max] = current->poses[resolver_max];
		current->poses[resolver_max_extra] = current->poses[resolver_max];
		current->poses[resolver_min_min] = current->poses[resolver_min];
		current->poses[resolver_min_extra] = current->poses[resolver_min];
		current->state[resolver_max_max] = current->state[resolver_max];
		current->state[resolver_max_extra] = current->state[resolver_max];
		current->state[resolver_min_min] = current->state[resolver_min];
		current->state[resolver_min_extra] = current->state[resolver_min];
		current->custom_layers[resolver_max_max] = current->custom_layers[resolver_max];
		current->custom_layers[resolver_max_extra] = current->custom_layers[resolver_max];
		current->custom_layers[resolver_min_min] = current->custom_layers[resolver_min];
		current->custom_layers[resolver_min_extra] = current->custom_layers[resolver_min];
	}

	void i_player_list::update_resolver_state(lag_record* const current, lag_record* const last)
	{
		if (!cfg.rage.enable.get() || current->player->is_fake_player() || !current->player->is_enemy()
			|| !game->local_player || !game->local_player->is_alive())
		{
			current->res_state = resolver_default;
			current->res_direction = resolver_networked;
			return;
		}

		const auto player = current->player;
		auto& entry = GET_PLAYER_ENTRY(player);
		auto& resolver = entry.resolver;

		// on shot?
		if (current->shot)
		{
			auto target = resolver.states[resolver.right][resolver_default].direction;

			switch (target)
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

			if (!resolver.states[resolver.right][resolver_shot].eliminated_positions.test(target))
				resolver.states[resolver.right][resolver_shot].direction = target;

			current->res_state = resolver_shot;
			current->res_right = resolver.right;
			current->res_direction = resolver.get_resolver_state(*current).direction;
			return;
		}

		// start pitch switch timer on pitch switch.
		const auto pitch = current->eye_ang.x - floorf(current->eye_ang.x / 360.f + .5f) * 360.f;
		if (fabsf(pitch) < 50.f)
		{
			if (resolver.pitch_timer <= 0.f)
				resolver.pitch_timer = game->globals->curtime;
		}
		else
			resolver.pitch_timer = 0.f;

		// determine rotation momentum and antiaim characteristics.
		const auto delta = std::remainderf(current->eye_ang.y - last->eye_ang.y, yaw_bounds);
		resolver.continuous_momentum += delta;
		resolver.steadiness += fabsf(delta) > flip_margin ? .1f : -.1f;
		resolver.steadiness = std::clamp(resolver.steadiness, 0.f, 1.f);

		// force safe points until pitch is steady.
		const auto force_safepoint = resolver.pitch_timer > 0.f && resolver.pitch_timer >= game->globals->curtime - 1.f;
		if (force_safepoint && !cfg.rage.hack.secure_point->test(cfg_t::secure_point_disabled))
			current->force_safepoint = true;

		// we don't have any clue where in the cycle he shot, just ignore them.
		if (!current->shot && last->shot)
		{
			current->res_state = resolver_shot;
			current->res_right = resolver.right;
			current->res_direction = resolver.get_resolver_state(*current).direction;
			current->valid = false;
			return;
		}

		// should we switch to or from the secondary mode?
		if (resolver.current_state == resolver_flip && resolver.continuous_momentum < -flip_margin)
		{
			resolver.current_state = resolver_default;
			resolver.continuous_momentum = 0.f;
		}
		else if (resolver.current_state == resolver_default && resolver.continuous_momentum > flip_margin)
		{
			resolver.current_state = resolver_flip;
			resolver.continuous_momentum = 0.f;
		}
		// account for flip from -180 to 180 deg.
		else if (fabsf(delta) >= 179.f)
		{
			resolver.current_state = resolver.current_state == resolver_default ? resolver_flip : resolver_default;
			resolver.continuous_momentum = 0.f;
		}

		// write resolver data to record.
		current->res_state = resolver.current_state;
		current->res_right = resolver.right;

		auto& resolver_state = resolver.get_resolver_state(*current);
		current->res_direction = resolver_state.direction;
		
		if (!current->valid || !last->valid || current->lag == 1 || force_safepoint)
			return;

		angle move, last_move;
		vector_angles(current->velocity, move);
		
		// perform layer 6 matching.
		if (current->is_moving() && last->is_moving() // both moving.
			&& current->layers[6].weight >= last->layers[6].weight && current->layers[6].playback_rate >= last->layers[6].playback_rate // accelerating.
			&& current->flags & cs_player_t::flags::on_ground && last->flags & cs_player_t::flags::on_ground // is player grounded?
			&& !resolver.stop_detection) // default resolve mode.
		{
			auto best_match = std::make_pair(resolver_networked, FLT_MAX);

			for (auto i = 0; i < resolver_direction_max; i++)
			{
				if (i != resolver_max && i != resolver_min)
					continue;

				if (current->layers[6].sequence != current->custom_layers[i][6].sequence)
					continue;
				
				const auto delta_weight = fabsf(current->custom_layers[i][6].weight - current->layers[6].weight);
				const auto delta_cycle = fabsf(current->custom_layers[i][6].cycle - current->layers[6].cycle);
				const auto delta_rate = fabsf(current->custom_layers[i][6].playback_rate - current->layers[6].playback_rate);
				const auto delta_total = delta_weight + delta_cycle + delta_rate;
				
				if (delta_total < best_match.second)
					best_match = { static_cast<resolver_direction>(i), delta_total };
				
				if (delta_weight < .000001 || delta_cycle < .000001f || delta_rate < .000001f)
					best_match = { static_cast<resolver_direction>(i), 0.f };
			}
			
			// update resolver.
			if (best_match.second < FLT_MAX)
			{
				resolver.states[false][resolver_default].set_direction(best_match.first);
				resolver.states[false][resolver_flip].set_direction(best_match.first);
				resolver.states[true][resolver_default].set_direction(best_match.first);
				resolver.states[true][resolver_flip].set_direction(best_match.first);

				current->res_direction = resolver_state.direction;
				current->resolved = resolver.detected_layer = true;
			}
		}

		// update matrices on all records.
		// find all records and update matrices.
		for (auto& other : entry.records)
		{
			other.res_direction = resolver.get_resolver_state(other).direction;
			if (other.has_matrix)
				memcpy(other.mat, other.res_mat[other.res_direction], sizeof(other.mat));
		}
	}

	void i_player_list::build_activity_modifiers(std::vector<uint16_t>& modifiers, bool uh) const
	{
		static const auto moving = XOR_STR_STORE("moving");
		static const auto crouch = XOR_STR_STORE("crouch");
		static const auto underhand = XOR_STR_STORE("underhand");

		modifiers.clear();

		const auto add_modifier = [&modifiers](const char* name)
		{
			char lookup[32];
			strcpy_s(lookup, sizeof(lookup), name);
			const auto sym = game->modifier_table->find_or_insert(lookup);

			if (sym != 0xFFFF)
				modifiers.push_back(sym);
		};

		const auto player = reinterpret_cast<cs_player_t* const>(game->client_entity_list->get_client_entity(game->engine_client->get_local_player()));
		if (!player || !player->is_alive())
			return;

		const auto state = player->get_anim_state();
		add_modifier(state->get_active_weapon_prefix());

		if (state->running_speed > .25f)
		{
			XOR_STR_STACK(m, moving);
			add_modifier(m);
		}

		if (state->duck_amount > .55f)
		{
			XOR_STR_STACK(c, crouch);
			add_modifier(c);
		}

		if (uh)
		{
			XOR_STR_STACK(u, underhand);
			add_modifier(u);
		}
	}

	i_player_list::player_entry::player_entry(cs_player_t* const player)
	{
		if (player == nullptr)
			INVALID_ARGUMENT("Tried to construct entry from null player.");

		this->player = player;
		spawntime = player->get_spawn_time();
		handle = player->get_handle();

		visuals.previous_health = std::clamp(player->get_health(), 0, 100) / 100.f;
		visuals.previous_armor = std::clamp(player->get_armor(), 0, 100) / 100.f;
		visuals.previous_ammo = 1.f;

		const auto wpn = reinterpret_cast<weapon_t*>(game->client_entity_list->get_client_entity_from_handle(player->get_active_weapon()));
		if (wpn && wpn->get_clip1() > 0)
		{
			const auto wpn_info = wpn->get_cs_weapon_data();
			if (wpn_info && wpn_info->maxclip1 > 0)
				visuals.previous_ammo = std::clamp(wpn->get_clip1(), 0, wpn_info->maxclip1) / static_cast<float>(wpn_info->maxclip1);
		}

		for (auto i = 0; i < 2; i++)
			for (auto j = 0; j< resolver_state_max; j++)
			{
				resolver.states[i][j].eliminated_positions.set(resolver_max_max);
				resolver.states[i][j].eliminated_positions.set(resolver_max_extra);
				resolver.states[i][j].eliminated_positions.set(resolver_min_min);
				resolver.states[i][j].eliminated_positions.set(resolver_min_extra);
			}
	}

	lag_record* i_player_list::player_entry::get_record(record_filter filter)
	{
		if (records.empty())
			return nullptr;

		if (filter == record_filter::oldest)
		{
			for (auto it = records.rbegin(); it != records.rend(); it = next(it))
			{
				if (!it->is_valid())
					continue;
				
				if (it->is_breaking_lagcomp() && it->should_delay_shot() && it->can_delay_shot())
					continue;
				
				if (!it->can_delay_shot() && it->is_breaking_lagcomp())
					continue;
				
				if (!it->shot)
					return &*it;
			}
			
			return nullptr;
		}

		auto best_match = std::make_pair<lag_record*, float>(nullptr, 12.5f);
		lag_record *first_valid = nullptr;
		for (auto it = records.begin(); it != records.end(); it = next(it))
		{
			if (!it->is_valid())
				continue;
			
			if (it->is_breaking_lagcomp() && it->should_delay_shot() && it->can_delay_shot())
				break;

			if (!it->can_delay_shot() && it->is_breaking_lagcomp())
				break;

			if (filter == record_filter::shot && it->shot)
				return &*it;

			if (filter == record_filter::resolved && it->resolved)
				return &*it;

			if (it->shot)
				continue;

			if (filter == record_filter::latest)
				return &*it;

			if (filter == record_filter::uncrouched && fabsf(it->duck) < .1f)
				return &*it;

			if (filter == record_filter::angle_diff)
			{
				if (!first_valid)
					first_valid = &*it;
				else
				{
					const auto diff = std::remainderf(it->eye_ang.y, first_valid->eye_ang.y);
					if (diff > best_match.second)
						best_match = std::make_pair(&*it, diff);
				}
			}
		}

		return best_match.first;
	}

	void i_player_list::player_entry::get_interpolation_records(lag_record** first, lag_record** second)
	{
		if (game->local_player)
		{
			// bring the tickbase in sync!
			game->local_player->get_tick_base()++;

			for (auto it = records.rbegin(); it != records.rend(); it = next(it))
			{
				if (it->has_visual_matrix && it->is_valid(true))
				{
					*first = &*it;
					game->local_player->get_tick_base()--;
					return;
				}
				else
					*second = &*it;
			}

			// move it back!
			game->local_player->get_tick_base()--;
		}

		*first = *second = nullptr;
	}

	bool i_player_list::player_entry::is_visually_fakeducking() const
	{
		if (records.size() < 2)
			return false;

		const auto& first = records[0];
		const auto& second = records[1];

		return (first.lag >= 10 || second.lag >= 10) && fabsf(first.duck - second.duck) < .25f && first.duck > 0.f && second.duck < .85f;
	}

	bool i_player_list::player_entry::is_charged() const
	{
		return !records.empty() && records.front().delta_time > TICKS_TO_TIME(7) && records.front().lag < 7;
	}

	bool i_player_list::player_entry::is_exploiting() const
	{
		return !records.empty() && records.front().sim_time < compensation_time;
	}

	bool i_player_list::player_entry::is_peeking() const
	{
		if (records.empty())
			return false;

		const auto first_velocity = records.front().velocity.length();

		if (first_velocity >= 50.f)
			return true;

		if (records.size() < 2)
			return false;

		return fabsf(first_velocity - records[1].velocity.length()) >= 10.f;
	}

	lag_record::lag_record(cs_player_t* const player)
	{
		this->player = player;
		index = player->index();
		dormant = player->is_dormant();
		origin = player->get_origin();
		abs_origin = player->get_abs_origin();
		obb_mins = player->get_collideable()->obb_mins();
		obb_maxs = player->get_collideable()->obb_maxs();
		layers = *player->get_animation_layers();
		has_state = player->get_anim_state();
		sim_time = player->get_simulation_time();
		recv_time = game->globals->curtime;
		delta_time = TICKS_TO_TIME(game->client_state->last_server_tick) - sim_time;
		server_tick = game->client_state->last_server_tick;
		strafing = player->get_strafing();
		duck = player->get_duck_amount();
		lower_body_yaw_target = player->get_lower_body_yaw_target();
		eye_ang = player->get_eye_angles();
		flags = player->get_flags();
		eflags = player->get_eflags();
		effects = player->get_effects();
		lag = TIME_TO_TICKS(player->get_simulation_time() - player->get_old_simulation_time());
		velocity_modifier = player->get_velocity_modifier();

		// initially populate resolver data.
		for (auto i = 0; i < resolver_direction_max; i++)
		{
			poses[i] = player->get_pose_parameter();
			abs_ang[i] = player->get_abs_angle();

			if (has_state)
				state[i] = *player->get_anim_state();
		}

		velocity = (player->get_origin() - *player->get_old_origin()) * (1.f / TICKS_TO_TIME(lag));
		if (flags & cs_player_t::on_ground)
			velocity.z = 0.f;

		if (!velocity.is_valid())
			velocity = { 0.f, 0.f, 0.f };
	}

	void lag_record::determine_simulation_ticks(lag_record* const last)
	{
		auto& player_entry = GET_PLAYER_ENTRY(player);
		
		if (!valid || !last)
			return;
		
		const auto& start = last->layers[11];
		const auto& end = layers[11];
		auto cycle = start.cycle;
		auto playback_rate = start.playback_rate;
		auto reached = false;

		for (auto i = 1; i <= TIME_TO_TICKS(1.f); i++)
		{
			const auto cycle_delta = playback_rate * TICKS_TO_TIME(1);
			cycle += cycle_delta;

			if (cycle >= 1.f)
				playback_rate = end.playback_rate;

			cycle -= (float) (int) cycle;

			if (cycle < 0.f)
				cycle += 1.f;

			if (cycle > 1.f)
				cycle -= 1.f;

			if (fabsf(cycle - end.cycle) <= .5f * cycle_delta)
			{
				reached = true;

				if ((TIME_TO_TICKS(sim_time) < TIME_TO_TICKS(last->sim_time)
					|| (player_entry.addt && TIME_TO_TICKS(sim_time) == TIME_TO_TICKS(last->sim_time))) && !last->dormant)
				{
					lag = i + 1;
					tickbase_drift = true;
				}
				else
					lag = i;
			}
		}

		if (!reached)
		{
			lag = 1;
			tickbase_drift = true;
		}
		
		if (!player_entry.addt && TIME_TO_TICKS(sim_time) == TIME_TO_TICKS(last->sim_time))
		{
			if (player_entry.ground_accel_linear_frac_last_time_stamp == game->client_state->command_ack
				&& fabsf(player_entry.ground_accel_linear_frac_last_time - sim_time - TICKS_TO_TIME(lag)) < 1.f)
				sim_time = player_entry.ground_accel_linear_frac_last_time;
			else
				sim_time += TICKS_TO_TIME(lag);
		}

		velocity = (player->get_origin() - *player->get_old_origin()) * (1.f / TICKS_TO_TIME(lag));
		if (flags & cs_player_t::on_ground || last->flags & cs_player_t::on_ground)
			velocity.z = 0.f;

		if (!velocity.is_valid())
			velocity = { 0.f, 0.f, 0.f };
	}

	void lag_record::build_matrix(const resolver_direction direction)
	{
		const auto hdr = player->get_studio_hdr();
		const auto layers = player->get_animation_layers();

		if (!hdr || !layers || layers->size() < 13)
			return;
		
		// keep track of old values.
		game->mdl_cache->begin_lock();
		const auto backup_effects = player->get_effects();
		const auto backup_flags = player->get_lod_data().flags;
		const auto backup_ik = player->get_ik();
		const auto backup_layer = layers->at(12).weight;
		const auto backup_eye = *player->eye_angles();
		const auto backup_vo = player->get_view_offset();
		
		// stop interpolation, snapshots and other shit.
		player->get_eflags() |= efl_setting_up_bones;
		player->get_effects() |= cs_player_t::ef_nointerp;
		player->get_lod_data().flags = lod_empty;
		player->get_snapshot_entire_body().player = player->get_snapshot_upper_body().player = nullptr;
		if (this != &local_fake_record)
			layers->at(12).weight = 0.f;
		
		// setup temporary storage for inverse kinematics.
		ik_context ik;
		uint8_t computed[0x100]{};
		auto angle = abs_ang[direction];
		auto& out = res_mat[direction];
		ik.init(hdr, &angle, &origin, sim_time, game->globals->framecount, XOR_32(bone_used_by_hitbox | bone_used_by_attachment));

		// perform bone blending.
		alignas(16) vec3 pos[max_bones] = { };
		alignas(16) quaternion_aligned q[max_bones] = { };
		player->get_ik() = &ik;
		player->standard_blending_rules(hdr, pos, q, sim_time, XOR_32(bone_used_by_anything));
		ik.update_targets(pos, q, out, computed);
		ik.solve_dependencies(pos, q, out, computed);

		// build chain.
		int32_t chain[max_bones] = {};
		const auto chain_length = hdr->numbones();
		for (auto i = 0; i < chain_length; i++)
			chain[chain_length - i - 1] = i;

		// build transformations.
		const auto rotation = angle_matrix(angle, origin);
		for (auto j = chain_length - 1; j >= 0; j--)
		{
			const auto i = chain[j];
			const auto parent = hdr->bone_parent.count() > i ? &hdr->bone_parent[i] : nullptr;

			if (!parent)
				continue;

			const auto qua = quaternion_matrix(q[i], pos[i]);

			if (*parent == -1)
				out[i] = concat_transforms(rotation, qua);
			else
				out[i] = concat_transforms(out[*parent], qua);
		}

		// post transformation setup.
		if (!player->get_predictable())
		{
			const auto eye_watcher = player->get_var_mapping().find(FNV1A("C_CSPlayer::m_iv_angEyeAngles"));
			if (eye_watcher)
				eye_watcher->reset_to_last_networked();

			const auto vo_watcher = player->get_var_mapping().find(FNV1A("C_BasePlayer::m_iv_vecViewOffset"));
			if (vo_watcher)
				vo_watcher->reset_to_last_networked();
		}

		if (direction == resolver_max)
		{
			player->eye_angles()->z = get_resolver_roll(resolver_max_max);
			memcpy(res_mat[resolver_max_max], out, sizeof(out));
			player->post_build_transformations(res_mat[resolver_max_max], XOR_32(bone_used_by_anything));
			player->eye_angles()->z = get_resolver_roll(resolver_max_extra);
			memcpy(res_mat[resolver_max_extra], out, sizeof(out));
			player->post_build_transformations(res_mat[resolver_max_extra], XOR_32(bone_used_by_anything));
		}
		else if (direction == resolver_min)
		{
			player->eye_angles()->z = get_resolver_roll(resolver_min_min);
			memcpy(res_mat[resolver_min_min], out, sizeof(out));
			player->post_build_transformations(res_mat[resolver_min_min], XOR_32(bone_used_by_anything));
			player->eye_angles()->z = get_resolver_roll(resolver_min_extra);
			memcpy(res_mat[resolver_min_extra], out, sizeof(out));
			player->post_build_transformations(res_mat[resolver_min_extra], XOR_32(bone_used_by_anything));
		}

		if (direction != resolver_networked)
			player->eye_angles()->z = get_resolver_roll(direction);

		player->post_build_transformations(out, XOR_32(bone_used_by_anything));

		// restore original values.
		player->get_eflags() &= ~efl_setting_up_bones;
		player->get_effects() = backup_effects;
		player->get_lod_data().flags = backup_flags;
		player->get_ik() = backup_ik;
		player->get_snapshot_entire_body().player = player->get_snapshot_upper_body().player = player;
		layers->at(12).weight = backup_layer;
		*player->eye_angles() = backup_eye;
		player->get_view_offset() = backup_vo;
		game->mdl_cache->end_lock();
	}

	void lag_record::perform_matrix_setup(bool extrapolated, bool only_max)
	{
		game->mdl_cache->begin_lock();
		const auto is_local = player->index() == game->engine_client->get_local_player();
		const auto is_fake = player->is_fake_player() || !player->is_enemy() || is_local || !cfg.rage.enable.get();
		if (is_fake)
			res_direction = resolver_networked;

		const auto backup_layers = *player->get_animation_layers();
		const auto backup_anim_state = *player->get_anim_state();
		const auto backup_poses = player->get_pose_parameter();
		const auto backup_abs_origin = player->get_abs_origin();
		const auto backup_abs_angle = player->get_abs_angle();

		for (auto i = 0; i < resolver_direction_max; i++)
		{
			if (is_fake && i != resolver_networked)
				continue;

			if (i == resolver_max_max || i == resolver_min_min || i == resolver_min_extra || i == resolver_max_extra)
				continue;

			if (only_max && i != resolver_max && i != resolver_min)
				continue;

			// restore animation data.
			*player->get_animation_layers() = extrapolated ? custom_layers[i] : layers;
			player->get_pose_parameter() = poses[i];
			player->set_abs_origin(origin);
			player->set_abs_angle(abs_ang[i]);

			// perform bone setup.
			build_matrix(static_cast<resolver_direction>(i));
		}

		// restore everything.
		*player->get_animation_layers() = backup_layers;
		*player->get_anim_state() = backup_anim_state;
		player->get_pose_parameter() = backup_poses;
		player->set_abs_origin(backup_abs_origin);
		player->set_abs_angle(backup_abs_angle);

		// write out final matrix.
		memcpy(mat, res_mat[res_direction], sizeof(mat));

		// cache off data.
		has_matrix = true;
		game->mdl_cache->end_lock();
	}

	bool lag_record::is_breaking_lagcomp() const
	{
		if (!game->cl_predict || !game->cl_lagcompensation)
			return true;
		
		const auto& entry = GET_PLAYER_ENTRY(player);

		if (entry.records.size() < 2 || dormant)
			return false;

		const auto front = entry.records.front();

		if (front.dormant)
			return true;

		auto prev_org = front.origin;
		auto skip_first = true;

		// walk context looking for any invalidating event.
		for (auto& cmp : entry.records)
		{
			if (skip_first)
			{
				skip_first = false;
				continue;
			}

			if (cmp.dormant)
				break;

			auto delta = cmp.origin - prev_org;
			if (delta.length2d_sqr() > teleport_dist)
				return true;

			if (cmp.sim_time <= sim_time)
				break;

			prev_org = cmp.origin;
		}

		return false;
	}

	bool lag_record::can_delay_shot(bool pure) const
	{
		if (!game->cl_predict || !game->cl_lagcompensation || dormant)
			return false;
		
		return lag > TIME_TO_TICKS(calculate_rtt()) || (!pure && !is_valid());
	}

	bool lag_record::should_delay_shot() const
	{
		if (!game->cl_predict || !game->cl_lagcompensation || dormant)
			return false;
		
		const auto behind = game->client_state->last_server_tick - server_tick;
		return TIME_TO_TICKS(calculate_rtt()) + behind > lag;
	}

	void lag_record::update_animations(user_cmd* const cmd, bool tickbase_drift)
	{
		game->mdl_cache->begin_lock();
		const auto local = player->index() == game->engine_client->get_local_player();
		
		// make a backup of globals.
		const auto backup_frametime = game->globals->frametime;
		const auto backup_curtime = game->globals->curtime;
		
		// fix animating twice per frame.
		if (player->get_anim_state()->last_framecount >= game->globals->framecount)
			player->get_anim_state()->last_framecount = game->globals->framecount - 1;

		// fixes for networked players.
		if (!local)
		{
			game->globals->frametime = game->globals->interval_per_tick;
			game->globals->curtime = sim_time;
			
			if (tickbase_drift)
				player->get_anim_state()->last_update = game->globals->curtime + game->globals->interval_per_tick;
			else
				player->get_anim_state()->last_update = game->globals->curtime - game->globals->interval_per_tick;
		}
		
		// update animations.
		play_additional_animations(cmd, predict_animation_state());
		const auto angles = *player->eye_angles();
		const auto state = player->get_anim_state();
		state->update(angles.y, angles.x);

		// update lower body yaw target, simtime and prevent adjust from blending too far into the future.
		if (state->speed > .1f || state->speed_z > 100.f)
		{
			player->get_lower_body_yaw_target() = state->eye_yaw;
			addon.next_lby_update = game->globals->curtime + .22f;
		}
		else if (game->globals->curtime > addon.next_lby_update && fabsf(angle_diff(state->foot_yaw, state->eye_yaw)) > 35.f)
		{
			player->get_lower_body_yaw_target() = state->eye_yaw;
			addon.next_lby_update = game->globals->curtime + 1.1f;
		}
		
		player->get_animation_layers()->at(3).weight = addon.adjust_weight;
		
		if (local)
			sim_time = game->globals->curtime;

		// restore globals.
		game->globals->frametime = backup_frametime;
		game->globals->curtime = backup_curtime;
		game->mdl_cache->end_lock();
	}

	anim_state lag_record::predict_animation_state(user_cmd* cmd, animation_layers* out)
	{
		game->mdl_cache->begin_lock();
		const auto backup_state = *player->get_anim_state();
		const auto backup_layers = *player->get_animation_layers();
		const auto backup_poses = player->get_pose_parameter();
		
		// fix animating twice per frame.
		if (player->get_anim_state()->last_framecount >= game->globals->framecount)
			player->get_anim_state()->last_framecount = game->globals->framecount - 1;

		// update animations and write out final state.
		const auto angles = *player->eye_angles();
		player->get_anim_state()->update(angles.y, angles.x);
		auto pred = *player->get_anim_state();
		if (cmd) play_additional_animations(cmd, predict_animation_state());
		if (out) *out = *player->get_animation_layers();

		*player->get_anim_state() = backup_state;
		*player->get_animation_layers() = backup_layers;
		player->get_pose_parameter() = backup_poses;
		game->mdl_cache->end_lock();
		return pred;
	}

	// note to myself: there are still a few minor bugs in this implementation, namely the exact tick
	// you start jumping / landing, some situations where you move very slowly and ducking.
	// none of these situations have any influence on the weapon_t shoot position, so we are going to clean up later (TM).
	void lag_record::play_additional_animations(sdk::user_cmd* const cmd, const sdk::anim_state& pred_state)
	{
		if (!player->get_anim_state() || !player->get_anim_state()->player || !player->get_studio_hdr() || !cmd)
			return;
		
		static const auto recrouch_generic = XOR_STR_STORE("recrouch_generic");
		static const auto crouch = XOR_STR_STORE("crouch");
		
		const auto s = player->get_anim_state();
		const auto p = &pred_state;
		const auto update_time = fmaxf(0.f, p->last_update - s->last_update);
		const auto wpn = reinterpret_cast<weapon_t * const>(
			game->client_entity_list->get_client_entity_from_handle(player->get_active_weapon()));
		const auto world_model = wpn ? reinterpret_cast<entity_t* const>(
			game->client_entity_list->get_client_entity_from_handle(wpn->get_weapon_world_model())) : nullptr;
		const auto hdr = player->get_studio_hdr();
		const auto seed = cmd->command_number;
		
		player_list.build_activity_modifiers(addon.activity_modifiers);
		
		if (wpn && wpn->get_weapon_type() == weapontype_knife && wpn->get_next_primary_attack() + .4f < game->globals->curtime)
			addon.swing_left = true;

		// CCSGOPlayerAnimState::DoAnimationEvent
		if ((cmd->buttons & user_cmd::jump) &&
			!(player->get_flags() & cs_player_t::on_ground) &&
			s->on_ground)
		{
			player->try_initiate_animation(4, XOR_32(985), addon.activity_modifiers, seed);
			addon.in_jump = true;
		}
		
		const auto modifiers = addon.activity_modifiers;
		auto& action = player->get_animation_layers()->at(1);
		switch (addon.vm)
		{
		// ACT_VM_RELEASE, ACT_VM_THROW, ACT_VM_PRIMARYATTACK, ACT_VM_PRIMARYATTACK_SILENCED
		case 219:
			player_list.build_activity_modifiers(addon.activity_modifiers, true);
		case 190:
		case 192:
		case 477:
			if (wpn->get_item_definition_index() != item_definition_index::weapon_c4)
				player->try_initiate_animation(1, XOR_32(961), addon.activity_modifiers, seed);
			if (addon.vm == 219)
				addon.activity_modifiers = modifiers;
			break;
		// ACT_VM_HITCENTER, ACT_VM_MISSCENTER
		case 200:
		case 206:
			player->try_initiate_animation(1, !addon.swing_left ? XOR_32(961) : XOR_32(962), addon.activity_modifiers, seed);
			addon.swing_left = !addon.swing_left;
			break;
		// ACT_VM_SWINGHIT
		case 211:
			player->try_initiate_animation(1, XOR_32(963), addon.activity_modifiers, seed);
			addon.swing_left = !addon.swing_left;
			break;
		// ACT_VM_SECONDARYATTACK
		case 193:
			player->try_initiate_animation(1, XOR_32(964), addon.activity_modifiers, seed);
			break;
		// ACT_VM_HITCENTER2, ACT_VM_MISSCENTER2
		case 201:
		case 207:
			player->try_initiate_animation(1, XOR_32(964), addon.activity_modifiers, seed);
			addon.swing_left = true;
			break;
		// ACT_VM_SWINGHARD
		case 209:
			player->try_initiate_animation(1, XOR_32(965), addon.activity_modifiers, seed);
			addon.swing_left = true;
			break;
		// ACT_VM_RELOAD, ACT_SECONDARY_VM_RELOAD
		case 194:
		case 831:
			if (wpn && wpn->get_weapon_type() == weapontype_shotgun && wpn->get_item_definition_index() != item_definition_index::weapon_mag7)
				player->try_initiate_animation(1, XOR_32(969), addon.activity_modifiers, seed);
			else
				player->try_initiate_animation(1, XOR_32(967), addon.activity_modifiers, seed);
			break;
		// ACT_SHOTGUN_RELOAD_START
		case 264:
			player->try_initiate_animation(1, XOR_32(968), addon.activity_modifiers, seed);
			break;
		// ACT_SHOTGUN_RELOAD_FINISH
		case 265:
			player->try_initiate_animation(1, XOR_32(970), addon.activity_modifiers, seed);
			break;
		// ACT_VM_PULLPIN
		case 191:
			player->try_initiate_animation(1, XOR_32(971), addon.activity_modifiers, seed);
			break;
		// ACT_VM_DRAW, ACT_VM_EMPTY_DRAW, ACT_VM_DRAW_SILENCED
		case 183:
		case 224:
		case 481:
			if (wpn && player->get_sequence_activity(action.sequence) == XOR_32(972)
				&& action.cycle < .15f)
				addon.in_rate_limit = true;
			else
				player->try_initiate_animation(1, XOR_32(972), addon.activity_modifiers, seed);
			break;
		}

		// CCSGOPlayerAnimState::SetupVelocity
		auto& layer3 = player->get_animation_layers()->at(3);

		if (!s->adjust_started && p->speed <= 0.f && s->standing_time <= 0.f && s->on_ground
			&& !s->on_ladder && !s->in_hit_ground && s->stutter_step < 50.f)
		{
			player->try_initiate_animation(3, XOR_32(980), addon.activity_modifiers, seed);
			s->adjust_started = true;
		}

		const auto layer3_act = player->get_sequence_activity(layer3.sequence);
		if (layer3_act == XOR_32(980) || layer3_act == XOR_32(979))
		{
			if (s->adjust_started && p->ducking_speed <= .25f)
			{
				layer3.increment_layer_cycle_weight_rate_generic(update_time, hdr);
				s->adjust_started = !layer3.has_sequence_completed(update_time);
			}
			else
			{
				const auto weight = layer3.weight;
				layer3.weight = approach(0.f, weight, update_time * 5.f);
				layer3.set_layer_weight_rate(update_time, weight);
				s->adjust_started = false;
			}
		}

		if (p->speed <= 1.f && s->on_ground && !s->on_ladder && !s->in_hit_ground
			&& fabsf(angle_diff(s->foot_yaw, p->foot_yaw)) / update_time > 120.f)
		{
			player->try_initiate_animation(3, XOR_32(979), addon.activity_modifiers, seed);
			s->adjust_started = true;
		}
		
		addon.adjust_weight = layer3.weight;
		layer3.weight = 0.f;
		
		// CCSGOPlayerAnimState::SetUpWeaponAction
		auto increment = true;
		if (wpn && addon.in_rate_limit && player->get_sequence_activity(action.sequence) == XOR_32(972))
		{
			if (world_model)
				world_model->get_effects() |= ef_nodraw;
			
			if (action.cycle >= .15f)
			{
				addon.in_rate_limit = false;
				player->try_initiate_animation(1, XOR_32(972), addon.activity_modifiers, seed);
				increment = false;
			}
		}
		
		auto& recrouch = player->get_animation_layers()->at(2);
		auto recrouch_weight = 0.f;
		if (action.weight > 0.f)
		{
			if (recrouch.sequence <= 0)
			{
				XOR_STR_STACK(r, recrouch_generic);
				
				const auto seq = player->lookup_sequence(r);
				recrouch.playback_rate = player->get_layer_sequence_cycle_rate(&recrouch, seq);
				recrouch.sequence = seq;
				recrouch.cycle = recrouch.weight = 0.f;
			}

			auto has_modifier = false;
			XOR_STR_STACK(c, crouch);
			const auto& seqdesc = player->get_studio_hdr()->get_seq_desc(action.sequence);
			for (auto i = 0; i < seqdesc->numactivitymodifiers; i++)
				if (!strcmp(seqdesc->get_activity_modifier(i)->get_name(), c))
				{
					has_modifier = true;
					break;
				}
			
			if (has_modifier)
			{
				if (p->duck_amount < 1.f)
					recrouch_weight = action.weight * (1.f - p->duck_amount);
			}
			else if (p->duck_amount > 0.f)
				recrouch_weight = action.weight * p->duck_amount;
		}
		else if (recrouch.weight > 0.f)
			recrouch_weight = approach(0.f, recrouch.weight, 4.f * update_time);
		
		recrouch.weight = std::clamp(recrouch_weight, 0.f, 1.f);
		
		if (increment)
		{
			action.increment_layer_cycle(update_time, false);
			const auto prev_weight = action.weight;
			action.set_layer_ideal_weight_from_sequence_cycle(hdr);
			action.set_layer_weight_rate(p->last_increment, action.weight);			
			action.cycle = std::clamp(action.cycle - p->last_increment * action.playback_rate, 0.f, 1.f);
			action.weight = std::clamp(action.weight - p->last_increment * action.weight_delta_rate, .0000001f, 1.f);
		}

		// CCSGOPlayerAnimState::SetupMovement
		if (player->get_predictable())
		{
			vec3 forward, right, up;
			angle_vectors(angle(0.f, p->foot_yaw, 0.f), forward, right, up);
			right.normalize();
			const auto to_forward_dot = p->last_accel_speed.dot(forward);
			const auto to_right_dot = p->last_accel_speed.dot(right);
			const auto move_right = (cmd->buttons & user_cmd::move_right) != 0;
			const auto move_left = (cmd->buttons & user_cmd::move_left) != 0;
			const auto move_forward = (cmd->buttons & user_cmd::forward) != 0;
			const auto move_backwards =  (cmd->buttons & user_cmd::back) != 0;
			const auto strafe_forward =	p->running_speed >= .65f && move_forward && !move_backwards && to_forward_dot < -.55f;
			const auto strafe_backwards = p->running_speed >= .65f && move_backwards && !move_forward && to_forward_dot > .55f;
			const auto strafe_right = p->running_speed >= .73f && move_right && !move_left && to_right_dot < -.63f;
			const auto strafe_left = p->running_speed >= .73f && move_left && !move_right && to_right_dot > .63f;
			player->get_strafing() = strafe_forward || strafe_backwards || strafe_right || strafe_left;
		}
		
		const auto swapped_ground = s->on_ground != p->on_ground || s->on_ladder != p->on_ladder;

		if (!s->on_ladder && p->on_ladder)
			player->try_initiate_animation(5, XOR_32(987), addon.activity_modifiers, seed);

		if (p->on_ground)
		{
			auto& layer5 = player->get_animation_layers()->at(5);

			if (!s->in_hit_ground && swapped_ground)
				player->try_initiate_animation(5, s->air_time > 1.f ? XOR_32(989) : XOR_32(988), addon.activity_modifiers, seed);

			if (p->in_hit_ground && player->get_sequence_activity(layer5.sequence) != XOR_32(987))
				addon.in_jump = false;

			if (!p->in_hit_ground && !addon.in_jump && p->ladder_speed <= 0.f)
				layer5.weight = 0.f;
		}
		else if (swapped_ground && !addon.in_jump)
			player->try_initiate_animation(4, XOR_32(986), addon.activity_modifiers, seed);
		
		// CCSGOPlayerAnimState::SetupAliveLoop
		auto& alive = player->get_animation_layers()->at(11);
		if (player->get_sequence_activity(alive.sequence) == XOR_32(981))
		{
			if (p->weapon && p->weapon != p->last_weapon)
			{
				const auto cycle = alive.cycle;
				player->try_initiate_animation(11, XOR_32(981), addon.activity_modifiers, seed);
				alive.cycle = cycle;
			}
			else if (!alive.has_sequence_completed(update_time))
				alive.weight = 1.f - std::clamp((p->walk_speed - .55f) / .35f, 0.f, 1.f);
		}
		
		addon.vm = 0;
	}

	float lag_record::get_resolver_angle(std::optional<resolver_direction> direction_override)
	{
		const auto wpn = reinterpret_cast<weapon_t*>(
			game->client_entity_list->get_client_entity_from_handle(player->get_active_weapon()));

		const auto speed = min(velocity.length2d(), max_cs_player_move_speed);
		const auto max_movement_speed = wpn ? max(wpn->get_max_speed(), .001f) : max_cs_player_move_speed;
		const auto movement_speed = std::clamp(speed / (max_movement_speed * .520f), 0.f, 1.f);
		const auto ducking_speed = std::clamp(speed / (max_movement_speed * .340f), 0.f, 1.f);
		const auto state = player->get_anim_state();
		
		yaw_modifier = (((state->ground_fraction * -.3f) - .2f) * movement_speed) + 1.f;

		if (duck > 0.f)
		{
			const auto ducking_modifier = std::clamp(ducking_speed, 0.f, 1.f);
			yaw_modifier += (duck * ducking_modifier) * (.5f - yaw_modifier);
		}

		switch (direction_override.value_or(res_direction))
		{
		case resolver_max:
		case resolver_max_max:
		case resolver_max_extra:
			return std::remainderf(eye_ang.y + 2.f * state->aim_yaw_max * yaw_modifier, yaw_bounds);
		case resolver_min:
		case resolver_min_min:
		case resolver_min_extra:
			return std::remainderf(eye_ang.y + 2.f * state->aim_yaw_min * yaw_modifier, yaw_bounds);
		default:
			break;
		}

		return std::remainderf(eye_ang.y, yaw_bounds);
	}

	float lag_record::get_resolver_roll(std::optional<resolver_direction> direction_override)
	{
		const auto state = player->get_anim_state();

		switch (direction_override.value_or(res_direction))
		{
		case resolver_max_max:
		case resolver_min_extra:
			return -signum(state->aim_yaw_max) * roll_bounds;
		case resolver_min_min:
		case resolver_max_extra:
			return -signum(state->aim_yaw_min) * roll_bounds;
		default:
			break;
		}

		return 0.f;
	}
}
