#pragma once
#include <Siv3D/Platform.hpp>

#if SIV3D_PLATFORM(WINDOWS) && !defined(SIV3D_LIBRARY_BUILD)
#if SIV3D_BUILD(DEBUG)
#pragma comment (lib, "MessageBus/siv3d-messagebus_d.lib")
#pragma comment (lib, "hiredis/hiredisd.lib")
#else
#pragma comment (lib, "MessageBus/siv3d-messagebus.lib")
#pragma comment (lib, "hiredis/hiredis.lib")
#endif
#endif
