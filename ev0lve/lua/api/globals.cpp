#include <lua/api_def.h>
#include <base/debug_overlay.h>
#include <hacks/tickbase.h>
#include <gui/gui.h>
#include <detail/player_list.h>
#include <lua/engine.h>
#include <lua/helpers.h>
#include <sdk/global_vars_base.h>

using namespace hacks;
using namespace detail;

int lua::api_def::panic(lua_State *l)
{
	runtime_state s(l);

	MessageBoxA(nullptr, XOR_STR("Critical error"), s.get_string(-1), MB_OK | MB_ICONERROR);
	return 0;
}

int lua::api_def::print(lua_State *l)
{
	runtime_state s(l);
	if (!s.get_stack_top())
	{
		s.error(XOR_STR("print() requires at least one argument"));
		return 0;
	}

	std::string output{};

	auto n = 1;
	do
	{
		if (s.is_boolean(n))
			output += s.get_boolean(n) ? XOR_STR("true") : XOR_STR("false");
		else if (s.is_number(n))
			output += std::to_string(s.get_number(n));
		else if (s.is_string(n))
			output += s.get_string(n);
	}
	while (++n <= s.get_stack_top());

	lua::helpers::print(output.c_str());
	return 0;
}

int lua::api_def::require(lua_State* l)
{
	runtime_state s(l);
	
	const auto r = s.check_arguments({
	{ ltd::string }
	});
	
	if (!r)
	{
		s.error(XOR_STR("usage: require(lib)"));
		return 0;
	}
	
	const auto me = api.find_by_state(l);
	if (!me)
	{
		s.error(XOR_STR("sandbox problem"));
		return 0;
	}
	
	// attempt loading lib
	script_file file{st_library, s.get_string(1)};
	file.parse_metadata();
	
	if (file.metadata.use_state)
	{
		if (!api.run_library(file))
			return 0;
		
		me->use_library(file.make_id());
	} else
	{
		s.load_file(file.get_file_path().c_str());
		if (!s.call(0, 1))
			s.error(s.get_last_error_description());
	}
	
	return 1;
}

int lua::api_def::stub_new_index(lua_State *l)
{
	runtime_state s(l);
	s.error(XOR_STR("access violation: overriding fields is forbidden"));

	return 0;
}

int lua::api_def::stub_index(lua_State *l)
{
	runtime_state s(l);
	return 0;
}

int lua::api_def::server_index(lua_State* l)
{
	if (!game->engine_client->is_ingame())
		return 0;

	runtime_state s(l);

	const auto key = util::fnv1a(s.get_as_string(2).c_str());
	switch (key)
	{
		case FNV1A("map_name"):
			s.push(game->client_state->level_name);
			return 1;
		case FNV1A("address"):
			s.push(game->client_state->net_channel->get_address());
			return 1;
		case FNV1A("max_players"):
			s.push(game->globals->max_clients);
			return 1;
		default: return 0;
	}
}

int lua::api_def::ev0lve_index(lua_State *l)
{
	runtime_state s(l);

	const auto key = util::fnv1a(s.get_as_string(2).c_str());
	switch (key)
	{
		case FNV1A("username"):
			s.push(evo::gui::ctx->user.username.c_str());
			return 1;
		case FNV1A("lag_ticks"):
			s.push(game->client_state ? game->client_state->choked_commands : 0);
			return 1;
		case FNV1A("can_fastfire"):
			s.push(cfg.rage.enable_fast_fire && tickbase.limit > 0);
			return 1;
		case FNV1A("in_fakeduck"):
			s.push(cfg.antiaim.fake_duck);
			return 1;
		case FNV1A("in_slowwalk"):
			s.push(cfg.antiaim.slow_walk);
			return 1;
		case FNV1A("desync"):
			s.push(std::clamp(local_record.yaw_modifier, 0.f, 1.f));
			return 1;
		default: return 0;
	}
}

int lua::api_def::global_vars_index(lua_State *l)
{
	runtime_state s(l);
	
	const auto key = util::fnv1a(s.get_as_string(2).c_str());
	switch (key)
	{
		case FNV1A("realtime"):
			s.push(game->globals->realtime);
			return 1;
		case FNV1A("framecount"):
			s.push(game->globals->framecount);
			return 1;
		case FNV1A("curtime"):
			s.push(game->globals->curtime);
			return 1;
		case FNV1A("frametime"):
			s.push(game->globals->frametime);
			return 1;
		case FNV1A("tickcount"):
			s.push(game->globals->tickcount);
			return 1;
		case FNV1A("interval_per_tick"):
			s.push(game->globals->interval_per_tick);
			return 1;
		default: return 0;
	}
}

int lua::api_def::game_rules_index(lua_State *l)
{
	runtime_state s(l);
	
	if (!game->rules->data)
		return 0;
	
	const auto key = util::fnv1a(s.get_as_string(2).c_str());
	switch (key)
	{
		case FNV1A("is_valve_server"):
			s.push(game->rules->data->get_is_valve_ds());
			return 1;
		case FNV1A("is_freeze_period"):
			s.push(game->rules->data->get_freeze_period());
			return 1;
		default: return 0;
	}
}

int lua::api_def::bus::index(lua_State* l)
{
	runtime_state s(l);
	
	const auto bus_weak = *reinterpret_cast<std::weak_ptr<state_bus>*>(s.get_user_data(1));
	const auto bus = bus_weak.lock();
	if (!bus)
	{
		s.error(XOR_STR("unable to lookup object"));
		return 0;
	}
	
	auto obj = bus->lookup_object(s.get_string(2));
	if (!obj)
	{
		s.error(XOR_STR("object does not exist"));
		return 0;
	}
	
	s.create_user_object(XOR_STR("bus_object"), &obj);
	return 1;
}

int lua::api_def::bus::invoke(lua_State* l)
{
	runtime_state s(l);
	
	auto obj = reinterpret_cast<bus_object*>(s.get_user_data(1));
	switch (obj->type)
	{
		case bus_object::bus_named:
			return obj->as<bus_named_object>()->invoke(s);
		case bus_object::bus_ref:
			return obj->as<bus_ref_object>()->invoke(s);
		default:
			return 0;
	}
}