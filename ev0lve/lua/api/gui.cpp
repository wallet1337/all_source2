
#include <lua/api_def.h>
#include <gui/gui.h>
#include <gui/controls.h>
#include <gui/helpers.h>
#include <lua/engine.h>
#include <lua/helpers.h>
#include <base/cfg.h>
#include <menu/menu.h>

#include <tuple>
#include <gui/controls/hotkey.h>

using namespace evo::gui;
using namespace evo::gui::helpers;

namespace lua::gui_helpers
{
	inline std::tuple<uint32_t, uint32_t, std::shared_ptr<control_container>> get_ids_and_container(runtime_state& s)
	{
		// get our id
		const auto id = hash(s.get_string(1));

		// get container
		const auto cont_id = hash(s.get_string(2));
		const auto cont = ctx->find<control_container>(cont_id);

		return std::make_tuple(id, cont_id, cont);
	}

	inline bool check_eligibility(runtime_state& s, const std::shared_ptr<control_container>& cont, uint32_t id)
	{
		// check if container does exist
		if (!cont)
		{
			s.error(tfm::format(XOR_STR("container was not found: %s"), s.get_string(2)));
			return false;
		}

		// check if element with the same id does already exist
		if (ctx->find(id))
		{
			s.error(tfm::format(XOR_STR("control id is already in use: %s"), s.get_string(1)));
			return false;
		}

		return true;
	}
	
	inline std::optional<int> find_set_bit(const std::string& s, const std::shared_ptr<combo_box>& cb)
	{
		const auto h = util::fnv1a(s.c_str());
		
		auto n = 0;
		cb->for_each_control_logical([&n, h](std::shared_ptr<control>& c) {
			const auto m_s = c->as<selectable>();
			if (util::fnv1a(m_s->text.c_str()) == h)
			{
				if (m_s->value != -1)
					n = m_s->value;
				
				return true;
			}
			
			++n;
			return false;
		});
		
		return n >= cb->count() ? std::nullopt : std::make_optional(n);
	}
}

int lua::api_def::gui::color_picker_construct(lua_State *l)
{
	runtime_state s(l);
	
	const auto r = s.check_arguments({
		{ ltd::string },
		{ ltd::string },
		{ ltd::string },
		{ ltd::table },
		{ ltd::boolean, true },
	});
	
	if (!r)
	{
		s.error(XOR_STR("usage: color_picker(id, container_id, label, default, allow_alpha = true)"));
		return 0;
	}
	
	// find our script beforehand
	const auto& me = api.find_by_state(l);
	if (!me)
	{
		s.error(XOR_STR("FATAL: could not find the script. Did it escape the sandbox?"));
		return 0;
	}
	
	// get stuff
	const auto [id, cont_id, cont] = gui_helpers::get_ids_and_container(s);
	
	// check if we can create this control at all
	if (!gui_helpers::check_eligibility(s, cont, id))
		return 0;
	
	// create the entry in config
	const auto already_exists = cfg.unk_color_entries.find(id) != cfg.unk_color_entries.end();
	if (!cfg.register_lua_color_entry(id))
	{
		s.error(XOR_STR("config entry ID is already in use internally"));
		return 0;
	}
	
	if (!already_exists)
		cfg.unk_color_entries[id].set({
			s.get_field_integer(XOR_STR("r"), 4),
			s.get_field_integer(XOR_STR("g"), 4),
			s.get_field_integer(XOR_STR("b"), 4),
			s.get_field_integer(XOR_STR("a"), 4)
		});
	
	// finally, create our control
	const auto el = std::make_shared<color_picker>(id, cfg.unk_color_entries[id], s.get_stack_top() == 5 ? s.get_boolean(5) : true);
	el->do_first_render_call();
	el->reset();
	
	// add with label to container
	const auto c = make_control(s.get_string(1), s.get_string(3), el);
	c->do_first_render_call();
	c->reset();
	
	cont->add(c);
	cont->process_queues();
	cont->reset();
	
	// register in the script
	me->add_gui_element(c->id);
	
	// create lua usertype
	std::weak_ptr<color_picker> obj{el};
	s.create_user_object<decltype(obj)>(XOR_STR("gui.color_picker"), &obj);
	
	return 1;
}

int lua::api_def::gui::textbox_construct(lua_State *l)
{
	runtime_state s(l);

	const auto r = s.check_arguments({
		{ ltd::string },
		{ ltd::string },
	});

	if (!r)
	{
		s.error(XOR_STR("usage: textbox(id, container_id)"));
		return 0;
	}

	// find our script beforehand
	const auto& me = api.find_by_state(l);
	if (!me)
	{
		s.error(XOR_STR("FATAL: could not find the script. Did it escape the sandbox?"));
		return 0;
	}

	// get stuff
	const auto [id, cont_id, cont] = gui_helpers::get_ids_and_container(s);

	// check if we can create this control at all
	if (!gui_helpers::check_eligibility(s, cont, id))
		return 0;

	// finally, create our control
	const auto el = std::make_shared<text_input>(id);
	el->do_first_render_call();
	el->reset();

	const auto c = make_inlined(hash(std::string(s.get_string(1)) + XOR("_content")), [&](std::shared_ptr<layout> &e) {
		e->add(el);
	});
	c->do_first_render_call();
	c->reset();

	cont->add(c);
	cont->process_queues();
	cont->reset();

	// register in the script
	me->add_gui_element(c->id);

	// create lua usertype
	std::weak_ptr<text_input> obj{el};
	s.create_user_object<decltype(obj)>(XOR_STR("gui.textbox"), &obj);

	return 1;
}

int lua::api_def::gui::checkbox_construct(lua_State *l)
{
	runtime_state s(l);

	const auto r = s.check_arguments({
		{ ltd::string },
		{ ltd::string },
		{ ltd::string },
	});

	if (!r)
	{
		s.error(XOR_STR("usage: checkbox(id, container_id, label)"));
		return 0;
	}

	// find our script beforehand
	const auto& me = api.find_by_state(l);
	if (!me)
	{
		s.error(XOR_STR("FATAL: could not find the script. Did it escape the sandbox?"));
		return 0;
	}

	// get stuff
	const auto [id, cont_id, cont] = gui_helpers::get_ids_and_container(s);

	// check if we can create this control at all
	if (!gui_helpers::check_eligibility(s, cont, id))
		return 0;

	// create the entry in config
	if (!cfg.register_lua_bool_entry(id))
	{
		s.error(XOR_STR("config entry ID is already in use internally"));
		return 0;
	}

	// finally, create our control
	const auto el = std::make_shared<checkbox>(id, cfg.unk_bool_entries[id]);
	el->do_first_render_call();
	el->reset();

	// add with label to container
	const auto c = make_control(s.get_string(1), s.get_string(3), el);
	c->do_first_render_call();
	c->reset();
	
	cont->add(c);
	cont->process_queues();
	cont->reset();

	// register in the script
	me->add_gui_element(c->id);

	// create lua usertype
	std::weak_ptr<checkbox> obj{el};
	s.create_user_object<decltype(obj)>(XOR_STR("gui.checkbox"), &obj);

	return 1;
}

int lua::api_def::gui::slider_construct(lua_State *l)
{
	runtime_state s(l);
	
	const auto r = s.check_arguments({
		{ ltd::string },
		{ ltd::string },
		{ ltd::string },
		{ ltd::number },
		{ ltd::number },
		{ ltd::string, true },
		{ ltd::number, true },
	});
	
	if (!r)
	{
		s.error(XOR_STR("usage: slider(id, container_id, label, min, max, format = \"%%.0f\")"));
		return 0;
	}
	
	// find our script beforehand
	const auto& me = api.find_by_state(l);
	if (!me)
	{
		s.error(XOR_STR("FATAL: could not find the script. Did it escape the sandbox?"));
		return 0;
	}
	
	// get stuff
	const auto [id, cont_id, cont] = gui_helpers::get_ids_and_container(s);
	
	// check if we can create this control at all
	if (!gui_helpers::check_eligibility(s, cont, id))
		return 0;
	
	// create the entry in config
	if (!cfg.register_lua_float_entry(id))
	{
		s.error(XOR_STR("config entry ID is already in use internally"));
		return 0;
	}
	
	// get format string
	std::string fmt{XOR_STR("%.0f")};
	if (s.get_stack_top() >= 6)
		fmt = s.get_string(6);
	
	// get step
	float step{1.f};
	if (s.get_stack_top() >= 7)
		step = s.get_float(7);

	// clamp default value
	if (s.get_float(4) != 0.f)
		cfg.unk_float_entries[id].set(s.get_float(4));
	
	// finally, create our control
	const auto el = std::make_shared<slider<float>>(id, s.get_float(4), s.get_float(5), cfg.unk_float_entries[id], fmt, step);
	el->do_first_render_call();
	el->reset();
	
	// add with label to container
	const auto c = make_control(s.get_string(1), s.get_string(3), el);
	c->do_first_render_call();
	c->reset();
	
	cont->add(c);
	cont->process_queues();
	cont->reset();
	
	// register in the script
	me->add_gui_element(c->id);
	
	// create lua usertype
	std::weak_ptr<slider<float>> obj{el};
	s.create_user_object<decltype(obj)>(XOR_STR("gui.slider"), &obj);
	
	return 1;
}

int lua::api_def::gui::combobox_construct(lua_State *l)
{
	runtime_state s(l);

	auto r = s.check_arguments({
		{ ltd::string },
		{ ltd::string },
		{ ltd::string },
		{ ltd::boolean },
		{ ltd::string },
	}, true);

	if (!r)
	{
		r = s.check_arguments({
			{ ltd::string },
			{ ltd::string },
			{ ltd::string },
			{ ltd::string },
		}, true);

		if (!r)
		{
			s.error(XOR_STR("usage: combobox(id, container_id, label, multi_select, values...)"));
			return 0;
		}

		lua::helpers::warn(XOR_STR("deprecated: combobox(id, container_id, label, values...)\nuse combobox(id, container_id, label, multi_select (true/false), values...) instead"));
	}
	
	// find our script beforehand
	const auto& me = api.find_by_state(l);
	if (!me)
	{
		s.error(XOR_STR("FATAL: could not find the script. Did it escape the sandbox?"));
		return 0;
	}
	
	// get stuff
	const auto [id, cont_id, cont] = gui_helpers::get_ids_and_container(s);
	
	// check if we can create this control at all
	if (!gui_helpers::check_eligibility(s, cont, id))
		return 0;
	
	// create the entry in config
	if (!cfg.register_lua_bit_entry(id))
	{
		s.error(XOR_STR("config entry ID is already in use internally"));
		return 0;
	}
	
	// finally, create our control
	const auto el = std::make_shared<combo_box>(id, cfg.unk_bit_entries[id]);

	// make multi_select (optional)
	if (s.is_boolean(4) && s.get_boolean(4))
		el->allow_multiple = true;

	for (auto i = 4; i <= s.get_stack_top(); ++i)
	{
		if (!s.check_argument_type(i, ltd::string))
			continue;
		
		const auto sel = std::make_shared<selectable>(hash(s.get_string(1) + std::string(XOR_STR(".")) + s.get_string(i)), s.get_string(i));
		el->add(sel);
	}
	
	el->process_queues();
	el->do_first_render_call();
	el->reset();
	
	// add with label to container
	const auto c = make_control(s.get_string(1), s.get_string(3), el);
	c->do_first_render_call();
	c->reset();
	
	cont->add(c);
	cont->process_queues();
	cont->reset();
	
	// register in the script
	me->add_gui_element(c->id);
	
	// create lua usertype
	std::weak_ptr<combo_box> obj{el};
	s.create_user_object<decltype(obj)>(XOR_STR("gui.combobox"), &obj);
	
	return 1;
}

int lua::api_def::gui::label_construct(lua_State* l)
{
	runtime_state s(l);

	const auto r = s.check_arguments({
		{ ltd::string },
		{ ltd::string },
		{ ltd::string },
	});

	if (!r)
	{
		s.error(XOR_STR("usage: label(id, container_id, text)"));
		return 0;
	}

	// find our script beforehand
	const auto& me = api.find_by_state(l);
	if (!me)
	{
		s.error(XOR_STR("FATAL: could not find the script. Did it escape the sandbox?"));
		return 0;
	}

	// get stuff
	const auto [id, cont_id, cont] = gui_helpers::get_ids_and_container(s);

	// check if we can create this control at all
	if (!gui_helpers::check_eligibility(s, cont, id))
		return 0;

	// finally, create our control
	const auto el = std::make_shared<label>(id, s.get_string(3));
	el->do_first_render_call();
	el->reset();

	cont->add(el);
	cont->process_queues();
	cont->reset();

	// register in the script
	me->add_gui_element(el->id);

	// create lua usertype
	std::weak_ptr<label> obj{el};
	s.create_user_object<decltype(obj)>(XOR_STR("gui.label"), &obj);

	return 1;
}

int lua::api_def::gui::button_construct(lua_State* l)
{
	runtime_state s(l);

	const auto r = s.check_arguments({
		{ ltd::string },
		{ ltd::string },
		{ ltd::string },
	});

	if (!r)
	{
		s.error(XOR_STR("usage: button(id, container_id, text)"));
		return 0;
	}

	// find our script beforehand
	const auto& me = api.find_by_state(l);
	if (!me)
	{
		s.error(XOR_STR("FATAL: could not find the script. Did it escape the sandbox?"));
		return 0;
	}

	// get stuff
	const auto [id, cont_id, cont] = gui_helpers::get_ids_and_container(s);

	// check if we can create this control at all
	if (!gui_helpers::check_eligibility(s, cont, id))
		return 0;

	// finally, create our control
	const auto el = std::make_shared<button>(id, s.get_string(3));
	el->size.x = 160.f;
	el->do_first_render_call();
	el->reset();

	cont->add(el);
	cont->process_queues();
	cont->reset();

	// register in the script
	me->add_gui_element(el->id);

	// create lua usertype
	std::weak_ptr<button> obj{el};
	s.create_user_object<decltype(obj)>(XOR_STR("gui.button"), &obj);

	return 1;
}

int lua::api_def::gui::color_picker_get_value(lua_State *l)
{
	runtime_state s(l);
	
	const auto r = s.check_arguments({
		{ ltd::user_data },
	});
	
	if (!r)
	{
		s.error(XOR_STR("usage: obj:get_value()"));
		return 0;
	}
	
	const auto obj = *reinterpret_cast<std::weak_ptr<color_picker>*>(s.get_user_data(1));
	if (const auto el = obj.lock(); el)
	{
		s.create_table();
		s.set_field(XOR("r"), el->value.get().r());
		s.set_field(XOR("g"), el->value.get().g());
		s.set_field(XOR("b"), el->value.get().b());
		s.set_field(XOR("a"), el->value.get().a());
		return 1;
	}
	
	s.error(XOR_STR("control does not exist"));
	return 0;
}

int lua::api_def::gui::color_picker_set_value(lua_State *l)
{
	runtime_state s(l);
	
	const auto r = s.check_arguments({
		{ ltd::user_data },
		{ ltd::table }
	});
	
	if (!r)
	{
		s.error(XOR_STR("usage: obj:set_value(v)"));
		return 0;
	}
	
	const auto obj = *reinterpret_cast<std::weak_ptr<color_picker>*>(s.get_user_data(1));
	if (const auto el = obj.lock(); el)
	{
		el->value.set({
			s.get_field_integer(XOR_STR("r"), 2),
			s.get_field_integer(XOR_STR("g"), 2),
			s.get_field_integer(XOR_STR("b"), 2),
			s.get_field_integer(XOR_STR("a"), 2)
		});
		
		el->reset();
		
		return 0;
	}
	
	s.error(XOR_STR("control does not exist"));
	return 0;
}

int lua::api_def::gui::checkbox_get_value(lua_State *l)
{
	runtime_state s(l);

	const auto r = s.check_arguments({
											 { ltd::user_data },
									 });

	if (!r)
	{
		s.error(XOR_STR("usage: obj:get_value()"));
		return 0;
	}

	const auto obj = *reinterpret_cast<std::weak_ptr<checkbox>*>(s.get_user_data(1));
	if (const auto el = obj.lock(); el)
	{
		s.push(el->value.get());
		return 1;
	}

	s.error(XOR_STR("control does not exist"));
	return 0;
}

int lua::api_def::gui::checkbox_set_value(lua_State *l)
{
	runtime_state s(l);

	const auto r = s.check_arguments({
		{ ltd::user_data },
		{ ltd::boolean }
	});

	if (!r)
	{
		s.error(XOR_STR("usage: obj:set_value(v)"));
		return 0;
	}

	const auto obj = *reinterpret_cast<std::weak_ptr<checkbox>*>(s.get_user_data(1));
	if (const auto el = obj.lock(); el)
	{
		el->value.set(s.get_boolean(2));
		el->reset();

		return 0;
	}

	s.error(XOR_STR("control does not exist"));
	return 0;
}

int lua::api_def::gui::textbox_get_value(lua_State *l)
{
	runtime_state s(l);

	const auto r = s.check_arguments({
		{ ltd::user_data },
	});

	if (!r)
	{
		s.error(XOR_STR("usage: obj:get_value()"));
		return 0;
	}

	const auto obj = *reinterpret_cast<std::weak_ptr<text_input>*>(s.get_user_data(1));
	if (const auto el = obj.lock(); el)
	{
		s.push(el->value.c_str());
		return 1;
	}

	s.error(XOR_STR("control does not exist"));
	return 0;
}

int lua::api_def::gui::textbox_set_value(lua_State *l)
{
	runtime_state s(l);

	const auto r = s.check_arguments({
		{ ltd::user_data },
		{ ltd::string }
	});

	if (!r)
	{
		s.error(XOR_STR("usage: obj:set_value(v)"));
		return 0;
	}

	const auto obj = *reinterpret_cast<std::weak_ptr<text_input>*>(s.get_user_data(1));
	if (const auto el = obj.lock(); el)
	{
		el->set_value(s.get_string(2));
		el->reset();

		return 0;
	}

	s.error(XOR_STR("control does not exist"));
	return 0;
}

int lua::api_def::gui::combobox_get_value(lua_State *l)
{
	runtime_state s(l);
	
	const auto r = s.check_arguments({
		{ ltd::user_data },
	});
	
	if (!r)
	{
		s.error(XOR_STR("usage: obj:get_value()"));
		return 0;
	}
	
	const auto obj = *reinterpret_cast<std::weak_ptr<combo_box>*>(s.get_user_data(1));
	if (const auto el = obj.lock(); el)
	{
		const auto set = el->value->first_set_bit();
		if (el->allow_multiple)
		{
			if (!set)
				return 0;
			
			auto n = 0; auto added = 0;
			el->for_each_control([&](std::shared_ptr<control>& m) {
				const auto m_s = m->as<selectable>();
				if (!m_s)
					return;
				
				const auto v = m_s->value != -1 ? m_s->value : n;
				if (el->value->test(v))
				{
					s.push(m_s->text.c_str());
					++added;
				}
				
				++n;
			});
			
			return added;
		} else
		{
			auto n = 0;
			el->for_each_control_logical([&](std::shared_ptr<control>& m) {
				const auto m_s = m->as<selectable>();
				if (!m_s)
					return false;
				
				const auto v = m_s->value != -1 ? m_s->value : n;
				
				if ((el->legacy_mode && static_cast<int>(el->value.get()) == m_s->value) ||
					(!el->legacy_mode && set == v))
				{
					s.push(m_s->text.c_str());
					return true;
				}
				
				n++;
				return false;
			});
			
			return 1;
		}
	}
	
	s.error(XOR_STR("control does not exist"));
	return 0;
}

int lua::api_def::gui::combobox_set_value(lua_State *l)
{
	runtime_state s(l);
	if (s.get_stack_top() < 2 || !s.check_argument_type(1, ltd::user_data))
	{
		s.error(XOR_STR("usage: obj:set_value(values...)"));
		return 0;
	}
	
	const auto obj = *reinterpret_cast<std::weak_ptr<combo_box>*>(s.get_user_data(1));
	if (const auto el = obj.lock(); el)
	{
		el->value.get().reset();
		if (!el->allow_multiple)
		{
			const auto n = gui_helpers::find_set_bit(s.get_string(2), el);
			if (!n.has_value())
			{
				s.error(XOR_STR("entry not found"));
				return 0;
			}
			
			el->legacy_mode ? el->value.set(*n) : el->value.get().set(static_cast<char>(*n));
		} else
		{
			for (auto i = 2; i <= s.get_stack_top(); ++i)
			{
				if (!s.check_argument_type(i, ltd::string))
					continue;
				
				const auto n = gui_helpers::find_set_bit(s.get_string(i), el);
				if (!n.has_value())
					continue;
				
				el->value.get().set(static_cast<char>(*n));
			}
		}
		
		el->reset();
	}
	
	return 0;
}

int lua::api_def::gui::slider_get_value(lua_State *l)
{
	runtime_state s(l);

	const auto r = s.check_arguments({
		{ ltd::user_data },
	});

	if (!r)
	{
		s.error(XOR_STR("usage: obj:get_value()"));
		return 0;
	}

	const auto obj = *reinterpret_cast<std::weak_ptr<slider<float>>*>(s.get_user_data(1));
	if (const auto el = obj.lock(); el)
	{
		s.push(el->value.get());
		return 1;
	}

	s.error(XOR_STR("control does not exist"));
	return 0;
}

int lua::api_def::gui::slider_set_value(lua_State *l)
{
	runtime_state s(l);

	const auto r = s.check_arguments({
		{ ltd::user_data },
		{ ltd::number }
	});

	if (!r)
	{
		s.error(XOR_STR("usage: obj:set_value(v)"));
		return 0;
	}

	const auto obj = *reinterpret_cast<std::weak_ptr<slider<float>>*>(s.get_user_data(1));
	if (const auto el = obj.lock(); el)
	{
		el->value.set(std::clamp(s.get_float(2), el->low, el->high));
		el->reset();

		return 0;
	}

	s.error(XOR_STR("control does not exist"));
	return 0;
}

int lua::api_def::gui::control_set_tooltip(lua_State *l)
{
	runtime_state s(l);

	const auto r = s.check_arguments({
		{ ltd::user_data },
		{ ltd::string }
	});

	if (!r)
	{
		s.error(XOR_STR("usage: obj:set_tooltip(text)"));
		return 0;
	}

	const auto obj = *reinterpret_cast<std::weak_ptr<control>*>(s.get_user_data(1));
	if (const auto el = obj.lock(); el)
	{
		el->tooltip = s.get_string(2);
		return 0;
	}

	s.error(XOR_STR("control does not exist"));
	return 0;
}

int lua::api_def::gui::control_set_visible(lua_State *l)
{
	runtime_state s(l);
	
	const auto r = s.check_arguments({
		{ ltd::user_data },
		{ ltd::boolean }
	});
	
	if (!r)
	{
		s.error(XOR_STR("usage: obj:set_visible(v)"));
		return 0;
	}
	
	const auto obj = *reinterpret_cast<std::weak_ptr<control>*>(s.get_user_data(1));
	if (const auto el = obj.lock(); el)
	{
		const auto p = el->parent.lock();
		if (!p || !p->is_container)
		{
			s.error(XOR_STR("broken control"));
			return 0;
		}

		if (p->as<layout>()->direction == s_inline)
			p->set_visible(s.get_boolean(2));
		else
			el->set_visible(s.get_boolean(2));

		return 0;
	}
	
	s.error(XOR_STR("control does not exist"));
	return 0;
}

int lua::api_def::gui::control_add_callback(lua_State *l)
{
	runtime_state s(l);
	
	const auto r = s.check_arguments({
		{ ltd::user_data },
		{ ltd::function }
	});
	
	if (!r)
	{
		s.error(XOR_STR("usage: obj:add_callback(callback())"));
		return 0;
	}
	
	const auto& me = api.find_by_state(l);
	if (!me)
	{
		s.error(XOR_STR("FATAL: could not find the script. Did it escape the sandbox?"));
		return 0;
	}
	
	const auto obj = *reinterpret_cast<std::weak_ptr<control>*>(s.get_user_data(1));
	if (const auto el = obj.lock(); el)
	{
		const auto ref = s.registry_add();
		el->universal_callbacks[me->id].emplace_back([ref, l]() {
			runtime_state s(l);
			s.registry_get(ref);
			
			if (!s.is_nil(-1))
			{
				if (!s.call(0, 0))
					s.error(s.get_last_error_description());
			}
			else
				s.pop(1);
		});
		
		me->add_gui_element_with_callback(el->id);
	}
	
	return 0;
}

int lua::api_def::gui::get_checkbox(lua_State *l)
{
	runtime_state s(l);

	const auto r = s.check_arguments({
		{ ltd::string },
	});

	if (!r)
	{
		s.error(XOR_STR("usage: get_checkbox(id)"));
		return 0;
	}

	const auto el = ctx->find<checkbox>(hash(s.get_string(1)));
	if (!el)
		return 0;

	std::weak_ptr<checkbox> obj{el};
	s.create_user_object<decltype(obj)>(XOR_STR("gui.checkbox"), &obj);

	return 1;
}

int lua::api_def::gui::get_color_picker(lua_State *l)
{
	runtime_state s(l);
	
	const auto r = s.check_arguments({
		{ ltd::string },
	});
	
	if (!r)
	{
		s.error(XOR_STR("usage: get_color_picker(id)"));
		return 0;
	}
	
	const auto el = ctx->find<color_picker>(hash(s.get_string(1)));
	if (!el)
		return 0;
	
	std::weak_ptr<color_picker> obj{el};
	s.create_user_object<decltype(obj)>(XOR_STR("gui.color_picker"), &obj);
	
	return 1;
}

int lua::api_def::gui::get_slider(lua_State *l)
{
	runtime_state s(l);

	const auto r = s.check_arguments({
		{ ltd::string },
	});

	if (!r)
	{
		s.error(XOR_STR("usage: get_slider(id)"));
		return 0;
	}

	const auto el = ctx->find<slider<float>>(hash(s.get_string(1)));
	if (!el)
		return 0;

	std::weak_ptr<slider<float>> obj{el};
	s.create_user_object<decltype(obj)>(XOR_STR("gui.slider"), &obj);

	return 1;
}

int lua::api_def::gui::get_combobox(lua_State *l)
{
	runtime_state s(l);
	
	const auto r = s.check_arguments({
		{ ltd::string },
	});
	
	if (!r)
	{
		s.error(XOR_STR("usage: get_combobox(id)"));
		return 0;
	}
	
	const auto el = ctx->find<combo_box>(hash(s.get_string(1)));
	if (!el)
		return 0;
	
	std::weak_ptr<combo_box> obj{el};
	s.create_user_object<decltype(obj)>(XOR_STR("gui.combobox"), &obj);
	
	return 1;
}

int lua::api_def::gui::add_notification(lua_State *l)
{
	runtime_state s(l);

	const auto r = s.check_arguments({
		{ ltd::string },
		{ ltd::string },
	});

	if (!r)
	{
		s.error(XOR_STR("usage: add_notification(header, body)"));
		return 0;
	}

	notify.add(std::make_shared<notification>(s.get_string(1), s.get_string(2)));
	return 0;
}

int lua::api_def::gui::for_each_hotkey(lua_State* l)
{
	runtime_state s(l);
	
	const auto r = s.check_arguments({
		{ ltd::function },
	});
	
	if (!r)
	{
		s.error(XOR_STR("usage: for_each_hotkey(callback(name, key, mode, is_active))"));
		return 0;
	}
	
	const auto ref = s.registry_add();
	for (const auto& [id, hk_weak] : ctx->hotkey_list)
	{
		const auto hk = hk_weak.lock();
		if (!hk)
			continue;
		
		if (hk->hotkey_type == hkt_none || hk->hotkeys.empty())
			continue;
		
		const auto p = hk->parent.lock();
		if (!p || !p->is_container)
			continue;
		
		const auto possible_label = p->as<control_container>()->at(0);
		if (!possible_label->is_label)
			continue;
		
		for (const auto& [hk_v, _] : hk->hotkeys)
		{
			// skip if it was unbound
			if (!hk_v)
				continue;
			
			s.registry_get(ref);
			s.push(possible_label->as<label>()->text.c_str());
			s.push(key_to_string(hk_v).c_str());
			s.push(static_cast<int>(hk->hotkey_mode));
			s.push(hk->as<checkbox>()->value.get());
			if (!s.call(4, 0))
				s.error(s.get_last_error_description());
		}
	}
	
	s.registry_remove(ref);
	return 0;
}

int lua::api_def::gui::is_menu_open(lua_State* l)
{
	runtime_state s(l);
	s.push(menu::menu.is_open());
	
	return 1;
}

int lua::api_def::gui::show_message(lua_State* l)
{
	runtime_state s(l);

	const auto r = s.check_arguments({
		{ ltd::string },
		{ ltd::string },
		{ ltd::string },
	});

	if (!r)
	{
		s.error(XOR_STR("usage: show_message(id, header, body)"));
		return 0;
	}

	const auto msg = std::make_shared<message_box>(util::fnv1a(s.get_string(1)), s.get_string(2), s.get_string(3));
	msg->open();

	return 0;
}

int lua::api_def::gui::show_dialog(lua_State* l)
{
	runtime_state s(l);

	const auto r = s.check_arguments({
		{ ltd::string },
		{ ltd::string },
		{ ltd::string },
		{ ltd::number },
		{ ltd::function },
	});

	if (!r)
	{
		s.error(XOR_STR("usage: show_dialog(id, header, body, buttons, callback)"));
		return 0;
	}

	// save up function in the registry
	const auto ref = s.registry_add();

	const auto dlg = std::make_shared<dialog_box>(util::fnv1a(s.get_string(1)), s.get_string(2), s.get_string(3), (dialog_buttons)s.get_integer(4));
	dlg->callback = [ref, l](dialog_result r) {
		runtime_state s(l);
		s.registry_get(ref);
		s.push((int)r);
		if (!s.call(1, 0))
			s.error(s.get_last_error_description());
		s.registry_remove(ref);
	};

	dlg->open();

	return 0;
}

int lua::api_def::gui::get_menu_rect(lua_State* l)
{
	runtime_state s(l);

	const auto wnd = menu::menu.main_wnd.lock();
	if (!wnd)
		return 0;

	const auto r = wnd->area_abs();
	s.push(r.mins.x);
	s.push(r.mins.y);
	s.push(r.maxs.x);
	s.push(r.maxs.y);

	return 4;
}