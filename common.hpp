#pragma once

#include <cassert>
#include <cstdint>
#include <span>

#define dlg_assert(x) assert((x))
#define dlg_assertm(x, t, ...) assert((x) && t)

#ifdef NDEBUG
	#define VIL_DEBUG_ONLY(x)
#else
	#define VIL_DEBUG_ONLY(x) x
#endif

#if __cplusplus >= 201902
	#define VIL_LIKELY [[likely]]
	#define VIL_UNLIKELY [[unlikely]]
#else
	#define VIL_LIKELY
	#define VIL_UNLIKELY
#endif

#define ExtZoneScoped
#define ZoneScoped

namespace vil {

using u16 = std::uint16_t;
using u32 = std::uint32_t;
using u64 = std::uint64_t;
using std::span;

template<typename A, typename B>
constexpr A alignPOT(A offset, B alignment) {
	dlg_assert(alignment != 0);
	dlg_assert((alignment & (alignment - 1)) == 0u); // POT
	return (offset + alignment - 1) & ~(alignment - 1);
}

} // namespace vil
