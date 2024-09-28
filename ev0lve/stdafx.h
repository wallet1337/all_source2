
#ifndef STDAFX_H
#define STDAFX_H

#define WIN32_LEAN_AND_MEAN

#define WIN32_NO_STATUS
#include <Windows.h>
#undef WIN32_NO_STATUS
 
#include <Windowsx.h>
#include <winternl.h>
#include <ntstatus.h>
#include <intrin.h>

#include <array>
#include <algorithm>
#include <vector>
#include <deque>
#include <string>
#include <sstream>
#include <iomanip>
#include <cstdint>
#include <stdexcept>
#include <utility>
#include <optional>
#include <functional>
#include <cctype>
#include <bitset>
#include <numeric>
#include <map>
#include <type_traits>

#include <macros.h>
#include <util/macros.h>

namespace sdk
{
#include <sdk/generated.h>
}

#endif // STDAFX_H
