
#include <hacks/movement.h>
#include <hacks/peek_assistant.h>
#include <detail/aim_helper.h>
#include <detail/custom_prediction.h>

using namespace sdk;
using namespace detail;
using namespace detail::aim_helper;

namespace hacks
{
	movement mov{};

	void movement::bhop(user_cmd* const cmd)
	{
		const auto move_type = game->local_player->get_move_type();
		const auto in_water = game->local_player->get_water_level() >= 2;
		const auto can_jump = !in_water && move_type != movetype_ladder && move_type != movetype_noclip && move_type != movetype_observer;

		if (cmd->buttons & user_cmd::buttons::jump
			&& can_jump)
		{
			if (cfg.misc.bhop.get() && !(game->local_player->get_flags() & user_cmd::on_ground))
				cmd->buttons &= ~user_cmd::buttons::jump;
		}

		autostrafe(cmd, can_jump);
	}

	void movement::fix_bhop(sdk::user_cmd* cmd)
	{
		if (cfg.misc.bhop.get() && !GET_CONVAR_INT("sv_autobunnyhopping") && game->local_player->is_on_ground())
		{
			cmd->buttons &= ~user_cmd::jump;
			pred.repredict(cmd);
		}
	}

	void movement::autostrafe(user_cmd* const cmd, bool can_jump)
	{
		if (!can_jump ||
			(cfg.misc.peek_assistant.get() && peek_assistant.has_shot) ||
			cmd->buttons & user_cmd::speed ||
			cfg.misc.autostrafe.get().test(cfg_t::autostrafe_disabled))
		{
			disabled_this_interval = true;
			return;
		}

		if (game->local_player->get_flags() & user_cmd::flags::on_ground)
			disabled_this_interval = false;

		if (game->local_player->get_flags() & user_cmd::flags::on_ground || disabled_this_interval)
			return;			

		const auto wpn = reinterpret_cast<weapon_t * const>(
			game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));

		if (wpn && wpn->get_item_definition_index() == item_definition_index::weapon_ssg08 && game->local_player->get_velocity().length2d() < 50.f)
			return;

		auto target = cmd->viewangles;

		if (cfg.misc.autostrafe.get().test(cfg_t::autostrafe_move))
		{
			if (cmd->buttons & user_cmd::forward)
			{
				if (cmd->buttons & user_cmd::move_left)
					target.y += 45.f;

				if (cmd->buttons & user_cmd::move_right)
					target.y -= 45.f;
			}
			else if (cmd->buttons & user_cmd::back)
			{
				if (cmd->buttons & user_cmd::move_left)
					target.y -= 45.f;

				if (cmd->buttons & user_cmd::move_right)
					target.y += 45.f;

				target.y += 180.f;
			}
			else
			{
				if (cmd->buttons & user_cmd::move_left)
					target.y += 90.f;

				if (cmd->buttons & user_cmd::move_right)
					target.y -= 90.f;
			}
		}

		constexpr auto max_wish = 30.f;
		const auto max_speed = GET_CONVAR_FLOAT("sv_maxspeed");
		auto vel = game->local_player->get_velocity();
		vel.z = 0.f;

		vec3 forward;
		angle_vectors(angle(0.f, target.y, 0.f), forward);

		const auto speed = vel.length2d();
		const auto start = game->local_player->get_origin();
		const auto end = start + forward * speed * game->globals->interval_per_tick * 3.f;

		ray r{};
		trace_world_filter filter{};
		game_trace tr{};
		r.init(start, end, game->local_player->get_collideable()->obb_mins(), game->local_player->get_collideable()->obb_maxs());
		game->engine_trace->trace_ray(r, mask_playersolid, &filter, &tr);

		if (tr.fraction < 1.f && fabsf(tr.plane.normal.z) < .0001f)
		{
			constexpr auto clip_velocity = [](const vec3& in, const vec3& normal, vec3& out)
			{
				const auto back = in.dot(normal);
				out = in - normal * back;

				if (auto adjust = out.dot(normal); adjust < 0.f)
					out -= normal * min(adjust, -.03125f);
			};

			vec3 out;
			clip_velocity(start + forward * speed, tr.plane.normal, out);

			angle ang;
			vector_angles(out, ang);

			if (fabsf(std::remainderf(ang.y - target.y, yaw_bounds)) > 90.f)
				ang.y += 180.f;

			r.init(tr.endpos + forward * max_speed * game->globals->interval_per_tick,
					tr.endpos + vec3(0, 0, game->local_player->get_step_size() + .03125f)
					+ forward * max_speed * game->globals->interval_per_tick,
					game->local_player->get_collideable()->obb_mins(),
					game->local_player->get_collideable()->obb_maxs());
			game->engine_trace->trace_ray(r, mask_playersolid, &filter, &tr);

			if (!tr.startsolid || tr.allsolid)
				target.y = ang.y;
		}

		if (speed > 0.f)
		{
			angle vel_ang;
			vector_angles(vel, vel_ang);

			auto delta = 0.f;
			const auto view_diff = std::remainderf(vel_ang.y - target.y, yaw_bounds);

			auto term = GET_CONVAR_FLOAT("sv_airaccelerate") * game->globals->interval_per_tick * max_speed * speed;
			if (term != 0.f)
			{
				term = max_wish / term;
				if (fabsf(term) < 1.f)
					delta = RAD2DEG(acosf(term));
			}

			if (fabsf(view_diff) > 10.f)
				delta += min(fabsf(view_diff), delta * .1f * (1.f - cfg.misc.autostrafe_turn_speed.get() * .01f));

			if (delta > .01f)
				target.y = std::remainderf(vel_ang.y + ((view_diff > 0.f || (cfg.misc.autostrafe.get().test(cfg_t::autostrafe_move)
					&& (cmd->buttons & (user_cmd::forward | user_cmd::back | user_cmd::move_left | user_cmd::move_right)) && fabsf(view_diff) > 160.f)) ? -delta : delta), yaw_bounds);
		}

		cmd->forwardmove = forward_bounds;
		cmd->sidemove = 0.f;
		fix_movement(cmd, target);
	}
}
