
#include <base/hook_manager.h>
#include <sdk/keyvalues.h>
#include <base/debug_overlay.h>

using namespace sdk;

namespace hooks::client_state
{
	void __fastcall packet_start(client_state_t* state, uint32_t edx, int32_t incoming_sequence, int32_t outgoing_acknowledged)
	{
		// erase commands that are out of range by a huge margin.
		game->cmds.erase(std::remove_if(game->cmds.begin(), game->cmds.end(),
			[&] (const uint32_t& cmd) { return abs(int32_t(outgoing_acknowledged - cmd)) >= input_max; }), game->cmds.end());

		// rollback the ack count to what we aimed for.
		auto target_acknowledged = outgoing_acknowledged;
		for (auto c : game->cmds)
			if (outgoing_acknowledged >= c)
				target_acknowledged = c;

		hook_manager.packet_start->call(state, edx, incoming_sequence, target_acknowledged);
	}

	bool __fastcall send_netmsg(net_channel* channel, uint32_t edx, net_message* msg, bool reliable, bool voice)
	{
		if (msg->get_type() == 18 && FNV1A_CMP(msg->get_name(), "CCLCMsg_CmdKeyValues"))
		{
			const auto values = keyvalues::from_msg(uintptr_t(msg) + 4);
			const auto cmp = FNV1A_CMP(values->get_name(), "UserExtraData");
			values->~keyvalues();

			if (cmp)
				return true;
		}

		// sv_pure bypass
		if (msg->get_type() == 14)
			return false;

		if (msg->get_group() == 9)
			voice = true;

		return hook_manager.send_netmsg->call(channel, edx, msg, reliable, voice);
	}
}
