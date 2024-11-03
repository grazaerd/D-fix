#include <algorithm>
#include <array>
#include <bit>
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
#include "shaders/Default.h"
#include "shaders/DiffSpheric.h"
#include "shaders/Grass.h"
#include "shaders/Particle1.h"
#include "shaders/Player.h"
#include "shaders/RadialBlur.h"
#include "shaders/Shadow.h"
#include "shaders/SkyBox.h"
#include "shaders/Spherical.h"
#include "shaders/Terrain.h"
#include "shaders/Tex.h"
#include "shaders/VolumeFog.h"

#include "util.h"


namespace atfix {

/** Hooking-related stuff */
using PFN_ID3D11Device_CreateVertexShader = HRESULT(STDMETHODCALLTYPE*) (ID3D11Device*, const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11VertexShader**);
using PFN_ID3D11Device_CreatePixelShader = HRESULT(STDMETHODCALLTYPE*) (ID3D11Device*, const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11PixelShader**);

using PFN_ID3D11DeviceContext_UpdateSubresource = void (STDMETHODCALLTYPE*) (ID3D11DeviceContext*, ID3D11Resource*, UINT, const D3D11_BOX*, const void*, UINT, UINT);
using PFN_ID3D11DeviceContext_UpdateSubresource1 = void (STDMETHODCALLTYPE*) (ID3D11DeviceContext*, ID3D11Resource*, UINT, const D3D11_BOX*, const void*, UINT, UINT, UINT);
using PFN_ID3D11DeviceContext_Map = HRESULT(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Resource*, UINT, D3D11_MAP, UINT, D3D11_MAPPED_SUBRESOURCE*);

struct DeviceProcs {
  PFN_ID3D11Device_CreateVertexShader                   CreateVertexShader            = nullptr;
  PFN_ID3D11Device_CreatePixelShader                    CreatePixelShader             = nullptr;
};

struct ContextProcs {
    PFN_ID3D11DeviceContext_Map                           Map = nullptr;
    PFN_ID3D11DeviceContext_UpdateSubresource             UpdateSubresource = nullptr;
    PFN_ID3D11DeviceContext_UpdateSubresource1             UpdateSubresource1 = nullptr;
};

namespace {
    mutex  g_hookMutex;
    uint32_t g_installedHooks = 0U;
}

DeviceProcs   g_deviceProcs;
ContextProcs  g_immContextProcs;
ContextProcs  g_defContextProcs;


const ContextProcs* getContextProcs(ID3D11DeviceContext* pContext) {
    return pContext->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE
        ? &g_immContextProcs
        : &g_defContextProcs;
}

constexpr uint32_t HOOK_DEVICE  = (1U << 0U);
constexpr uint32_t HOOK_IMM_CTX = (1u << 1U);
constexpr uint32_t HOOK_DEF_CTX = (1u << 2U);

const DeviceProcs* getDeviceProcs([[maybe_unused]] ID3D11Device* pDevice) {
  return &g_deviceProcs;
}
bool isImmediatecontext(
    ID3D11DeviceContext* pContext) {
    return pContext->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;
}

// This game hates when other shaders 
// don't have a vertex shader when doing 
// pixel shader changes, but it's fine the other way around.

HRESULT STDMETHODCALLTYPE ID3D11Device_CreateVertexShader(
        ID3D11Device*           pDevice,
        const void*             pShaderBytecode,
        SIZE_T                  BytecodeLength,
        ID3D11ClassLinkage*     pClassLinkage,
        ID3D11VertexShader**    ppVertexShader) {
    const auto* procs = getDeviceProcs(pDevice);

    static uint32_t TextureVal = 0U;
    static uint32_t QualityVal = 0U;

    if (atfix::SettingsAddress != nullptr) {
        TextureVal = *std::bit_cast<uint32_t*>(&atfix::SettingsAddress + 4);
        QualityVal = *std::bit_cast<uint32_t*>(atfix::SettingsAddress);
    }

    static constexpr std::array<uint32_t, 4> ParticleShader1 = { 0x231fb2e6, 0xc211f72b, 0x1a0b5fbb, 0xe9e36557 };
    static constexpr std::array<uint32_t, 4> ParticleShader2 = { 0x003ca944, 0x7fb09127, 0xed8e5b6e, 0x4cbdd6e9 };
    static constexpr std::array<uint32_t, 4> VolumeFogShader = { 0xdf94514a, 0xbe2cf252, 0xf86fcdba, 0x640e1563 };
    static constexpr std::array<uint32_t, 4> GrassShader = { 0x5272db3c, 0xdc7a397a, 0xb7bf11d5, 0x078d9485 };
    static constexpr std::array<uint32_t, 4> ShadowPlayerShader = { 0xe4c7cd57, 0xbc029e48, 0xabcb38c1, 0xeae68c10 };
    static constexpr std::array<uint32_t, 4> ShadowPropShader = { 0xefbe9f94, 0x5c300015, 0x29ab6626, 0xb640836c };
    static constexpr std::array<uint32_t, 4> TerrainShader = { 0xe0dfec90, 0xc8480b86, 0x20262b5d, 0xf0ace17e };
    static constexpr std::array<uint32_t, 4> DefaultShader = { 0x49d8396e, 0x5b9dfd57, 0xb4f45dba, 0xe6d8b741 };
    static constexpr std::array<uint32_t, 4> PlayerShader = { 0xe8462ec7, 0xd4f1f7cc, 0x68fe051f, 0xe00219ea };
    static constexpr std::array<uint32_t, 4> SkyBoxShader = { 0x8b1472b4, 0xed87bde5, 0x202fd66c, 0x80b1ce96 };
    static constexpr std::array<uint32_t, 4> SkyBoxAniShader = { 0x1003ef76, 0x5d689bc0, 0x8042f17a, 0x52709a00 };

    const auto* hash = std::bit_cast<const uint32_t*>(std::bit_cast<const uint8_t*>(pShaderBytecode) + 4);

    if (std::equal(ParticleShader1.begin(), ParticleShader1.end(), hash)) {
        if (!Particle1B) {
            Particle1B = true;
            log("Particle found");
        }
        if (isAMD) {
            return procs->CreateVertexShader(pDevice, FIXED_PARTICLE_SHADER1.data(), FIXED_PARTICLE_SHADER1.size(), pClassLinkage, ppVertexShader);
        } else {
            return procs->CreateVertexShader(pDevice, pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
        }

    } else if (std::equal(ParticleShader2.begin(), ParticleShader2.end(), hash)) {

        if (!Particle2B) {
            Particle2B = true;
            log("Particle Iterate found");
        }
        if (isAMD) {
            return procs->CreateVertexShader(pDevice, FIXED_PARTICLE_SHADER2.data(), FIXED_PARTICLE_SHADER2.size(), pClassLinkage, ppVertexShader);
        } else {
            return procs->CreateVertexShader(pDevice, pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
        }
    }
    else if (std::equal(VolumeFogShader.begin(), VolumeFogShader.end(), hash) && (QualityVal < 2)) {

        if (!VolumeFogB) {
            VolumeFogB = true;
            log("Volumefog found");
        }
        return procs->CreateVertexShader(pDevice, NO_VOLUMEFOG_SHADER.data(), NO_VOLUMEFOG_SHADER.size(), pClassLinkage, ppVertexShader);

    } else if (std::equal(GrassShader.begin(), GrassShader.end(), hash) && (QualityVal < 2)) {

        if (!GrassB) {
            GrassB = true;
            log("Grass found");
        }
        return procs->CreateVertexShader(pDevice, SIMPLIFIED_VS_GRASS_SHADER.data(), SIMPLIFIED_VS_GRASS_SHADER.size(), pClassLinkage, ppVertexShader);

    } else if (std::equal(ShadowPlayerShader.begin(), ShadowPlayerShader.end(), hash) && (QualityVal < 2)) {

        if (!ShadowPlayerB) {
            ShadowPlayerB = true;
            log("Shadow Player found");
        }
        return procs->CreateVertexShader(pDevice, FIXED_PLAYER_SHADOW_SHADER.data(), FIXED_PLAYER_SHADOW_SHADER.size(), pClassLinkage, ppVertexShader);

    } else if (std::equal(ShadowPropShader.begin(), ShadowPropShader.end(), hash) && (QualityVal < 2)) {

        if (!ShadowPropB) {
            ShadowPropB = true;
            log("Shadow Prop found");
        }
        return procs->CreateVertexShader(pDevice, FIXED_PROP_SHADOW_SHADER.data(), FIXED_PROP_SHADOW_SHADER.size(), pClassLinkage, ppVertexShader);
    } 
    else if (std::equal(TerrainShader.begin(), TerrainShader.end(), hash) && (QualityVal == 2)) {

        if (!TerrainB) {
            TerrainB = true;
            log("Terrain found");
        }
        return procs->CreateVertexShader(pDevice, LOW_VS_TERRAIN_SHADER.data(), LOW_VS_TERRAIN_SHADER.size(), pClassLinkage, ppVertexShader);

    } 
    else if (std::equal(PlayerShader.begin(), PlayerShader.end(), hash) && (TextureVal == 0)) {

        if (!VSPlayerB) {
            VSPlayerB = true;
            log("VS Player found");
        }
        return procs->CreateVertexShader(pDevice, SIMPLIFIED_VS_PLAYER_SHADER.data(), SIMPLIFIED_VS_PLAYER_SHADER.size(), pClassLinkage, ppVertexShader);

    }
    else if (std::equal(DefaultShader.begin(), DefaultShader.end(), hash) && (QualityVal == 2)) {

        if (!DefaultB) {
            DefaultB = true;
            log("Default found");
        }
        return procs->CreateVertexShader(pDevice, SIMPLIFIED_VS_DEFAULT_SHADER.data(), SIMPLIFIED_VS_DEFAULT_SHADER.size(), pClassLinkage, ppVertexShader);

    }
    else if (std::equal(SkyBoxShader.begin(), SkyBoxShader.end(), hash)) {
        if (!SkyBoxB) {
            SkyBoxB = true;
            log("SkyBox found");
        }
        return procs->CreateVertexShader(pDevice, VS_SKYBOX.data(), VS_SKYBOX.size(), pClassLinkage, ppVertexShader);
    }
    else if (std::equal(SkyBoxAniShader.begin(), SkyBoxAniShader.end(), hash)) {
        if (!SkyBoxAniB) {
            SkyBoxAniB = true;
            log("SkyBox Ani found");
        }
        return procs->CreateVertexShader(pDevice, VS_SKYBOX_ANI.data(), VS_SKYBOX_ANI.size(), pClassLinkage, ppVertexShader);
    }
    return procs->CreateVertexShader(pDevice, pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
}

HRESULT STDMETHODCALLTYPE ID3D11Device_CreatePixelShader(
    ID3D11Device* pDevice,
    const void* pShaderBytecode,
    SIZE_T                  BytecodeLength,
    ID3D11ClassLinkage* pClassLinkage,
    ID3D11PixelShader** ppPixelShader) {
    const auto* procs = getDeviceProcs(pDevice);

    static uint32_t TextureVal = 0U;
    static uint32_t QualityVal = 0U;

    if (atfix::SettingsAddress != nullptr) {
        TextureVal = *std::bit_cast<uint32_t*>(&atfix::SettingsAddress + 4);
        QualityVal = *std::bit_cast<uint32_t*>(atfix::SettingsAddress);
    }

    static constexpr std::array<uint32_t, 4> TexShader = { 0x4342435a, 0xd5824908, 0x23e6147a, 0x3ec4c9ea };
    static constexpr std::array<uint32_t, 4> RadialShader = { 0xcf3dfb4b, 0x6c82c337, 0xec6459ee, 0x0a2b4c01 };
    static constexpr std::array<uint32_t, 4> GrassShader = { 0xb2f29488, 0x210994ca, 0x07510660, 0x301d1575 };
    static constexpr std::array<uint32_t, 4> ShadowShader = { 0xbb5a2d0a, 0x29d139b7, 0x40992005, 0xf3b46588 };
    static constexpr std::array<uint32_t, 4> TerrainShader = { 0x74a9f538, 0x75cb0ce6, 0x3da09498, 0x7bc641bd };
    static constexpr std::array<uint32_t, 4> DefaultShader = { 0x5cbbb737, 0x265384da, 0x36d6d037, 0x1b052f54 };
    static constexpr std::array<uint32_t, 4> SphericalShader = { 0xba0db34b, 0xd2bc2581, 0x36622cd8, 0xacd2a10c };
    static constexpr std::array<uint32_t, 4> PlayerHairShader = { 0xbbc7bc71, 0xf2d316d1, 0xaba24d5f, 0xd9b9460d };
    static constexpr std::array<uint32_t, 4> PlayerFaceShader = { 0x8cd3d34a, 0x50d06bec, 0x40d80094, 0x2beeabc2 };
    static constexpr std::array<uint32_t, 4> PlayerCostumeShader = { 0xa28f0898, 0xf65ab2ec, 0x2736d0ab, 0x34b5d802 };
    static constexpr std::array<uint32_t, 4> SkyBoxShader = { 0x6ef64758, 0xb4bf8c73, 0x37b6097d, 0x357e47ef };
    static constexpr std::array<uint32_t, 4> SkyBoxAniShader = { 0x6306d045, 0x71e3ab0e, 0x1036971b, 0x1534b744 };
    static constexpr std::array<uint32_t, 4> DiffSphericShader = { 0xba0db34b, 0xd2bc2581, 0x36622cd8, 0xacd2a10c };

    const auto* hash = std::bit_cast<const uint32_t*>(std::bit_cast<const uint8_t*>(pShaderBytecode) + 4);


    if (std::equal(TexShader.begin(), TexShader.end(), hash)) {

        if (!DiffVolTexB) {
            DiffVolTexB = true;
            log("DiffVolTex found");
        }
        return procs->CreatePixelShader(pDevice, SIMPLIFIED_TEX_SHADER.data(), SIMPLIFIED_TEX_SHADER.size(), pClassLinkage, ppPixelShader);

    }
    else if (std::equal(RadialShader.begin(), RadialShader.end(), hash) && (QualityVal < 2)) {

        if (!RadialBlurB) {
            RadialBlurB = true;
            log("RadialBlur found");
        }
        return procs->CreatePixelShader(pDevice, NO_RADIALBLUR_SHADER.data(), NO_RADIALBLUR_SHADER.size(), pClassLinkage, ppPixelShader);

    } else if (std::equal(GrassShader.begin(), GrassShader.end(), hash) && (QualityVal < 2)) {

        return procs->CreatePixelShader(pDevice, SIMPLIFIED_FS_GRASS_SHADER.data(), SIMPLIFIED_FS_GRASS_SHADER.size(), pClassLinkage, ppPixelShader);

    } else if (std::equal(ShadowShader.begin(), ShadowShader.end(), hash) && (TextureVal == 0)) {

        if (!FragmentShadowB) {
            FragmentShadowB = true;
            log("Fragment Shadow found");
        }
        return procs->CreatePixelShader(pDevice, SIMPLIFIED_FS_SHADOW_SHADER.data(), SIMPLIFIED_FS_SHADOW_SHADER.size(), pClassLinkage, ppPixelShader);
    }
    else if (std::equal(SphericalShader.begin(), SphericalShader.end(), hash) && (QualityVal < 2)) {
        if (!SphericalB) {
            SphericalB = true;
            log("Spherical Map found");
        }
        return procs->CreatePixelShader(pDevice, SIMPLIFIED_FS_HIGH_SPHERICAL_SHADER.data(), SIMPLIFIED_FS_HIGH_SPHERICAL_SHADER.size(), pClassLinkage, ppPixelShader);
    }
    else if (std::equal(TerrainShader.begin(), TerrainShader.end(), hash) && (QualityVal == 2)) {

        return procs->CreatePixelShader(pDevice, LOW_FS_TERRAIN_SHADER.data(), LOW_FS_TERRAIN_SHADER.size(), pClassLinkage, ppPixelShader);
    }    
    else if (std::equal(DefaultShader.begin(), DefaultShader.end(), hash) && (QualityVal == 2)) {

        return procs->CreatePixelShader(pDevice, SIMPLIFIED_FS_DEFAULT_SHADER.data(), SIMPLIFIED_FS_DEFAULT_SHADER.size(), pClassLinkage, ppPixelShader);
    }    
    else if (std::equal(SphericalShader.begin(), SphericalShader.end(), hash) && (QualityVal == 2)) {
        if (!SphericalB) {
            SphericalB = true;
            log("Spherical Map found");
        }
        return procs->CreatePixelShader(pDevice, SIMPLIFIED_FS_LOW_SPHERICAL_SHADER.data(), SIMPLIFIED_FS_LOW_SPHERICAL_SHADER.size(), pClassLinkage, ppPixelShader);
    }    
    else if (std::equal(PlayerHairShader.begin(), PlayerHairShader.end(), hash) && (QualityVal == 2)) {
        if (!PlayerHairB) {
            PlayerHairB = true;
            log("Player Hair found");
        }
        return procs->CreatePixelShader(pDevice, SIMPLIFIED_FS_HAIR_PLAYER_SHADER.data(), SIMPLIFIED_FS_HAIR_PLAYER_SHADER.size(), pClassLinkage, ppPixelShader);
    }    
    else if (std::equal(PlayerFaceShader.begin(), PlayerFaceShader.end(), hash) && (QualityVal == 2)) {
        if (!PlayerFaceB) {
            PlayerFaceB = true;
            log("Player Face found");
        }
        return procs->CreatePixelShader(pDevice, SIMPLIFIED_FS_FACE_PLAYER_SHADER.data(), SIMPLIFIED_FS_FACE_PLAYER_SHADER.size(), pClassLinkage, ppPixelShader);
    }    
    else if (std::equal(PlayerCostumeShader.begin(), PlayerCostumeShader.end(), hash) && (QualityVal == 2)) {
        if (!PlayerBodyB) {
            PlayerBodyB = true;
            log("Player Body found");
        }
        return procs->CreatePixelShader(pDevice, SIMPLIFIED_FS_COSTUME_PLAYER_SHADER.data(), SIMPLIFIED_FS_COSTUME_PLAYER_SHADER.size(), pClassLinkage, ppPixelShader);
    }
    else if (std::equal(SkyBoxShader.begin(), SkyBoxShader.end(), hash)) {
        return procs->CreatePixelShader(pDevice, FS_SKYBOX.data(), FS_SKYBOX.size(), pClassLinkage, ppPixelShader);
    }
    else if (std::equal(SkyBoxAniShader.begin(), SkyBoxAniShader.end(), hash)) {
        return procs->CreatePixelShader(pDevice, FS_SKYBOX_ANI.data(), FS_SKYBOX_ANI.size(), pClassLinkage, ppPixelShader);
    }
    else if (std::equal(DiffSphericShader.begin(), DiffSphericShader.end(), hash) && (QualityVal == 2)) {
        if (!DiffSphericB) {
            DiffSphericB = true;
            log("Diff Spheric found");
        }
        return procs->CreatePixelShader(pDevice, LOW_DIFFSPHERIC_SHADER.data(), LOW_DIFFSPHERIC_SHADER.size(), pClassLinkage, ppPixelShader);
    }
    else if (std::equal(DiffSphericShader.begin(), DiffSphericShader.end(), hash) && (QualityVal < 2)) {
        if (!DiffSphericB) {
            DiffSphericB = true;
            log("Diff Spheric found");
        }
        return procs->CreatePixelShader(pDevice, HIGH_DIFFSPHERIC_SHADER.data(), HIGH_DIFFSPHERIC_SHADER.size(), pClassLinkage, ppPixelShader);
    }

    return procs->CreatePixelShader(pDevice, pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);
}
void STDMETHODCALLTYPE ID3D11DeviceContext_UpdateSubresource(
    ID3D11DeviceContext*      pContext,
    ID3D11Resource*           pResource,
    UINT                      Subresource,
    const D3D11_BOX*          pBox,
    const void*               pData,
    UINT                      RowPitch,
    UINT                      SlicePitch) {
    const auto* procs = getContextProcs(pContext);


    procs->UpdateSubresource(pContext, pResource,
        Subresource, pBox, pData, RowPitch, SlicePitch);
}
HRESULT STDMETHODCALLTYPE ID3D11DeviceContext_Map(
    ID3D11DeviceContext* pContext,
    ID3D11Resource* pResource,
    UINT                     Subresource,
    D3D11_MAP                MapType,
    UINT                     MapFlags,
    D3D11_MAPPED_SUBRESOURCE* pMappedResource
) {
    const auto* procs = getContextProcs(pContext);

    return procs->Map(pContext, pResource, Subresource, MapType, MapFlags, pMappedResource);
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

void hookContext(ID3D11DeviceContext* pContext) {
    const std::lock_guard lock(g_hookMutex);

    uint32_t flag = HOOK_IMM_CTX;
    ContextProcs* procs = &g_immContextProcs;

    if (!isImmediatecontext(pContext)) {
        flag = HOOK_DEF_CTX;
        procs = &g_defContextProcs;
    }

    if (g_installedHooks & flag)
        return;
#ifndef NDEBUG
    log("Hooking context ", pContext);
#endif
    HOOK_PROC(ID3D11DeviceContext, pContext, procs, 14, Map);
    HOOK_PROC(ID3D11DeviceContext, pContext, procs, 48, UpdateSubresource);

    g_installedHooks |= flag;


    if (flag & HOOK_IMM_CTX) {
        g_defContextProcs = g_immContextProcs;
    }
}

}