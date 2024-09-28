
#ifndef SDK_MODEL_RENDER_H
#define SDK_MODEL_RENDER_H

#include <sdk/vec2.h>
#include <sdk/vec3.h>
#include <sdk/mat.h>
#include <sdk/color.h>
#include <sdk/macros.h>
#include <sdk/keyvalues.h>
#include <sdk/client_entity.h>
#include <sdk/model_info_client.h>

namespace sdk
{
	inline constexpr auto studio_render = 1;

	enum material_primitive_type
	{
		material_points,
		material_lines,
		material_triangles,
		material_triangle_strip,
		material_line_strip,
		material_line_loop,
		material_polygon,
		material_quads,
		material_subd_quads_extra,
		material_subd_quads_reg,
		material_instanced_quads,
		material_heterogenous
	};
	
	struct model
	{
		uintptr_t pad;
		char name[255];
	};

	struct model_render_info_t
	{
		vec3 origin;
		angle angles;
		char pad[0x4];
		client_renderable* renderable;
		const model* model;
		const mat3x4* model_to_world;
		const mat3x4* lighting_offset;
		const vec3* lighting_origin;
		int flags;
		int entity_index;
		int skin;
		int body;
		int hitboxset;
		uint32_t instance;
	};

	class material_var
	{
		VIRTUAL(10, set_vector_internal(const float x, const float y), void(__thiscall*)(void*, float, float))(x, y)
		VIRTUAL(11, set_vector_internal(const float x, const float y, const float z), void(__thiscall*)(void*, float, float, float))(x, y, z)

	public:
		VIRTUAL(4, set_float(const float val), void(__thiscall*)(void*, float))(val)
		VIRTUAL(5, set_int(const int val), void(__thiscall*)(void*, int))(val)
		VIRTUAL(20, set_matrix(viewmat& matrix), void(__thiscall*)(void*, viewmat&))(matrix)
		VIRTUAL(26, set_vector_component(const float val, const int comp), void(__thiscall*)(void*, float, int))(val, comp)

		void set_vector(const vec2 vector)
		{
			set_vector_internal(vector.x, vector.y);
		}

		void set_vector(const vec3 vector)
		{
			set_vector_internal(vector.x, vector.y, vector.z);
		}
	};

	enum material_var_flags
	{
		material_var_debug = 1 << 0,
		material_var_no_debug_override = 1 << 1,
		material_var_no_draw = 1 << 2,
		material_var_use_in_fillrate_mode = 1 << 3,
		material_var_vertexcolor = 1 << 4,
		material_var_vertexalpha = 1 << 5,
		material_var_selfillum = 1 << 6,
		material_var_additive = 1 << 7,
		material_var_alphatest = 1 << 8,
		material_var_znearer = 1 << 10,
		material_var_model = 1 << 11,
		material_var_flat = 1 << 12,
		material_var_nocull = 1 << 13,
		material_var_nofog = 1 << 14,
		material_var_ignorez = 1 << 15,
		material_var_decal = 1 << 16,
		material_var_envmapsphere = 1 << 17,
		material_var_envmapcameraspace = 1 << 19,
		material_var_basealphaenvmapmask = 1 << 20,
		material_var_translucent = 1 << 21,
		material_var_normalmapalphaenvmapmask = 1 << 22,
		material_var_needs_software_skinning = 1 << 23,
		material_var_opaquetexture = 1 << 24,
		material_var_envmapmode = 1 << 25,
		material_var_suppress_decals = 1 << 26,
		material_var_halflambert = 1 << 27,
		material_var_wireframe = 1 << 28,
		material_var_allowalphatocoverage = 1 << 29,
		material_var_alpha_modified_by_proxy = 1 << 30,
		material_var_vertexfog = 1 << 31
	};

	struct renderable_info
	{		
		client_renderable* renderable;
		uintptr_t alpha_prop;
		int32_t enum_count;
		int32_t render_frame;
		uint16_t first_shadow;
		uint16_t first_leaf;
		int16_t area;
		uint16_t flags;
		uint16_t render_fast_reflection : 1;
		uint16_t disable_shadow_depth_rendering : 1;
		uint16_t disable_csm_rendering : 1;
		uint16_t disable_shadow_depth_caching : 1;
		uint16_t split_screen_enabled : 2;
		uint16_t translucency_type : 2;
		uint16_t model_type : 8;
		vec3 bloated_abs_mins;
		vec3 bloated_abs_maxs;
		vec3 abs_mins;
		vec3 abs_maxs;
		int32_t pad;
	};

	class texture
	{
	public:
		VIRTUAL(0, get_name(), const char*(__thiscall*)(void*))()
		VIRTUAL(3, get_actual_width(), int32_t(__thiscall*)(void*))()
		VIRTUAL(4, get_actual_height(), int32_t(__thiscall*)(void*))()
	};

	class material;
	
	struct stencil_state
	{
		bool enable = true;
		uint32_t fail = 1;
		uint32_t zfail = 3;
		uint32_t pass = 1;
		uint32_t cmp = 8;
		int32_t reference = 1;
		uint32_t test = 0xFFFFFFFF;
		uint32_t write = 0xFFFFFFFF;
	};

	class mat_render_context
	{
	public:
		VIRTUAL(1, release(), int(__thiscall*)(void*))()
		VIRTUAL(6, set_render_target(texture* t), void(__thiscall*)(void*, texture*))(t)
		VIRTUAL(7, get_render_target(), texture*(__thiscall*)(void*))()
		VIRTUAL(41, get_view_port(int32_t* x, int32_t* y, int32_t* width, int32_t* height), void(__thiscall*)(void*, int32_t*, int32_t*, int32_t*, int32_t*))(x, y, width, height)
		VIRTUAL(114, draw_screen_space_rectangle(material* mat, int x, int y, int w, int h, float text_x0, float text_y0, float text_x1, float text_y1, int text_w, int text_h, void* renderable = nullptr, int x_dice = -1, int y_dice = -1), void(__thiscall*)(void*, material*, int, int, int, int, float, float, float, float, int, int, void*, int, int))(mat, x, y, w, h, text_x0, text_y0, text_x1, text_y1, text_w, text_h, renderable, x_dice, y_dice)
		VIRTUAL(119, push_render_target_and_viewport(), void(__thiscall*)(void*))()
		VIRTUAL(120, pop_render_target_and_viewport(), void(__thiscall*)(void*))()
		VIRTUAL(128, set_stencil_state(stencil_state* state), void(__thiscall*)(void*, stencil_state*))(state)
		VIRTUAL(154, set_tone_mapping_scale_linear(vec3* vec), void(__thiscall*)(void*, vec3*))(vec)
		VIRTUAL(155, get_tone_mapping_scale_linear(), vec3(__thiscall*)(void*))()

		inline material* get_current_material() { return *reinterpret_cast<material**>(uintptr_t(this) + 0xC); }
		inline void set_current_material(material* mat) { *reinterpret_cast<material**>(uintptr_t(this) + 0xC) = mat; }

		inline material* get_override_material() { return *reinterpret_cast<material**>(uintptr_t(this) + 0x24C); }
		inline void set_override_material(material* mat) { *reinterpret_cast<material**>(uintptr_t(this) + 0x24C) = mat; }
	};

	class shader
	{
	public:
		VIRTUAL(0, get_name(), const char*(__thiscall*)(void*))()
	};

	class material
	{
		VIRTUAL(28, color_modulate_internal(const float r, const float g, const float b), void(__thiscall*)(void*, float, float, float))(r, g, b)

	public:
		VIRTUAL(0, get_name(), const char* (__thiscall*)(void*))()
		VIRTUAL(1, get_group(), const char* (__thiscall*)(void*))()
		VIRTUAL(12, increment_reference_count(), void(__thiscall*)(void*))()
		VIRTUAL(13, decrement_reference_count(), void(__thiscall*)(void*))()
		VIRTUAL(27, alpha_modulate(const float a), void(__thiscall*)(void*, float))(a)
		VIRTUAL(29, set_material_var_flag(const material_var_flags flag, const bool on), void(__thiscall*)(void*, material_var_flags, bool))(flag, on)
		VIRTUAL(37, refresh(), void(__thiscall*)(void*))()
		VIRTUAL(47, find_var_fast(char const* name, uint32_t* token), material_var*(__thiscall*)(void*, const char*, uint32_t*))(name, token)
		VIRTUAL(68, get_shader(), shader*(__thiscall*)(void*))()

		inline void modulate(const color clr)
		{
			color_modulate_internal(clr.red() / 255.f, clr.green() / 255.f, clr.blue() / 255.f);
			alpha_modulate(clr.alpha() / 255.f);
		}
	};
	
	struct mesh_instance_data
	{
		int32_t index_offset;
		int32_t index_count;
		int32_t bone_count;
		uintptr_t bone_remap;
		mat3x4* pose;
		texture* env_cubemap;
		uintptr_t lighting_state;
		material_primitive_type prim_type;
		uintptr_t vertex;
		int32_t vertex_offset;
		uintptr_t index;
		uintptr_t color;
		int32_t color_offset;
		uintptr_t stencil_state;
		quaternion diffuse_modulation;
		int32_t lightmap_page_id;
		bool indirect_lighting_only;
	};

	class model_render_t
	{
	public:
		VIRTUAL(0, draw_model(int32_t flags, client_renderable* ren, model_instance_handle inst, int32_t index, const model* m, const vec3* org, const angle* ang, int32_t skin, int32_t body, int32_t set, mat3x4* to_world = nullptr, mat3x4* lighting = nullptr),
				int32_t(__thiscall*)(void*, int32_t, client_renderable*, model_instance_handle, int32_t, const model*, const vec3*, const angle*, int32_t, int32_t, int32_t, mat3x4*, mat3x4*))(flags, ren, inst, index, m, org, ang, skin, body, set, to_world, lighting)
		VIRTUAL(1, forced_material_override(material* mat), void(__thiscall*)(void*, material*, int, int))(mat, 0, -1)
		VIRTUAL(4, create_instance(client_renderable* ren, void* cache = nullptr), model_instance_handle(__thiscall*)(void*, client_renderable*, void*))(ren, cache)
		VIRTUAL(5, destroy_instance(model_instance_handle handle), void(__thiscall*)(void*, model_instance_handle))(handle)
	};

	class material_system_t
	{
	public:
		VIRTUAL(83, create_material(const char* name, keyvalues* kv), material*(__thiscall*)(void*, const char*, keyvalues*))(name, kv)
		VIRTUAL(84, find_material(const char* mat, const char* group = nullptr), material*(__thiscall*)(void*, const char*, const char*, bool, const char*))(mat, group, true, nullptr)
		VIRTUAL(91, find_texture(const char* name, const char* groupname, bool complain = true, int32_t flags = 0), texture*(__thiscall*)(void*, const char*, const char*, bool, int32_t))(name, groupname, complain, flags)
		VIRTUAL(115, get_render_context(), mat_render_context*(__thiscall*)(void*))()
	};
}

#endif // SDK_MODEL_RENDER_H
