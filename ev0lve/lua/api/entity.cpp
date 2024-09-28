#include <lua/api_def.h>
#include <lua/helpers.h>
#include <sdk/client_entity_list.h>
#include <sdk/cs_player.h>
#include <detail/aim_helper.h>

int lua::api_def::entities::index(lua_State *l)
{
	runtime_state s(l);

	if (s.is_number(2))
	{
		const auto key = s.get_integer(2);

		auto ent = game->client_entity_list->get_client_entity(key);

		if (!ent)
			return 0;

		s.create_user_object_ptr(XOR_STR("csgo.entity"), ent);

		return 1;
	}

	return 0;
}

int lua::api_def::entities::get_entity(lua_State *l)
{
	runtime_state s(l);

	if (!s.is_number(1))
	{
		s.error(XOR_STR("usage: entities.get_entity(number)"));
		return 0;
	}

	auto ent = game->client_entity_list->get_client_entity(s.get_integer(1));

	if (!ent)
		return 0;

	s.create_user_object_ptr(XOR_STR("csgo.entity"), ent);

	return 1;
}

int lua::api_def::entities::get_entity_from_handle(lua_State *l)
{
	runtime_state s(l);

	if (!s.is_number(1))
	{
		s.error(XOR_STR("usage: entities.get_entity_from_handle(number)"));
		return 0;
	}

	auto ent = game->client_entity_list->get_client_entity_from_handle(s.get_integer(1));

	if (!ent)
		return 0;

	s.create_user_object_ptr(XOR_STR("csgo.entity"), ent);

	return 1;
}

int lua::api_def::entities::for_each(lua_State *l)
{
	runtime_state s(l);

	if (!s.is_function(1))
	{
		s.error(XOR_STR("usage: entities.for_each(function)"));
		return 0;
	}

	auto func_ref = s.registry_add();

	game->client_entity_list->for_each([&](sdk::client_entity* ent) {
		s.registry_get(func_ref);
		s.create_user_object_ptr(XOR_STR("csgo.entity"), ent);
		if (!s.call(1, 0))
			s.error(s.get_last_error_description());
	});

	s.registry_remove(func_ref);

	return 0;
}

int lua::api_def::entities::for_each_z(lua_State *l)
{
	runtime_state s(l);

	if (!s.is_function(1))
	{
		s.error(XOR_STR("usage: entities.for_each_z(function)"));
		return 0;
	}

	auto func_ref = s.registry_add();

	game->client_entity_list->for_each_z([&](sdk::client_entity* ent) {
		s.registry_get(func_ref);
		s.create_user_object_ptr(XOR_STR("csgo.entity"), ent);
		if (!s.call(1, 0))
			s.error(s.get_last_error_description());
	});

	s.registry_remove(func_ref);

	return 0;
}

int lua::api_def::entities::for_each_player(lua_State *l)
{
	runtime_state s(l);

	if (!s.is_function(1))
	{
		s.error(XOR_STR("usage: entities.for_each_player(function)"));
		return 0;
	}

	auto func_ref = s.registry_add();

	game->client_entity_list->for_each_player([&](sdk::cs_player_t* ent)
	{
		s.registry_get(func_ref);
		s.create_user_object_ptr(XOR_STR("csgo.entity"), ent);

		if (!s.call(1, 0))
			s.error(s.get_last_error_description());
	});

	s.registry_remove(func_ref);

	return 0;
}

bool lua::api_def::entity::is_sane(sdk::entity_t* ent)
{
	return std::find(game->client_entity_list->begin(), game->client_entity_list->end(), ent) != game->client_entity_list->end();
}

int lua::api_def::entity::get_index(lua_State *l)
{
	runtime_state s(l);

	const auto ent = extract_type<sdk::entity_t>(s, XOR_STR("usage: ent:get_index()"));

	if (!ent || !is_sane(ent))
		return 0;

	s.push(ent->index());
	return 1;
}

int lua::api_def::entity::is_valid(lua_State *l)
{
	runtime_state s(l);

	const auto ent = extract_type<sdk::cs_player_t>(s, XOR_STR("usage: ent:is_valid()"));

	if (!ent || !is_sane(ent))
		return 0;

	s.push(ent->is_player() && ent->is_valid());

	return 1;
}

int lua::api_def::entity::is_dormant(lua_State *l)
{
	runtime_state s(l);

	const auto ent = extract_type<sdk::cs_player_t>(s, XOR_STR("usage: ent:is_dormant()"));

	if (!ent || !is_sane(ent))
		return 0;

	s.push(ent->is_dormant());

	return 1;
}

int lua::api_def::entity::is_player(lua_State *l)
{
	runtime_state s(l);

	const auto ent = extract_type<sdk::cs_player_t>(s, XOR_STR("usage: ent:is_player()"));

	if (!ent || !is_sane(ent))
		return 0;

	s.push(ent->is_player());

	return 1;
}

int lua::api_def::entity::is_enemy(lua_State *l)
{
	runtime_state s(l);

	const auto ent = extract_type<sdk::cs_player_t>(s, XOR_STR("usage: ent:is_enemy()"));

	if (!ent || !is_sane(ent))
		return 0;

	s.push(ent->is_player() && ent->is_enemy());

	return 1;
}

int lua::api_def::entity::get_class(lua_State *l)
{
	runtime_state s(l);

	const auto ent = extract_type<sdk::cs_player_t>(s, XOR_STR("usage: ent:get_class()"));

	if (!ent || !is_sane(ent))
		return 0;

	s.push(ent->get_client_class()->network_name);

	return 1;
}

int lua::api_def::entity::get_hitbox_position(lua_State *l)
{
	runtime_state s(l);

	if (!s.check_arguments({
		{ ltd::user_data },
		{ ltd::number } }))
	{
		s.error(XOR_STR("usage: ent:get_hitbox_position(number)"));
		return 0;
	}

	const auto ent = *reinterpret_cast<sdk::cs_player_t**>(s.get_user_data(1));

	if (!ent || !is_sane(ent))
		return 0;

	sdk::mat3x4* mtx{};
	if (ent->index() == game->engine_client->get_local_player())
	{
		if (!detail::local_record.has_visual_matrix)
			return 0;

		mtx = detail::local_record.vis_mat;
		// fixup position
		sdk::move_matrix(mtx, detail::local_record.abs_origin, ent->get_abs_origin());
	} else
	{
		auto pl = GET_PLAYER_ENTRY(ent);
		if (pl.visuals.has_matrix)
			return 0;

		mtx = pl.visuals.matrix;
	}

	const auto hitbox_pos = detail::aim_helper::get_hitbox_position(ent, static_cast<sdk::cs_player_t::hitbox>(s.get_integer(2)), mtx);
	if (hitbox_pos == std::nullopt)
		return 0;

	s.push(hitbox_pos->x);
	s.push(hitbox_pos->y);
	s.push(hitbox_pos->z);

	return 3;
}

int lua::api_def::entity::get_eye_position(lua_State *l)
{
	runtime_state s(l);

	const auto ent = extract_type<sdk::cs_player_t>(s, XOR_STR("usage: ent:get_eye_position()"));

	if (!ent || !is_sane(ent))
		return 0;

	const auto eye_pos = ent->get_eye_position();

	s.push(eye_pos.x);
	s.push(eye_pos.y);
	s.push(eye_pos.z);

	return 3;
}

int lua::api_def::entity::get_player_info(lua_State *l)
{
	runtime_state s(l);

	const auto ent = extract_type<sdk::cs_player_t>(s, XOR_STR("usage: ent:get_player_info()"));

	if (!ent || !is_sane(ent) || !ent->is_player())
		return 0;

	const auto player_info = game->engine_client->get_player_info(ent->index());

	s.create_table();
	{
		s.set_field(XOR("name"), player_info.name);
		s.set_field(XOR("user_id"), player_info.user_id);
		s.set_field(XOR("steam_id"), player_info.steam_id);
		s.set_field(XOR("steam_id64"), std::to_string(player_info.steam_id64).c_str());
		s.set_field(XOR("steam_id64_low"), player_info.xuid_low);
		s.set_field(XOR("steam_id64_high"), player_info.xuid_high);
	}

	return 1;
}

int lua::api_def::entity::get_prop(lua_State *l)
{
	runtime_state s(l);

	if (!s.check_arguments({
	   { ltd::user_data },
	   { ltd::string },
	   { ltd::number, true }}))
	{
		s.error(XOR_STR("usage: ent:get_prop(name, index (optional))"));
		return 0;
	}

	auto index = 0;

	// 3 IS 3!
	if (s.is_number(3))
		index = s.get_integer(3);

	const auto ent = *reinterpret_cast<sdk::entity_t**>(s.get_user_data(1));

	if (!ent || !is_sane(ent))
		return 0;

	const auto client_class = ent->get_client_class();

	auto prop_name = std::string(s.get_string(2));
	const auto name_hash = util::fnv1a(prop_name.c_str());

	const auto ref = helpers::get_netvar(prop_name.c_str(), client_class->table);

	if (!ref.offset)
		return 0;

	const auto addr = uint32_t(ent) + ref.offset;
	switch (ref.type)
	{
		default:
		case sdk::send_prop_type::dpt_int:
			if (prop_name.find(XOR_STR("m_b")) != std::string::npos)
				s.push(reinterpret_cast<bool*>(addr)[index]);
			else if (prop_name.find(XOR_STR("m_f")) != std::string::npos && name_hash != FNV1A("m_fFlags"))
				s.push(reinterpret_cast<float*>(addr)[index]);
			else if (prop_name.find(XOR_STR("m_v")) != std::string::npos ||
				prop_name.find(XOR_STR("m_ang")) != std::string::npos)
			{
				s.push(reinterpret_cast<float*>(addr)[index]);
				s.push(reinterpret_cast<float*>(addr)[index + 1]);
				s.push(reinterpret_cast<float*>(addr)[index + 2]);
				return 3;
			}
			else if (prop_name.find(XOR_STR("m_psz")) != std::string::npos ||
				prop_name.find(XOR_STR("m_sz")) != std::string::npos)
				s.push(reinterpret_cast<const char*>(addr));
			else
			{
				if (name_hash == FNV1A("m_iItemDefinitionIndex"))
					s.push(reinterpret_cast<short*>(addr)[index]);
				else
					s.push(reinterpret_cast<int*>(addr)[index]);
			}

			return 1;
		case sdk::send_prop_type::dpt_float:
			s.push(reinterpret_cast<float*>(addr)[index]);
			return 1;
		case sdk::send_prop_type::dpt_vector:
			s.push(reinterpret_cast<float*>(addr)[index * 3]);
			s.push(reinterpret_cast<float*>(addr)[index * 3 + 1]);
			s.push(reinterpret_cast<float*>(addr)[index * 3 + 2]);
			return 3;
		case sdk::send_prop_type::dpt_vectorxy:
			s.push(reinterpret_cast<float*>(addr)[index * 2]);
			s.push(reinterpret_cast<float*>(addr)[index * 2 + 1]);
			return 2;
		case sdk::send_prop_type::dpt_string:
			s.push(reinterpret_cast<const char*>(addr));
			return 1;
		case sdk::send_prop_type::dpt_int64:
		{
			const auto var = reinterpret_cast<int64_t*>(addr)[index];
			s.push(std::to_string(var).c_str());
			return 1;
		}
	}
}

int lua::api_def::entity::set_prop(lua_State *l)
{
	runtime_state s(l);

	if (!s.check_arguments({
		{ ltd::user_data },
		{ ltd::string },
		{ ltd::number }}, true))
	{
		s.error(XOR_STR("usage: ent:set_prop(name, index, value(s))"));
		return 0;
	}

	auto index = 0;

	// 3 IS 3!
	if (s.is_number(3))
		index = s.get_integer(3);

	const auto ent = *reinterpret_cast<sdk::entity_t**>(s.get_user_data(1));

	if (!ent || !is_sane(ent))
		return 0;

	const auto client_class = ent->get_client_class();

	auto prop_name = std::string(s.get_string(2));

	const auto ref = helpers::get_netvar(prop_name.c_str(), client_class->table);

	if (!ref.offset)
		return 0;

	const auto addr = uint32_t(ent) + ref.offset;

	switch (ref.type)
	{
		default:
		case sdk::send_prop_type::dpt_int:
			if (s.is_number(4))
				reinterpret_cast<int*>(addr)[index] = s.get_integer(4);
			else if (s.is_boolean(4))
				reinterpret_cast<bool*>(addr)[index] = s.get_boolean(4);
			break;
		case sdk::send_prop_type::dpt_float:
			if (s.is_number(4))
				reinterpret_cast<float*>(addr)[index] = s.get_float(4);
			break;
		case sdk::send_prop_type::dpt_vector:
			if (s.is_number(4))
				reinterpret_cast<float*>(addr)[index] = s.get_float(4);
			if (s.is_number(5))
				reinterpret_cast<float*>(addr)[index] = s.get_float(5);
			if (s.is_number(6))
				reinterpret_cast<float*>(addr)[index] = s.get_float(6);
			break;
		case sdk::send_prop_type::dpt_vectorxy:
			if (s.is_number(4))
				reinterpret_cast<float*>(addr)[index] = s.get_float(4);
			if (s.is_number(5))
				reinterpret_cast<float*>(addr)[index] = s.get_float(5);
			break;
		case sdk::send_prop_type::dpt_string:
		case sdk::send_prop_type::dpt_int64:
			s.error("Not implemented");
			break;
	}
	return 0;
}

int lua::api_def::entity::get_move_type(lua_State *l)
{
	runtime_state s(l);

	const auto ent = extract_type<sdk::cs_player_t>(s, XOR_STR("usage: ent:get_move_type()"));

	if (!ent || !is_sane(ent) || !ent->is_player())
		return 0;

	s.push(ent->get_move_type());

	return 1;
}