
#ifndef SDK_MACROS_H
#define SDK_MACROS_H

#define FORWARD_ARGS(...) (this, __VA_ARGS__ ); }
#define FUNC(fn, func, sig) __forceinline auto func { return reinterpret_cast<sig>(fn) FORWARD_ARGS
#define MEMBER(offset, name, type) __forceinline type& get_##name() \
{ \
	constexpr auto z = ::util::random::_uint<__COUNTER__, 0xFFFFFFFF>::value; \
	constexpr auto p = offset - z; \
	return *(type*) ::util::add<uintptr_t>(uintptr_t(this), z, p); \
}
#define AMEMBER(offset, name, type) __forceinline type* get_##name() \
{ \
	constexpr auto z = ::util::random::_uint<__COUNTER__, 0xFFFFFFFF>::value; \
	constexpr auto p = offset - z; \
	return (type*) ::util::add<uintptr_t>(uintptr_t(this), z, p); \
}
#define VAR(cl, name, type) MEMBER(cl::name, name, type)
#define AVAR(cl, name, type) AMEMBER(cl::name, name, type)
#define VIRTUAL(index, func, sig) __forceinline auto func { return reinterpret_cast<sig>((*reinterpret_cast<uint32_t**>(this))[XOR_16(index)]) FORWARD_ARGS

#endif // SDK_MACROS_H
