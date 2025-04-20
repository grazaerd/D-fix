#ifndef IMPL_H
#define IMPL_H

#include <d3d11.h>

#include "log.h"

namespace atfix {

void hookDevice(ID3D11Device* pDevice);
void hookContext(ID3D11DeviceContext* pContext);
void CreateShaderOnStart(ID3D11Device* pDevice);
// NOLINTBEGIN (cppcoreguidelines-avoid-non-const-global-variables)
inline void* SettingsAddress = nullptr;
inline bool isAMD = false;
/* lives in main.cpp */
extern Log log;
// NOLINTEND

}

#endif
