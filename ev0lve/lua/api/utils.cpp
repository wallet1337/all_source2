#include <lua/api_def.h>
#include <sdk/math.h>
#include <sdk/weapon_system.h>
#include <sdk/debug_overlay.h>
#include <base/cfg.h>
#include <util/fnv1a.h>
#include <lua/helpers.h>
#include <lua/engine.h>
#include <sdk/client_state.h>
#include <sdk/generated.h>

#include <wininet.h>

using namespace lua;

int api_def::utils::random_int(lua_State *l)
{
	runtime_state s(l);

	const auto r = s.check_arguments({
		{ ltd::number },
		{ ltd::number }
	});

	if (!r)
	{
		s.error(XOR("usage: random_int(min, max)"));
		return 0;
	}

	s.push(sdk::random_int(s.get_integer(1), s.get_integer(2)));
	return 1;
}

int api_def::utils::random_float(lua_State *l)
{
	runtime_state s(l);

	const auto r = s.check_arguments({
		{ ltd::number },
		{ ltd::number }
	});

	if (!r)
	{
		s.error(XOR("usage: random_float(min, max)"));
		return 0;
	}

	s.push(sdk::random_float(s.get_float(1), s.get_float(2)));
	return 1;
}

int api_def::utils::flags(lua_State *l)
{
	runtime_state s(l);

	uint32_t result{};
	for (auto i = 1; i <= s.get_stack_top(); ++i)
	{
		if (s.is_number(i))
			result |= s.get_integer(i);
	}

	s.push(static_cast<int>(result));
	return 1;
}

int api_def::utils::find_interface(lua_State *l)
{
	runtime_state s(l);

	if (!cfg.lua.allow_insecure.get())
	{
		s.error(XOR_STR("find_interface is not available with unsafe mode disabled"));
		return 0;
	}

	const auto r = s.check_arguments({
		{ ltd::string },
		{ ltd::string }
	});

	if (!r)
	{
		s.error(XOR("usage: find_interface(module, name)"));
		return 0;
	}

	try
	{
		s.push((int)helpers::get_interface(util::fnv1a(s.get_string(1)), util::fnv1a(s.get_string(2))));
	}
	catch (...)
	{
		s.push_nil();
	}

	return 1;
}

int api_def::utils::find_pattern(lua_State *l)
{
	runtime_state s(l);

	if (!cfg.lua.allow_insecure.get())
	{
		s.error(XOR_STR("find_pattern is not available with unsafe mode disabled"));
		return 0;
	}

	const auto r = s.check_arguments({
		{ ltd::string },
		{ ltd::string },
		{ ltd::number, true }
	});

	if (!r)
	{
		s.error(XOR("usage: find_pattern(module, pattern, offset = 0)"));
		return 0;
	}

	const auto v = s.get_stack_top() >= 3 ? s.get_integer(3) : 0;

	try
	{
		s.push((int)helpers::find_pattern(helpers::find_module(util::fnv1a(s.get_string(1))), s.get_string(2), v));
	}
	catch (...)
	{
		s.push_nil();
	}

	return 1;
}

int api_def::utils::get_weapon_info(lua_State *l)
{
	runtime_state s(l);

	const auto r = s.check_arguments({
		{ ltd::number }
	});

	if (!r)
	{
		s.error(XOR("usage: get_weapon_info(item_definition_index)"));
		return 0;
	}

	const auto n = s.get_integer(1);
	if (!n)
		return 0;

	const auto weapon_data = game->weapon_system->get_weapon_data((sdk::item_definition_index)n);

	s.create_table();
	{
		s.set_field(XOR("console_name"), weapon_data->consolename);
		s.set_field(XOR("max_clip1"), weapon_data->maxclip1);
		s.set_field(XOR("max_clip2"), weapon_data->imaxclip2);
		s.set_field(XOR("world_model"), weapon_data->szworldmodel);
		s.set_field(XOR("view_model"), weapon_data->szviewmodel);
		s.set_field(XOR("weapon_type"), weapon_data->weapontype);
		s.set_field(XOR("weapon_price"), weapon_data->iweaponprice);
		s.set_field(XOR("kill_reward"), weapon_data->ikillaward);
		s.set_field(XOR("cycle_time"), weapon_data->cycle_time);
		s.set_field(XOR("is_full_auto"), weapon_data->bfullauto);
		s.set_field(XOR("damage"), weapon_data->idamage);
		s.set_field(XOR("range"), weapon_data->range);
		s.set_field(XOR("range_modifier"), weapon_data->flrangemodifier);
		s.set_field(XOR("throw_velocity"), weapon_data->flthrowvelocity);
		s.set_field(XOR("has_silencer"), weapon_data->bhassilencer);
		s.set_field(XOR("max_player_speed"), weapon_data->flmaxplayerspeed);
		s.set_field(XOR("max_player_speed_alt"), weapon_data->flmaxplayerspeedalt);
		s.set_field(XOR("zoom_fov1"), weapon_data->zoom_fov1);
		s.set_field(XOR("zoom_fov2"), weapon_data->zoom_fov2);
		s.set_field(XOR("zoom_levels"), weapon_data->zoom_levels);
	}

	return 1;
}

int api_def::utils::create_timer(lua_State *l, bool run)
{
	runtime_state s(l);

	const auto r = s.check_arguments({
		{ ltd::number },
		{ ltd::function }
	});

	if (!r)
	{
		s.error(XOR("usage: new_timer(delay, function)"));
		return 0;
	}

	const auto& me = lua::api.find_by_state(l);
	if (!me)
	{
		s.error(XOR_STR("FATAL: could not find the script. Did it escape the sandbox?"));
		return 0;
	}

	const auto ref = s.registry_add();
	const auto tmr = std::make_shared<helpers::timed_callback>(s.get_float(1), [ref, l]() {
		runtime_state s(l);
		s.registry_get(ref);

		if (!s.is_nil(-1))
		{
			if (!s.call(0, 0))
				s.error(s.get_last_error_description());
		}
		else
			s.pop(1);
	});

	helpers::timers[me->id].emplace_back(tmr);

	if (run)
	{
		tmr->run_once();
		return 0;
	}

	// create lua usertype
	std::weak_ptr<helpers::timed_callback> obj{tmr};
	s.create_user_object<decltype(obj)>(XOR_STR("utils.timer"), &obj);

	return 1;
}

int api_def::utils::new_timer(lua_State *l)
{
	return create_timer(l);
}

int api_def::utils::run_delayed(lua_State *l)
{
	return create_timer(l, true);
}

int api_def::utils::world_to_screen(lua_State* l)
{
	runtime_state s(l);

	const auto r = s.check_arguments({
		{ ltd::number },
		{ ltd::number },
		{ ltd::number },
	});

	if (!r)
	{
		s.error(XOR_STR("usage: world_to_screen(x, y, z): x, y | nil"));
		return 0;
	}

	sdk::vec3 in{ s.get_float(1), s.get_float(2), s.get_float(3) }, out{};
	if (!world_to_screen(in, out))
		return 0;

	s.push(out.x);
	s.push(out.y);
	return 2;
}

int api_def::utils::get_rtt(lua_State* l)
{
	runtime_state s(l);

	if (!game->engine_client->is_ingame())
	{
		s.push(0.f);
		return 0;
	}

	s.push(game->client_state->net_channel->get_avg_latency(sdk::flow_incoming) + game->client_state->net_channel->get_avg_latency(sdk::flow_outgoing));
	return 1;
}

int api_def::utils::get_time(lua_State* l)
{
	runtime_state s(l);

	const auto time = std::time(nullptr);
	const auto tm = std::localtime(&time);

	s.create_table();
	s.set_field(XOR("sec"), tm->tm_sec);
	s.set_field(XOR("min"), tm->tm_min);
	s.set_field(XOR("hour"), tm->tm_hour);
	s.set_field(XOR("month_day"), tm->tm_mday);
	s.set_field(XOR("month"), tm->tm_mon + 1);
	s.set_field(XOR("year"), tm->tm_year + 1900);
	s.set_field(XOR("week_day"), tm->tm_wday + 1);
	s.set_field(XOR("year_day"), tm->tm_yday + 1);

	return 1;
}

int api_def::utils::http_get(lua_State* l)
{
	runtime_state s(l);
	const auto r = s.check_arguments({
		{ ltd::string },
		{ ltd::string },
		{ ltd::function },
	});

	if (!r)
	{
		s.error(XOR_STR("usage: utils.http_get(url, headers, callback(response))"));
		return 0;
	}

	if (!cfg.lua.allow_insecure.get())
	{
		s.error(XOR_STR("http_get is not available with unsafe mode disabled"));
		return 0;
	}

	const auto url = helpers::parse_uri(s.get_string(1));
	const auto headers = std::string(s.get_string(2));
	const auto cbk = s.registry_add();

	if (!url)
	{
		s.error(XOR_STR("invalid url specified"));
		return 0;
	}

	// run this in a separate thread to avoid blocking.
	std::thread([l, cbk, url, headers]() {
		const auto inet = InternetOpenA(XOR_STR("ev0lve"), INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
		if (!inet)
			return;

		const auto conn = InternetConnectA(inet, url->host.c_str(), url->port, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 1);
		if (!conn)
		{
			InternetCloseHandle(inet);
			return;
		}

		const auto req = HttpOpenRequestA(conn, XOR_STR("GET"), url->path.c_str(),
			nullptr, nullptr, nullptr, INTERNET_FLAG_KEEP_CONNECTION | (url->is_secure ? INTERNET_FLAG_SECURE : 0), 1);
		if (!req)
		{
			InternetCloseHandle(conn);
			InternetCloseHandle(inet);
			return;
		}

		runtime_state s(l);
		if (HttpSendRequestA(req, headers.c_str(), headers.size(), nullptr, 0))
		{
			std::string result;

			char buffer[1024]{}; unsigned long bytes_read{};
			while (InternetReadFile(req, buffer, 1024, &bytes_read) && bytes_read) {
				result += buffer;
				memset(buffer, 0, 1024);
			}

			InternetCloseHandle(req);
			InternetCloseHandle(conn);
			InternetCloseHandle(inet);

			s.registry_get(cbk);
			s.push(result.c_str());
			if (!s.call(1, 0))
				s.error(s.get_last_error_description());
		}

		s.registry_remove(cbk);
	}).detach();

	return 0;
}

int api_def::utils::http_post(lua_State* l)
{
	runtime_state s(l);
	const auto r = s.check_arguments({
		{ ltd::string },
		{ ltd::string },
		{ ltd::string },
		{ ltd::function },
	});

	if (!r)
	{
		s.error(XOR_STR("usage: utils.http_post(url, headers, body, callback(response))"));
		return 0;
	}

	if (!cfg.lua.allow_insecure.get())
	{
		s.error(XOR_STR("http_get is not available with unsafe mode disabled"));
		return 0;
	}

	const auto url = helpers::parse_uri(s.get_string(1));
	const auto headers = std::string(s.get_string(2));
	const auto body = std::string(s.get_string(3));
	const auto cbk = s.registry_add();

	if (!url)
	{
		s.error(XOR_STR("invalid url specified"));
		return 0;
	}

	// run this in a separate thread to avoid blocking.
	std::thread([l, cbk, url, headers, body]() {
		const auto inet = InternetOpenA(XOR_STR("ev0lve"), INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
		if (!inet)
			return;

		const auto conn = InternetConnectA(inet, url->host.c_str(), url->port, nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 1);
		if (!conn)
		{
			InternetCloseHandle(inet);
			return;
		}

		const auto req = HttpOpenRequestA(conn, XOR_STR("POST"), url->path.c_str(),
		nullptr, nullptr, nullptr, INTERNET_FLAG_KEEP_CONNECTION | (url->is_secure ? INTERNET_FLAG_SECURE : 0), 1);
		if (!req)
		{
			InternetCloseHandle(conn);
			InternetCloseHandle(inet);
			return;
		}

		runtime_state s(l);
		if (HttpSendRequestA(req, headers.c_str(), headers.size(), (void*)body.c_str(), body.size()))
		{
			std::string result;

			char buffer[1024]{}; unsigned long bytes_read{};
			while (InternetReadFile(req, buffer, 1024, &bytes_read) && bytes_read) {
				result += buffer;
				memset(buffer, 0, 1024);
			}

			InternetCloseHandle(req);
			InternetCloseHandle(conn);
			InternetCloseHandle(inet);

			s.registry_get(cbk);
			s.push(result.c_str());
			if (!s.call(1, 0))
				s.error(s.get_last_error_description());
		}

		s.registry_remove(cbk);
	}).detach();

	return 0;
}

int api_def::utils::json_encode(lua_State* l)
{
	runtime_state s(l);
	const auto r = s.check_arguments({
		{ ltd::table }
	});

	if (!r)
	{
		s.error(XOR_STR("usage: utils.json_encode(table)"));
		return 0;
	}

	const auto res = helpers::parse_table(l, 1);
	s.push(res.dump().c_str());
	return 1;
}

int api_def::utils::json_decode(lua_State* l)
{
	runtime_state s(l);
	const auto r = s.check_arguments({
		{ ltd::string }
	});

	if (!r)
	{
		s.error(XOR_STR("usage: utils.json_decode(json)"));
		return 0;
	}

	try
	{
		auto data = nlohmann::json::parse(s.get_string(1));
		return helpers::load_table(l, data);
	} catch (...)
	{
		s.error(XOR_STR("invalid json string"));
		return 0;
	}
}

int api_def::utils::set_clan_tag(lua_State* l)
{
	runtime_state s(l);
	const auto r = s.check_arguments({
		{ ltd::string }
	});

	if (!r)
	{
		s.error(XOR_STR("usage: utils.set_clan_tag(tag)"));
		return 0;
	}

	const auto set_clantag = reinterpret_cast<void(__fastcall*)(const char*, const char*)>(game->engine.at(sdk::functions::set_clantag));
	set_clantag(s.get_string(1), s.get_string(1));
	return 0;
}