#include <lua/api_def.h>
#include <sdk/game_event_manager.h>

int lua::api_def::event::get_name(lua_State *l)
{
	runtime_state s(l);
	
	if (!s.check_arguments({ { ltd::user_data } }))
	{
		s.error(XOR_STR("usage: obj:get_name()"));
		return 0;
	}
	
	const auto ev = *reinterpret_cast<sdk::game_event**>(s.get_user_data(1));
	s.push(ev->get_name());
	return 1;
}

int lua::api_def::event::get_bool(lua_State *l)
{
	runtime_state s(l);
	
	if (!s.check_arguments({ { ltd::user_data }, { ltd::string } }))
	{
		s.error(XOR_STR("usage: obj:get_bool(key_name)"));
		return 0;
	}
	
	const auto ev = *reinterpret_cast<sdk::game_event**>(s.get_user_data(1));
	s.push(ev->get_bool(s.get_string(2)));
	return 1;
}

int lua::api_def::event::get_int(lua_State *l)
{
	runtime_state s(l);
	
	if (!s.check_arguments({ { ltd::user_data }, { ltd::string } }))
	{
		s.error(XOR_STR("usage: obj:get_int(key_name)"));
		return 0;
	}
	
	const auto ev = *reinterpret_cast<sdk::game_event**>(s.get_user_data(1));
	s.push(ev->get_int(s.get_string(2)));
	return 1;
}

int lua::api_def::event::get_float(lua_State *l)
{
	runtime_state s(l);
	
	if (!s.check_arguments({ { ltd::user_data }, { ltd::string } }))
	{
		s.error(XOR_STR("usage: obj:get_float(key_name)"));
		return 0;
	}
	
	const auto ev = *reinterpret_cast<sdk::game_event**>(s.get_user_data(1));
	s.push(ev->get_float(s.get_string(2)));
	return 1;
}

int lua::api_def::event::get_string(lua_State *l)
{
	runtime_state s(l);
	
	if (!s.check_arguments({ { ltd::user_data }, { ltd::string } }))
	{
		s.error(XOR_STR("usage: obj:get_string(key_name)"));
		return 0;
	}
	
	const auto ev = *reinterpret_cast<sdk::game_event**>(s.get_user_data(1));
	s.push(ev->get_string(s.get_string(2)));
	return 1;
}