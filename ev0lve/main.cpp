
#include <base/game.h>
#include <util/memory.h>
#include <util/anti_debug.h>
#include <thread>

bool __stdcall DllMain(uintptr_t base, uint32_t reason_for_call, uint32_t reserved)
{
	if (reason_for_call == DLL_PROCESS_ATTACH)
	{
		LdrDisableThreadCalloutsForDll(PVOID(LOOKUP_MODULE("client.dll")));
		std::thread([base, reserved]()
		{
			game = std::make_unique<game_t>(base, reserved);
			game->init();
		}).detach();
	}

	return true;
}
