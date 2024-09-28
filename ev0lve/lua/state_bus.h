
#ifndef LUA_STATE_BUS_H
#define LUA_STATE_BUS_H

#include <lua/state.h>
#include <mutex>

namespace lua
{
	class state_bus;

	/**
	 * Performs cross-state call.
	 * @param caller Caller state
	 * @param callee Callee state
	 * @return Return results amount
	 */
	int perform_call(lua_State* caller, lua_State* callee);

	/**
	 * Copies one stack to another across states.
	 * @param from From state
	 * @param to To state
	 * @param top From state's top
	 * @param bottom From state's bottom
	 */
	void copy_stack(lua_State* from, lua_State* to, int top, int bottom);

	/**
	 * Basic bus object.
	 */
	class bus_object
	{
	public:
		enum : char
		{
			bus_ref,
			bus_named,
		};

		template<typename T>
		T* as()
		{
			return reinterpret_cast<T*>(this);
		}

		char type{};
	};

	/**
	 * References function wrapper.
	 */
	class bus_ref_object : public bus_object
	{
	public:
		bus_ref_object(std::weak_ptr<state_bus>  b, int r) : bus(std::move(b)), ref(r)
		{
			type = bus_ref;
		}

		int invoke(runtime_state &s);

	private:
		int ref{};
		std::weak_ptr<state_bus> bus{};
	};

	/**
	 * Named function wrapper.
	 */
	class bus_named_object : public bus_object
	{
	public:
		bus_named_object(std::weak_ptr<state_bus>  b, const char* name) : bus(std::move(b)), name(name)
		{
			type = bus_named;
		}

		int invoke(runtime_state& s);
	private:
		std::string name{};
		std::weak_ptr<state_bus> bus{};
	};

	/**
	 * State bus: object, that holds target state. Allows to lookup methods in the target state.
	 */
	class state_bus : public std::enable_shared_from_this<state_bus>
	{
	public:
		explicit state_bus(state& s) : target(s)
		{
			table_ref = s.registry_add();
		}
		
		void clear_stack();

		lua_State* get_state();
		std::optional<bus_named_object> lookup_object(const char* n);
		
		int table_ref{};
		std::mutex invoke_mutex{};
	private:
		std::vector<uint32_t> existing_objects{};
		state& target;
	};
}

#endif // LUA_STATE_BUS_H
