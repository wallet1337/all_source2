
#ifndef HACKS_ESP_H
#define HACKS_ESP_H

#include <gui_legacy/gui.h>
#include <sdk/cs_player.h>
#include <sdk/weapon.h>

namespace hacks
{
	class esp : public gui_legacy::drawable
	{
	public:
		void draw(const gui_legacy::draw_adapter& draw) final;

	private:
		void draw_player(const gui_legacy::draw_adapter& draw, sdk::cs_player_t* const ply) const;
		void draw_player_offscreen(const gui_legacy::draw_adapter& draw, sdk::cs_player_t* const ply, const sdk::color& clr) const;
		void draw_entity(const gui_legacy::draw_adapter& draw, sdk::entity_t* const ent) const;
		sdk::vec2 draw_nade_impact(const gui_legacy::draw_adapter& draw, sdk::weapon_t* const wpn, bool molotov = false) const;

		void draw_box(const gui_legacy::draw_adapter& draw, const std::pair<gui_legacy::vec2, gui_legacy::vec2>& box, const sdk::color& clr, bool drop_shadow = true) const;
		void draw_bar(const gui_legacy::draw_adapter& draw, const std::pair<gui_legacy::vec2, gui_legacy::vec2>& box, const sdk::color& clr, float percent, float old_percent, bool text = false) const;
		void draw_bones(const gui_legacy::draw_adapter& draw, sdk::cs_player_t* player, const sdk::color& clr) const;

		std::wstring get_player_name(sdk::cs_player_t* const player) const;
		void shorten_str(std::wstring& str) const;
		uint32_t calculate_distance_to_object(sdk::entity_t* const entity) const;
		float estimate_nade_damage_fraction(sdk::cs_player_t* const thrower, sdk::cs_player_t* const player, const sdk::vec3& impact, bool molotov) const;

		std::optional<std::pair<gui_legacy::vec2, gui_legacy::vec2>> get_player_bounds(sdk::cs_player_t* const player) const;
		bool is_point_in_viewport(sdk::vec3& vec) const;
		void normalize_box(std::pair<gui_legacy::vec2, gui_legacy::vec2>& box) const;
		sdk::vec2 rotate_offscreen(sdk::vec3 pos, float offset, float& rotation) const;

		std::array<std::pair<sdk::entity_t*, float>, size_t(sdk::class_id::spore_trail) + 1> closest_entity{};
	};

	extern std::shared_ptr<esp> box_esp;
}

#endif // HACKS_ESP_H
