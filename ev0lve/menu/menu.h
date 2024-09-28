
#ifndef MENU_MENU_H
#define MENU_MENU_H

#include <gui/gui.h>
#include <gui/controls.h>

namespace menu
{
	namespace tools
	{
		using namespace evo::gui;
		using namespace evo::ren;

		std::shared_ptr<control> make_stacked_groupboxes(uint32_t id, const vec2& size, const std::vector<std::shared_ptr<control>>& groups);
	}

	namespace group
	{
		using namespace evo::gui;
		using namespace evo::ren;

		void rage_tab(std::shared_ptr<layout>& e);
		void rage_general(std::shared_ptr<layout>& e);
		void rage_antiaim(std::shared_ptr<layout>& e);
		void rage_fakelag(std::shared_ptr<layout>& e);
		void rage_desync(std::shared_ptr<layout>& e);

		std::shared_ptr<layout> rage_weapon(const std::string& group, int num, const vec2& size, bool vis = false);

		void visuals_players(std::shared_ptr<layout>& e);
		void visuals_world(std::shared_ptr<layout>& e);
		void visuals_local(std::shared_ptr<layout>& e);
		void visuals_removals(std::shared_ptr<layout>& e);

		void misc_general(std::shared_ptr<layout>& e);
		void misc_settings(std::shared_ptr<layout>& e);
		void misc_helpers(std::shared_ptr<layout>& e);
		void misc_configs(std::shared_ptr<layout>& e);

		void scripts_general(std::shared_ptr<layout>& e);
		void scripts_elements(std::shared_ptr<layout>& e);

        void skinchanger_select(std::shared_ptr<layout>& e);
        void skinchanger_settings(std::shared_ptr<layout>& e);
        void skinchanger_preview(std::shared_ptr<layout>& e);
	}

	class menu_manager
	{
	public:
		void create();
		void toggle();
		void finalize();

		bool is_open();

		bool did_finalize{};
		std::weak_ptr<evo::gui::window> main_wnd{};
	private:
		std::shared_ptr<evo::ren::texture> create_in_game_texture(const char* name);
		std::shared_ptr<evo::ren::texture> create_in_game_weapon_texture(const char* name);
	};

	inline menu_manager menu{};
}

#endif // MENU_MENU_H
