
#ifndef GUI_PUBLIC_SOFTWARE_ITEM_H
#define GUI_PUBLIC_SOFTWARE_ITEM_H

#include <gui/control.h>

namespace gui
{
	class public_software_item : public evo::gui::control
	{
	public:
		public_software_item(uint32_t id, const std::string& name, const std::string& license, const std::string& url)
			: evo::gui::control(id, {}, {0.f, 40.f}), name(name), license(license), url(url)
		{
			size_to_parent_w = true;
			init();
		}
		
		void on_mouse_down(char key) override;
		void on_mouse_enter() override;
		void on_mouse_leave() override;
		
		void render() override;
		
	private:
		void init();
		
		std::string name{};
		std::string license{};
		std::string url{};
	};
}

#endif // GUI_PUBLIC_SOFTWARE_ITEM_H
