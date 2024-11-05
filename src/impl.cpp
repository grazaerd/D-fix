#include <algorithm>
#include <array>
#include <cstdint>
#include <mutex>

#include <basetsd.h>
#include <d3d11.h>
#include <d3d11_1.h>
#include <minwindef.h>
#include <winnt.h>

#include "impl.h"
#include "MinHook.h"
#include "shaderbool.h"

#include "util.h"


namespace atfix {

/** Hooking-related stuff */
using PFN_ID3D11Device_CreateVertexShader = HRESULT(STDMETHODCALLTYPE*) (ID3D11Device*, const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11VertexShader**);
using PFN_ID3D11Device_CreatePixelShader = HRESULT(STDMETHODCALLTYPE*) (ID3D11Device*, const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11PixelShader**);

struct DeviceProcs {
  PFN_ID3D11Device_CreateVertexShader                   CreateVertexShader            = nullptr;
  PFN_ID3D11Device_CreatePixelShader                    CreatePixelShader             = nullptr;
};


namespace {
    mutex  g_hookMutex;
    uint32_t g_installedHooks = 0U;
}

DeviceProcs   g_deviceProcs;

constexpr uint32_t HOOK_DEVICE  = (1U << 0U);

const DeviceProcs* getDeviceProcs([[maybe_unused]] ID3D11Device* pDevice) {
  return &g_deviceProcs;
}

HRESULT STDMETHODCALLTYPE ID3D11Device_CreateVertexShader(
        ID3D11Device*           pDevice,
        const void*             pShaderBytecode,
        SIZE_T                  BytecodeLength,
        ID3D11ClassLinkage*     pClassLinkage,
        ID3D11VertexShader**    ppVertexShader) {
    const auto* procs = getDeviceProcs(pDevice);

    return procs->CreateVertexShader(pDevice, pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
}

HRESULT STDMETHODCALLTYPE ID3D11Device_CreatePixelShader(
    ID3D11Device* pDevice,
    const void* pShaderBytecode,
    SIZE_T                  BytecodeLength,
    ID3D11ClassLinkage* pClassLinkage,
    ID3D11PixelShader** ppPixelShader) {
    const auto* procs = getDeviceProcs(pDevice);

    return procs->CreatePixelShader(pDevice, pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);
}

#define HOOK_PROC(iface, object, table, index, proc) \
  hookProc(object, #iface "::" #proc, &table->proc, &iface ## _ ## proc, index)


template<typename T>
void hookProc(void* pObject,[[maybe_unused]] const char* pName, T** ppOrig, T* pHook, uint32_t index) {
    void** vtbl = *std::bit_cast<void***>(pObject);

    MH_STATUS mh = MH_CreateHook(vtbl[index], std::bit_cast<void*>(pHook), std::bit_cast<void**>(ppOrig));

    if (mh) {
        if (mh != MH_ERROR_ALREADY_CREATED) {
#ifndef NDEBUG
            log("Failed to create hook for ", pName, ": ", MH_StatusToString(mh));
#endif
        }
        return;
    }

    mh = MH_EnableHook(vtbl[index]);

    if (mh) {
#ifndef NDEBUG
        log("Failed to enable hook for ", pName, ": ", MH_StatusToString(mh));
#endif
        return;
    }
#ifndef NDEBUG
        log("Created hook for ", pName, " @ ", reinterpret_cast<void*>(pHook));
    #endif
}

void hookDevice(ID3D11Device* pDevice) {
    const std::lock_guard lock(g_hookMutex);

    if (g_installedHooks & HOOK_DEVICE) {
        return;
    }

#ifndef NDEBUG
    log("Hooking device ", pDevice);
#endif

    DeviceProcs* procs = &g_deviceProcs;
    HOOK_PROC(ID3D11Device, pDevice, procs, 12,  CreateVertexShader);
    HOOK_PROC(ID3D11Device, pDevice, procs, 15,  CreatePixelShader);

    g_installedHooks |= HOOK_DEVICE;
}

}