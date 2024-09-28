#include <lua/state_bus.h>
#include <base/event_log.h>
#include <lua/engine.h>

std::optional<lua::bus_named_object> lua::state_bus::lookup_object(const char* n)
{
	if (std::find(existing_objects.begin(), existing_objects.end(), util::fnv1a(n)) != existing_objects.end())
		return bus_named_object(weak_from_this(), n);
	
	std::lock_guard _lock(invoke_mutex);
	
	const auto l = target.get_state();
	lua_rawgeti(l, LUA_REGISTRYINDEX, table_ref);
	lua_getfield(l, -1, n);

	if (lua_isnil(l, -1))
	{
		lua_pop(l, 2);
		return std::nullopt;
	}

	lua_pop(l, 2);

	existing_objects.emplace_back(util::fnv1a(n));
	return bus_named_object(weak_from_this(), n);
}

void lua::state_bus::clear_stack()
{
	target.pop(target.get_stack_top());
}

lua_State* lua::state_bus::get_state()
{
	return target.get_state();
}

int lua::bus_named_object::invoke(runtime_state &s)
{
	const auto b = bus.lock();
	if (!b)
	{
		s.error(XOR_STR("unable to invoke cross state function"));
		return 0;
	}
	
	std::lock_guard _lock(b->invoke_mutex);
	
	const auto l = b->get_state();
	const auto t = s.get_state();
	
	// get the library table
	lua_rawgeti(l, LUA_REGISTRYINDEX, b->table_ref);
	lua_getfield(l, -1, name.c_str());
	if (lua_isnil(l, -1))
	{
		lua_pop(l, 2);
		s.error(XOR_STR("attempt to call nil"));
		return 0;
	}
	
	lua_remove(l, -2);
	return perform_call(t, l);
}

int lua::bus_ref_object::invoke(lua::runtime_state &s)
{
	const auto b = bus.lock();
	if (!b)
	{
		s.error(XOR_STR("unable to invoke cross state function"));
		return 0;
	}

	const auto l = b->get_state();
	const auto t = s.get_state();
	
	lua_rawgeti(l, LUA_REGISTRYINDEX, ref);
	if (lua_isnil(l, -1))
	{
		lua_pop(l, 1);
		s.error(XOR_STR("attempt to call nil"));
		return 0;
	}
	
	return perform_call(t, l);
}

void lua::copy_stack(lua_State* from, lua_State* to, int top, int bottom)
{
#ifdef _DEBUG
	runtime_state from_s(from);
	runtime_state to_s(to);
#endif
	
	for (auto i = bottom + 1; i <= top; ++i)
	{
		switch (static_cast<ltd>(lua_type(from, i)))
		{
			case ltd::string:
				lua_pushstring(to, lua_tostring(from, i));
				break;
			case lua_type_def::nil:
				lua_pushnil(to);
				break;
			case lua_type_def::boolean:
				lua_pushboolean(to, lua_toboolean(from, i));
				break;
			case lua_type_def::light_user_data:
				lua_pushlightuserdata(to, lua_touserdata(from, i));
				break;
			case lua_type_def::number:
				lua_pushnumber(to, lua_tonumber(from, i));
				break;
			case lua_type_def::table:
#ifdef _DEBUG
				from_s.print_stack_dump();
				to_s.print_stack_dump();
#endif
				
				lua_pushstring(to, XOR_STR("table arguments are not yet implemented"));
				lua_error(to);
				return;
			case lua_type_def::function:
#ifdef _DEBUG
				from_s.print_stack_dump();
				to_s.print_stack_dump();
#endif
				
				lua_pushstring(to, XOR_STR("function arguments are not yet implemented"));
				lua_error(to);
				break;
			case lua_type_def::user_data:
				lua_pushlightuserdata(to, lua_touserdata(from, i));
				break;
			default: break;
		}
	}
}

int lua::perform_call(lua_State* caller, lua_State* callee)
{
	// copy arguments from caller to callees stack
	copy_stack(caller, callee, lua_gettop(caller), 1);

	// call and handle error
	const auto result = lua_pcall(callee, lua_gettop(caller) - 1, LUA_MULTRET, 0);
	if (result != LUA_OK)
	{
		eventlog.add(0x02, lua_tostring(callee, -1));
		eventlog.force_output();
		
		lua_pop(callee, 1);
		
		// now stop calling script
		const auto s = api.find_by_state(caller);
		if (s)
		{
			s->unload();
			s->is_running = false;
		}
		
		return 0;
	}

	// copy stack back to caller
	const auto callee_post = lua_gettop(callee);
	if (callee_post > 0)
	{
		copy_stack(callee, caller, callee_post, 0);
		lua_pop(callee, callee_post);
	}
	
	// return resulting number
	return callee_post;
}