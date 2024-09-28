
#ifndef MENU_MACROS_H
#define MENU_MACROS_H

#define MAKE_TAB(i, n) std::make_shared<tab>(GUI_HASH(i), XOR(n), GUI_HASH(i "_container"), draw.get_texture(GUI_HASH("icon_" i)))
#define MAKE(h, t, ...) std::make_shared<t>(GUI_HASH(h), __VA_ARGS__)
#define MAKE_RUNTIME(h, t, ...) std::make_shared<t>(hash((h).c_str()), __VA_ARGS__)

#define ADD(n, h, t, ...) e->add(make_control(XOR(n), MAKE(h, t, __VA_ARGS__)))
#define ADD_TOOLTIP(n, h, x, t, ...) e->add(make_control(XOR(n), MAKE(h, t, __VA_ARGS__)->make_tooltip(XOR(x))))
#define ADD_RUNTIME(n, h, t, ...) e->add(make_control(XOR(n), MAKE_RUNTIME((h), t, __VA_ARGS__)))
#define ADD_INLINE(l, ...) e->add(make_inlined(GUI_HASH(l "_content"), [&](std::shared_ptr<layout>& e) { \
	const std::vector<std::shared_ptr<control>> _ { __VA_ARGS__ };                                                           \
	e->add(std::make_shared<label>(XOR(l)));                                                                                 \
	for (const auto& c : _)                                                                                                  \
		e->add(c);																										     \
}))

#define ADD_INLINE_RUNTIME(l, ...) e->add(make_inlined(hash((l + std::string(XOR("_content"))).c_str()), [&](std::shared_ptr<layout>& e) { \
	const std::vector<std::shared_ptr<control>> _ { __VA_ARGS__ };                                                           \
	e->add(std::make_shared<label>(XOR(l)));                                                                                 \
	for (const auto& c : _)                                                                                                  \
		e->add(c);																										     \
}))

#define ADD_LINE(l, f, ...) e->add(make_inlined(GUI_HASH(l "_content"), [&](std::shared_ptr<layout>& e) { \
	const std::vector<std::shared_ptr<control>> _ { __VA_ARGS__ };                                                           \
	e->add(f);                                                                                 \
	for (const auto& c : _)                                                                                                  \
		e->add(c);																										     \
}))

#define USE_NAMESPACES using namespace menu; \
using namespace evo; \
using namespace evo::gui; \
using namespace evo::ren; \
using namespace helpers;

#endif // MENU_MACROS_H
