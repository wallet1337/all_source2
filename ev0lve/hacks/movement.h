
#ifndef HACKS_MOVEMENT_H
#define HACKS_MOVEMENT_H

#include <sdk/cs_player.h>
#include <sdk/math.h>
#include <sdk/engine_client.h>
#include <base/cfg.h>

namespace hacks
{
	class movement
	{
	public:
		void bhop(sdk::user_cmd* cmd);
		void fix_bhop(sdk::user_cmd* cmd);

	private:
		void autostrafe(sdk::user_cmd* cmd, bool can_jump);

		bool disabled_this_interval{};
	};

	extern movement mov;
}

#endif // HACKS_MOVEMENT_H
