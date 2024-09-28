
#include <base/hook_manager.h>
#include <detail/custom_prediction.h>
#include <base/debug_overlay.h>

using namespace sdk;
using namespace detail;

namespace hooks::entity
{
	void __fastcall on_latch_interpolated_variables(entity_t* ent, uint32_t edx, int32_t flags)
	{
		if (ent->get_predictable())
		{
			if (flags & interpolate_omit_update_last_networked && pred.is_computing())
				return;

			const auto backup_time = game->globals->curtime;
			if (ent->is_player())
				game->globals->curtime = TICKS_TO_TIME(reinterpret_cast<cs_player_t*>(ent)->get_command_context().cmd_number);
			if (ent->get_class_id() == class_id::predicted_view_model)
			{
				const auto owner = reinterpret_cast<cs_player_t*>(game->client_entity_list->get_client_entity_from_handle(ent->get_viewmodel_owner()));
				if (owner && owner->is_player())
					game->globals->curtime = TICKS_TO_TIME(owner->get_command_context().cmd_number);
			}

			hook_manager.on_latch_interpolated_variables->call(ent, edx, flags);
			game->globals->curtime = backup_time;
			return;
		}

		if (!ent->is_player() || !(flags & latch_simulation_var))
			return hook_manager.on_latch_interpolated_variables->call(ent, edx, flags);

		const auto player = reinterpret_cast<cs_player_t*>(ent);
		auto& entry = GET_PLAYER_ENTRY(player);
		constexpr auto clock_correction = .03f;
		const auto interp = GET_CONVAR_FLOAT("cl_interp");
		const auto backup_time = player->get_simulation_time();
		player->get_simulation_time() = std::clamp(player->get_simulation_time() + entry.dt_interp,
				game->globals->curtime - clock_correction + interp,
				game->globals->curtime + clock_correction + interp);
		hook_manager.on_latch_interpolated_variables->call(ent, edx, flags);
		player->get_simulation_time() = backup_time;
		entry.dt_interp = game->globals->curtime - player->get_simulation_time() + interp;
	}

	void __fastcall do_animation_events(entity_t* ent, uint32_t edx, cstudiohdr* hdr)
	{
		ent->get_prev_reset_events_parity() = ent->get_reset_events_parity();
		hook_manager.do_animation_events->call(ent, edx, hdr);
	}

	void __fastcall maintain_sequence_transitions(entity_t* ent, uintptr_t edx, uintptr_t bone_setup, vec3* pos, quaternion* q)
	{
		float cycle;
		__asm movss cycle, xmm2;

		const auto get_sequence_flags = (uintptr_t(__thiscall*)(void*, int32_t)) game->client.at(functions::hdr::get_sequence_flags);
		const auto flags = reinterpret_cast<uint32_t*>(get_sequence_flags(ent->get_studio_hdr(), ent->get_sequence()) + 12);

		if (game->local_player && ent->get_class_id() == class_id::predicted_view_model && ent->get_new_sequence_parity() != ent->get_prev_new_sequence_parity() && !(*flags & 1))
		{
			const auto player = reinterpret_cast<cs_player_t* const>(
				game->client_entity_list->get_client_entity_from_handle(ent->get_viewmodel_owner()));

			if (player && player->get_is_holding_look_at_weapon())
			{
				const auto dt = game->globals->curtime - TICKS_TO_TIME(game->globals->tickcount);
				ent->get_cycle_offset() = -1.f * ((TICKS_TO_TIME(game->client_state->command_ack) + dt - ent->get_anim_time()) * ent->get_sequence_cycle_rate(ent->get_studio_hdr(), ent->get_sequence()));
				cycle = 0.f;
			}
			else
				ent->get_cycle_offset() = 0.f;

			*flags |= 1;
			__asm movss xmm2, cycle;
			hook_manager.maintain_sequence_transitions->call(ent, edx, bone_setup, pos, q);
			*flags &= ~1;
		}
		else
		{
			__asm movss xmm2, cycle;
			hook_manager.maintain_sequence_transitions->call(ent, edx, bone_setup, pos, q);
		}
	}

	void __fastcall set_abs_angles(entity_t* ent, uint32_t edx, angle* ang)
	{
		if (uintptr_t(_ReturnAddress()) != game->client.at(functions::return_to_anim_state_update)
			|| (!cfg.rage.enable.get() && ent->index() != game->engine_client->get_local_player()))
			hook_manager.set_abs_angles->call(ent, edx, ang);
	}

	void __fastcall estimate_abs_velocity(entity_t* ent, uint32_t edx, vec3* out)
	{
		// grab return address from stack frame.
		uintptr_t* _ebp;
		__asm mov _ebp, ebp;
		auto& ret_addr = _ebp[1];

		// game tried to setup velocity for something irrelevant.
		if (ret_addr != game->client.at(functions::return_to_anim_state_setup_velocity) || !ent->is_player())
			return hook_manager.estimate_abs_velocity->call(ent, edx, out);

		// setup for other players when legit.
		if (!cfg.rage.enable.get() && ent->index() != game->engine_client->get_local_player())
			return hook_manager.estimate_abs_velocity->call(ent, edx, out);

		// write 'real' velocity to output.
		auto velocity = ent->get_velocity();
		*out = velocity;

		// skip velocity clamping shit.
		ret_addr += 0x101;

		// emulate volatile registers between the two addresses.
		__asm
		{
			movss xmm1, velocity.z
			movss xmm3, velocity.y
			movss xmm4, velocity.x
		}
	}

	bool __fastcall setup_bones(entity_t* ent, uint32_t edx, mat3x4* out, int max_bones, int mask, float time)
	{
		static const auto r_jiggle_bones = game->cvar->find_var(XOR_STR("r_jiggle_bones"));
		const auto p = reinterpret_cast<cs_player_t*>(uintptr_t(ent) - 4);

		if ((*(uintptr_t**)(util::get_ebp()))[1] == game->client.at(functions::return_to_hitbox_to_world_transforms) || game->prediction->is_active())
		{
			if (out)
			{
				if (!p->get_bone_accessor().out || !p->get_studio_hdr())
					return false;
				memcpy(out, p->get_bone_accessor().out, sizeof(mat3x4) * min(p->get_studio_hdr()->numbones(), max_bones));
			}

			return true;
		}

		const auto should_fix = cfg.rage.enable.get() || p->index() == game->engine_client->get_local_player();

		if (uintptr_t(_ReturnAddress()) != game->client.at(functions::return_to_cs_player_setup_bones))
		{
			const auto back = r_jiggle_bones->get_int();
			if (should_fix && (p->is_player() ||
				(p->get_shadow_parent() && p->get_shadow_parent()->get_client_unknown() && p->get_shadow_parent()->get_client_unknown()->get_client_entity() &&
					p->get_shadow_parent()->get_client_unknown()->get_client_entity()->is_player())))
				r_jiggle_bones->set_int(0);
			const auto ret = hook_manager.setup_bones->call(ent, edx, out, max_bones, mask, time);
			r_jiggle_bones->set_int(back);
			return ret;
		}

		// store off some stuff...
		auto& entry = GET_PLAYER_ENTRY(p);
		const auto state = p->get_anim_state();
		const auto offset = p->get_view_offset();
		const auto eye_angle = *p->eye_angles();
		const auto obb_maxs = p->get_collideable()->obb_maxs();
		const auto ground_entity = p->get_ground_entity();
		const auto view_height = p->get_view_height();
		auto rollback_weapon = base_handle();

		// reevaluate the animlod.
		p->get_lod_data().flags = lod_empty;
		p->get_lod_data().frame = 0;
		hook_manager.reevauluate_anim_lod->call(p, edx, mask);
		const auto flags = p->get_lod_data().flags;

		if (should_fix)
		{
			p->get_lod_data().flags |= lod_invisible;

			// restore animation.
			if (p->get_predictable())
				pred.restore_animation(hacks::tickbase.lag.second, true);
			else
			{
				const auto eye_watcher = p->get_var_mapping().find(FNV1A("C_CSPlayer::m_iv_angEyeAngles"));
				if (eye_watcher)
					eye_watcher->reset_to_last_networked();
				if (!entry.records.empty())
					p->eye_angles()->z = entry.records.front().get_resolver_roll();
				const auto vo_watcher = p->get_var_mapping().find(FNV1A("C_BasePlayer::m_iv_vecViewOffset"));
				if (vo_watcher)
					vo_watcher->reset_to_last_networked();
				p->get_view_height() = p->get_abs_origin().z + entry.view_delta;
			}

			// disable the feet planting.
			if (state)
			{
				state->first_foot_plant = true;

				if (p->get_predictable())
					p->set_abs_angle({ 0.f, state->foot_yaw, 0.f });
			}

			// rollback the stick to the beginning.
			if (p->is_alive() && !p->get_is_ragdoll() && state)
				for (const auto& handle : p->get_my_weapons())
				{
					const auto cur = reinterpret_cast<weapon_t*>(game->client_entity_list->get_client_entity_from_handle(handle));

					if (cur == state->weapon)
					{
						rollback_weapon = p->get_active_weapon();
						p->get_active_weapon() = handle;
						break;
					}
				}
		}

		// call original.
		const auto back = r_jiggle_bones->get_int();
		if (should_fix)
			r_jiggle_bones->set_int(0);
		const auto ret = hook_manager.setup_bones->call(ent, edx, out, max_bones, mask, time);
		r_jiggle_bones->set_int(back);

		// restore the rollback weapon and lod flags.
		if (should_fix)
		{
			if (rollback_weapon)
				p->get_active_weapon() = rollback_weapon;
			p->get_lod_data().flags = flags;
			p->get_view_offset() = offset;
			if (p->get_predictable())
				game->prediction->set_local_view_angles(eye_angle);
			else
				*p->eye_angles() = eye_angle;
			p->get_collideable()->obb_maxs() = obb_maxs;
			p->get_ground_entity() = ground_entity;
			p->get_view_height() = view_height;
		}
		return ret;
	}
}
