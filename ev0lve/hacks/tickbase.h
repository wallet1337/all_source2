
#ifndef HACKS_TICKBASE_H
#define HACKS_TICKBASE_H

#include <sdk/misc.h>
#include <sdk/game_rules.h>

#define max_new_cmds (sv_maxusrcmdprocessticks - 2)

namespace hacks
{	
	class tickbase_t
	{
	public:
		void reset();
		void adjust_shift(int32_t cmd, int32_t ticks);
		void adjust_limit_dynamic(bool finalize = false);
		bool attempt_shift_back(bool& send_packet);
		void prepare_cycle();
		void on_finalize_cycle(bool& send_packet);
		void apply_static_configuration();
		int32_t determine_optimal_shift() const;
		int32_t determine_optimal_limit() const;

		int32_t limit{}, to_recharge{}, to_shift{}, to_adjust{}, clock_drift{}, last_drift{};
			
		std::pair<int32_t, int32_t> lag;
		
		bool force_choke{}, force_drop{}, skip_next_adjust{}, fast_fire{}, hide_shot{};
	};

	extern tickbase_t tickbase;
}

#endif // HACKS_TICKBASE_H
