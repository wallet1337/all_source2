#include <lua/engine.h>
#include <lua/helpers.h>
#include <base/event_log.h>

void lua::library::lock()
{
	std::lock_guard _lock(ref_mutex);
	++reference_count;
}

void lua::library::unlock()
{
	std::lock_guard _lock(ref_mutex);
	--reference_count;

	if (!reference_count)
		api.stop_library(id);
}

void lua::library::call_main()
{
	// taking off!
	if (!l.call(0, 1))
	{
		// add to error log
		lua::helpers::error(XOR_STR("runtime_error"), l.get_last_error_description().c_str());

		throw std::runtime_error(XOR_STR("Unable to initialize the library.       \nSee console for more details."));
	}

	// initialize state bus
	bus = std::make_shared<state_bus>(l);
	bus->clear_stack();

	// find all forwards
	find_forwards();
}