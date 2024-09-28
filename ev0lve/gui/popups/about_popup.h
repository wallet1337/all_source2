
#ifndef GUI_POPUPS_ABOUT_POPUP_H
#define GUI_POPUPS_ABOUT_POPUP_H

#include <gui/controls/popup.h>

namespace gui::popups
{
	class about_popup : public evo::gui::popup
	{
	public:
		explicit about_popup(uint32_t id) : evo::gui::popup(id, {}, {})
		{
			allow_move = true;
		}
		
		void on_first_render_call() override;
		void render() override;
		
	private:
		std::vector<std::tuple<std::string, std::string>> credits {
			{ XOR("raxer"), XOR("Core / Bugs") },
			{ XOR("imi-tat0r"), XOR("Core / Misc") },
			{ XOR("panzerfaust"), XOR("Lua API / GUI") },
		};
		
		std::vector<std::tuple<std::string, std::string>> special_thanks {
			{ XOR("nardow"), XOR("Design") },
			{ XOR("enQ"), XOR("API Docs / Testing") },
			{ XOR("Alpha Team"), XOR("Testing") },
			{ XOR("Beta Team"), XOR("Testing") },
		};
	};
}

#endif // GUI_POPUPS_ABOUT_POPUP_H
