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
#include "shaders/SwordTrail.h"

#ifdef OLD_SHADERS
#include "oldshaders/Default.h"
#include "oldshaders/Grass.h"
#include "oldshaders/Shadow.h"
#include "oldshaders/Terrain.h"
#include "oldshaders/Tex.h"
#include "oldshaders/Unk-shader.h"
#endif

#include "util.h"

// #define OLD_SHADERS
// #define NO_GRASS
namespace atfix {

/** Hooking-related stuff */
using PFN_ID3D11Device_CreateVertexShader = HRESULT(STDMETHODCALLTYPE*) (ID3D11Device*, const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11VertexShader**);
using PFN_ID3D11Device_CreatePixelShader = HRESULT(STDMETHODCALLTYPE*) (ID3D11Device*, const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11PixelShader**);
using PFN_ID3D11Device_CreateBuffer = HRESULT(STDMETHODCALLTYPE*)(ID3D11Device*, const D3D11_BUFFER_DESC*, const D3D11_SUBRESOURCE_DATA*, ID3D11Buffer**);


using PFN_ID3D11DeviceContext_IASetIndexBuffer = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Buffer*, DXGI_FORMAT, UINT);
using PFN_ID3D11DeviceContext_PSSetShader = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11PixelShader*,ID3D11ClassInstance* const*, UINT);
using PFN_ID3D11DeviceContext_DrawIndexed = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, INT);
using PFN_ID3D11DeviceContext_Draw = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT);
using PFN_ID3D11DeviceContext_UpdateSubresource = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Resource*, UINT, const D3D11_BOX*, const void*, UINT, UINT);
using PFN_ID3D11DeviceContext_Map = HRESULT(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Resource*, UINT, D3D11_MAP, UINT, D3D11_MAPPED_SUBRESOURCE*);

using PFN_IDXGISwapChain_Present = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);

struct DeviceProcs {
    PFN_ID3D11Device_CreateBuffer                           CreateBuffer                    = nullptr;
    PFN_ID3D11Device_CreateVertexShader                     CreateVertexShader              = nullptr;
    PFN_ID3D11Device_CreatePixelShader                      CreatePixelShader               = nullptr;
};

struct ContextProcs {
    PFN_ID3D11DeviceContext_IASetIndexBuffer                IASetIndexBuffer                = nullptr;
    PFN_ID3D11DeviceContext_PSSetShader                     PSSetShader                     = nullptr;
    PFN_ID3D11DeviceContext_DrawIndexed                     DrawIndexed                     = nullptr;
    PFN_ID3D11DeviceContext_Draw                            Draw                            = nullptr;
    PFN_ID3D11DeviceContext_UpdateSubresource               UpdateSubresource               = nullptr;
    PFN_ID3D11DeviceContext_Map                             Map                             = nullptr;
};

struct DxgiProcs {
    PFN_IDXGISwapChain_Present  Present = nullptr;
};

struct UpdateSubresourceCache {
    ID3D11Resource* resource = nullptr;
    UINT subresource;
    std::vector<uint8_t> data;
};
namespace {
    mutex  g_hookMutex;
    uint32_t g_installedHooks = 0U;
}

inline bool simd_equal(const std::array<uint32_t, 4>& arr1, const uint32_t* ptr) {
    const __m128i v1 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(arr1.data()));
    const __m128i v2 = _mm_loadu_si128(reinterpret_cast<const __m128i*>(ptr));

    return (_mm_movemask_epi8(_mm_cmpeq_epi32(v1, v2)) == 0xFFFF);
}
DeviceProcs   g_deviceProcs;
ContextProcs  g_immContextProcs;
ContextProcs  g_defContextProcs;
DxgiProcs g_dxgiProcs;

constexpr uint32_t HOOK_DEVICE  = (1u << 0);
constexpr uint32_t HOOK_IMM_CTX = (1u << 1);
constexpr uint32_t HOOK_DEF_CTX = (1u << 2);

inline const DxgiProcs* getDxgiProcs([[maybe_unused]] IDXGISwapChain* pSwapchain) {
    return &g_dxgiProcs;
}
inline const DeviceProcs* getDeviceProcs([[maybe_unused]] ID3D11Device* pDevice) {
    return &g_deviceProcs;
}
inline const ContextProcs* getContextProcs(ID3D11DeviceContext* pContext) {
    return pContext->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE
        ? &g_immContextProcs
        : &g_defContextProcs;
}

inline bool isImmediatecontext(
        ID3D11DeviceContext*      pContext) {
  return pContext->GetType() == D3D11_DEVICE_CONTEXT_IMMEDIATE;
}

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
        QualityVal = *std::bit_cast<uint32_t*>(atfix::SettingsAddress);
        TextureVal = *std::bit_cast<uint32_t*>(std::bit_cast<std::uintptr_t>(atfix::SettingsAddress) + 4);
    }

    static constexpr std::array<uint32_t, 4> ParticleShader1 = { 0x231fb2e6, 0xc211f72b, 0x1a0b5fbb, 0xe9e36557 };
    static constexpr std::array<uint32_t, 4> ParticleShader2 = { 0x003ca944, 0x7fb09127, 0xed8e5b6e, 0x4cbdd6e9 };
    static constexpr std::array<uint32_t, 4> VolumeFogShader = { 0xdf94514a, 0xbe2cf252, 0xf86fcdba, 0x640e1563 };
    static constexpr std::array<uint32_t, 4> GrassShader = { 0x5272db3c, 0xdc7a397a, 0xb7bf11d5, 0x078d9485 };
    static constexpr std::array<uint32_t, 4> TerrainShader = { 0xe0dfec90, 0xc8480b86, 0x20262b5d, 0xf0ace17e };
    static constexpr std::array<uint32_t, 4> DefaultShader = { 0x49d8396e, 0x5b9dfd57, 0xb4f45dba, 0xe6d8b741 };
    static constexpr std::array<uint32_t, 4> PlayerShader = { 0xe8462ec7, 0xd4f1f7cc, 0x68fe051f, 0xe00219ea };
    static constexpr std::array<uint32_t, 4> SkyBoxShader = { 0x8b1472b4, 0xed87bde5, 0x202fd66c, 0x80b1ce96 };
    static constexpr std::array<uint32_t, 4> SkyBoxAniShader = { 0x1003ef76, 0x5d689bc0, 0x8042f17a, 0x52709a00 };

#ifdef OLD_SHADERS
    static constexpr std::array<uint32_t, 4> ShadowPlayerShader = { 0x548d4f5c, 0x4517ea54, 0xc8a730a3, 0x1599278c };
    static constexpr std::array<uint32_t, 4> ShadowPropShader = { 0x14aa73c0, 0x9172f259, 0xe9175393, 0x26863db4 };
#else
    static constexpr std::array<uint32_t, 4> ShadowPlayerShader = { 0xe4c7cd57, 0xbc029e48, 0xabcb38c1, 0xeae68c10 };
    static constexpr std::array<uint32_t, 4> ShadowPropShader = { 0xefbe9f94, 0x5c300015, 0x29ab6626, 0xb640836c };
#endif

    const auto* hash = std::bit_cast<const uint32_t*>(std::bit_cast<const uint8_t*>(pShaderBytecode) + 4);

    if (simd_equal(ParticleShader1, hash)) {

        if (!Particle1B) {
            Particle1B = true;
            log("Particle found");
        }
        if (isAMD) {
            return procs->CreateVertexShader(pDevice, FIXED_PARTICLE_SHADER1.data(), FIXED_PARTICLE_SHADER1.size(), pClassLinkage, ppVertexShader);
        } else {
            return procs->CreateVertexShader(pDevice, pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
        }

    } else if (simd_equal(ParticleShader2, hash)) {

        if (!Particle2B) {
            Particle2B = true;
            log("Particle Iterate found");
        }
        if (isAMD) {
            return procs->CreateVertexShader(pDevice, FIXED_PARTICLE_SHADER2.data(), FIXED_PARTICLE_SHADER2.size(), pClassLinkage, ppVertexShader);
        } else {
            return procs->CreateVertexShader(pDevice, pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
        }

    } else if (simd_equal(VolumeFogShader, hash) && (QualityVal < 2)) {

        if (!VolumeFogB) {
            VolumeFogB = true;
            log("Volumefog found");
        }
        return procs->CreateVertexShader(pDevice, NO_VOLUMEFOG_SHADER.data(), NO_VOLUMEFOG_SHADER.size(), pClassLinkage, ppVertexShader);

    } else if (simd_equal(GrassShader, hash) && (QualityVal < 2)) {

        if (!GrassB) {
            GrassB = true;
            log("Grass found");
        }
#ifdef NO_GRASS
        return procs->CreateVertexShader(pDevice, NO_VS_GRASS_SHADER.data(), NO_VS_GRASS_SHADER.size(), pClassLinkage, ppVertexShader);
#else
        return procs->CreateVertexShader(pDevice, SIMPLIFIED_VS_GRASS_SHADER.data(), SIMPLIFIED_VS_GRASS_SHADER.size(), pClassLinkage, ppVertexShader);
#endif

    }/* else if (simd_equal(ShadowPlayerShader, hash) && (QualityVal < 2)) {
        // crashes
        if (!ShadowPlayerB) {
            ShadowPlayerB = true;
            log("Shadow Player found");
        }
#ifdef OLD_SHADERS
        return procs->CreateVertexShader(pDevice, NO_VS_PLAYER_SHADOW_SHADER.data(), NO_VS_PLAYER_SHADOW_SHADER.size(), pClassLinkage, ppVertexShader);
#else
        return procs->CreateVertexShader(pDevice, FIXED_PLAYER_SHADOW_SHADER.data(), FIXED_PLAYER_SHADOW_SHADER.size(), pClassLinkage, ppVertexShader);
#endif

    } /* else if (simd_equal(ShadowPropShader, hash) && (QualityVal < 2)) {

        if (!ShadowPropB) {
            ShadowPropB = true;
            log("Shadow Prop found");
        }
#ifdef OLD_SHADERS
        return procs->CreateVertexShader(pDevice, NO_VS_PROP_SHADOW_SHADER.data(), NO_VS_PROP_SHADOW_SHADER.size(), pClassLinkage, ppVertexShader);
#else
        return procs->CreateVertexShader(pDevice, FIXED_PROP_SHADOW_SHADER.data(), FIXED_PROP_SHADOW_SHADER.size(), pClassLinkage, ppVertexShader);
#endif

    } */ else if (simd_equal(TerrainShader, hash) && (QualityVal == 2)) {

        if (!TerrainB) {
            TerrainB = true;
            log("Terrain found");
        }
        return procs->CreateVertexShader(pDevice, LOW_VS_TERRAIN_SHADER.data(), LOW_VS_TERRAIN_SHADER.size(), pClassLinkage, ppVertexShader);

    } else if (simd_equal(PlayerShader, hash) && (TextureVal == 0)) {

        if (!VSPlayerB) {
            VSPlayerB = true;
            log("VS Player found");
        }
        return procs->CreateVertexShader(pDevice, SIMPLIFIED_VS_PLAYER_SHADER.data(), SIMPLIFIED_VS_PLAYER_SHADER.size(), pClassLinkage, ppVertexShader);

    } else if (simd_equal(DefaultShader, hash) && (QualityVal == 2)) {

        if (!DefaultB) {
            DefaultB = true;
            log("Default found");
        }
        return procs->CreateVertexShader(pDevice, SIMPLIFIED_VS_DEFAULT_SHADER.data(), SIMPLIFIED_VS_DEFAULT_SHADER.size(), pClassLinkage, ppVertexShader);

    } else if (simd_equal(SkyBoxShader, hash)) {

        if (!SkyBoxB) {
            SkyBoxB = true;
            log("SkyBox found");
        }
        return procs->CreateVertexShader(pDevice, VS_SKYBOX.data(), VS_SKYBOX.size(), pClassLinkage, ppVertexShader);

    } else if (simd_equal(SkyBoxAniShader, hash)) {

        if (!SkyBoxAniB) {
            SkyBoxAniB = true;
            log("SkyBox Ani found");
        }
        return procs->CreateVertexShader(pDevice, VS_SKYBOX_ANI.data(), VS_SKYBOX_ANI.size(), pClassLinkage, ppVertexShader);
    }

    return procs->CreateVertexShader(pDevice, pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
}
ID3D11PixelShader** TexPS2 = nullptr;

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
        QualityVal = *std::bit_cast<uint32_t*>(atfix::SettingsAddress);
        TextureVal = *std::bit_cast<uint32_t*>(std::bit_cast<std::uintptr_t>(atfix::SettingsAddress) + 4);
    }

    static constexpr std::array<uint32_t, 4> RadialShader = { 0xcf3dfb4b, 0x6c82c337, 0xec6459ee, 0x0a2b4c01 };
    static constexpr std::array<uint32_t, 4> GrassShader = { 0xb2f29488, 0x210994ca, 0x07510660, 0x301d1575 };
    static constexpr std::array<uint32_t, 4> SphericalShader = { 0xba0db34b, 0xd2bc2581, 0x36622cd8, 0xacd2a10c };
    static constexpr std::array<uint32_t, 4> PlayerHairShader = { 0xbbc7bc71, 0xf2d316d1, 0xaba24d5f, 0xd9b9460d };
    static constexpr std::array<uint32_t, 4> PlayerFaceShader = { 0x8cd3d34a, 0x50d06bec, 0x40d80094, 0x2beeabc2 };
    static constexpr std::array<uint32_t, 4> PlayerCostumeShader = { 0xa28f0898, 0xf65ab2ec, 0x2736d0ab, 0x34b5d802 };
    static constexpr std::array<uint32_t, 4> SkyBoxShader = { 0x6ef64758, 0xb4bf8c73, 0x37b6097d, 0x357e47ef };
    static constexpr std::array<uint32_t, 4> SkyBoxAniShader = { 0x6306d045, 0x71e3ab0e, 0x1036971b, 0x1534b744 };
    static constexpr std::array<uint32_t, 4> DiffSphericShader = { 0xba0db34b, 0xd2bc2581, 0x36622cd8, 0xacd2a10c };
    static constexpr std::array<uint32_t, 4> SwordTrailShader = { 0x1d818da3, 0xb176cb2b, 0xf5d08e9f, 0x2947ef26 };

#ifdef OLD_SHADERS
    static constexpr std::array<uint32_t, 4> TexShader = { 0xab773669, 0x8ead9335, 0xe33741f7, 0x7fbcde5d };
    static constexpr std::array<uint32_t, 4> DefaultShader = { 0xaf4aca80, 0xd95b17ff, 0x57513390, 0x9ff66e9c };
    static constexpr std::array<uint32_t, 4> ShadowShader = { 0xbb5a2d0a, 0x29d139b7, 0x40992005, 0xf3b46588 };
    static constexpr std::array<uint32_t, 4> TerrainShader = { 0x1944825b, 0x1132acd3, 0xd610686c, 0x218895d4 };
    static constexpr std::array<uint32_t, 4> UnkShader = { 0x2ea93aff, 0xb6e39b4a, 0xe969047e, 0x17b2ea60 };
#else
    static constexpr std::array<uint32_t, 4> TexShader = { 0x4342435a, 0xd5824908, 0x23e6147a, 0x3ec4c9ea };
    static constexpr std::array<uint32_t, 4> DefaultShader = { 0x5cbbb737, 0x265384da, 0x36d6d037, 0x1b052f54 };
    static constexpr std::array<uint32_t, 4> ShadowShader = { 0xbb5a2d0a, 0x29d139b7, 0x40992005, 0xf3b46588 };
    static constexpr std::array<uint32_t, 4> TerrainShader = { 0x74a9f538, 0x75cb0ce6, 0x3da09498, 0x7bc641bd };
#endif

    const auto* hash = std::bit_cast<const uint32_t*>(std::bit_cast<const uint8_t*>(pShaderBytecode) + 4);


    if (simd_equal(TexShader, hash)) {

        if (!DiffVolTexB) {
            DiffVolTexB = true;
            log("DiffVolTex found");
        }
#ifdef OLD_SHADERS
        return procs->CreatePixelShader(pDevice, SIMPLIFIED_TEXOLD_SHADER.data(), SIMPLIFIED_TEXOLD_SHADER.size(), pClassLinkage, ppPixelShader);
#else
        return procs->CreatePixelShader(pDevice, SIMPLIFIED_TEX_SHADER.data(), SIMPLIFIED_TEX_SHADER.size(), pClassLinkage, ppPixelShader);
#endif
    } else if (simd_equal(RadialShader, hash) && (QualityVal < 2)) {

        if (!RadialBlurB) {
            RadialBlurB = true;
            log("RadialBlur found");
        }
        return procs->CreatePixelShader(pDevice, NO_RADIALBLUR_SHADER.data(), NO_RADIALBLUR_SHADER.size(), pClassLinkage, ppPixelShader);

    } else if (simd_equal(GrassShader, hash) && (QualityVal < 2)) {
#ifdef NO_GRASS
        return procs->CreatePixelShader(pDevice, NO_FS_GRASS_SHADER.data(), NO_FS_GRASS_SHADER.size(), pClassLinkage, ppPixelShader);
#else
        return procs->CreatePixelShader(pDevice, SIMPLIFIED_FS_GRASS_SHADER.data(), SIMPLIFIED_FS_GRASS_SHADER.size(), pClassLinkage, ppPixelShader);
#endif
    } /* else if (simd_equal(ShadowShader, hash) && (TextureVal == 0)) {
        if (!FragmentShadowB) {
            FragmentShadowB = true;
            log("Fragment Shadow found");
        }
#ifdef OLD_SHADERS
        return procs->CreatePixelShader(pDevice, NO_FS_SHADOW_SHADER.data(), NO_FS_SHADOW_SHADER.size(), pClassLinkage, ppPixelShader);
#else
        return procs->CreatePixelShader(pDevice, SIMPLIFIED_FS_SHADOW_SHADER.data(), SIMPLIFIED_FS_SHADOW_SHADER.size(), pClassLinkage, ppPixelShader);
#endif

    }*/  else if (simd_equal(SphericalShader, hash) && (QualityVal < 2)) {

        if (!SphericalB) {
            SphericalB = true;
            log("Spherical Map found");
        }
        return procs->CreatePixelShader(pDevice, SIMPLIFIED_FS_HIGH_SPHERICAL_SHADER.data(), SIMPLIFIED_FS_HIGH_SPHERICAL_SHADER.size(), pClassLinkage, ppPixelShader);

    } else if (simd_equal(TerrainShader, hash) && (QualityVal == 2)) {
#ifdef OLD_SHADERS
        return procs->CreatePixelShader(pDevice, LOW_FS_TERRAIN_SHADER.data(), LOW_FS_TERRAIN_SHADER.size(), pClassLinkage, ppPixelShader);
#else
        return procs->CreatePixelShader(pDevice, LOW_FS_TERRAIN_SHADER.data(), LOW_FS_TERRAIN_SHADER.size(), pClassLinkage, ppPixelShader);
#endif

    } else if (simd_equal(DefaultShader, hash) && (QualityVal == 2)) {
#ifdef OLD_SHADERS
        return procs->CreatePixelShader(pDevice, SIMPLIFIED_FS_DEFAULT_OLD_SHADER.data(), SIMPLIFIED_FS_DEFAULT_OLD_SHADER.size(), pClassLinkage, ppPixelShader);
#else
        return procs->CreatePixelShader(pDevice, SIMPLIFIED_FS_DEFAULT_SHADER.data(), SIMPLIFIED_FS_DEFAULT_SHADER.size(), pClassLinkage, ppPixelShader);
        // return procs->CreatePixelShader(pDevice, TEST_FS_DEFAULT_SHADER.data(), TEST_FS_DEFAULT_SHADER.size(), pClassLinkage, ppPixelShader);
#endif

    } else if (simd_equal(SphericalShader, hash) && (QualityVal == 2)) {

        if (!SphericalB) {
            SphericalB = true;
            log("Spherical Map found");
        }
        return procs->CreatePixelShader(pDevice, SIMPLIFIED_FS_LOW_SPHERICAL_SHADER.data(), SIMPLIFIED_FS_LOW_SPHERICAL_SHADER.size(), pClassLinkage, ppPixelShader);

    } else if (simd_equal(PlayerHairShader, hash) && (QualityVal == 2)) {

        if (!PlayerHairB) {
            PlayerHairB = true;
            log("Player Hair found");
        }
        return procs->CreatePixelShader(pDevice, SIMPLIFIED_FS_HAIR_PLAYER_SHADER.data(), SIMPLIFIED_FS_HAIR_PLAYER_SHADER.size(), pClassLinkage, ppPixelShader);

    } else if (simd_equal(PlayerFaceShader, hash) && (QualityVal == 2)) {

        if (!PlayerFaceB) {
            PlayerFaceB = true;
            log("Player Face found");
        }
        return procs->CreatePixelShader(pDevice, SIMPLIFIED_FS_FACE_PLAYER_SHADER.data(), SIMPLIFIED_FS_FACE_PLAYER_SHADER.size(), pClassLinkage, ppPixelShader);

    } else if (simd_equal(PlayerCostumeShader, hash) && (QualityVal == 2)) {

        if (!PlayerBodyB) {
            PlayerBodyB = true;
            log("Player Body found");
        }
        return procs->CreatePixelShader(pDevice, SIMPLIFIED_FS_COSTUME_PLAYER_SHADER.data(), SIMPLIFIED_FS_COSTUME_PLAYER_SHADER.size(), pClassLinkage, ppPixelShader);

    } else if (simd_equal(SkyBoxShader, hash)) {

        return procs->CreatePixelShader(pDevice, FS_SKYBOX.data(), FS_SKYBOX.size(), pClassLinkage, ppPixelShader);

    } else if (simd_equal(SkyBoxAniShader, hash)) {

        return procs->CreatePixelShader(pDevice, FS_SKYBOX_ANI.data(), FS_SKYBOX_ANI.size(), pClassLinkage, ppPixelShader);

    } else if (simd_equal(DiffSphericShader, hash) && (QualityVal == 2)) {

        if (!DiffSphericB) {
            DiffSphericB = true;
            log("Diff Spheric found");
        }
        return procs->CreatePixelShader(pDevice, LOW_DIFFSPHERIC_SHADER.data(), LOW_DIFFSPHERIC_SHADER.size(), pClassLinkage, ppPixelShader);

    } else if (simd_equal(DiffSphericShader, hash) && (QualityVal < 2)) {

        if (!DiffSphericB) {
            DiffSphericB = true;
            log("Diff Spheric found");
        }
        return procs->CreatePixelShader(pDevice, HIGH_DIFFSPHERIC_SHADER.data(), HIGH_DIFFSPHERIC_SHADER.size(), pClassLinkage, ppPixelShader);
    }
#ifdef OLD_SHADERS
     else if (simd_equal(UnkShader, hash)) {

        return procs->CreatePixelShader(pDevice, SIMPLIFIED_FS_UNK_SHADER.data(), SIMPLIFIED_FS_UNK_SHADER.size(), pClassLinkage, ppPixelShader);

    }
#endif
    else  if (simd_equal(SwordTrailShader, hash)) {

        return procs->CreatePixelShader(pDevice, FS_SWORDTRAIL_DNPERF.data(), FS_SWORDTRAIL_DNPERF.size(), pClassLinkage, ppPixelShader);

    }
    
    return procs->CreatePixelShader(pDevice, pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);
}

void STDMETHODCALLTYPE ID3D11DeviceContext_UpdateSubresource(
        ID3D11DeviceContext*             pContext,
        ID3D11Resource  *pDstResource,
        UINT            DstSubresource,
        const D3D11_BOX *pDstBox,
        const void      *pSrcData,
        UINT            SrcRowPitch,
        UINT            SrcDepthPitch) {
    auto procs = getContextProcs(pContext);

    procs->UpdateSubresource(pContext, pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);

}

HRESULT STDMETHODCALLTYPE ID3D11DeviceContext_Map(ID3D11DeviceContext* pContext, ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource) {
    auto procs = getContextProcs(pContext);
    HRESULT result = procs->Map(pContext, pResource, Subresource, MapType, MapFlags, pMappedResource);
    return result;
}

inline std::uint64_t crc32(const void* data) {
    const uint8_t* bytes = static_cast<const uint8_t*>(data);
    std::uint64_t crc = 0;
    const uint64_t* qwords = reinterpret_cast<const uint64_t*>(bytes);
    for (size_t i = 0; i < 4; ++i) {
        crc = __builtin_ia32_crc32di(crc, qwords[i]);
    }
    return crc;
}
uint32_t hashv = 0;
HRESULT STDMETHODCALLTYPE ID3D11Device_CreateBuffer(
        ID3D11Device*             pDevice,
  const D3D11_BUFFER_DESC*        pDesc,
  const D3D11_SUBRESOURCE_DATA*   pData,
        ID3D11Buffer**            ppBuffer) {
  auto procs = getDeviceProcs(pDevice);
    if (pDesc->ByteWidth == 136) {
        log(crc32(pDesc));
    }
    if (pDesc->ByteWidth == 576 && pDesc->Usage == D3D11_USAGE_IMMUTABLE && pDesc->BindFlags == D3D11_BIND_INDEX_BUFFER) {
        log(crc32(pDesc));
    }
  return procs->CreateBuffer(pDevice, pDesc, pData, ppBuffer);
}
ID3D11PixelShader* DefPS = nullptr;
ID3D11VertexShader* DefVS = nullptr;
void CreateShaderOnStart(ID3D11Device* pDevice) {
    pDevice->CreatePixelShader(EFFECTS_FS_DEFAULT_SHADER.data(), EFFECTS_FS_DEFAULT_SHADER.size(), nullptr, &DefPS);
    pDevice->CreateVertexShader(EFFECTS_VS_DEFAULT_SHADER.data(), EFFECTS_VS_DEFAULT_SHADER.size(), nullptr, &DefVS);
}

D3D11_BUFFER_DESC desc = { };
ID3D11Buffer* buffer = nullptr;
constexpr std::uint64_t hashwave = 1061255302ull;
constexpr std::uint64_t hashwave2 = 3340896148ull;

void STDMETHODCALLTYPE ID3D11DeviceContext_IASetIndexBuffer(
        ID3D11DeviceContext* pContext,
        ID3D11Buffer* pIndexBuffer,
        DXGI_FORMAT Format,
        UINT Offset) {
    const auto* procs = getContextProcs(pContext);
    if (pIndexBuffer) {
        pIndexBuffer->GetDesc(&desc);
        if (crc32(&desc) == hashwave) {
            pContext->PSSetShader(DefPS, nullptr, 0);
            pContext->VSSetShader(DefVS, nullptr, 0);
        }
        if (crc32(&desc) == hashwave2) {
            pContext->PSSetShader(DefPS, nullptr, 0);
            pContext->VSSetShader(DefVS, nullptr, 0);
        }
    }

    procs->IASetIndexBuffer(pContext, pIndexBuffer, Format, Offset);
}

void STDMETHODCALLTYPE ID3D11DeviceContext_PSSetShader(
        ID3D11DeviceContext* pContext,
        ID3D11PixelShader* pPixelShader,
        ID3D11ClassInstance* const* ppClassInstances,
        UINT NumClassInstances) {
    auto procs = getContextProcs(pContext);
    // pContext->IAGetIndexBuffer(&buffer, nullptr, nullptr);
    // if (buffer) {
    //     buffer->GetDesc(&desc);
    //     buffer->Release();
    //     if (crc32(&desc) == hashwave) {
    //         // pContext->VSSetShader();
    //         // pContext->PSSetShader(DefPS, nullptr, 0);
    //         procs->PSSetShader(pContext, DefPS, ppClassInstances, NumClassInstances);
    //         return;
    //     }
    // }


    // if (pPixelShader == DefPS) {
    //     log("shader was set");
    // }
    procs->PSSetShader(pContext, pPixelShader, ppClassInstances, NumClassInstances);
}

void STDMETHODCALLTYPE ID3D11DeviceContext_DrawIndexed(
        ID3D11DeviceContext* pContext,
        UINT IndexCount,
        UINT StartIndexLocation,
        INT BaseVertexLocation) {
    auto procs = getContextProcs(pContext);
    pContext->IAGetIndexBuffer(&buffer, nullptr, nullptr);
    if (buffer) {
        buffer->GetDesc(&desc);
        buffer->Release();
        if (crc32(&desc) == hashwave) {
            // pContext->VSSetShader();
            pContext->PSSetShader(DefPS, nullptr, 0);
            // log("found");
        }
    }
    procs->DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
}
void STDMETHODCALLTYPE ID3D11DeviceContext_Draw(
        ID3D11DeviceContext* pContext,
        UINT IndexCount,
        UINT StartIndexLocation) {
    // static auto lastFlushTime = std::chrono::steady_clock::now();
    auto procs = getContextProcs(pContext);
    procs->Draw(pContext, IndexCount, StartIndexLocation);
    pContext->Flush();

}
HRESULT STDMETHODCALLTYPE Present(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags) {
    // auto procs = 
    // Allow tearing when SyncInterval is 0 (VSync off)
    if (SyncInterval == 0) {
        Flags |= DXGI_PRESENT_ALLOW_TEARING;
    }
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
    // HOOK_PROC(ID3D11Device, pDevice, procs, 3,  CreateBuffer);
    HOOK_PROC(ID3D11Device, pDevice, procs, 12,  CreateVertexShader); //crashes on AMD
    HOOK_PROC(ID3D11Device, pDevice, procs, 15,  CreatePixelShader);

    g_installedHooks |= HOOK_DEVICE;
}
void hookContext(ID3D11DeviceContext* pContext) {
  std::lock_guard lock(g_hookMutex);

  uint32_t flag = HOOK_IMM_CTX;
  ContextProcs* procs = &g_immContextProcs;

  if (!isImmediatecontext(pContext)) {
    flag = HOOK_DEF_CTX;
    procs = &g_defContextProcs;
  }

  if (g_installedHooks & flag)
    return;

//   HOOK_PROC(ID3D11DeviceContext, pContext, procs, 9, PSSetShader);
//   HOOK_PROC(ID3D11DeviceContext, pContext, procs, 12, DrawIndexed);
//   HOOK_PROC(ID3D11DeviceContext, pContext, procs, 13, Draw);
  HOOK_PROC(ID3D11DeviceContext, pContext, procs, 19, IASetIndexBuffer);
  //   HOOK_PROC(ID3D11DeviceContext, pContext, procs, 14, Map);
    // HOOK_PROC(ID3D11DeviceContext, pContext, procs, 48,  UpdateSubresource);

  g_installedHooks |= flag;

  /* Immediate context and deferred context methods may share code */
  if (flag & HOOK_IMM_CTX)
    g_defContextProcs = g_immContextProcs;
}
}
