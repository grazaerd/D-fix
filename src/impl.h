#ifndef IMPL_H
#define IMPL_H

#include <bit>
#include <cstdint>
#include <d3d11.h>

#include "log.h"

namespace atfix {

void hookDevice(ID3D11Device* pDevice);
/* lives in main.cpp */
extern Log log;

}

#endif