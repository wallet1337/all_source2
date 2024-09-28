
#include <base/hook_manager.h>
#include <detail/custom_prediction.h>
#include <hacks/antiaim.h>

using namespace sdk;
using namespace detail;

namespace hooks::cs_player
{
	void __fastcall physics_simulate(cs_player_t* player, uint32_t edx)
	{
		auto& context = player->get_command_context();
		const auto needs_processing = context.needs_processing
			&& player->get_simulation_tick() != game->globals->tickcount
			&& player->is_alive();

		if (context.cmd.tick_count == INT_MAX)
		{
			auto& current = pred.info[context.cmd_number % input_max].animation;
			auto& before = pred.info[(context.cmd_number - 1) % input_max].animation;
			before.restore(player);
			local_record.addon.vm = 0;
			current = animation_copy(context.cmd_number, player);
			return;
		}

		const auto viewmodel = reinterpret_cast<entity_t*>(game->client_entity_list->get_client_entity_from_handle(player->get_view_model_0()));
		const auto view_time = viewmodel ? viewmodel->get_anim_time() : 0.f;
		const auto fov_time = player->get_fovtime();

		hook_manager.physics_simulate->call(player, edx);

		if (viewmodel && viewmodel->get_anim_time() != view_time)
			viewmodel->get_anim_time() = TICKS_TO_TIME(context.cmd_number);

		if (fov_time != player->get_fovtime())
			player->get_fovtime() = TICKS_TO_TIME(context.cmd_number);

		if (needs_processing)
		{
			player->get_thirdperson_recoil() = player->get_punch_angle_scaled().x;

			if (game->rules->data)
			{
				if (!(player->get_flags() & cs_player_t::ducking) && !player->get_ducking() && !player->get_ducked())
					player->get_view_offset() = game->rules->data->get_view_vectors()->view;
				else if (player->get_ducked() && !player->get_ducking())
					player->get_view_offset() = game->rules->data->get_view_vectors()->duck_view;
			}

			const auto fresh = game->input->verified_commands[context.cmd_number % input_max].crc ==
				*reinterpret_cast<int32_t*>(&game->globals->interval_per_tick);
			if (fresh && context.cmd.buttons & (user_cmd::attack | user_cmd::attack2 | user_cmd::reload))
				player->get_is_looking_at_weapon() = player->get_is_holding_look_at_weapon() = false;

			const auto sequence = player->get_sequence();
			const auto cycle = player->get_cycle();
			const auto playback_rate = player->get_playback_rate();
			player_list.merge_local_animation(player, &context.cmd);

			player->get_sequence() = sequence;
			player->get_cycle() = cycle;
			player->get_playback_rate() = playback_rate;

			if (fresh)
				hook_manager.do_animation_events_overlay->call(player, edx, player->get_studio_hdr());

			const auto wpn = reinterpret_cast<weapon_t*>(
				game->client_entity_list->get_client_entity_from_handle(player->get_active_weapon()));
			const auto world_model = wpn ? reinterpret_cast<entity_t*>(
					game->client_entity_list->get_client_entity_from_handle(wpn->get_weapon_world_model())) : nullptr;

			auto& info = pred.info[(context.cmd.command_number - 1) % input_max];
			if (info.sequence == context.cmd.command_number - 1 && info.ev == 52 && world_model)
				world_model->get_effects() &= ~ef_nodraw;

			// failsafe to show active world model if it fails to unhide by the time deploy is complete
			if (world_model)
			{
				const auto act = wpn->get_sequence_activity(wpn->get_sequence());
				if (act != XOR_32(183) && act != XOR_32(224))
					world_model->get_effects() &= ~ef_nodraw;
			}

			if (fresh)
				player_list.update_addon_bits(player);
		}
	}

	void __fastcall post_data_update(cs_player_t* player, uint32_t edx, uint32_t type)
	{
		const auto p = reinterpret_cast<cs_player_t*>(uintptr_t(player) - 8);
		player_list.on_update_end(p);

		if (!cfg.rage.enable.get() && p->index() != game->engine_client->get_local_player())
			return hook_manager.post_data_update->call(player, edx, type);

		*p->get_old_origin() = p->get_origin_pred();
		*reinterpret_cast<angle*>(uintptr_t(p->get_old_origin()) + sizeof(vec3)) = p->get_rotation_pred();

		hook_manager.post_data_update->call(player, edx, type);
	}

	void __fastcall do_animation_events(cs_player_t* player, uint32_t edx, cstudiohdr* hdr)
	{
		if (!player->get_predictable())
			hook_manager.do_animation_events_overlay->call(player, edx, hdr);
	}

	void __fastcall calc_view(cs_player_t* player, uint32_t edx, vec3* eye, angle* ang, float* znear, float* zfar, float* fov)
	{
		const auto wpn = reinterpret_cast<weapon_t* const>(game->client_entity_list->get_client_entity_from_handle(player->get_active_weapon()));
		const auto iron_sight = wpn ? &wpn->get_iron_sight_controller() : nullptr;
		const auto bak_iron_sight = iron_sight ? *iron_sight : 0;

		if (cfg.local_visuals.fov_changer.get() && iron_sight)
			*iron_sight = 0;

		auto& state = player->get_anim_state();
		const auto bak_state = state;
		const auto bak_view_z = player->get_view_offset().z;
		state = nullptr;
		if ((cfg.rage.enable.get() || cfg.antiaim.enable.get()) && player == game->local_player)
			player->get_view_offset().z = hacks::aa.get_visual_eye_position().z - player->get_abs_origin().z;
		hook_manager.calc_view->call(player, edx, eye, ang, znear, zfar, fov);
		state = bak_state;
		player->get_view_offset().z = bak_view_z;

		if (cfg.removals.no_visual_recoil.get())
		{
			const auto org = player->eye_angles();
			ang->x = org->x;
			ang->y = org->y;
		}

		if (cfg.local_visuals.fov_changer.get() && iron_sight)
			*iron_sight = bak_iron_sight;
	}

	void __fastcall fire_event(cs_player_t* player, uint32_t edx, vec3* origin, angle* angles, int event, const char* options)
	{
		hook_manager.fire_event->call(player, edx, origin, angles, event, options);

		if (player->index() == game->engine_client->get_local_player())
		{
			auto& context = player->get_command_context();
			auto& info = pred.info[(context.cmd.command_number - 1) % input_max];
			if (info.sequence == context.cmd.command_number - 1)
				info.ev = event;
		}
	}

	void __fastcall obb_change_callback(cs_player_t* player, uint32_t edx, vec3* old_mins, vec3* mins, vec3* old_maxs, vec3* maxs)
	{
		if (game->cs_game_movement->is_processing)
			return;

		if (player->get_predictable() && game->prediction->is_active())
		{
			GET_PLAYER_ENTRY(player).view_delta = old_maxs->z;
			player->get_view_height() = player->get_origin_pred().z + old_maxs->z;
			player->get_bounds_change_time() = game->globals->curtime;
		}
		else if (!player->get_predictable())
		{
			if (!cfg.rage.enable.get())
				return hook_manager.obb_change_callback->call(player, edx, old_mins, mins, old_maxs, maxs);
			GET_PLAYER_ENTRY(player).old_maxs_z = old_maxs->z;
		}
	}

	void __fastcall reevauluate_anim_lod(cs_player_t* player, uint32_t edx, int32_t bone_mask)
	{
		// stubbed this.
	}

	double __fastcall get_fov(cs_player_t* player, uintptr_t edx)
	{
		constexpr auto default_fov = 90;

		if (game->prediction->is_active())
			return hook_manager.get_fov->call(player, edx);

		if (game->client.at(functions::return_to_should_draw_viewmodel) == uintptr_t(_ReturnAddress()))
			return cfg.local_visuals.viewmodel_in_scope.get() ? default_fov : -1.f;

		const auto backup_fov = player->get_fov();
		const auto backup_fov_start = player->get_fovstart();

		const auto wpn = reinterpret_cast<weapon_t*>(game->client_entity_list->get_client_entity_from_handle(player->get_active_weapon()));
		if (cfg.local_visuals.fov_changer.get() && wpn)
		{
			const auto info = wpn->get_cs_weapon_data();

			auto remap_fov = [&](int32_t in)
			{
				if (in == default_fov || in == 0)
					return default_fov + static_cast<int32_t>(cfg.local_visuals.fov.get());

				if (in == info->zoom_fov1)
				{
					if (cfg.local_visuals.skip_first_scope.get() && info->zoom_levels > 1)
						return default_fov + static_cast<int32_t>(cfg.local_visuals.fov.get());
					else
						return in + static_cast<int32_t>(cfg.local_visuals.fov2.get());
				}

				if (in == info->zoom_fov2 && cfg.local_visuals.skip_first_scope.get() && info->zoom_levels > 1)
					return info->zoom_fov1 + static_cast<int32_t>(cfg.local_visuals.fov.get());

				return in;
			};

			player->get_fov() = remap_fov(player->get_fov());
			player->get_fovstart() = remap_fov(player->get_fovstart());
		}

		const auto ret = hook_manager.get_fov->call(player, edx);
		player->get_fov() = backup_fov;
		player->get_fovstart() = backup_fov_start;
		return ret;
	}

	int32_t __fastcall get_default_fov(cs_player_t* player, uintptr_t edx)
	{
		const auto org = hook_manager.get_default_fov->call(player, edx);

		if (game->prediction->is_active() || !cfg.local_visuals.fov_changer.get())
			return org;

		return org + static_cast<int32_t>(cfg.local_visuals.fov.get());
	}

	void __fastcall update_clientside_animation(cs_player_t* player, uint32_t edx)
	{
		const auto state = player->get_anim_state();
		if (state && (cfg.rage.enable.get() || player->index() == game->engine_client->get_local_player()))
		{
			state->player = nullptr;
			hook_manager.update_clientside_animation->call(player, edx);
			state->player = player;
		}
		else
			hook_manager.update_clientside_animation->call(player, edx);
	}
}