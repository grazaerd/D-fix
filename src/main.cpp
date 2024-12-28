
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

#ifdef _MSC_VER
  #define DLLEXPORT
  extern "C" bool cpuidfn();
#else
  #define DLLEXPORT __declspec(dllexport)
#endif

namespace atfix {

// NOLINTBEGIN
Log log("atfix.log");
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

inline std::uint32_t GetExeSize(HMODULE hModule) {
    if (!hModule) { return 0; }

    const auto dosHeader = reinterpret_cast<IMAGE_DOS_HEADER*>(hModule);
    if (dosHeader->e_magic != IMAGE_DOS_SIGNATURE) { return 0; }

    const auto ntHeaders = reinterpret_cast<IMAGE_NT_HEADERS*>(reinterpret_cast<std::uint8_t*>(hModule) + dosHeader->e_lfanew);
    if (ntHeaders->Signature != IMAGE_NT_SIGNATURE) { return 0; }

    return ntHeaders->OptionalHeader.SizeOfImage;
}

void GameRD() {
    const HMODULE modulehandle = GetModuleHandleA(nullptr);
    const auto modulebase = std::bit_cast<std::uintptr_t>(modulehandle);
    static constexpr std::string_view sig1 = "83 3d ?? ?? ?? ?? ?? 0f 4d c1";
    static constexpr std::string_view sig2 = "83 3d ?? ?? ?? ?? ?? 0f 8f";

    const void* result = LightningScanner::Scanner(sig1).Find(std::bit_cast<void*>(modulebase), GetExeSize(modulehandle)).Get<void*>();\
    if (result == nullptr) {
      result = LightningScanner::Scanner(sig2).Find(std::bit_cast<void*>(modulebase), GetExeSize(modulehandle)).Get<void*>();
    }

    if (result != nullptr) {
        const auto RVA = *std::bit_cast<DWORD*>(std::bit_cast<uintptr_t>(result) + 2);
        const auto AbsoAddress = static_cast<DWORD>((RVA + static_cast<DWORD>(std::bit_cast<uintptr_t>(result) + 7)) - modulebase);
        SettingsAddress = std::bit_cast<void*>(modulebase + AbsoAddress);
    } else {
        log("Address not found. Default shaders: High");
    }
}

bool cpuinfo() {
    bool result;
    asm (
        "xor %%eax, %%eax \n\t"
        "cpuid \n\t"
        "cmp $0x444d4163, %%ecx \n\t"
        "setz %0 \n\t"
        : "=r" (result)
        :                              
        : "eax", "ebx", "ecx", "edx"
    );
    return result;
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
#ifdef _MSC_VER
  atfix::isAMD = cpuidfn();
#else
  atfix::isAMD = atfix::cpuinfo();
#endif
  if (atfix::isAMD) {
    atfix::log("CPU Vendor: AMD");
  } else {
    atfix::log("CPU Vendor: Intel");
  }
  atfix::GameRD();

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

DLLEXPORT HRESULT __stdcall D3D11CreateDeviceAndSwapChain(
        IDXGIAdapter*         pAdapter,
        D3D_DRIVER_TYPE       DriverType,
        HMODULE               Software,
        UINT                  Flags,
  const D3D_FEATURE_LEVEL*    pFeatureLevels,
        UINT                  FeatureLevels,
        UINT                  SDKVersion,
  const DXGI_SWAP_CHAIN_DESC* pSwapChainDesc,
        IDXGISwapChain**      ppSwapChain,
        ID3D11Device**        ppDevice,
        D3D_FEATURE_LEVEL*    pFeatureLevel,
        ID3D11DeviceContext** ppImmediateContext) {
  if (ppDevice) {
      *ppDevice = nullptr;
  }

  if (ppSwapChain) {
      *ppSwapChain = nullptr;
  }

  const auto proc = atfix::loadSystemD3D11();

  if (!proc.D3D11CreateDeviceAndSwapChain) {
      return E_FAIL;
  }

  ID3D11Device* device = nullptr;

  const HRESULT hrt = (*proc.D3D11CreateDeviceAndSwapChain)(pAdapter, DriverType, Software,
    Flags, pFeatureLevels, FeatureLevels, SDKVersion, pSwapChainDesc, ppSwapChain,
    &device, pFeatureLevel, ppImmediateContext);

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