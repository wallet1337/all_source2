
#ifndef GUI_POPUPS_PUBLIC_SOFTWARE_POPUP_H
#define GUI_POPUPS_PUBLIC_SOFTWARE_POPUP_H

#include <gui/controls/popup.h>

namespace gui::popups
{
	class public_software_popup : public evo::gui::popup
	{
	public:
		explicit public_software_popup(uint32_t id) : evo::gui::popup(id, {}, {})
		{
			allow_move = true;
		}
		
		void on_first_render_call() override;
		void render() override;
	
	private:
		// TODO: use XOR_STR_STORE instead, otherwise you have the unencrypted string in memory...
		std::vector<std::tuple<std::string, std::string, std::string>> links
		{
			{ XOR("libressl"), XOR("OpenSSL License"), XOR("https://github.com/libressl/libressl/blob/master/src/LICENSE") },
			{ XOR("json"), XOR("MIT License"), XOR("https://github.com/nlohmann/json/blob/develop/LICENSE.MIT") },
			{ XOR("gzip-hpp"), XOR("BSD 2-Clause License"), XOR("https://github.com/mapbox/gzip-hpp/blob/master/LICENSE.md") },
			{ XOR("LuaJIT"), XOR("MIT License"), XOR("https://github.com/LuaJIT/LuaJIT/blob/v2.1/COPYRIGHT") },
		};
	};
}

#endif // GUI_POPUPS_PUBLIC_SOFTWARE_POPUP_H
