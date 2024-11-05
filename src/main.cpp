
#include "util.h"
#include "impl.h"
#include "MinHook.h"

#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <d3d11.h>
#include <d3dcommon.h>
#include <dxgi.h>
#include <libloaderapi.h>
#include <LightningScanner.hpp>
#include <minwindef.h>
#include <mutex>
#include <winerror.h>
#include <winnt.h>

#ifndef __GNUC__ || __clang__
  #define DLLEXPORT
#else
  #define DLLEXPORT __declspec(dllexport)
#endif

namespace atfix {

// NOLINTBEGIN
Log log("valfix.log");
// NOLINTEND

/** Load system D3D11 DLL and return entry points */
using PFN_D3D11CreateDevice = HRESULT (__stdcall *) (
  IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*,
  UINT, UINT, ID3D11Device**, D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

using PFN_D3D11CreateDeviceAndSwapChain = HRESULT (__stdcall *) (
  IDXGIAdapter*, D3D_DRIVER_TYPE, HMODULE, UINT, const D3D_FEATURE_LEVEL*,
  UINT, UINT, const DXGI_SWAP_CHAIN_DESC*, IDXGISwapChain**, ID3D11Device**,
  D3D_FEATURE_LEVEL*, ID3D11DeviceContext**);

struct D3D11Proc {
  PFN_D3D11CreateDevice             D3D11CreateDevice             = nullptr;
  PFN_D3D11CreateDeviceAndSwapChain D3D11CreateDeviceAndSwapChain = nullptr;
};

D3D11Proc loadSystemD3D11() {
  static mutex initMutex;
  static D3D11Proc d3d11Proc;

  if (d3d11Proc.D3D11CreateDevice) {
      return d3d11Proc;
  }

  const std::lock_guard lock(initMutex);

  if (d3d11Proc.D3D11CreateDevice) {
      return d3d11Proc;
  }

  HMODULE libD3D11 = LoadLibraryA("d3d11_proxy.dll");

  if (libD3D11) {
#ifndef NDEBUG
    log("Using d3d11_proxy.dll");
#endif
  } else {
    std::array<char, MAX_PATH + 1> path = { };

    if (!GetSystemDirectoryA(path.data(), MAX_PATH)) {
        return D3D11Proc();
    }

    std::strncat(path.data(), "\\d3d11.dll", MAX_PATH);
#ifndef NDEBUG
    log("Using ", path.data());
#endif
    libD3D11 = LoadLibraryA(path.data());

    if (!libD3D11) {
#ifndef NDEBUG
      log("Failed to load d3d11.dll (", path.data(), ")");
#endif
      return D3D11Proc();
    }
  }

  d3d11Proc.D3D11CreateDevice = std::bit_cast<PFN_D3D11CreateDevice>(
    GetProcAddress(libD3D11, "D3D11CreateDevice"));
  d3d11Proc.D3D11CreateDeviceAndSwapChain = std::bit_cast<PFN_D3D11CreateDeviceAndSwapChain>(
    GetProcAddress(libD3D11, "D3D11CreateDeviceAndSwapChain"));
#ifndef NDEBUG
  log("D3D11CreateDevice             @ ", std::bit_cast<void*>(d3d11Proc.D3D11CreateDevice));
  log("D3D11CreateDeviceAndSwapChain @ ", std::bit_cast<void*>(d3d11Proc.D3D11CreateDeviceAndSwapChain));
#endif
  return d3d11Proc;
}

}
extern "C" {

DLLEXPORT HRESULT __stdcall D3D11CreateDevice(
        IDXGIAdapter*         pAdapter,
        D3D_DRIVER_TYPE       DriverType,
        HMODULE               Software,
        UINT                  Flags,
  const D3D_FEATURE_LEVEL*    pFeatureLevels,
        UINT                  FeatureLevels,
        UINT                  SDKVersion,
        ID3D11Device**        ppDevice,
        D3D_FEATURE_LEVEL*    pFeatureLevel,
        ID3D11DeviceContext** ppImmediateContext) {

  if (ppDevice) {
      *ppDevice = nullptr;
  }

  const auto proc = atfix::loadSystemD3D11();
  if (!proc.D3D11CreateDevice) {
      return E_FAIL;
  }

  ID3D11Device* device = nullptr;

  const HRESULT hrt = (*proc.D3D11CreateDevice)(pAdapter, DriverType, Software,
    Flags, pFeatureLevels, FeatureLevels, SDKVersion, &device, pFeatureLevel,
      ppImmediateContext);

  if (FAILED(hrt)) {
      return hrt;
  }

  atfix::hookDevice(device);

  if (ppDevice) {
    device->AddRef();
    *ppDevice = device;
  }

  device->Release();

  return hrt;
}

BOOL WINAPI DllMain([[maybe_unused]] HINSTANCE hinstDLL, DWORD fdwReason,[[maybe_unused]] LPVOID lpvReserved) {
  switch (fdwReason) {
    case DLL_PROCESS_ATTACH:
      MH_Initialize();
      break;
    case DLL_PROCESS_DETACH:
      MH_Uninitialize();
      break;
    default:
        break;
  }

  return TRUE;
}

}