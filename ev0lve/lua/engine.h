
#ifndef LUA_ENGINE_H
#define LUA_ENGINE_H

#include <lua/state.h>
#include <lua/state_bus.h>
#include <mutex>

namespace lua
{
	enum script_type
	{
		st_script,	// regular script
		st_library,	// library
		st_remote	// workshop script
	};

	struct script_metadata
	{
		std::optional<std::string> name{};
		std::optional<std::string> description{};
		std::optional<std::string> author{};
		bool use_state{};
	};
	
	struct script_file
	{
		script_type type{};
		std::string name{};
		script_metadata metadata{};
		bool should_load{};
		bool should_unload{};
		
		std::string get_name() const
		{
			return metadata.name.value_or(name);
		}
		
		std::string get_file_path() const;
		void parse_metadata();

		[[nodiscard]]
		uint32_t make_id() const;
	};
	
	class library;

	class script
	{
	public:
		~script()
		{
			unload();
		}

		void initialize();
		virtual void call_main();
		void unload();

		void create_forward(const char* n);
		bool has_forward(uint32_t hash);
		void call_forward(uint32_t hash, const std::function<int(state&)>& arg_callback = nullptr);

		void run_timers() const;

		void use_library(uint32_t lib);
		
		void add_gui_element(uint32_t id);
		void add_gui_element_with_callback(uint32_t id);
		void add_font(uint32_t id);
		void add_texture(uint32_t id);
		void add_animator(uint32_t id);

		uint32_t id{};		// hashed name
		std::string name{};	// script name
		std::string file{}; // file path

		bool is_running{};	// determines if script should run
		script_type type{};	// script type
		state l{};			// state wrapper
	
	protected:
		std::unordered_map<uint32_t, std::string> forwards{};	// global function list
		std::vector<uint32_t> gui_elements{};
		std::vector<uint32_t> gui_callbacks{};
		std::vector<uint32_t> fonts{};
		std::vector<uint32_t> textures{};
		std::vector<uint32_t> animators{};
		std::vector<std::shared_ptr<library>> libs{};

		void find_forwards();
		void hide_globals();
		void register_namespaces();
		void register_globals();
		void register_types();
	};
	
	class library : public script
	{
	public:
		void call_main() override;

		std::shared_ptr<state_bus> get_bus()
		{
			return bus;
		}
		
		void lock();
		void unlock();
	
	private:
		std::mutex ref_mutex{};
		int reference_count{};
		
		std::shared_ptr<state_bus> bus{};
	};
	
	using script_t = std::shared_ptr<script>;

	class engine
	{
	public:
		~engine()
		{
			scripts.clear();
			libraries.clear();
		}
		
		void refresh_scripts();

		void run_autoload();
		void enable_autoload(const script_file& file);
		void disable_autoload(const script_file& file);
		bool is_autoload_enabled(const script_file& file);
		
		bool run_script(const script_file& file, bool sounds = true);
		void stop_script(const script_file& file);
		void stop_all();
		
		bool run_library(const script_file& file);
		void stop_library(uint32_t id);

		void run_timers();

		script_t find_by_state(lua_State* s);
		script_t find_by_id(uint32_t id);
		
		void create_callback(const char* n);
		void callback(uint32_t id, const std::function<int(state&)>& arg_callback = nullptr);
		void for_each_script_name(const std::function<void(const script_file&)>& fn);

		bool exists(uint32_t id)
		{
			return scripts.find(id) != scripts.end() && scripts[id]->is_running;
		}

		bool in_render{};

	private:
		void write_autoload();
		
		std::mutex access_mutex{};
		std::mutex library_mutex{};
		
		std::unordered_map<uint32_t, script_t> scripts{};
		std::unordered_map<uint32_t, script_t> libraries{};
		std::vector<script_file> script_files{};
		std::vector<uint32_t> autoload{};
	};

	inline engine api{};
}

#endif // LUA_ENGINE_H
