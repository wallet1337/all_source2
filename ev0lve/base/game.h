
#ifndef BASE_GAME_H
#define BASE_GAME_H

#include <util/value_obfuscation.h>

namespace sdk
{
	struct engine_client_t;
	struct hl_client_t;
	struct client_entity_list_t;
	struct cvar_t;
	struct cs_game_movement_t;
	struct mdl_cache_t;
	struct localize_t;
	struct prediction_t;
	struct engine_trace_t;
	struct surface_props_t;
	struct model_info_client_t;
	struct debug_overlay_t;
	struct render_view_t;
	struct mat_surface_t;
	struct model_render_t;
	struct material_system_t;
	struct string_table_t;
	struct hardware_config_t;
	struct input_system_t;
	struct game_event_manager_t;

	struct global_vars_base_t;
	struct mem_alloc_t;
	struct cs_player_resource_t;
	struct client_state_t;
	struct input_t;
	struct weapon_system_t;
	struct cs_game_rules_t;
	struct client_mode_t;
	struct view_render_t;
	struct glow_manager_t;
	struct leaf_system_t;
	struct key_values_system_t;
	struct modifier_table_t;
	struct model_loader_t;

	struct cs_player_t;
};

struct game_t
{
	NO_COPY_OR_MOVE(game_t);

	game_t(uintptr_t base, uint32_t reserved);

	void init();
	void deinit();

	/* modules */
	util::encrypted_ptr<uintptr_t> client, engine, server, gameoverlayrenderer, vguimatsurface,
		inputsystem, tier0, vstdlib, datacache, local, vphysics, panorama, mat, shaderapidx9;

	/* interfaces */
	util::encrypted_ptr<sdk::engine_client_t> engine_client;
	util::encrypted_ptr<sdk::hl_client_t> hl_client;
	util::encrypted_ptr<sdk::client_entity_list_t> client_entity_list;
	util::encrypted_ptr<sdk::cvar_t> cvar;
	util::encrypted_ptr<sdk::cs_game_movement_t> cs_game_movement;
	util::encrypted_ptr<sdk::mdl_cache_t> mdl_cache;
	util::encrypted_ptr<sdk::localize_t> localize;
	util::encrypted_ptr<sdk::prediction_t> prediction;
	util::encrypted_ptr<sdk::engine_trace_t> engine_trace;
	util::encrypted_ptr<sdk::surface_props_t> surface_props;
	util::encrypted_ptr<sdk::model_info_client_t> model_info_client;
	util::encrypted_ptr<sdk::debug_overlay_t> debug_overlay;
	util::encrypted_ptr<sdk::render_view_t> render_view;
	util::encrypted_ptr<sdk::mat_surface_t> surface;
	util::encrypted_ptr<sdk::model_render_t> model_render;
	util::encrypted_ptr<sdk::material_system_t> material_system;
	util::encrypted_ptr<sdk::string_table_t> string_table;
	util::encrypted_ptr<sdk::hardware_config_t> hardware_config;
	util::encrypted_ptr<sdk::input_system_t> input_system;
	util::encrypted_ptr<sdk::game_event_manager_t> game_event_manager;

	/* globals of the game */
	util::encrypted_ptr<sdk::global_vars_base_t> globals;
	util::encrypted_ptr<sdk::mem_alloc_t> mem_alloc;
	util::encrypted_ptr<sdk::cs_player_resource_t> cs_player_resource;
	util::encrypted_ptr<sdk::client_state_t> client_state;
	util::encrypted_ptr<sdk::input_t> input;
	util::encrypted_ptr<sdk::weapon_system_t> weapon_system;
	util::encrypted_ptr<sdk::cs_game_rules_t> rules;
	util::encrypted_ptr<sdk::client_mode_t> client_mode;
	util::encrypted_ptr<sdk::view_render_t> view_render;
	util::encrypted_ptr<sdk::glow_manager_t> glow_manager;
	util::encrypted_ptr<sdk::leaf_system_t> leaf_system;
	util::encrypted_ptr<sdk::key_values_system_t> key_values_system;
	util::encrypted_ptr<sdk::modifier_table_t> modifier_table;
	util::encrypted_ptr<sdk::model_loader_t> model_loader;

	/* globals of the cheat */
	sdk::cs_player_t* local_player = nullptr;
	bool cl_lagcompensation, cl_predict, on_map_change;
	std::array<float, 64> nade_throw_times{};
	std::vector<uint32_t> cmds{};

	/* base of the cheat */
	uintptr_t base;

	/* debug flag */
#ifndef NDEBUG
	bool debug{};
#endif
};

extern std::unique_ptr<game_t> game;

#endif // BASE_GAME_H
