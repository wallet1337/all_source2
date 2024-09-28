
#include <base/hook_manager.h>
#include <base/game.h>
#include <sdk/game_event_manager.h>
#include <detail/events.h>

using namespace sdk;

void hook_manager_t::init()
{
	using namespace sdk::functions;
	using namespace sdk::offsets;

	/* steam */
	create_hook(present, game->gameoverlayrenderer.at(gameoverlayrenderer::present), &hooks::steam::present);
	create_hook(reset, game->gameoverlayrenderer.at(gameoverlayrenderer::reset), &hooks::steam::reset);

	/* mat_system_surface */
	create_hook(lock_cursor, game->vguimatsurface.at(mat_system_surface::lock_cursor), &hooks::mat_system_surface::lock_cursor);

	/* inputsystem */
	create_hook(wnd_proc, game->inputsystem.at(inputsystem::wnd_proc), &hooks::inputsystem::wnd_proc);

	/* hl_client */
	create_hook(frame_stage_notify, game->client.at(hl_client::frame_stage_notify), &hooks::hl_client::frame_stage_notify);
	create_hook(create_move, game->client.at(hl_client::create_move), &hooks::hl_client::create_move);
	create_hook(level_init_pre_entity, game->client.at(hl_client::level_init_pre_entity), &hooks::hl_client::level_init_pre_entity);

	/* engine_client */
	create_hook(get_aspect_ratio, game->engine.at(engine_client::get_aspect_ratio), &hooks::engine_client::get_aspect_ratio);
	create_hook(cl_move, game->engine.at(engine_client::cl_move), &hooks::engine_client::cl_move);

	/* prediction */
	create_hook(run_command, game->client.at(prediction::run_command), &hooks::prediction::run_command);
	create_hook(perform_prediction, game->client.at(prediction::perform_prediction), &hooks::prediction::perform_prediction);

	/* game_movement */
	create_hook(process_movement, game->client.at(cs_game_movement::process_movement), &hooks::game_movement::process_movement);

	/* client_state */
	create_hook(packet_start, game->engine.at(client_state::packet_start), &hooks::client_state::packet_start);
	create_hook(send_netmsg, game->engine.at(client_state::send_netmsg), &hooks::client_state::send_netmsg);

	/* client_mode */
	create_hook(override_view, game->client.at(client_mode::override_view), &hooks::client_mode::override_view);
	create_hook(do_post_screen_space_effects, game->client.at(client_mode::do_post_screen_space_effects), &hooks::client_mode::do_post_screen_space_effects);

	/* leaf_system */
	create_hook(calc_renderable_world_space_aabb_bloated, game->client.at(leaf_system::calc_renderable_world_space_aabb_bloated), &hooks::leaf_system::calc_renderable_world_space_aabb_bloated);
	create_hook(add_renderables_to_render_list, game->client.at(leaf_system::add_renderables_to_render_list), &hooks::leaf_system::add_renderables_to_render_list);
	create_hook(extract_occluded_renderables, game->client.at(leaf_system::extract_occluded_renderables), &hooks::leaf_system::extract_occluded_renderables);

	/* model_render */
	create_hook(draw_model_execute, game->engine.at(model_render::draw_model_execute), &hooks::model_render::draw_model_execute);

	/* entity */
	create_hook(on_latch_interpolated_variables, game->client.at(base_entity::fn_on_latch_interpolated_variables), &hooks::entity::on_latch_interpolated_variables);
	create_hook(do_animation_events, game->client.at(base_animating::fn_do_animation_events), &hooks::entity::do_animation_events);
	create_hook(maintain_sequence_transitions, game->client.at(base_animating::fn_maintain_sequence_transitions), &hooks::entity::maintain_sequence_transitions);
	create_hook(set_abs_angles, game->client.at(base_entity::fn_set_abs_angles), &hooks::entity::set_abs_angles);
	create_hook(estimate_abs_velocity, game->client.at(base_entity::fn_estimate_abs_velocity), &hooks::entity::estimate_abs_velocity);
	create_hook(setup_bones, game->client.at(base_animating::fn_setup_bones), &hooks::entity::setup_bones);

	/* cs_player */
	create_hook(physics_simulate, game->client.at(base_player::fn_physics_simulate), &hooks::cs_player::physics_simulate);
	create_hook(post_data_update, game->client.at(csplayer::fn_post_data_update), &hooks::cs_player::post_data_update);
	create_hook(do_animation_events_overlay, game->client.at(base_animating_overlay::fn_do_animation_events), &hooks::cs_player::do_animation_events);
	create_hook(calc_view, game->client.at(csplayer::fn_calc_view), &hooks::cs_player::calc_view);
	create_hook(fire_event, game->client.at(csplayer::fn_fire_event), &hooks::cs_player::fire_event);
	create_hook(obb_change_callback, game->client.at(csplayer::fn_obb_change_callback), &hooks::cs_player::obb_change_callback);
	create_hook(reevauluate_anim_lod, game->client.at(csplayer::fn_reevauluate_anim_lod), &hooks::cs_player::reevauluate_anim_lod);
	create_hook(get_fov, game->client.at(csplayer::fn_get_fov), &hooks::cs_player::get_fov);
	create_hook(get_default_fov, game->client.at(csplayer::fn_get_default_fov), &hooks::cs_player::get_default_fov);
	create_hook(update_clientside_animation, game->client.at(csplayer::fn_update_clientside_animation), &hooks::cs_player::update_clientside_animation);

	/* weapon */
	create_hook(send_weapon_anim, game->client.at(base_combat_weapon::fn_send_weapon_anim), &hooks::weapon::send_weapon_anim);
	create_hook(deploy, game->client.at(weapon_csbase::fn_deploy), &hooks::weapon::deploy);
	create_hook(get_weapon_type, game->client.at(weapon_csbase::fn_get_weapon_type), &hooks::weapon::get_weapon_type);

	/* cs_grenade_projectile */
	create_hook(client_think, game->client.at(base_csgrenade_projectile::fn_client_think), &hooks::cs_grenade_projectile::client_think);
	create_hook(projectile_post_data_update, game->client.at(base_csgrenade_projectile::fn_post_data_update), &hooks::cs_grenade_projectile::post_data_update);

	/* trace_filter_for_player_head_collision */
	create_hook(should_hit_entity, game->client.at(trace_filter_for_player_head_collision::should_hit_entity), &hooks::trace_filter_for_player_head_collision::should_hit_entity);

	/* recv_proxies */
	create_hook(layer_sequence, reinterpret_cast<recv_prop*>(game->client.at(props::animationlayer::sequence))->proxy, &hooks::recv_proxies::layer_sequence);
	create_hook(layer_cycle, reinterpret_cast<recv_prop*>(game->client.at(props::animationlayer::cycle))->proxy, &hooks::recv_proxies::layer_cycle);
	create_hook(layer_playback_rate, reinterpret_cast<recv_prop*>(game->client.at(props::animationlayer::playback_rate))->proxy, &hooks::recv_proxies::layer_playback_rate);
	create_hook(layer_weight, reinterpret_cast<recv_prop*>(game->client.at(props::animationlayer::weight))->proxy, &hooks::recv_proxies::layer_weight);
	create_hook(layer_weight_delta_rate, reinterpret_cast<recv_prop*>(game->client.at(props::animationlayer::weight_delta_rate))->proxy, &hooks::recv_proxies::layer_weight_delta_rate);
	create_hook(layer_order, reinterpret_cast<recv_prop*>(game->client.at(props::animationlayer::order))->proxy, &hooks::recv_proxies::layer_order);
	create_hook(general, reinterpret_cast<recv_prop*>(game->client.at(props::animationlayer::prev_cycle))->proxy, &hooks::recv_proxies::general);
	create_hook(general_int, reinterpret_cast<recv_prop*>(game->client.at(props::base_entity::model_index))->proxy, &hooks::recv_proxies::general_int);
	create_hook(general_vec, reinterpret_cast<recv_prop*>(game->client.at(props::local::aim_punch_angle))->proxy, &hooks::recv_proxies::general_vec);
	create_hook(viewmodel_sequence, reinterpret_cast<recv_prop*>(game->client.at(props::base_view_model::sequence))->proxy, &hooks::recv_proxies::viewmodel_sequence);
	create_hook(weapon_handle, reinterpret_cast<recv_prop*>(game->client.at(props::base_view_model::weapon))->proxy, &hooks::recv_proxies::weapon_handle);
	create_hook(simulation_time, reinterpret_cast<recv_prop*>(game->client.at(props::base_entity::simulation_time))->proxy, &hooks::recv_proxies::simulation_time);
	create_hook(effects, reinterpret_cast<recv_prop*>(game->client.at(props::base_weapon_world_model::effects))->proxy, &hooks::recv_proxies::effects);

	/* miscellaneous */
	create_hook(get_module_handle_ex_a, FIND_EXPORT("kernel32.dll", "GetModuleHandleExA"), &hooks::miscellaneous::get_module_handle_ex_a);
	create_hook(host_shutdown, game->engine.at(functions::host_shutdown), &hooks::miscellaneous::host_shutdown);
	create_hook(hud_draw_scope, game->client.at(functions::hud_draw_scope), &hooks::miscellaneous::hud_draw_scope);
	create_hook(ent_info_list_link_before, game->client.at(functions::ent_info_list_link_before), &hooks::miscellaneous::ent_info_list_link_before);
	create_hook(process_input, game->client.at(functions::process_input), &hooks::miscellaneous::process_input);
	create_hook(start_sound_immediate, game->engine.at(functions::start_sound_immediate), &hooks::miscellaneous::start_sound_immediate);
	create_hook(check_param, game->tier0.at(functions::check_param), &hooks::miscellaneous::check_param);
	create_hook(move_helper_start_sound, game->client.at(functions::move_helper_start_sound), &hooks::miscellaneous::move_helper_start_sound);
	create_hook(get_tonemap_settings_from_env_tonemap_controller, game->client.at(functions::get_tonemap_settings_from_env_tonemap_controller), &hooks::miscellaneous::get_tonemap_settings_from_env_tonemap_controller);
	create_hook(draw_mesh, game->shaderapidx9.at(functions::draw_mesh), &hooks::miscellaneous::draw_mesh);
	create_hook(draw_3d_debug_overlays, game->engine.at(functions::draw_3d_debug_overlays), &hooks::miscellaneous::draw_3d_debug_overlays);
	create_hook(render_glow_boxes, game->client.at(functions::render_glow_boxes), &hooks::miscellaneous::render_glow_boxes);
	create_hook(spawn_smoke_effect, game->client.at(functions::spawn_smoke_effect), &hooks::miscellaneous::spawn_smoke_effect);
	create_hook(render_iron_sight_scope_effect, game->client.at(functions::render_iron_sight_scope_effect), &hooks::miscellaneous::render_iron_sight_scope_effect);
	create_hook(convar_network_change_callback, game->engine.at(functions::convar_network_change_callback), &hooks::miscellaneous::convar_network_change_callback);
	create_hook(tefirebullets_post_data_update, game->client.at(functions::tefirebullets_post_data_update), &hooks::miscellaneous::tefirebullets_post_data_update);
	create_hook(nt_query_virtual_memory, FIND_EXPORT("ntdll.dll", "NtQueryVirtualMemory"), &hooks::miscellaneous::nt_query_virtual_memory);
}

void hook_manager_t::attach()
{
	get_module_handle_ex_a->attach();
	send_netmsg->attach();

	for (auto& hook : hooks)
		if (hook != get_module_handle_ex_a && hook != send_netmsg)
			hook->attach();

	game->game_event_manager->add_listener_global(&detail::evnt, false);
}

void hook_manager_t::detach()
{
	for (auto& hook : hooks)
		if (hook != get_module_handle_ex_a && hook != send_netmsg)
			hook->detach();

	game->game_event_manager->remove_listener(&detail::evnt);

	send_netmsg->detach();
	get_module_handle_ex_a->detach();
}

hook_manager_t hook_manager;
