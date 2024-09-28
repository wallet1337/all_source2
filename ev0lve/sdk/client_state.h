
#ifndef SDK_STATE_H
#define SDK_STATE_H

#include <sdk/macros.h>

namespace sdk
{
	inline static constexpr auto flow_outgoing = 0;
	inline static constexpr auto flow_incoming = 1;
	inline static constexpr auto net_frames_mask = 63;
	inline static constexpr auto sv_max_unlag = .2f;

	class net_message
	{
	public:
		VIRTUAL(7, get_type(), int32_t(__thiscall*)(void*))()
		VIRTUAL(8, get_group(), int32_t(__thiscall*)(void*))()
		VIRTUAL(9, get_name(), const char*(__thiscall*)(void*))()
	};

	class net_channel
	{
	public:
		VIRTUAL(1, get_address(), const char* (__thiscall*)(void*))()
		VIRTUAL(9, get_latency(const int flow), float(__thiscall*)(void*, int))(flow)
		VIRTUAL(10, get_avg_latency(const int flow), float(__thiscall*)(void*, int))(flow)
		VIRTUAL(40, send_net_msg(const uintptr_t msg, bool reliable = false, bool voice = false), bool(__thiscall*)(void*, uintptr_t, bool, bool))(msg, reliable, voice)
		VIRTUAL(46, send_datagram(), int(__thiscall*)(void*, void*))(nullptr)

		char pad_0000[20];
		bool processing_messages;
		bool should_delete;
		char pad_0016[2];
		int out_sequence_nr;
		int in_sequence_nr;
		int out_sequence_nr_ack;
		int out_reliable_state;
		int in_reliable_state;
		int choked_packets;
		char pad_0030[1044];
	};

	class client_state_t
	{
	public:
		inline void request_full_update()
		{
			delta_tick = -1;
		}
		
		inline void process_sockets()
		{
			if (net_channel && !paused)
				((void(__fastcall*)(uint32_t, uintptr_t)) game->engine.at(functions::net_channel::process_sockets))(0, uintptr_t(this) + 12);
		}

		void __declspec(noinline) netmsg_tick_ctor(uintptr_t thisptr, int32_t tick)
		{
			const auto ctor = game->engine.at(functions::net_channel::netmsg_tick_ctor);
			const auto host = game->engine.at(functions::net_channel::host);
			const auto xmm0_param = *(float*)*(uintptr_t*)(host + 0x4);
			const auto xmm3_param = *(float*)*(uintptr_t*)(host + 0x8 + 0x4);
			const auto xmm2_param = *(float*)*(uintptr_t*)(host + 0x10 + 0x4);

			__asm
			{
				movss xmm0, xmm0_param
				push xmm0_param
				movss xmm3, xmm3_param
				movss xmm2, xmm2_param
				push tick
				mov ecx, thisptr
				mov eax, [ctor]
				call eax
			}
		}

		inline void send_netmsg_tick()
		{
			if (!net_channel || paused)
				return;

			auto netmsg_tick_dtor = (void(__thiscall *)(void *)) game->engine.at(functions::net_channel::netmsg_tick_dtor);
			uint8_t net_msg[0x44];
			netmsg_tick_ctor((uintptr_t)net_msg, delta_tick);
			net_channel->send_net_msg((uintptr_t)net_msg);
			netmsg_tick_dtor(net_msg);
		}

		uint8_t pad_0000[156];
		net_channel* net_channel;
		uint8_t pad_00A0[104];
		uint32_t signon_state;
		uint32_t pad;
		double next_cmd_time;
		uint8_t pad_010C[80];
		uint32_t cur_clock_offset;
		uint32_t server_tick;
		uint32_t client_tick;
		int32_t delta_tick;
		bool paused;
		uint8_t pad_0179[275];
		char level_name[260];
		uint8_t pad_0390[18844];
		int32_t last_command;
		int32_t choked_commands;
		int32_t last_command_ack;
		int32_t last_server_tick;
		int32_t command_ack;
	};
}

#endif // SDK_STATE_H
