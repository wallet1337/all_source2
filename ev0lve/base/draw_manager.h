
#ifndef BASE_DRAW_MANAGER_H
#define BASE_DRAW_MANAGER_H

#include <gui_legacy/gui.h>
#include <detail/dx_adapter.h>

class draw_manager
{
	using draw_impl = detail::dx_adapter;

public:
	draw_manager();

	void draw(bool ontop = false);
	void reset() const;

private:
	draw_impl adapter;
	std::vector<std::shared_ptr<gui_legacy::drawable>> draw_list;
	std::vector<std::shared_ptr<gui_legacy::drawable>> draw_list_ontop;
};

extern draw_manager draw_mgr;

#endif // BASE_DRAW_MANAGER_H
