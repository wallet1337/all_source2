#include <lua/helpers.h>
#include <base/debug_overlay.h>
#include <util/definitions.h>

namespace lua::helpers
{
	var get_netvar(const char* name, sdk::recv_table* table, size_t offset)
	{
		for (auto i = 0; i < table->count; ++i)
		{
			const auto current = &table->props[i];
			if (current->type == sdk::dpt_datatable)
			{
				const auto found = get_netvar(name, current->data_table, offset + current->offset);
				if (found.offset != 0)
					return found;
			}

			if (isdigit(current->name[0]))
				continue;

			if (util::fnv1a(name) == util::fnv1a(current->name))
				return {static_cast<size_t>(current->offset) + offset, static_cast<sdk::send_prop_type>(current->type), sdk::dpt_int };
		}

		return {0,sdk::dpt_int, sdk::dpt_int};
	}

	uintptr_t get_interface(const uint32_t module, const uint32_t name)
	{
		const auto list = **reinterpret_cast<interface_reg***>(find_pattern(find_module(module), XOR_STR_OT("55 8B EC 56 8B 35 ? ? ? ? 57 85 F6 74 38 8B"), 6));

		for (auto current = list; current; current = current->next)
			if (FNV1A_CMP_IM(current->name, name))
				return reinterpret_cast<uintptr_t>(current->create());

		RUNTIME_ERROR("Could not find registered interface.");
	}

	uintptr_t find_pattern(const uint8_t* module, const char* pattern, const ptrdiff_t off)
	{
		static auto pattern_to_bytes = [](const char* pattern) -> std::vector<int> {
			auto bytes = std::vector<int32_t>{ };
			const auto start = const_cast<char*>(pattern);
			const auto end = const_cast<char*>(pattern) + strlen(pattern);

			for (auto current = start; current < end; ++current) {
				if (*current == '?') {
					current++;

					if (*current == '?')
						current++;

					bytes.push_back(-1);
				}
				else
					bytes.push_back(static_cast<int32_t>(strtoul(current, &current, 0x10)));
			}

			return bytes;
		};

		const auto nt_headers = PIMAGE_NT_HEADERS(module + PIMAGE_DOS_HEADER(module)->e_lfanew);
		auto pattern_bytes = pattern_to_bytes(pattern);
		const auto s = pattern_bytes.size();
		const auto d = pattern_bytes.data();

		for (auto i = 0ul; i < nt_headers->OptionalHeader.SizeOfImage - s; ++i) {
			auto found = true;

			for (auto j = 0ul; j < s; ++j)
				if (module[i + j] != d[j] && d[j] != -1) {
					found = false;
					break;
				}

			if (found)
				return reinterpret_cast<uintptr_t>(uintptr_t(&module[i]) + off);
		}

		RUNTIME_ERROR("Pattern scan failed.");
	}

	const uint8_t* find_module(const uint32_t hash)
	{
		static auto file_name_w = [](wchar_t* path)
		{
			wchar_t* slash = path;

			while (path && *path)
			{
				if ((*path == '\\' || *path == '/' || *path == ':') &&
					path[1] && path[1] != '\\' && path[1] != '/')
					slash = path + 1;
				path++;
			}

			return slash;
		};

		const auto peb = NtCurrentTeb()->ProcessEnvironmentBlock;

		if (!peb)
			return nullptr;

		const auto ldr = reinterpret_cast<PPEB_LOADER_DATA>(peb->Ldr);

		if (!ldr)
			return nullptr;

		const auto head = &ldr->InLoadOrderModuleList;
		auto current = head->Flink;

		while (current != head)
		{
			const auto module = CONTAINING_RECORD(current, LDR_MODULE, InLoadOrderModuleList);
			std::wstring wide(file_name_w(module->FullDllName.Buffer));
			std::string name(wide.begin(), wide.end());
			std::transform(name.begin(), name.end(), name.begin(), ::tolower);

			if (FNV1A_CMP_IM(name.c_str(), hash))
				return reinterpret_cast<uint8_t*>(module->BaseAddress);

			current = current->Flink;
		}

		return nullptr;
	}

	json parse_table(lua_State* L, int index)
	{
		runtime_state s(L);

		json root;

		// Push another reference to the table on top of the stack (so we know
		// where it is, and this function can work for negative, positive and
		// pseudo indices
		lua_pushvalue(L, index);
		// stack now contains: -1 => table
		lua_pushnil(L);
		// stack now contains: -1 => nil; -2 => table
		while (lua_next(L, -2))
		{
			// stack now contains: -1 => value; -2 => key; -3 => table
			// copy the key so that lua_tostring does not modify the original
			lua_pushvalue(L, -2);
			// stack now contains: -1 => key; -2 => value; -3 => key; -4 => table

			json sub;

			// set value by type
			switch (lua_type(L, -2))
			{
				case LUA_TNIL:
					sub = nullptr;
					break;
				case LUA_TNUMBER:
					sub = lua_tonumber(L, -2);
					break;
				case LUA_TBOOLEAN:
					sub = lua_toboolean(L, -2);
					break;
				case LUA_TSTRING:
					sub = lua_tostring(L, -2);
					break;
				case LUA_TTABLE:
					sub = parse_table(L, -2);
					break;
				default:
					break;
			}

			// set value to key
			if (lua_isnumber(L, -1))
				root.push_back(sub);
			else
				root[lua_tostring(L, -1)] = sub;


			// pop value + copy of key, leaving original key
			lua_pop(L, 2);
			// stack now contains: -1 => key; -2 => table
		}
		// stack now contains: -1 => table (when lua_next returns 0 it pops the key
		// but does not push anything.)
		// Pop table
		lua_pop(L, 1);
		// Stack is now the same as it was on entry to this function

		return root;
	}
	int load_table(lua_State* L, json& j, bool array)
	{
		runtime_state s(L);

		s.create_table();
		{
			// array iterator (lua arrays start at 1)
			auto i = 1;
			for (auto &kv: j.items())
			{
				if (kv.value().is_string())
					array ? s.set_i(i, kv.value().get<std::string>().c_str()) : s.set_field(kv.key().c_str(), kv.value().get<std::string>().c_str());
				else if (kv.value().is_number())
					array ? s.set_i(i, kv.value().get<double>()) : s.set_field(kv.key().c_str(), kv.value().get<double>());
				else if (kv.value().is_boolean())
					array ? s.set_i(i, kv.value().get<bool>()) : s.set_field(kv.key().c_str(), kv.value().get<bool>());
				else if (kv.value().is_object() || kv.value().is_array())
				{
					load_table(L, kv.value(), kv.value().is_array());
					array ? s.set_i(i) : s.set_field(kv.key().c_str());
				}

				i++;
			}
		}

		return 1;
	}

	void print(const char* info_string)
	{
		print_to_console(sdk::color(255, 255, 255), XOR_STR("["));
		print_to_console(sdk::color(75, 105, 255), XOR_STR("ev0"));
		print_to_console(sdk::color(255, 255, 255), XOR_STR("lve] "));
		print_to_console(sdk::color(255, 255, 255), info_string);
		print_to_console(XOR_STR("\n"));
	}

	void warn(const char* info_string)
	{
		print_to_console(sdk::color(255, 255, 255), XOR_STR("["));
		print_to_console(sdk::color(75, 105, 255), XOR_STR("ev0"));
		print_to_console(sdk::color(255, 255, 255), XOR_STR("lve] "));
		print_to_console(sdk::color(255, 128, 0), XOR_STR("warning: "));
		print_to_console(sdk::color(255, 255, 255), info_string);
		print_to_console(XOR_STR("\n"));
	}

	void error(const char* error_type, const char* error_string)
	{
		print_to_console(sdk::color(255, 255, 255), XOR_STR("["));
		print_to_console(sdk::color(75, 105, 255), XOR_STR("ev0"));
		print_to_console(sdk::color(255, 255, 255), XOR_STR("lve] "));
		print_to_console(sdk::color(255, 0, 0), error_type);
		print_to_console(sdk::color(255, 0, 0), XOR_STR(": "));
		print_to_console(sdk::color(255, 255, 255), error_string);
		print_to_console(XOR_STR("\n"));
	}

	std::optional<uri> parse_uri(const std::string& u)
	{
		if (u.find(XOR_STR("http")) != 0 || u.size() < 9)
			return std::nullopt;

		uri res{};
		res.is_secure = u.find(XOR_STR("https:")) == 0;

		const auto host_offset = 7 + (int)res.is_secure;
		res.host = u.substr(host_offset, u.find(XOR_STR("/"), 8) - host_offset);
		res.path = u.substr(host_offset + res.host.size());

		if (const auto port_p = res.host.find(XOR_STR(":")); port_p != std::string::npos)
		{
			const auto port_s = res.host.substr(port_p + 1);
			res.host = res.host.substr(0, port_p);
			res.port = std::stoi(port_s);
		} else
			res.port = res.is_secure ? 443 : 80;

		if (res.path.empty())
			res.path = XOR_STR("/");

		return res;
	}

	static std::unordered_map<uint32_t, std::vector<std::shared_ptr<timed_callback>>> timers{};

	timed_callback::timed_callback(float delay, std::function<void()> callback)
	{
		this->delay = delay;
		this->callback = std::move(callback);
	}
}