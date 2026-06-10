// TakeGear — cross-version compatibility layer (Cinema 4D 2024 / 2025 / 2026).
//
// MAXON_API_ABI_VERSION reference: 2026 -> 2026000, 2025 -> 2025000,
// 2024.5 -> 2023904. The classic API moved into `namespace cinema` with 2025;
// on 2024 the same symbols live in the global namespace, so an empty cinema
// namespace keeps our `using namespace cinema;` directives valid everywhere.
#ifndef TC_COMPAT_H__
#define TC_COMPAT_H__

#include "c4d.h"
#include "maxon/apibase_version.h" // MAXON_API_ABI_VERSION

#if !defined(MAXON_API_ABI_VERSION)
	#error "TakeGear requires the Cinema 4D 2024+ SDK"
#endif

#if MAXON_API_ABI_VERSION >= 2025000
	#define TC_HAS_CINEMA_NS 1
#else
	#define TC_HAS_CINEMA_NS 0
namespace cinema
{
} // classic API is global before 2025
#endif

#endif // TC_COMPAT_H__
