#include <lua/api_def.h>
#include <util/fnv1a.h>

using namespace lua;
using namespace sdk;

namespace lua::api_def::math
{
	int vec3_new(lua_State* l)
	{
		runtime_state s(l);

		if (s.get_stack_top() > 3)
		{
			s.error(XOR_STR("usage: vec3(x?, y?, z?)"));
			return 0;
		}

		vec3 vec {
			s.get_stack_top() >= 1 ? s.get_float(1) : 0.f,
			s.get_stack_top() >= 2 ? s.get_float(2) : 0.f,
			s.get_stack_top() >= 3 ? s.get_float(3) : 0.f,
		};

		s.create_user_object(XOR_STR("vec3"), &vec);
		return 1;
	}

	int vec3_length(lua_State* l)
	{
		runtime_state s(l);
		const auto r = s.check_arguments({{ltd::user_data}});
		if (!r)
		{
			s.error(XOR_STR("usage: obj:length()"));
			return 0;
		}

		s.push(s.user_data<vec3>(1).length());
		return 1;
	}

	int vec3_length2d(lua_State* l)
	{
		runtime_state s(l);
		const auto r = s.check_arguments({{ltd::user_data}});
		if (!r)
		{
			s.error(XOR_STR("usage: obj:length2d()"));
			return 0;
		}

		s.push(s.user_data<vec3>(1).length2d());
		return 1;
	}

	int vec3_dot(lua_State* l)
	{
		runtime_state s(l);
		const auto r = s.check_arguments({{ltd::user_data}, {ltd::user_data}});
		if (!r)
		{
			s.error(XOR_STR("usage: obj:dot(vec3 other)"));
			return 0;
		}

		s.push(s.user_data<vec3>(1).dot(s.user_data<vec3>(2)));
		return 1;
	}

	int vec3_cross(lua_State* l)
	{
		runtime_state s(l);
		const auto r = s.check_arguments({{ltd::user_data}, {ltd::user_data}});
		if (!r)
		{
			s.error(XOR_STR("usage: obj:cross(vec3 other)"));
			return 0;
		}

		auto vec = s.user_data<vec3>(1).cross(s.user_data<vec3>(2));
		s.create_user_object(XOR_STR("vec3"), &vec);
		return 1;
	}

	int vec3_normalize(lua_State* l)
	{
		runtime_state s(l);
		const auto r = s.check_arguments({{ltd::user_data}});
		if (!r)
		{
			s.error(XOR_STR("usage: obj:normalize()"));
			return 0;
		}

		auto &vec = s.user_data<vec3>(1);
		vec.normalize();

		s.create_user_object(XOR_STR("vec3"), &vec);
		return 1;
	}

	int vec3_add(lua_State* l)
	{
		runtime_state s(l);
		const auto r = s.check_arguments({{ltd::user_data}, {ltd::user_data}});
		if (!r)
		{
			s.error(XOR_STR("usage: obj:add(vec3 other)"));
			return 0;
		}

		auto &vec = s.user_data<vec3>(1);
		vec += s.user_data<vec3>(2);

		s.create_user_object(XOR_STR("vec3"), &vec);
		return 1;
	}

	int vec3_sub(lua_State* l)
	{
		runtime_state s(l);
		const auto r = s.check_arguments({{ltd::user_data}, {ltd::user_data}});
		if (!r)
		{
			s.error(XOR_STR("usage: obj:sub(vec3 other)"));
			return 0;
		}

		auto &vec = s.user_data<vec3>(1);
		vec -= s.user_data<vec3>(2);

		s.create_user_object(XOR_STR("vec3"), &vec);
		return 1;
	}

	int vec3_mul(lua_State* l)
	{
		runtime_state s(l);
		const auto r = s.check_arguments({{ltd::user_data}, {ltd::user_data}});
		if (!r)
		{
			s.error(XOR_STR("usage: obj:mul(vec3 other)"));
			return 0;
		}

		auto &vec = s.user_data<vec3>(1);
		vec *= s.user_data<vec3>(2);

		s.create_user_object(XOR_STR("vec3"), &vec);
		return 1;
	}

	int vec3_div(lua_State* l)
	{
		runtime_state s(l);
		const auto r = s.check_arguments({{ltd::user_data}, {ltd::user_data}});
		if (!r)
		{
			s.error(XOR_STR("usage: obj:div(vec3 other)"));
			return 0;
		}

		auto &vec = s.user_data<vec3>(1);
		vec /= s.user_data<vec3>(2);

		s.create_user_object(XOR_STR("vec3"), &vec);
		return 1;
	}

	int vec3_index(lua_State* l)
	{
		runtime_state s(l);
		if (!s.check_arguments({{ltd::user_data}, {ltd::string}}))
		{
			s.error(XOR_STR("wtf are you doing"));
			return 0;
		}

		const auto vec = s.user_data<vec3>(1);
		switch (util::fnv1a(s.get_string(2)))
		{
			case FNV1A("x"):
				s.push(vec.x);
				return 1;
			case FNV1A("y"):
				s.push(vec.y);
				return 1;
			case FNV1A("z"):
				s.push(vec.z);
				return 1;
			case FNV1A("length"):
				s.push(vec3_length);
				return 1;
			case FNV1A("length2d"):
				s.push(vec3_length2d);
				return 1;
			case FNV1A("dot"):
				s.push(vec3_dot);
				return 1;
			case FNV1A("cross"):
				s.push(vec3_cross);
				return 1;
			case FNV1A("normalize"):
				s.push(vec3_normalize);
				return 1;
			default:
				return 0;
		}
	}

	int vec3_new_index(lua_State* l)
	{
		runtime_state s(l);
		if (!s.check_arguments({{ltd::user_data}, {ltd::string}, {ltd::number}}))
		{
			s.error(XOR_STR("second operand should be a number"));
			return 0;
		}

		auto& vec = s.user_data<vec3>(1);
		switch (util::fnv1a(s.get_string(2)))
		{
			case FNV1A("x"):
				vec.x = s.get_float(3); break;
			case FNV1A("y"):
				vec.y = s.get_float(3); break;
			case FNV1A("z"):
				vec.z = s.get_float(3); break;
		}

		return 0;
	}

//	int vec3_add(lua_State* l)
//	{
//		runtime_state s(l);
//
//	}
//
//	int vec3_sub(lua_State* l)
//	{
//
//	}
//
//	int vec3_mul(lua_State* l)
//	{
//
//	}
//
//	int vec3_div(lua_State* l)
//	{
//
//	}
//
//	int vec3_eq(lua_State* l)
//	{
//
//	}
//
//	int vec3_lt(lua_State* l)
//	{
//
//	}
//
//	int vec3_le(lua_State* l)
//	{
//
//	}
}
