
#ifndef SDK_VIEW_RENDER_H
#define SDK_VIEW_RENDER_H

#include <sdk/macros.h>
#include <sdk/vec3.h>

namespace sdk
{
	class view_setup
	{
	private:
		char _0x0000[16];
	public:
		int32_t x;
		int32_t x_old;
		int32_t y;
		int32_t y_old;
		int32_t width;
		int32_t width_old;
		int32_t height;
		int32_t height_old;
	private:
		char _0x0030[128];
	public:
		float fov;
		float fov_viewmodel;
		vec3 origin;
		angle angles;
		float z_near;
		float z_far; 
		float z_near_viewmodel;
		float z_far_viewmodel; 
		float aspect_ratio;
		float near_blur_depth;
		float near_focus_depth;
		float far_focus_depth;
		float far_blur_depth;
		float near_blur_radius;
		float far_blur_radius; 
		float dof_quality;
		int32_t motion_blur_mode;
	private:
		char _0x0104[68];
	public:
		int32_t edge_blur;
	};

	class view_render_t
	{
	public:
		VIRTUAL(13, get_view_setup(), view_setup* const (__thiscall*)(void*))()
	};
}

#endif // SDK_VIEW_RENDER_H
