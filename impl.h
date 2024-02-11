#pragma once

#include <d3d11.h>
#ifndef NDEBUG
#include "log.h"
#endif
namespace atfix {

void hookDevice(ID3D11Device* pDevice);
ID3D11DeviceContext* hookContext(ID3D11DeviceContext* pContext);

/* lives in main.cpp */
#ifndef NDEBUG
extern Log log;
#endif
}
