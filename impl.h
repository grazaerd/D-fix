#ifndef IMPL_H
#define IMPL_H

#include <bit>
#include <cstdint>
#include <d3d11.h>

#include "log.h"

namespace atfix {

void hookDevice(ID3D11Device* pDevice);
// NOLINTBEGIN (cppcoreguidelines-avoid-non-const-global-variables)
inline void* SettingsAddress = nullptr;
inline bool isAMD = false;
/* lives in main.cpp */
extern Log log;
// NOLINTEND

}

#endif