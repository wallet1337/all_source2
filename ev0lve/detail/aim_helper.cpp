
#include <detail/aim_helper.h>
#include <sdk/client_entity_list.h>
#include <sdk/weapon_system.h>
#include <sdk/weapon.h>
#include <sdk/cs_player.h>
#include <sdk/surface_props.h>
#include <detail/dispatch_queue.h>
#include <base/debug_overlay.h>
#include <detail/shot_tracker.h>
#include <sdk/debug_overlay.h>
#include <hacks/antiaim.h>
#include <detail/custom_prediction.h>
#include <hacks/rage.h>
#include <hacks/tickbase.h>
#include <unordered_set>
#include <sdk/cs_player_resource.h>

using namespace sdk;
using namespace hacks;

namespace detail
{
namespace aim_helper
{
	static std::vector<seed> precomputed_seeds = {};

	int32_t hitbox_to_hitgroup(cs_player_t::hitbox box)
	{
		switch (box)
		{
		case cs_player_t::hitbox::head:
		case cs_player_t::hitbox::neck:
			return hitgroup_head;
		case cs_player_t::hitbox::upper_chest:
		case cs_player_t::hitbox::chest:
			return hitgroup_chest;
		case cs_player_t::hitbox::thorax:
		case cs_player_t::hitbox::body:
		case cs_player_t::hitbox::pelvis:
			return hitgroup_stomach;
		case cs_player_t::hitbox::left_forearm:
		case cs_player_t::hitbox::left_upper_arm:
		case cs_player_t::hitbox::left_hand:
			return hitgroup_leftarm;
		case cs_player_t::hitbox::right_forearm:
		case cs_player_t::hitbox::right_upper_arm:
		case cs_player_t::hitbox::right_hand:
			return hitgroup_rightarm;
		case cs_player_t::hitbox::left_calf:
		case cs_player_t::hitbox::left_foot:
		case cs_player_t::hitbox::left_thigh:
			return hitgroup_leftleg;
		case cs_player_t::hitbox::right_calf:
		case cs_player_t::hitbox::right_foot:
		case cs_player_t::hitbox::right_thigh:
			return hitgroup_rightleg;
		default: break;
		}

		return hitgroup_generic;
	}

	bool is_limbs_hitbox(cs_player_t::hitbox box)
	{
		return box >= cs_player_t::hitbox::right_thigh;
	}

	bool is_body_hitbox(cs_player_t::hitbox box)
	{
		return box == cs_player_t::hitbox::body || box == cs_player_t::hitbox::pelvis;
	}

	bool is_multipoint_hitbox(cs_player_t::hitbox box, bool potential)
	{
		if (!is_aim_hitbox_rage(box, potential))
			return false;		
		
		return box != cs_player_t::hitbox::left_thigh &&
			box != cs_player_t::hitbox::right_thigh &&
			box != cs_player_t::hitbox::left_foot &&
			box != cs_player_t::hitbox::right_foot &&
			box != cs_player_t::hitbox::body;
	}

	bool is_aim_hitbox_legit(cs_player_t::hitbox box)
	{
		const auto hitgroup = hitbox_to_hitgroup(box);

		if (box == cs_player_t::hitbox::head && cfg.legit.hack.hitbox.get().test(cfg_t::aim_head))
			return true;

		if ((hitgroup == hitgroup_leftarm || hitgroup == hitgroup_rightarm) && cfg.legit.hack.hitbox.get().test(cfg_t::aim_arms))
			return true;

		if (hitgroup == hitgroup_chest && cfg.legit.hack.hitbox.get().test(cfg_t::aim_upper_body))
			return true;

		if (hitgroup == hitgroup_stomach && cfg.legit.hack.hitbox.get().test(cfg_t::aim_lower_body))
			return true;

		return ((hitgroup == hitgroup_leftleg || hitgroup == hitgroup_rightleg) && cfg.legit.hack.hitbox.get().test(cfg_t::aim_legs));
	}

	bool is_aim_hitbox_rage(cs_player_t::hitbox box, bool potential)
	{
		if (GET_CONVAR_INT("mp_damage_headshot_only"))
			return box == cs_player_t::hitbox::head && (potential || cfg.rage.hack.hitbox.get().test(cfg_t::aim_head));

		const auto hitgroup = hitbox_to_hitgroup(box);

		if (box == cs_player_t::hitbox::head && (potential || cfg.rage.hack.hitbox.get().test(cfg_t::aim_head)))
			return true;

		if ((box == cs_player_t::hitbox::left_upper_arm || box == cs_player_t::hitbox::right_upper_arm) && (potential || cfg.rage.hack.hitbox.get().test(cfg_t::aim_arms)))
			return true;

		if (hitgroup == hitgroup_chest && box != cs_player_t::hitbox::chest && (potential || cfg.rage.hack.hitbox.get().test(cfg_t::aim_upper_body)))
			return true;

		if (hitgroup == hitgroup_stomach && (potential || cfg.rage.hack.hitbox.get().test(cfg_t::aim_lower_body)))
			return true;

		return (hitgroup == hitgroup_leftleg || hitgroup == hitgroup_rightleg) && box != cs_player_t::hitbox::left_thigh
			&& box != cs_player_t::hitbox::right_thigh && (potential || cfg.rage.hack.hitbox.get().test(cfg_t::aim_legs));
	}

	bool should_avoid_hitbox(cs_player_t::hitbox box, bool potential)
	{
		if (GET_CONVAR_INT("mp_damage_headshot_only"))
			return false;

		const auto hitgroup = hitbox_to_hitgroup(box);

		if (box == cs_player_t::hitbox::head && cfg.rage.hack.avoid_hitbox.get().test(cfg_t::aim_head))
			return true;

		if ((box == cs_player_t::hitbox::left_upper_arm || box == cs_player_t::hitbox::right_upper_arm) && cfg.rage.hack.avoid_hitbox.get().test(cfg_t::aim_arms))
			return true;

		if (hitgroup == hitgroup_chest && box != cs_player_t::hitbox::chest && cfg.rage.hack.avoid_hitbox.get().test(cfg_t::aim_upper_body))
			return true;

		if (hitgroup == hitgroup_stomach && cfg.rage.hack.avoid_hitbox.get().test(cfg_t::aim_lower_body))
			return true;

		return (hitgroup == hitgroup_leftleg || hitgroup == hitgroup_rightleg) && box != cs_player_t::hitbox::left_thigh
				&& box != cs_player_t::hitbox::right_thigh && cfg.rage.hack.avoid_hitbox.get().test(cfg_t::aim_legs);
	}

	bool is_shooting(sdk::user_cmd* cmd)
	{
		const auto weapon_handle = game->local_player->get_active_weapon();

		if (!weapon_handle)
			return false;

		const auto wpn = reinterpret_cast<weapon_t*>(
			game->client_entity_list->get_client_entity_from_handle(weapon_handle));

		if (!wpn)
			return false;

		const auto is_zeus = wpn->get_item_definition_index() == item_definition_index::weapon_taser;
		const auto is_secondary = (!is_zeus && wpn->get_weapon_type() == weapontype_knife)
				|| wpn->get_item_definition_index() == item_definition_index::weapon_shield;

		auto shooting = false;

		if (wpn->is_grenade())
		{
			const auto& prev = pred.info[(cmd->command_number - 1) % input_max];
			shooting = prev.sequence == cmd->command_number - 1 && prev.throw_time > 0.f && wpn->get_throw_time() == 0.f;
		}
		else if (is_secondary)
			shooting = (pred.had_attack || pred.had_secondary_attack) && pred.can_shoot;
		else
			shooting = pred.had_attack && pred.can_shoot;
		
		return shooting;
	}

	float get_hitchance(cs_player_t* const player)
	{
		const auto wpn = reinterpret_cast<weapon_t*>(
			game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));
		const auto hc = cfg.rage.hack.hitchance_value.get() * .01f;
		
		if (wpn && wpn->get_item_definition_index() == item_definition_index::weapon_taser)
			return game->local_player->is_on_ground() ? hc : hc / 2.f;

		if (hc < .01f)
			return 0.f;
	
		const auto& entry = GET_PLAYER_ENTRY(player);
		return std::clamp(hc + min(entry.spread_miss, 3) / float(wpn->get_cs_weapon_data()->maxclip1), 0.f, 1.f);
	}
	
	int32_t get_adjusted_health(cs_player_t* const player)
	{
		return max(player->get_player_health() - shot_track.calculate_health_correction(player), 1);
	}

	float get_mindmg(cs_player_t* const player, const std::optional<cs_player_t::hitbox> hitbox, const bool approach)
	{
		const auto& entry = GET_PLAYER_ENTRY(player);
		const auto wpn = reinterpret_cast<weapon_t*>(
			game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));
		const auto target_min = cfg.rage.hack.min_dmg.get();

		if (!wpn || wpn->get_item_definition_index() == item_definition_index::weapon_taser || !cfg.rage.hack.dynamic_min_dmg.get())
			return target_min;

		if (wpn->get_item_definition_index() == item_definition_index::weapon_shield)
			return 1.f;
		
		const auto info = wpn->get_cs_weapon_data();
		const auto hitgroup = hitbox_to_hitgroup(hitbox.value_or(cs_player_t::hitbox::pelvis));
		const auto wpn_dmg = max(trace.scale_damage(player, static_cast<float>(info->idamage),
			info->flarmorratio, hitgroup) - 1.f, 0);

		if (target_min > wpn_dmg)
			return wpn_dmg;

		if (!approach)
			return target_min;

		const auto target_time = std::clamp(entry.aimbot.target_time * 2.f, 0.f, .25f);
		return wpn_dmg - 4.f * target_time * (wpn_dmg - target_min);
	}

	cs_player_t* get_closest_target()
	{
		cs_player_t* target = nullptr;
		auto current_fov = FLT_MAX;

		if (game->local_player && game->local_player->is_alive())
			game->client_entity_list->for_each_player([&target, &current_fov](cs_player_t* const player)
			{
				if (!player->is_valid(true) || !player->is_enemy() || game->local_player == player)
					return;

				if (player->is_dormant() && GET_PLAYER_ENTRY(player).visuals.last_update < game->client_state->last_server_tick - TIME_TO_TICKS(5.f))
					return;

				const auto fov = get_fov_simple(game->engine_client->get_view_angles(), game->local_player->get_eye_position(), player->get_eye_position());

				if (current_fov > fov)
				{
					target = player;
					current_fov = fov;
				}
			});

		return target;
	}

	bool in_range(vec3 start, vec3 end)
	{
		constexpr auto ray_extension = 8.f;

		ray r{};
		trace_no_player_filter filter{};

		const auto length = (end - start).length();
		const auto direction = (end - start).normalize();

		auto distance = 0.f, thickness = 0.f;
		auto penetrations = 4;

		while (distance <= length)
		{
			if (penetrations < 0)
				return false;

			distance += ray_extension;
			r.init(start + direction * distance, end);
			game_trace tr{};
			game->engine_trace->trace_ray(r, mask_shot_hull, &filter, &tr);

			if (tr.fraction == 1.f)
				return true;

			penetrations--;
			thickness += ray_extension;
			distance += (end - r.start).length() * tr.fraction + ray_extension;

			while (distance <= length)
			{
				auto check = start + direction * distance;
				if (!!(game->engine_trace->get_point_contents_world_only(check, mask_shot_hull) & mask_shot_hull))
					thickness += ray_extension;
				else
				{
					thickness = 0.f;
					break;
				}

				if (thickness >= 90.f)
					return false;

				distance += ray_extension;
			}
		}

		return true;
	}

	std::optional<sdk::vec3> get_hitbox_position(cs_player_t* const player, const cs_player_t::hitbox id, const mat3x4* const bones)
	{
		const auto studio_model = game->model_info_client->get_studio_model(player->get_model());

		if (studio_model)
		{
			const auto hitbox = studio_model->get_hitbox(static_cast<uint32_t>(id), player->get_hitbox_set());

			if (hitbox)
			{
				auto tmp = angle_matrix(hitbox->rotation);
				tmp = concat_transforms(bones[hitbox->bone], tmp);
				vec3 min{}, max{};
				vector_transform(hitbox->bbmin, tmp, min);
				vector_transform(hitbox->bbmax, tmp, max);
				return (min + max) * .5f;
			}
		}

		return std::nullopt;
	}

	std::vector<rage_target> perform_hitscan(const std::shared_ptr<lag_record>& record, const bool minimal, const bool everything)
	{
		std::vector<rage_target> targets;
		
		const auto wpn = reinterpret_cast<weapon_t*>(
			game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));
		const auto studio_model = game->model_info_client->get_studio_model(record->player->get_model());
		if (!wpn || !studio_model)
			return targets;

		std::vector<dispatch_queue::fn> calls;
		std::array<std::deque<rage_target>, static_cast<size_t>(cs_player_t::hitbox::max)> reductions;
		const auto start = game->local_player->get_eye_position();
		
		// determine multipoint adjustment.
		const auto spread_rad = wpn->get_spread() + get_lowest_inaccuray();
		const auto corner_rad = DEG2RAD(90.f - RAD2DEG(spread_rad));
		const auto dist = (record->origin - game->local_player->get_origin()).length();
		const auto spread_radius = (dist / sinf(corner_rad)) * sinf(spread_rad);

		// build run tasks.
		for (const auto& hitbox : cs_player_t::hitboxes)
		{
			if (!is_aim_hitbox_rage(hitbox, everything))
				continue;
			
			calls.push_back([&record, &start, &reductions, &hitbox, &minimal, &everything, &studio_model, &spread_radius]()
			{
				auto& reduction = reductions[static_cast<size_t>(hitbox)];
				calculate_multipoint(reduction, record, hitbox, nullptr, everything, spread_radius);
				if (reduction.empty())
					return;
				
				std::random_device rd;
				std::mt19937 g(rd());
				const auto center = reduction.front();
				reduction.pop_front();
				std::shuffle(reduction.begin(), reduction.end(), g);
				reduction.push_front(center);
				const auto begin = std::chrono::steady_clock::now();

				for (auto target = reduction.begin(); target != reduction.end(); target = std::next(target))
				{
					// rate limit the deep reduction scan but only after 3 points have been processed.
					if (std::distance(reduction.begin(), target) > 2)
					{
						std::chrono::duration<double, std::micro> duration = std::chrono::steady_clock::now() - begin;
						if (duration.count() > 1000.0)
						{
							target->pen.did_hit = false;
							continue;
						}
					}
					
					target->pen = trace.wall_penetration(start, target->position, record, !minimal);
					if (!target->pen.did_hit)
						continue;

					if (target->pen.damage > get_mindmg(target->record->player, target->pen.hitbox)
						|| target->pen.damage > get_adjusted_health(target->record->player) + health_buffer)
						std::tie(target->likelihood, target->likelihood_roll) = calculate_likelihood(&*target);
					else
						target->likelihood = target->likelihood_roll = 0.f;
					target->position = target->pen.end;
					target->hitbox = target->pen.hitbox;
					target->hitgroup = target->pen.hitgroup;
					const auto box = studio_model->get_hitbox(static_cast<uint32_t>(target->hitbox),
						record->player->get_hitbox_set());

					if (!is_aim_hitbox_rage(target->hitbox, everything))
					{
						target->pen.did_hit = false;
						continue;
					}
					
					if (box)
					{
						auto mat = record->mat[box->bone];
						mat = concat_transforms(angle_matrix(box->rotation), mat);
						vec3 min, max;
						vector_transform(box->bbmax, mat, max);
						vector_transform(box->bbmin, mat, min);
						target->center = (min + max) * .5f;
						
						const auto forward = (target->center - start).normalize();
						target->dist = distance_point_to_line(target->position, start, forward);
						target->dist = floorf(target->dist * 1000.f) / 1000.f;
					}
				}
			});
		}

		// perform hitscan.
		dispatch.evaluate(calls);

		// perform reduction.
		for (const auto& reduction : reductions)
			std::copy(reduction.begin(), reduction.end(), std::back_inserter(targets));

		// report result.
		return targets;
	}

	void calculate_multipoint(std::deque<rage_target>& targets, const std::shared_ptr<lag_record>& record, const cs_player_t::hitbox box,
		cs_player_t* override_player, const bool potential, const float adjustment)
	{
		const auto studio_model = game->model_info_client->get_studio_model(record->player->get_model());
		if (!studio_model)
			return;

		const auto hitbox = studio_model->get_hitbox(static_cast<uint32_t>(box), record->player->get_hitbox_set());
		if (!hitbox)
			return;
		
		const auto wpn = reinterpret_cast<weapon_t*>(
			game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));
		if (!wpn)
			return;
		
		auto tmp = angle_matrix(hitbox->rotation);
		tmp = concat_transforms(record->mat[hitbox->bone], tmp);
		vec3 min{}, max{};
		vector_transform(hitbox->bbmin, tmp, min);
		vector_transform(hitbox->bbmax, tmp, max);
		const auto center = (min + max) * .5f;
		
		const auto is_zeus = wpn->get_item_definition_index() == item_definition_index::weapon_taser;
		const auto hitgroup = hitbox_to_hitgroup(box);
		auto rs = .975f;
		
		if (!override_player)
		{
			// scale it down when we are missing due to spread.
			auto& entry = GET_PLAYER_ENTRY(record->player);
			if (entry.spread_miss > 2)
				rs *= 1.f - (entry.spread_miss - 2) * .2f;

			// scale it down further when it's a limb.
			if (is_limbs_hitbox(box))
				rs *= .75f;
		}
		
		if (!override_player && (is_zeus || !is_multipoint_hitbox(box, potential) || hitbox->radius == -1.f))
			rs = 0.f;
		
		targets.emplace_back(center, record, false, center, box, hitgroup, 0.f);
		
		rs *= .5f + .5f * cfg.rage.hack.multipoint.get() * .01f;
		if (!game->cl_lagcompensation || !game->cl_predict)
			rs *= .8f;

		rs -= adjustment * .1f;
		if (rs <= .25f)
			return;
		
		const auto scan_reduction = rs <= .4f ? 2 : (rs <= .65f ? 1 : 0);
		rs *= hitbox->radius;
		const auto norm = (min - max).normalize();
		
		if (override_player)
		{
			const auto current_angle = calc_angle(center, override_player->get_eye_position());
			vec3 forward;
			angle_vectors(current_angle, forward);
			auto right = forward.cross(vec3(0, 0, 1)) * rs;
			auto left = vec3(-right.x, -right.y, -right.z);
			const auto top = vec3(0, 0, 1) * rs;

			angle hitbox_angle;
			vector_angles(norm, hitbox_angle);
			auto ang = hitbox_angle - current_angle;
			normalize(ang);

			const auto is_horizontal = hitbox_angle.x < 45.f && hitbox_angle.x > -45.f;
			if (ang.y < 0.f)
				std::swap(left, right);

			targets.pop_back();
			if (is_horizontal)
			{
				targets.emplace_back(max - right, record, false, center, box, hitgroup, 0.f);
				targets.emplace_back(min - left, record, false, center, box, hitgroup, 0.f);
			}
			else
			{
				targets.emplace_back(max - left, record, false, center, box, hitgroup, 0.f);
				targets.emplace_back(max - right, record, false, center, box, hitgroup, 0.f);

				if (box == cs_player_t::hitbox::right_calf || box == cs_player_t::hitbox::left_calf)
				{
					targets.emplace_back(min - left, record, false, center, box, hitgroup, 0.f);
					targets.emplace_back(min - right, record, false, center, box, hitgroup, 0.f);
				}
			}

			if (box == cs_player_t::hitbox::head)
				targets.emplace_back(max + top, record, false, center, box, hitgroup, 0.f);

			return;
		}		
		
		const auto rotation_space = vector_matrix(vec3(0, 0, 1));
		const auto capsule_space = vector_matrix(norm);
		auto len = max - min;
		
		vec3 capsule[capsule_vert_size];
		for (auto i = 0; i < capsule_vert_size; i++)
		{
			if (scan_reduction == 1 && i > 2 && i % 3 == 0)
				continue;
			
			auto vert = vec3(capsule_verts[i][0], capsule_verts[i][1], capsule_verts[i][2]);
			vert = vector_rotate(vert, rotation_space);
			vert = vector_rotate(vert, capsule_space);
			vert *= rs;
			if (capsule_verts[i][2] > 0.f)
				vert += len;
			capsule[i] = min + vert;
			
			if (scan_reduction == 2 && i > 2 && i % 3 != 0)
				continue;
			
			targets.emplace_back(capsule[i], record, false, center, box, hitgroup, 0.f);
		}
		
		for (auto i = 1; i < capsule_line_size; i += 2)
		{
			const auto dir = capsule[capsule_lines[i]] - capsule[capsule_lines[i - 1]];
			const auto points = static_cast<int8_t>(ceilf(dir.length() / (5.f + static_cast<float>(scan_reduction))));
			for (auto j = 0; j < points; j++)
				targets.emplace_back(capsule[capsule_lines[i - 1]] + dir * ((j + 1) / (points + 1.f)), record, false, center, box, hitgroup, 0.f);
		}
	}
	
	void optimize_cornerpoint(rage_target* target)
	{
		const auto health = get_adjusted_health(target->record->player);
		
		auto compare_points = [target, &health](const vec3& dir1, const vec3& dir2)
		{
			const auto p1 = target->position + dir1 * 1.5f;
			const auto p2 = target->position + dir2 * 1.5f;
			
			auto r1 = trace.wall_penetration(game->local_player->get_eye_position(), p1, target->record, true, std::nullopt, nullptr, true);
			auto r2 = trace.wall_penetration(game->local_player->get_eye_position(), p2, target->record, true, std::nullopt, nullptr, true);
			
			if (target->pen.secure_point)
			{
				r1.did_hit = r1.did_hit && r1.secure_point && (r1.min_damage >= get_mindmg(target->record->player, r1.hitbox)
					|| r1.min_damage >= get_adjusted_health(target->record->player) + health_buffer);
				r2.did_hit = r2.did_hit && r2.secure_point && (r2.min_damage >= get_mindmg(target->record->player, r2.hitbox)
					|| r2.min_damage >= get_adjusted_health(target->record->player) + health_buffer);
			}
			
			if (r1.did_hit && r1.damage >= get_mindmg(target->record->player, r1.hitbox)
				&& (!r2.did_hit || r2.damage < get_mindmg(target->record->player, r2.hitbox)))
			{
				target->position = p1;
				target->pen = r1;
			}
			else if (r2.did_hit && r2.damage >= get_mindmg(target->record->player, r2.hitbox)
				&& (!r1.did_hit || r1.damage < get_mindmg(target->record->player, r1.hitbox)))
			{
				target->position = p2;
				target->pen = r2;
			}
			else if (r1.did_hit && r1.secure_point && !r2.secure_point)
			{
				target->position = p1;
				target->pen = r1;
			}
			else if (r2.did_hit && r2.secure_point && !r1.secure_point)
			{
				target->position = p2;
				target->pen = r2;
			}
			else if (r1.did_hit && r1.damage >= health && (!r2.did_hit || r2.damage < health))
			{
				target->position = p1;
				target->pen = r1;
			}
			else if (r2.did_hit && r2.damage >= health && (!r1.did_hit || r1.damage < health))
			{
				target->position = p2;
				target->pen = r2;
			}
			else if (r1.did_hit && r2.did_hit && r1.damage > r2.damage + 25.f)
			{
				target->position = p1;
				target->pen = r1;
			}
			else if (r1.did_hit && r2.did_hit && r2.damage > r1.damage + 25.f)
			{
				target->position = p2;
				target->pen = r2;
			}
			else
			{
				auto t1 = *target, t2 = *target;
				t1.position = p1;
				t1.pen = r1;
				t2.position = p2;
				t2.pen = r2;

				if (r1.did_hit)
					std::tie(t1.likelihood, t1.likelihood_roll) = calculate_likelihood(&t1);
				else
					t1.likelihood = t1.likelihood_roll = 0.f;

				if (r2.did_hit)
					std::tie(t2.likelihood, t2.likelihood_roll) = calculate_likelihood(&t2);
				else
					t2.likelihood = t2.likelihood_roll = 0.f;

				auto had_roll = false;

				if (t1.likelihood_roll > target->likelihood_roll && t1.likelihood_roll > .2f)
				{
					had_roll = true;
					*target = t1;
				}

				if (t2.likelihood_roll > target->likelihood_roll && t2.likelihood_roll > .2f)
				{
					had_roll = true;
					*target = t2;
				}

				if (!had_roll)
				{
					if (t1.likelihood > target->likelihood)
						*target = t1;
					if (t2.likelihood > target->likelihood)
						*target = t2;
				}
			}
		};
		
		const auto top = (target->position - target->center).normalize();
		const auto bottom = vec3(-1, -1, -1) * top;
		const auto right = top.cross(vec3(1, 0, 0));
		const auto left = vec3(-1, -1, -1) * right;
		
		compare_points(left, right);
		compare_points(top, bottom);
	}

	std::pair<float, float> calculate_likelihood(rage_target* target)
	{
		if (precomputed_seeds.empty())
			return { 0.f, 0.f };

		const auto wpn = reinterpret_cast<weapon_t*>(
				game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));

		if (!wpn || wpn->get_item_definition_index() == item_definition_index::weapon_revolver)
			return { 0.f, 0.f };

		const auto studio_model = game->model_info_client->get_studio_model(target->record->player->get_model());

		if (!studio_model)
			return { 0.f, 0.f };

		const auto start = game->local_player->get_eye_position();
		const auto info = wpn->get_cs_weapon_data();

		// calculate inaccuracy and spread.
		const auto weapon_inaccuracy = wpn->get_inaccuracy();
		const auto weapon_spread = info->flspread;

		// are we playing on a no-spread server?
		const auto is_spread = weapon_inaccuracy > 0.f;

		// abort here for no spread.
		if (!is_spread)
			return { 0.f, 0.f };

		// calculate angle.
		const auto aim_angle = calc_angle(start, target->position);
		vec3 forward, right, up;
		angle_vectors(aim_angle, forward, right, up);

		// setup calculation parameters.
		vec3 total_spread, spread_angle, end;
		float inaccuracy, spread, total_x, total_y;
		seed* s;
		ray r;

		// cache hitbox transformations.
		const std::vector<resolver_direction> directions =
		{
			resolver_min,
			resolver_max,
			resolver_center,
			resolver_min_min,
			resolver_max_max,
			resolver_min_extra,
			resolver_max_extra
		};
		const std::vector<resolver_direction> directions_fake = { resolver_networked };
		const auto& dirs = target->record->player->is_fake_player() ? directions_fake : directions;
		struct
		{
			bool valid = false;
			int32_t group;
			float radius;
			vec3 min, max;
		} boxes[resolver_direction_max][size_t(cs_player_t::hitbox::max)];

		for (auto i : dirs)
		{
			for (const auto& hitbox : cs_player_t::hitboxes)
			{
				const auto box = studio_model->get_hitbox(static_cast<uint32_t>(hitbox),
						target->record->player->get_hitbox_set());

				if (box)
				{
					auto mat = target->record->res_mat[i][box->bone];
					mat = concat_transforms(angle_matrix(box->rotation), mat);
					vec3 min, max;
					vector_transform(box->bbmax, mat, max);
					vector_transform(box->bbmin, mat, min);
					boxes[i][static_cast<uint32_t>(hitbox)] = { true, box->group, box->radius, min, max };
				}
			}
		}

		// run traces.
		auto total_hits = 0, total_roll_hits = 0;
		auto missed = false, missed_roll = false, hit = false;
		const auto total = total_seeds / 2;
		for (auto i = 0; i < total; i++)
		{
			// get seed.
			s = &precomputed_seeds[i];

			// calculate spread.
			inaccuracy = s->inaccuracy;
			spread = s->spread;

			// correct spread value for different weapons.
			if (wpn->get_item_definition_index() == item_definition_index::weapon_negev && wpn->get_recoil_index() < 3.f)
				for (auto j = 3; j > static_cast<int32_t>(wpn->get_recoil_index()); j--)
					inaccuracy *= inaccuracy;

			inaccuracy *= weapon_inaccuracy;
			spread *= weapon_spread;
			total_x = (s->cos_inaccuracy * inaccuracy) + (s->cos_spread * spread);
			total_y = (s->sin_inaccuracy * inaccuracy) + (s->sin_spread * spread);
			total_spread = (forward + right * total_x + up * total_y).normalize();

			// calculate angle with spread applied.
			vector_angles(total_spread, spread_angle);

			// calculate end point of trace.
			angle_vectors(spread_angle, end);
			end = start + end.normalize() * info->range;
			r.init(start, end);

			// trace the bullet.
			missed = missed_roll = false;
			for (auto j : dirs)
			{
				hit = false;
				for (const auto& hitbox : cs_player_t::hitboxes)
				{
					auto& box = boxes[j][static_cast<uint32_t>(hitbox)];
					if (!box.valid)
						continue;

					if (box.radius == -1.f)
					{
						if (intersect_line_with_bb(r, box.min, box.max))
						{
							hit = true;
							break;
						}
					}
					else if (intersect_line_with_capsule(r, box.min, box.max, box.radius))
					{
						hit = true;
						break;
					}
				}

				if (!hit)
				{
					if (j == resolver_max_extra || j == resolver_max_max || j == resolver_min_extra || j == resolver_min_min)
						missed_roll = true;
					else
						missed = missed_roll = true;
					break;
				}
			}

			if (!missed)
				total_hits++;

			if (!missed_roll)
				total_roll_hits++;
		}

		return
		{
			min(static_cast<float>(total_hits * info->ibullets) / static_cast<float>(total), 1.f),
			min(static_cast<float>(total_roll_hits * info->ibullets) / static_cast<float>(total), 1.f)
		};
	}

	float calculate_hitchance(rage_target* target)
	{
		if (precomputed_seeds.empty())
			return 0.f;

		const auto wpn = reinterpret_cast<weapon_t*>(
			game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));

		if (!wpn)
			return 0.f;

		const auto studio_model = game->model_info_client->get_studio_model(target->record->player->get_model());

		if (!studio_model)
			return 0.f;

		const auto start = game->local_player->get_eye_position();
		const auto info = wpn->get_cs_weapon_data();

		// calculate inaccuracy and spread.
		const auto weapon_inaccuracy = wpn->get_inaccuracy();
		const auto weapon_spread = info->flspread;

		// calculate angle.
		const auto aim_angle = calc_angle(start, target->position);
		vec3 forward, right, up;
		angle_vectors(aim_angle, forward, right, up);

		// setup calculation parameters.
		vec3 total_spread, spread_angle, end;
		float inaccuracy, spread, total_x, total_y;
		seed* s;

		// are we playing on a no-spread server?
		const auto is_spread = weapon_inaccuracy > 0.f;
		const auto total = is_spread ? total_seeds : 1;

		// setup all traces.
		std::vector<dispatch_queue::fn> calls;
		std::vector<custom_tracing::wall_pen> traces(total);

		if (is_spread)
			for (auto i = 0; i < total; i++)
			{
				// get seed.
				s = &precomputed_seeds[i];

				// calculate spread.
				inaccuracy = s->inaccuracy;
				spread = s->spread;

				// correct spread value for different weapons.
				if (wpn->get_item_definition_index() == item_definition_index::weapon_negev && wpn->get_recoil_index() < 3.f)
					for (auto j = 3; j > static_cast<int32_t>(wpn->get_recoil_index()); j--)
						inaccuracy *= inaccuracy;

				inaccuracy *= weapon_inaccuracy;
				spread *= weapon_spread;
				total_x = (s->cos_inaccuracy * inaccuracy) + (s->cos_spread * spread);
				total_y = (s->sin_inaccuracy * inaccuracy) + (s->sin_spread * spread);
				total_spread = (forward + right * total_x + up * total_y).normalize();

				// calculate angle with spread applied.
				vector_angles(total_spread, spread_angle);

				// calculate end point of trace.
				angle_vectors(spread_angle, end);
				end = start + end.normalize() * info->range;

				// insert call instruction.
				calls.push_back([&traces, &start, end, &target, i]()
				{
					traces[i] = trace.wall_penetration(start, end, target->record);
				});
			}
		else
			traces[0] = trace.wall_penetration(start, target->position, target->record);
		
		// dispatch traces to tracing pool.
		dispatch.evaluate(calls);

		// calculate statistics.
		auto total_hits = 0;
		auto initial_shots_to_kill = target->get_shots_to_kill();
		if (wpn->get_item_definition_index() == item_definition_index::weapon_revolver)
			initial_shots_to_kill++;

		for (const auto& tr : traces)
		{
			if (!tr.did_hit)
				continue;

			const auto old_damage = target->pen.damage;
			target->pen.damage = tr.damage;
			if (target->get_shots_to_kill() <= initial_shots_to_kill)
				total_hits++;
			target->pen.damage = old_damage;
		}

		// return result.
		return min(static_cast<float>(total_hits * info->ibullets) / static_cast<float>(total), 1.f);
	}
	
	float get_lowest_inaccuray()
	{
		const auto wpn = reinterpret_cast<weapon_t*>(game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));
		const auto info = wpn->get_cs_weapon_data();
		
		if (game->local_player->get_flags() & cs_player_t::ducking)
			return wpn->is_scoped_weapon() ? info->flinaccuracycrouchalt : info->flinaccuracycrouch;
		
		return wpn->is_scoped_weapon() ? info->flinaccuracystandalt : info->flinaccuracystand;
	}

	bool has_full_accuracy()
	{
		const auto wpn = reinterpret_cast<weapon_t*>(game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));
		return wpn->get_inaccuracy() == 0.f || fabsf(wpn->get_inaccuracy() - get_lowest_inaccuray()) < .001f;
	}

	bool should_prefer_record(std::optional<rage_target>& target, std::optional<rage_target>& alternative, bool compare_hc)
	{
		static constexpr auto hitchance_epsilon = 2.5f;
		static constexpr auto max_body_volume = 1600.f;

		if (alternative.has_value() && alternative->pen.did_hit && (!target.has_value() || !target->pen.did_hit))
			return true;

		if (!alternative.has_value() || !alternative->pen.did_hit || !target.has_value() || !target->pen.did_hit)
			return false;

		const auto to_kill = alternative->get_shots_to_kill();
		const auto old_to_kill = target->get_shots_to_kill();
		const auto equal_shots = to_kill == old_to_kill || (to_kill > 3 && old_to_kill > 3);
		
		if (to_kill > old_to_kill && !equal_shots)
			return false;

		if (old_to_kill > 2 && floorf(alternative->pen.damage) < floorf(target->pen.damage))
			return false;

		if (target->pen.secure_point != alternative->pen.secure_point && alternative->pen.secure_point && equal_shots)
			return true;

		if (alternative->likelihood_roll < target->likelihood_roll - .05f && old_to_kill <= to_kill)
			return false;

		if (alternative->likelihood < target->likelihood - .05f && old_to_kill <= to_kill)
			return false;

		if (equal_shots && target->pen.secure_point == alternative->pen.secure_point && target->record->server_tick > alternative->record->server_tick)
			return false;
		
		if (target->pen.secure_point && alternative->pen.secure_point)
		{
			const auto alt_damage = alternative->pen.damage;
			const auto damage = target->pen.damage;
			alternative->pen.damage = alternative->pen.min_damage;
			target->pen.damage = target->pen.min_damage;
			const auto higher = alternative->get_shots_to_kill() > target->get_shots_to_kill();
			alternative->pen.damage = alt_damage;
			target->pen.damage = damage;
			if (higher)
				return false;
		}
		
		if (equal_shots)
		{
			if (compare_hc && alternative->hc > target->hc)
				return true;
			
			if (alternative->pen.damage > target->pen.damage + 4.f * health_buffer && old_to_kill > 1)
				return true;

			if (fabsf(alternative->likelihood - target->likelihood) <= .05f && fabsf(alternative->likelihood_roll - target->likelihood_roll) <= .05f)
			{
				const auto target_hdr = target->record->player->get_studio_hdr()->hdr;
				const auto alternative_hdr = alternative->record->player->get_studio_hdr()->hdr;
				const auto alternative_hitbox = alternative_hdr->get_hitbox(static_cast<uint32_t>(alternative->pen.hitbox), 0);
				const auto target_hitbox = target_hdr->get_hitbox(static_cast<uint32_t>(target->pen.hitbox), 0);
				const auto alternative_volume = min(max_body_volume, static_cast<float>(sdk::pi) * alternative_hitbox->radius * alternative_hitbox->radius *
						(4.f / 3.f * alternative_hitbox->radius + (alternative_hitbox->bbmax - alternative_hitbox->bbmin).length()));
				const auto target_volume = min(max_body_volume, static_cast<float>(sdk::pi) * target_hitbox->radius * target_hitbox->radius *
						(4.f / 3.f * target_hitbox->radius + (target_hitbox->bbmax - target_hitbox->bbmin).length()));

				if (alternative_volume < target_volume - 2.f)
					return false;

				if (alternative->pen.hitbox > cs_player_t::hitbox::body)
					return false;

				if (alternative->pen.hitbox == cs_player_t::hitbox::body && target->pen.hitbox == cs_player_t::hitbox::pelvis)
					return false;

				if (alternative->pen.hitbox > cs_player_t::hitbox::body && target->pen.hitbox > cs_player_t::hitbox::body && alternative->pen.hitbox > target->pen.hitbox)
					return false;

				if (fabsf(alternative_volume - target_volume) < 2.f && alternative->dist > target->dist)
					return false;
			}
		}

		return true;
	}

	bool is_attackable()
	{
		if (rag.has_priority_targets())
			return true;

		auto found = false;
		
		game->client_entity_list->for_each_player([&](cs_player_t* const player)
		{
			if (!player->is_valid() || !player->is_alive() || !player->is_enemy())
				return;
				
			const auto& entry = GET_PLAYER_ENTRY(player);

			if (entry.hittable || entry.danger)
				found = true;
		});

		return found;
	}

	bool holds_tick_base_weapon()
	{
		const auto wpn = reinterpret_cast<weapon_t*>(
			game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));

		if (!wpn)
			return false;

		return wpn->get_item_definition_index() != item_definition_index::weapon_taser
			&& wpn->get_item_definition_index() != item_definition_index::weapon_fists
			&& wpn->get_item_definition_index() != item_definition_index::weapon_c4
			&& !wpn->is_grenade()
			&& wpn->get_class_id() != class_id::snowball
			&& wpn->get_weapon_type() != weapontype_knife
			&& wpn->get_weapon_type() != weapontype_unknown;
	}

	void fix_movement(user_cmd* const cmd, angle& wishangle)
	{
		vec3 view_fwd, view_right, view_up, cmd_fwd, cmd_right, cmd_up;
		angle_vectors(wishangle, view_fwd, view_right, view_up);
		angle_vectors(cmd->viewangles, cmd_fwd, cmd_right, cmd_up);

		view_fwd.z = view_right.z = cmd_fwd.z = cmd_right.z = 0.f;
		view_fwd.normalize();
		view_right.normalize();
		cmd_fwd.normalize();
		cmd_right.normalize();

		const auto dir = view_fwd * cmd->forwardmove + view_right * cmd->sidemove;
		const auto denom = cmd_right.y * cmd_fwd.x - cmd_right.x * cmd_fwd.y;

		cmd->sidemove = (cmd_fwd.x * dir.y - cmd_fwd.y * dir.x) / denom;
		cmd->forwardmove = (cmd_right.y * dir.x - cmd_right.x * dir.y) / denom;

		normalize_move(cmd->forwardmove, cmd->sidemove, cmd->upmove);
		wishangle = cmd->viewangles;
	}
	
	void slow_movement(user_cmd* const cmd, float target_speed)
	{
		pred.repredict(&game->input->commands[(cmd->command_number - 1) % input_max]);
		auto data = game->cs_game_movement->setup_move(game->local_player, cmd);
		auto changed_movement = slow_movement(&data, target_speed);
		std::tie(cmd->forwardmove, cmd->sidemove) = std::tie(data.forward_move, data.side_move);
		normalize_move(cmd->forwardmove, cmd->sidemove, cmd->upmove);
		pred.repredict(cmd);

		if (aa.is_active() && !cfg.antiaim.desync.get().test(cfg_t::desync_none) && game->local_player->is_on_ground()
			&& data.velocity.length() <= 1.1f && game->local_player->get_velocity().length() <= 1.f && !changed_movement)
		{
			cmd->forwardmove = (game->client_state->choked_commands % 2 ? -1.01f : 1.01f);
			changed_movement = true;
		}

		if (changed_movement)
		{
			const auto factor = game->local_player->get_duck_amount() * .34f + 1.f - game->local_player->get_duck_amount();
			cmd->forwardmove /= factor;
			cmd->sidemove /= factor;
			normalize_move(cmd->forwardmove, cmd->sidemove, cmd->upmove);
		}
	}

	bool slow_movement(sdk::move_data* const data, float target_speed)
	{
		const auto player = reinterpret_cast<cs_player_t*>(game->client_entity_list->get_client_entity_from_handle(data->player_handle));
		const auto velocity = data->velocity;
		const auto current_speed = velocity.length2d();
		const auto stop_speed = max(GET_CONVAR_FLOAT("sv_stopspeed"), current_speed);
		const auto friction = GET_CONVAR_FLOAT("sv_friction") * player->get_surface_friction();
		const auto applied_stop_speed = stop_speed * friction * game->globals->interval_per_tick;
		const auto friction_speed = max(current_speed - applied_stop_speed, 0.f);
		const auto friction_velocity = friction_speed == current_speed ? velocity :
				(vec3(velocity.x, velocity.y, 0.f) * (friction_speed / current_speed));
		auto move_direction = vec3(data->forward_move, data->side_move, 0.f);
		auto next = *data;

		if (friction_velocity.length2d() > target_speed && friction_speed > friction)
		{
			angle ang;
			vector_angles(friction_velocity * -1.f, ang);
			ang.y = data->view_angles.y - ang.y;

			vec3 forward;
			angle_vectors(ang, forward);
			auto max_speed_x = current_speed - target_speed - forward_bounds * friction * game->globals->interval_per_tick;
			auto max_speed_y = current_speed - target_speed - side_bounds * friction * game->globals->interval_per_tick;
			max_speed_x = std::clamp(max_speed_x < 0.f ? friction_speed : forward_bounds, 0.f, forward_bounds);
			max_speed_y = std::clamp(max_speed_y < 0.f ? friction_speed : side_bounds, 0.f, side_bounds);
			std::tie(data->forward_move, data->side_move) = std::make_pair(forward.x * max_speed_x, forward.y * max_speed_y);
			return true;
		}
		else if (game->cs_game_movement->process_movement(player, &next); move_direction.length2d() > 0.f && friction_speed > friction && next.velocity.length() > target_speed)
		{
			vec3 forward, right, up;
			angle_vectors(data->view_angles, forward, right, up);
			forward.z = right.z = 0.f;

			const auto wish = vec3(forward.x * move_direction.x + right.x * move_direction.y,
					forward.y * move_direction.x + right.y * move_direction.y, 0.f);

			const auto move = move_direction;
			move_direction.x = 0.f;
			move_direction.y = 0.f;

			if (wish.length2d() > 0.f)
			{
				const auto normalized_wish = vec3(wish).normalize();
				const auto magnitude = friction_velocity.dot(normalized_wish) + (target_speed - friction_velocity.length2d());

				if (magnitude > 0.f)
				{
					move_direction = move;
					move_direction.normalize();
					move_direction *= magnitude;
				}
			}

			std::tie(data->forward_move, data->side_move) = std::make_pair(move_direction.x, move_direction.y);
			return true;
		}
		else if (friction_speed < applied_stop_speed && target_speed < applied_stop_speed)
			std::tie(data->forward_move, data->side_move) = std::make_pair(0.f, 0.f);

		return false;
	}

	void build_seed_table()
	{
		if (precomputed_seeds.empty())
			for (auto i = 0; i < total_seeds; i++)
			{
				random_seed(i + 1);

				seed a;
				a.inaccuracy = random_float(0.f, 1.f);
				auto p = random_float(0.f, 2.f * sdk::pi);
				a.sin_inaccuracy = sin(p);
				a.cos_inaccuracy = cos(p);
				a.spread = random_float(0.f, 1.f);
				p = random_float(0.f, 2.f * sdk::pi);
				a.sin_spread = sin(p);
				a.cos_spread = cos(p);

				precomputed_seeds.push_back(a);
			}
	}
	
	inline int32_t rage_target::get_shots_to_kill()
	{
		const auto wpn = reinterpret_cast<weapon_t*>(
			game->client_entity_list->get_client_entity_from_handle(game->local_player->get_active_weapon()));
		const auto health = get_adjusted_health(record->player);
		auto shots = static_cast<int32_t>(ceilf(health / fminf(pen.damage, health + health_buffer)));

		if (tickbase.fast_fire && holds_tick_base_weapon() && cfg.rage.hack.secure_fast_fire.get() && tickbase.limit > 0)
			shots -= static_cast<int32_t>(floorf(TICKS_TO_TIME(tickbase.limit) / (.75f * wpn->get_cs_weapon_data()->cycle_time)));

		return max(1, shots);
	}
	
	bool is_visible(const vec3& pos)
	{
		if (!game->local_player || game->local_player->get_flash_bang_time() > game->globals->curtime)
			return false;
		
		const auto eye = game->local_player->get_eye_position();
		// TODO:
		//static const auto line_goes_through_smoke = PATTERN(uint32_t, "client.dll", "55 8B EC 83 EC 08 8B 15 ? ? ? ? 0F");
		//if (cfg.legit.hack.smokecheck.get() && reinterpret_cast<bool(__cdecl*)(vec3, vec3)>(line_goes_through_smoke())(eye, pos))
		//	return false;

		trace_no_player_filter filter{};
		game_trace t;
		ray r;
		r.init(eye, pos);
		game->engine_trace->trace_ray(r, mask_visible | contents_blocklos, &filter, &t);
		return t.fraction == 1.f;
	}

	std::pair<float, float> perform_freestanding(const vec3& from, const vec3& to, bool* previous)
	{
		// calculate distance and angle to target.
		const auto at_target = calc_angle(from, to);
		const auto dist = (to - from).length();
		
		// directions of local player.
		const auto direction_left = std::remainderf(at_target.y - 90.f, yaw_bounds);
		const auto direction_right = std::remainderf(at_target.y + 90.f, yaw_bounds);
		const auto direction_back = std::remainderf(at_target.y + 180.f, yaw_bounds);

		// calculate scan positions.
		const auto local_left = rotate_2d(from, direction_left, freestand_width);
		const auto local_right = rotate_2d(from, direction_right, freestand_width);
		const auto local_half_left = rotate_2d(from, direction_left, .5f * freestand_width);
		const auto local_half_right = rotate_2d(from, direction_right, .5f * freestand_width);
		const auto enemy_left = rotate_2d(to, direction_left, freestand_width);
		const auto enemy_right = rotate_2d(to, direction_right, freestand_width);

		// setup data for tracing.
		const auto info = *game->weapon_system->get_weapon_data(item_definition_index::weapon_awp);

		// comperator function.
		const auto compare = [&](const vec3& fleft, const vec3& fright, const vec3& l, const vec3& r, const bool check = false, const bool check2 = false) -> float
		{
			auto cinfo = info;
			if (!check && !check2)
				cinfo.idamage = 200;

			const auto res_left = trace.wall_penetration(fleft, l, nullptr, false, std::nullopt, nullptr, false, &cinfo);
			if (check)
				return (res_left.damage > 0 || res_left.impact_count > 0 && (res_left.impacts.front() - fleft).length() > (res_left.impacts.front() - l).length())
					? FLT_MAX : direction_back;
			if (check2)
				return res_left.damage > 0 ? FLT_MAX : direction_back;

			const auto res_right = trace.wall_penetration(fright, r, nullptr, false, std::nullopt, nullptr, false, &cinfo);
			if (!res_right.damage && res_left.damage)
				return direction_right;
			if (res_right.damage && !res_left.damage)
				return direction_left;
			return direction_back;
		};

		auto real = direction_back;

		if (const auto r1 = compare(local_left, local_right, enemy_left, enemy_right); r1 != direction_back
			&& compare(from, from, r1 == direction_left ? enemy_left : enemy_right, enemy_right, false, true) == direction_back)
			real = r1;
		else if (const auto r2 = compare(local_left, local_right, enemy_right, enemy_left); r2 != direction_back
			&& compare(from, from, r2 == direction_left ? enemy_left : enemy_right, enemy_right, false, true) == direction_back)
			real = r2;

		if (real != direction_back && !previous && compare(real == direction_left ? local_half_left : local_half_right, to, to, to, true) != direction_back)
			real = direction_back;

		if (previous)
		{
			if (real == direction_back)
				real = *previous ? direction_left : direction_right;
			*previous = real == direction_left;
		}

		auto fake = direction_right;

		if (const auto r1 = compare(local_left, local_right, to, to); r1 != direction_back)
			fake = r1;
		else if (const auto r2 = compare(local_half_left, local_half_right, to, to); r2 != direction_back)
			fake = r2;

		if (fake != direction_back && !previous && compare(fake == direction_left ? local_half_left : local_half_right, to, to, to, true) != direction_back)
			fake = direction_right;
		
		return std::make_pair(real, fake);
	}
}
}
