
#include <base/hook_manager.h>
#include <base/cfg.h>
#include <hacks/tickbase.h>
#include <hacks/antiaim.h>
#include <hacks/misc.h>
#include <detail/aim_helper.h>
#include <detail/custom_prediction.h>

using namespace detail;
using namespace detail::aim_helper;
using namespace hacks;
using namespace sdk;

namespace hooks::engine_client
{
	float __fastcall get_aspect_ratio(engine_client_t* client, uint32_t edx, int32_t width, int32_t height)
	{
		if (!cfg.local_visuals.aspect_changer.get())
			return hook_manager.get_aspect_ratio->call(client, edx, width, height);

		constexpr auto min = 1.f;
		constexpr auto max = 21.f / 9.f;
		return min + std::clamp(cfg.local_visuals.aspect.get() / 100.f, 0.f, 100.f) * (max - min);
	}

	void __declspec(noinline) org_cl_move(float accumulated_extra_samples, bool final_tick)
	{
		auto original = uintptr_t(hook_manager.cl_move->get_original());

		__asm
		{
			movss xmm0, accumulated_extra_samples
			mov cl, final_tick
			push original
			call [esp]
			add esp, 0x4
		}
	}

	void __cdecl cl_move_hook(float accumulated_extra_samples, bool final_tick)
	{
		const auto start = game->client_state->last_command;

		if (!game->engine_client->is_ingame() || !game->client_state->net_channel || game->client_state->paused || !game->local_player || !game->local_player->is_alive())
		{
			tickbase.reset();
			org_cl_move(accumulated_extra_samples, final_tick);
			if (start != game->client_state->last_command)
				game->cmds.push_back(game->client_state->last_command);
			return;
		}

		game->client_state->process_sockets();
		game->prediction->update(game->client_state->delta_tick, true, game->client_state->last_command_ack, game->client_state->last_command + game->client_state->choked_commands);

		if (tickbase.to_recharge > 0 && !game->client_state->choked_commands)
		{
			aa.prepare_on_peek();
			tickbase.adjust_limit_dynamic();
		}

		const auto recharge = tickbase.to_recharge > 0 && !game->client_state->choked_commands;
		if (recharge)
		{
			if (tickbase.limit <= sv_maxusrcmdprocessticks)
				tickbase.limit++;

			tickbase.to_recharge--;
			tickbase.last_drift = game->client_state->last_command + 1;

			auto& cmd = game->input->commands[tickbase.last_drift % input_max];
			auto& verified = game->input->verified_commands[tickbase.last_drift % input_max];
			cmd = game->input->commands[start % input_max];
			cmd.command_number = tickbase.last_drift;
			cmd.tick_count = INT_MAX;
			verified.cmd = cmd;
			verified.crc = cmd.get_checksum();
		}
		else
		{
			tickbase.to_adjust = 0;
			tickbase.force_choke = false;
			org_cl_move(accumulated_extra_samples, final_tick);

			if (tickbase.limit >= 0 && tickbase.to_shift > 0)
			{
				game->prediction->update(game->client_state->delta_tick, true, game->client_state->last_command_ack, game->client_state->last_command + game->client_state->choked_commands);

				for (auto i = 0; i < tickbase.to_shift; i++)
				{
					tickbase.force_choke = true;
					tickbase.limit--;
					org_cl_move(game->globals->interval_per_tick, final_tick);
				}

				tickbase.to_shift = 0;
				tickbase.last_drift = game->client_state->last_command + game->client_state->choked_commands + 1;
			}

			if (start == game->client_state->last_command)
			{
				game->client_state->net_channel->out_sequence_nr--;
				game->client_state->net_channel->choked_packets = 0;
			}
		}

		if (start == game->client_state->last_command)
		{
			game->client_state->send_netmsg_tick();
			const auto next_command = game->client_state->net_channel->send_datagram();

			if (recharge)
				game->client_state->last_command = next_command;
		}
		else
			game->cmds.push_back(game->client_state->last_command);
	}

	__declspec(naked) void cl_move()
	{
		__asm
		{
			mov eax, dword ptr ss:[ebp-0x4]
			push ebp
			mov ebp, esp

			movzx ebx, cl
			push ebx // final_tick
			push eax // accumulated_extra_samples
			call cl_move_hook
			add esp, 0x8
			mov eax, 0

			pop ebp
			ret
		}
	}
}
