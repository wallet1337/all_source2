
#include <base/hook_manager.h>
#include <menu/menu.h>
#include <sdk/input_system.h>
#include <base/cfg.h>

#ifdef CSGO_LUA
#include <lua/engine.h>
#endif /* CSGO_LUA */

namespace hooks::inputsystem
{
	LRESULT __stdcall wnd_proc(HWND wnd, UINT msg, WPARAM w_param, LPARAM l_param)
	{
		if (!evo::gui::ctx)
			return hook_manager.wnd_proc->call(wnd, msg, w_param, l_param);

#ifdef CSGO_LUA
		lua::api.callback(FNV1A("on_input"), [&](lua::state& s) -> int {
			s.push(static_cast<int>(msg));
			s.push(static_cast<int>(w_param));
			s.push(static_cast<int>(l_param));

			return 3;
		});
#endif

		if (!evo::gui::ctx->active && msg == XOR_32(WM_KEYDOWN))
		{
			if (w_param == XOR_32(VK_DELETE) || w_param == XOR_32(VK_INSERT))
			{
				menu::menu.toggle();
				return true;
			}

			if (w_param == XOR_32(VK_ESCAPE) && menu::menu.is_open())
			{
				if (evo::gui::ctx->active_popup)
					evo::gui::ctx->active_popup->close();
				else
					menu::menu.toggle();

				return true;
			}
		}

		if (menu::menu.is_open())
			SetCursor(nullptr);

		auto ret = evo::gui::ctx->refresh(msg, w_param, l_param) && menu::menu.is_open();
		return ret || hook_manager.wnd_proc->call(wnd, msg, w_param, l_param);
	}
}
