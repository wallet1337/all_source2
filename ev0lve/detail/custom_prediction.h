
#ifndef DETAIL_CUSTOM_PREDICTION_H
#define DETAIL_CUSTOM_PREDICTION_H

#include <sdk/cs_player.h>
#include <sdk/prediction.h>
#include <sdk/input.h>
#include <detail/player_list.h>

namespace detail
{
	inline static constexpr auto prone_time = 10.f / 290.f;
	inline static constexpr auto post_delay = .2f;
		
	class custom_prediction
	{
		struct prediction_info
		{
			uint32_t flags{}, prev_flags{}, move_type{};
			int32_t sequence{}, checksum{}, tick_base{}, shift{}, ev{};
			float post_pone_time{}, throw_time{}, last_shot{}, view_offset{};
			sdk::vec3 origin{}, velocity{};
			sdk::vec3 obb_maxs{};
			sdk::base_handle wpn{};
			sdk::base_handle ground_entity{};
			float view_delta{};
			float view_height{};
			float bounds_change_time{};
			animation_copy animation{};
		};

	public:
		void initial(sdk::user_cmd* cmd);
		void repredict(sdk::user_cmd* cmd);
		void restore(sdk::user_cmd* cmd, bool send_packet = false);
		void restore_animation(int32_t sequence, bool env = false);
		bool is_predicting() const;
		bool is_computing() const;

		sdk::user_cmd original_cmd{};
		std::array<prediction_info, sdk::input_max> info{};
		int32_t last_sequence = -1, prone_delay = -1, real_model{};
		bool can_shoot{}, had_attack{}, had_secondary_attack{};

	private:
		void predict_post_pone_time(sdk::user_cmd* cmd);
		void predict_can_fire(sdk::user_cmd* cmd);
	
		float backup_curtime{}, backup_frametime{};
		int32_t backup_predicted{};
		bool predicting{}, computing{};
	};

	extern custom_prediction pred;
}

#endif // DETAIL_CUSTOM_PREDICTION_H
