
#include <base/debug_overlay.h>
#include <detail/custom_prediction.h>
#include <base/hook_manager.h>
#include <base/cfg.h>
#include <sdk/client_entity_list.h>
#include <sdk/engine_client.h>
#include <sdk/prediction.h>
#include <sdk/client_state.h>
#include <sdk/game_rules.h>
#include <sdk/math.h>
#include <sdk/convar.h>
#include <hacks/rage.h>
#include <hacks/antiaim.h>
#include <hacks/movement.h>
#include <hacks/visuals.h>
#include <hacks/esp.h>
#include <hacks/misc.h>
#include <hacks/chams.h>
#include <hacks/grenade_prediction.h>
#include <detail/aim_helper.h>
#include <detail/events.h>
#include <detail/shot_tracker.h>
#include <sdk/misc.h>
#include <hacks/advertisement.h>
#include <hacks/tickbase.h>
#include <hacks/peek_assistant.h>
#include <sdk/debug_overlay.h>
#include <sdk/mdl_cache.h>
#include <hacks/skinchanger.h>
#include <util/anti_debug.h>
#include <menu/menu.h>
#include <detail/dx_adapter.h>
#include <ren/renderer.h>
#include <ren/adapter_dx9.h>

#ifdef CSGO_LUA
#include <lua/engine.h>
#include <lua/helpers.h>
#include <gui/selectable_script.h>
#endif

using namespace sdk;
using namespace detail;
using namespace detail::aim_helper;
using namespace hacks;
using namespace evo;
using namespace ren;

namespace hooks::hl_client
{
	void __fastcall frame_stage_notify(hl_client_t* client, uint32_t edx, framestage stage)
	{
		player_list.refresh_local();

		if (!game->engine_client->is_ingame())
			evnt.is_round_end = false;

		if (stage == frame_net_update_postdataupdate_start)
			vis->on_post_data_update();

		if (stage == frame_render_start)
		{
			player_list.refresh_local();
			util::check_integrity();
			util::check_game_stream();
			vis->on_frame_render_start();
		}
		
		if (stage == frame_net_update_end)
		{
			shot_track.on_update_end();
			misc::on_post_data();
		}

		misc::on_frame_stage_notify(stage);

#ifdef CSGO_LUA
		lua::api.callback(FNV1A("on_frame_stage_notify"), [&](lua::state& s) -> int {
			s.push(static_cast<int>(stage));
			s.push(true);
			
			return 2;
		});
#endif

		hook_manager.frame_stage_notify->call(client, edx, stage);

		if (stage == frame_render_start)
			vis->on_frame_render_start(false);

		if (stage == frame_render_end && evo::ren::adapter && dx_adapter::mtx.try_lock())
		{
			dx_adapter::clear_buffer();
			draw_mgr.draw();
			draw_mgr.draw(true);

#ifdef CSGO_LUA
			if (menu::menu.did_finalize)
			{
				static const auto toggle = evo::gui::ctx->find<evo::gui::button>(GUI_HASH("scripts.general.toggle"));
				static const auto scripts = evo::gui::ctx->find<evo::gui::list>(GUI_HASH("scripts.general.list"));

				scripts->for_each_control([&](std::shared_ptr<evo::gui::control> &c)
				{
					const auto sel = c->as<::gui::selectable_script>();
					if (sel->file.should_load)
					{
						sel->file.should_load = false;

						if (lua::api.run_script(sel->file))
						{
							toggle->text = XOR_STR("Unload");
							sel->is_loaded = true;
							sel->reset();
						}
						else
							sel->file.should_unload = true;
					}
					else if (sel->file.should_unload)
					{
						lua::api.stop_script(sel->file);
						toggle->text = XOR_STR("Load");
						sel->file.should_unload = false;
						sel->is_loaded = false;
						sel->reset();
					}
				});
			}

			lua::api.in_render = true;
			lua::api.callback(FNV1A("on_paint"));
			lua::api.in_render = false;
#endif

			dx_adapter::ready = true;
			dx_adapter::mtx.unlock();
		}

		if (stage == frame_net_update_postdataupdate_start)
			skin_changer->on_post_data_update();

#ifdef CSGO_LUA
		lua::api.callback(FNV1A("on_frame_stage_notify"), [&](lua::state& s) -> int {
			s.push(static_cast<int>(stage));
			s.push(false);
			
			return 2;
		});

		if (stage == frame_start)
			lua::api.run_timers();
#endif
	}

	void __cdecl create_move_proxy(hl_client_t* client, int32_t sequence_number, float input_sample_frametime, bool active, bool& send_packet)
	{
		auto cmd = &game->input->commands[sequence_number % input_max];

		hook_manager.create_move->call(client, 0, sequence_number, input_sample_frametime, active);

		misc::on_before_move();
		peek_assistant.on_before_move();

		auto did_rage_shoot = false;
		const auto attempted_to_jump = cmd->buttons & user_cmd::jump;
		rag.set_attempt_jump(attempted_to_jump);

		advert.clantag_changer();

		player_list.refresh_local();
		const auto old_local_player = game->local_player;
		game->local_player = local_record.player = local_shot_record.player = local_fake_record.player
			= reinterpret_cast<cs_player_t*>(game->client_entity_list->get_client_entity(game->engine_client->get_local_player()));

#ifdef CSGO_LUA
		// run this even when local player is dead
		lua::api.callback(FNV1A("on_setup_move"), [cmd, send_packet](lua::state& s) -> int {
			s.create_user_object_ptr(XOR_STR("csgo.user_cmd"), cmd);
			s.push(send_packet);
			return 2;
		});
#endif

		const auto local_player = game->local_player;
		if (!local_player || !local_player->is_alive())
		{
			game->local_player = nullptr;
			return;
		}

		if (!old_local_player)
			evnt.is_game_end = evnt.is_round_end = false;

		const auto wpn = reinterpret_cast<weapon_t* const>(game->client_entity_list->get_client_entity_from_handle(local_player->get_active_weapon()));

		if (!wpn)
			return;

		game->mdl_cache->begin_lock();

		cfg.on_create_move(wpn);
		misc::on_create_move(cmd);
		peek_assistant.on_create_move(cmd);

		trace.check_wallbang();
		mov.bhop(cmd);
		pred.initial(cmd);
		if (attempted_to_jump)
			mov.fix_bhop(cmd);

		peek_assistant.on_post_move(cmd);

		if (!tickbase.force_choke)
			aa.prepare_on_peek();
		tickbase.adjust_limit_dynamic();
		if (tickbase.force_choke)
			send_packet = false;

		if (local_player->get_flags() & cs_player_t::frozen || game->rules->data->get_freeze_period() || evnt.is_game_end)
			cmd->buttons &= ~user_cmd::jump;

		if (!tickbase.force_choke && !tickbase.to_shift)
			send_packet = aa.calculate_lag(cmd);

		rag.autostop(cmd, false);

		if (cfg.rage.enable.get() && !tickbase.force_choke)
			did_rage_shoot = rag.on_create_move(cmd, send_packet);

		if (!evnt.is_freezetime && ((cfg.rage.enable.get() && wpn->is_shootable_weapon() && wpn->get_item_definition_index() != item_definition_index::weapon_revolver && !pred.can_shoot) ||
			(aa.get_shot_cmd() >= cmd->command_number - game->client_state->choked_commands && aa.get_shot_cmd() < cmd->command_number)))
			pred.had_attack = false;
		/*else if (!cfg.rage.enable.get())
		{
			if (cfg.trigger.enable.get())
				trig.on_create_move(cmd);

			if (cfg.legit.enable.get())
				leg.on_create_move(cmd);
		}*/

		if (did_rage_shoot && !tickbase.force_choke)
			send_packet = true;

		// respect manual shot in fakeduck.
		if (aa.is_fakeducking())
		{
			if (wpn->is_shootable_weapon() && is_shooting(cmd) && game->client_state->choked_commands != aa.determine_maximum_lag() / 2 - 1)
			{
				cmd->buttons &= ~user_cmd::attack;
				pred.had_attack = false;
			}
			else if (wpn->is_grenade() && wpn->get_throw_time() > game->globals->curtime && game->client_state->choked_commands != aa.determine_maximum_lag() / 2 - 1 - TIME_TO_TICKS(.1f))
				pred.had_attack = true;
		}

		if (is_shooting(cmd) && (aa.get_shot_cmd() < cmd->command_number - game->client_state->choked_commands || aa.get_shot_cmd() >= cmd->command_number))
		{
			if (tickbase.attempt_shift_back(send_packet))
			{
				if (!tickbase.force_choke && !aa.is_fakeducking())
					send_packet = true;
				if (!wpn->is_grenade() && tickbase.force_choke)
					pred.had_attack = false;
				else
				{
					if (peek_assistant.pos.has_value())
						peek_assistant.take_shot();

					if (!wpn->is_knife() && !wpn->is_grenade() && !rag.did_shoot())
					{
						auto dir = cmd->viewangles;
						if (!cfg.rage.enable.get())
							dir += game->local_player->get_punch_angle_scaled();

						vec3 forward;
						angle_vectors(dir, forward);

						auto s = shot();
						s.start = local_player->get_eye_position();
						s.end = local_player->get_eye_position() + forward * wpn->get_cs_weapon_data()->range;
						s.manual = true;
						s.time = game->globals->curtime;
						s.cmd_num = cmd->command_number;
						rag.on_manual(cmd, std::move(s));
					}

					aa.set_shot_cmd(cmd, !send_packet);
				}
			}
			else
				pred.had_attack = false;
		}
		else if (rag.did_shoot())
		{
			if (!tickbase.attempt_shift_back(send_packet))
				rag.drop_shot();
		}

		if (peek_assistant.pos.has_value() && pred.can_shoot && (pred.had_attack || rag.did_shoot()))
			peek_assistant.take_shot();

		if (!rag.did_stop() && rag.did_stop_last_interval() && game->client_state->choked_commands == 0 && !attempted_to_jump)
		{
			std::tie(cmd->forwardmove, cmd->sidemove) = std::tie(pred.original_cmd.forwardmove, pred.original_cmd.sidemove);
			rag.autostop(cmd, true);
		}

		if (!rag.did_stop())
		{
			cmd->forwardmove = pred.original_cmd.forwardmove;
			cmd->sidemove = pred.original_cmd.sidemove;
			cmd->buttons |= pred.original_cmd.buttons & user_cmd::speed;
			pred.repredict(cmd);
			
			if (local_player->is_on_ground())
			{
				std::tie(cmd->forwardmove, cmd->sidemove) = std::tie(pred.original_cmd.forwardmove, pred.original_cmd.sidemove);

				// full stop.
				if (aa.is_active() && !cfg.antiaim.desync.get().test(cfg_t::desync_none)
					&& cmd->forwardmove == 0.f && cmd->sidemove == 0.f && !attempted_to_jump)
					slow_movement(cmd, 0.f);
				else if ((cfg.antiaim.slow_walk.get() || aa.is_fakeducking()) && !peek_assistant.has_shot)
					rag.autostop(cmd, true, true);
				// prevent move layer from reaching highest weight...
				else if (aa.is_active() && !attempted_to_jump && !cfg.antiaim.desync.get().test(cfg_t::desync_none)
					&& cmd->command_number > 1 && pred.info[(cmd->command_number - 1) % input_max].flags & cs_player_t::on_ground)
				{
					const auto jitter = fabs(sin(fmodf(TICKS_TO_TIME(game->globals->tickcount), .4f) / .2f * sdk::pi));
					const auto amnt = local_player->get_velocity().length2d() / 25.f;
					slow_movement(cmd, max(wpn->get_max_speed() - 2.5f - amnt * jitter, 0.f));
				}
			}
		}

		if (send_packet)
		{
			// finalize tickbase shift.
			tickbase.prepare_cycle();
			
			// perform antiaim.
			aa.on_send_packet(cmd);

			// reset prediction.
			pred.restore(cmd);
			pred.repredict(&game->input->commands[game->client_state->last_command % input_max]);
			auto& log = pred.info[(game->client_state->last_command + 1) % input_max];
			if (log.sequence == game->client_state->last_command + 1)
				game->globals->curtime += TICKS_TO_TIME(log.shift);

			// run animation loop.
			const auto shot_cmd = aa.get_shot_cmd();
			auto highest_z = 0.f;
			for (auto i = 0; i <= game->client_state->choked_commands; i++)
			{	
				auto& c = game->input->commands[(cmd->command_number + i - game->client_state->choked_commands) % input_max];
				auto& verified = game->input->verified_commands[c.command_number % input_max];
				
				// restore state.
				game->globals->curtime += game->globals->interval_per_tick;
				pred.restore_animation(c.command_number - 1);

				// fix up shooting frame.
				if (c.command_number == shot_cmd)
				{
					auto backup_model = -1;
					auto backup_most_recent_bone_cache_counter = local_player->get_most_recent_model_bone_counter();
					auto backup_accessor = local_player->get_bone_accessor();
					auto backup_bones = *local_player->get_bone_accessor().out;

					const auto offset = local_player->get_view_offset().z;
					const auto obb_maxs = local_player->get_collideable()->obb_maxs();
					const auto ground_entity = local_player->get_ground_entity();

					local_shot_record.state[resolver_networked] = *local_player->get_anim_state();
					local_shot_record.layers = local_shot_record.custom_layers[resolver_networked] = *local_player->get_animation_layers();
					local_shot_record.poses[resolver_networked] = local_player->get_pose_parameter();
					local_shot_record.abs_ang[resolver_networked] = { 0.f, local_player->get_anim_state()->foot_yaw, 0.f };
					local_shot_record.origin = local_player->get_origin();
					local_shot_record.abs_origin = local_player->get_abs_origin();
					local_shot_record.valid = true;

					// perform pitch scaling.
					local_shot_record.poses[resolver_networked][cs_player_t::body_pitch] = .5f * c.viewangles.x / pitch_bounds + .5f;

					// reset model.
					if (pred.real_model != local_player->get_model_index())
					{
						backup_model = local_player->get_model_index();
						local_player->set_model_index(pred.real_model);
					}

					// setup matrix.
					local_shot_record.perform_matrix_setup();
					rag.on_finalize_cycle(&c, send_packet);
					local_player->get_view_offset().z = offset;
					local_player->get_collideable()->obb_maxs() = obb_maxs;
					local_player->get_ground_entity() = ground_entity;

					// reset model.
					if (backup_model != -1)
					{
						local_player->set_model_index(backup_model);
						local_player->update_addon_models(true);
						local_player->get_most_recent_model_bone_counter() = backup_most_recent_bone_cache_counter;
						local_player->get_bone_accessor().readable = backup_accessor.readable;
						local_player->get_bone_accessor().writeable = backup_accessor.writeable;
						*local_player->get_bone_accessor().out = backup_bones;
					}
				}
				
				// run antiaim on target cmd.
				const auto original = c.viewangles;
				aa.on_send_command(&c);

#ifdef CSGO_LUA
				lua::api.callback(FNV1A("on_run_command"), [c](lua::state& s) -> int
				{
					s.create_user_object_ptr(XOR_STR("csgo.user_cmd"), &c);
					return 1;
				});
#endif

				// emulate p-silent fix and nade throw.
				const auto& expected = pred.info[c.command_number % input_max];
				const auto shot = !wpn->is_grenade() && shot_cmd >= cmd->command_number - game->client_state->choked_commands && shot_cmd <= c.command_number;
				const auto nade_throw = wpn->is_grenade() && expected.sequence == c.command_number && wpn->get_throw_time() > 0.f
					&& (expected.throw_time == 0.f || game->globals->curtime > expected.throw_time);
				if (shot || nade_throw)
				{
					auto org_angle = c.viewangles;
					c.viewangles = nade_throw ? original : game->input->commands[shot_cmd % input_max].viewangles;
					
					if (nade_throw)
						grenade_predict->adjust_throw_velocity(cmd->viewangles);

					
					if (peek_assistant.pos.has_value())
						peek_assistant.take_shot();
					
					fix_movement(&c, org_angle);
				}
				
				normalize(c.viewangles);
				normalize_move(c.forwardmove, c.sidemove, c.upmove);

				// make sure predicted data for animation is correct.
				verified.crc = *reinterpret_cast<int32_t*>(&game->globals->interval_per_tick);
				pred.repredict(&c);
				
				// store off z position.
				highest_z = max(local_player->get_eye_position().z - local_player->get_abs_origin().z, highest_z);

				// verify command.
				verified.cmd = c;
				verified.crc = c.get_checksum();
			}
			
			// update visual z position.
			aa.set_highest_z(highest_z);
			
			// store animation.
			if (TIME_TO_TICKS(game->globals->curtime) >= tickbase.lag.first)
			{
				local_record.origin = local_player->get_abs_origin();
				local_record.abs_ang[resolver_networked] = { 0.f, local_player->get_anim_state()->foot_yaw, 0.f };
				local_record.state[resolver_networked] = *local_player->get_anim_state();
				local_record.layers = *local_player->get_animation_layers();
				local_record.poses[resolver_networked] = local_player->get_pose_parameter();
				local_record.lag = cmd->command_number;
				local_record.duck = local_player->get_duck_amount();

				// build fake chams.
				player_list.build_fake_animation(local_player, cmd);
			}
		}

#ifdef CSGO_LUA
		lua::api.callback(FNV1A("on_create_move"), [cmd, send_packet](lua::state& s) -> int
		{
			s.create_user_object_ptr(XOR_STR("csgo.user_cmd"), cmd);
			s.push(send_packet);
			return 2;
		});
#endif

		game->prediction->set_local_view_angles(cmd->viewangles);
		pred.restore(cmd, send_packet);

		if (send_packet)
			tickbase.on_finalize_cycle(send_packet);

		game->mdl_cache->end_lock();
	}

	__declspec(naked) void __fastcall create_move(hl_client_t* client, uint32_t edx, int32_t sequence_number, float input_sample_frametime, bool active)
	{
		__asm
		{
			push ebp
			mov ebp, esp

			movzx ebx, bl
			push ebx
			push esp /* reference to our own stackframe, which contains send_packet, see line above. */
			push dword ptr [ebp + 16]
			push dword ptr [ebp + 12]
			push dword ptr [ebp + 8]
			push ecx
			call create_move_proxy
			add esp, 20
			pop ebx

			mov esp, ebp
			pop ebp
			ret 12
		}
	}

	void __fastcall level_init_pre_entity(hl_client_t* client, uint32_t edx, const char* level_name)
	{
		if (client)
			hook_manager.level_init_pre_entity->call(client, edx, level_name);

		// cl_interp max patch
		const auto cl_interp_patch = reinterpret_cast<float*>(game->client.at(globals::patch_cl_interp_clamp));
		if (*cl_interp_patch != .5f)
		{
			util::mem_guard guard(PAGE_EXECUTE_READWRITE, cl_interp_patch, sizeof(float));
			*cl_interp_patch = .5f;
		}

		// set the best convars.
		SET_CONVAR_FLOAT("cl_interp", .03125f);
		SET_CONVAR_FLOAT("cl_interp_ratio", 2.f);
		SET_CONVAR_FLOAT("r_modelAmbientMin", 0.f);
		SET_CONVAR_FLOAT("mat_software_aa_strength", 0.f);
		SET_CONVAR_INT("cl_pred_doresetlatch", 0);
		
		const auto r_player_visibility_mode = game->cvar->find_var(XOR_STR("r_player_visibility_mode"));
		r_player_visibility_mode->flags &= ~FCVAR_ARCHIVE;
		r_player_visibility_mode->set_int(0);

		const auto dsp_slow_cpu = game->cvar->find_var(XOR_STR("dsp_slow_cpu"));
		dsp_slow_cpu->flags &= ~FCVAR_CHEAT;
		dsp_slow_cpu->set_int(1);

		const auto dsp_enhance_stereo = game->cvar->find_var(XOR_STR("dsp_enhance_stereo"));
		dsp_enhance_stereo->flags &= ~FCVAR_ARCHIVE;
		dsp_enhance_stereo->set_int(0);

		const auto fps_max = game->cvar->find_var(XOR_STR("fps_max"));
		if (fps_max->get_int() == 0 || fps_max->get_int() > 500)
		{
			fps_max->flags &= ~FCVAR_ARCHIVE;
			fps_max->set_int(500);
		}

		const auto rate = game->cvar->find_var(XOR_STR("rate"));
		if (rate->get_int() > 524288 || rate->get_int() == 196608)
		{
			rate->flags &= ~FCVAR_ARCHIVE;
			rate->set_int(524288);
		}

		// load convar states.
		game->cl_lagcompensation = !FNV1A_CMP(game->cvar->find_var(XOR_STR("cl_lagcompensation"))->string, "0");
		game->cl_predict = !FNV1A_CMP(game->cvar->find_var(XOR_STR("cl_predict"))->string, "0");
		
#ifndef NDEBUG
		game->cvar->unlock();
#endif

		// force the game to enable hdr on non-hdr maps.
		game->hardware_config->set_hdr_enable(true);
		game->model_loader->override_last_loaded_hdr_state(true);
		cham.reload();

		// notify on map change.
		game->on_map_change = true;
		game->client_state->request_full_update();

		// reset player entries.
		player_list.reset();

		// reset antiaim.
		user_cmd cmd{};
		cmd.command_number = 0;
		aa.set_shot_cmd(&cmd);
		local_record = local_shot_record = local_fake_record = lag_record();
		evnt.is_game_end = evnt.is_round_end = false;
		peek_assistant = peek_assistant_t();
		aa.clear_times();

#ifdef CSGO_LUA
		lua::api.callback(FNV1A("on_level_init"));
#endif
	}
}
