
#ifndef SDK_RENDER_VIEW_H
#define SDK_RENDER_VIEW_H

#include <sdk/macros.h>
#include <sdk/color.h>

namespace sdk
{
	class render_view_t
	{
		VIRTUAL(6, set_color_modulation_internal(float const* col), void(__thiscall*)(void*, float const*))(col)

	public:
		VIRTUAL(4, set_blend(const float blend), void(__thiscall*)(void*, float))(blend)
		VIRTUAL(5, get_blend(), float(__thiscall*)(void*))()

		void set_color_modulation(const color color)
		{
			float col[4] = { color.red() / 255.f, color.green() / 255.f, color.blue() / 255.f, color.alpha() / 255.f };
			set_color_modulation_internal(col);
		}
	};
}

#endif // SDK_RENDER_VIEW_H
