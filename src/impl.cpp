#include <array>
#include <bit>
#include <cstdint>
#include <cstring>
#include <mutex>
#include <unordered_map>
#include <vector>

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
using PFN_ID3D11Device_CreateQuery = HRESULT(STDMETHODCALLTYPE*)(ID3D11Device*, const D3D11_QUERY_DESC*, ID3D11Query **);


using PFN_ID3D11DeviceContext_IASetIndexBuffer = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Buffer*, DXGI_FORMAT, UINT);
using PFN_ID3D11DeviceContext_PSSetShader = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11PixelShader*,ID3D11ClassInstance* const*, UINT);
using PFN_ID3D11DeviceContext_DrawIndexed = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, INT);
using PFN_ID3D11DeviceContext_Draw = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT);
using PFN_ID3D11DeviceContext_UpdateSubresource = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Resource*, UINT, const D3D11_BOX*, const void*, UINT, UINT);
using PFN_ID3D11DeviceContext_Map = HRESULT(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Resource*, UINT, D3D11_MAP, UINT, D3D11_MAPPED_SUBRESOURCE*);
using PFN_ID3D11DeviceContext_VSSetShader = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11VertexShader*, ID3D11ClassInstance* const*, UINT);
using PFN_ID3D11DeviceContext_VSSetConstantBuffers = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, ID3D11Buffer* const*);
using PFN_ID3D11DeviceContext_PSSetShaderResources = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, ID3D11ShaderResourceView* const*);
using PFN_ID3D11DeviceContext_IASetVertexBuffers = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, ID3D11Buffer* const*, const UINT*, const UINT*);
using PFN_ID3D11DeviceContext_OMSetRenderTargets = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, ID3D11RenderTargetView* const*, ID3D11DepthStencilView*);
using PFN_ID3D11DeviceContext_OMSetRenderTargetsAndUnorderedAccessViews = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, ID3D11RenderTargetView* const*, ID3D11DepthStencilView*, UINT, UINT, ID3D11UnorderedAccessView* const*, const UINT*);
using PFN_ID3D11DeviceContext_ClearState = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*);
using PFN_ID3D11DeviceContext_ExecuteCommandList = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11CommandList*, BOOL);
using PFN_ID3D11DeviceContext_FinishCommandList = HRESULT(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, BOOL, ID3D11CommandList**);
using PFN_ID3D11DeviceContext_Unmap = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11Resource*, UINT);
using PFN_ID3D11DeviceContext_PSSetConstantBuffers = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, ID3D11Buffer* const*);
using PFN_ID3D11DeviceContext_PSSetSamplers = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, UINT, ID3D11SamplerState* const*);
using PFN_ID3D11DeviceContext_IASetInputLayout = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11InputLayout*);
using PFN_ID3D11DeviceContext_IASetPrimitiveTopology = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, D3D11_PRIMITIVE_TOPOLOGY);
using PFN_ID3D11DeviceContext_OMSetBlendState = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11BlendState*, const FLOAT*, UINT);
using PFN_ID3D11DeviceContext_OMSetDepthStencilState = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11DepthStencilState*, UINT);
using PFN_ID3D11DeviceContext_RSSetState = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, ID3D11RasterizerState*);
using PFN_ID3D11DeviceContext_RSSetViewports = void(STDMETHODCALLTYPE*)(ID3D11DeviceContext*, UINT, const D3D11_VIEWPORT*);

using PFN_IDXGISwapChain_Present = HRESULT(STDMETHODCALLTYPE*)(IDXGISwapChain*, UINT, UINT);

struct DeviceProcs {
    PFN_ID3D11Device_CreateBuffer                           CreateBuffer                    = nullptr;
    PFN_ID3D11Device_CreateVertexShader                     CreateVertexShader              = nullptr;
    PFN_ID3D11Device_CreatePixelShader                      CreatePixelShader               = nullptr;
    PFN_ID3D11Device_CreateQuery                            CreateQuery                     = nullptr;
};

struct ContextProcs {
    PFN_ID3D11DeviceContext_IASetIndexBuffer                IASetIndexBuffer                = nullptr;
    PFN_ID3D11DeviceContext_PSSetShader                     PSSetShader                     = nullptr;
    PFN_ID3D11DeviceContext_DrawIndexed                     DrawIndexed                     = nullptr;
    PFN_ID3D11DeviceContext_Draw                            Draw                            = nullptr;
    PFN_ID3D11DeviceContext_UpdateSubresource               UpdateSubresource               = nullptr;
    PFN_ID3D11DeviceContext_Map                             Map                             = nullptr;
    PFN_ID3D11DeviceContext_VSSetShader VSSetShader = nullptr;
    PFN_ID3D11DeviceContext_VSSetConstantBuffers VSSetConstantBuffers = nullptr;
    PFN_ID3D11DeviceContext_PSSetShaderResources PSSetShaderResources = nullptr;
    PFN_ID3D11DeviceContext_IASetVertexBuffers IASetVertexBuffers = nullptr;
    PFN_ID3D11DeviceContext_OMSetRenderTargets OMSetRenderTargets = nullptr;
    PFN_ID3D11DeviceContext_OMSetRenderTargetsAndUnorderedAccessViews OMSetRenderTargetsAndUnorderedAccessViews = nullptr;
    PFN_ID3D11DeviceContext_ClearState ClearState = nullptr;
    PFN_ID3D11DeviceContext_ExecuteCommandList ExecuteCommandList = nullptr;
    PFN_ID3D11DeviceContext_FinishCommandList FinishCommandList = nullptr;
    PFN_ID3D11DeviceContext_Unmap Unmap = nullptr;
    PFN_ID3D11DeviceContext_PSSetConstantBuffers PSSetConstantBuffers = nullptr;
    PFN_ID3D11DeviceContext_PSSetSamplers PSSetSamplers = nullptr;
    PFN_ID3D11DeviceContext_IASetInputLayout IASetInputLayout = nullptr;
    PFN_ID3D11DeviceContext_IASetPrimitiveTopology IASetPrimitiveTopology = nullptr;
    PFN_ID3D11DeviceContext_OMSetBlendState OMSetBlendState = nullptr;
    PFN_ID3D11DeviceContext_OMSetDepthStencilState OMSetDepthStencilState = nullptr;
    PFN_ID3D11DeviceContext_RSSetState RSSetState = nullptr;
    PFN_ID3D11DeviceContext_RSSetViewports RSSetViewports = nullptr;
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

static void* const kUnknown = reinterpret_cast<void*>(~static_cast<uintptr_t>(0));

struct ShadowState {
    void* vs;
    void* ps;
    void* ib;
    DXGI_FORMAT ibFmt;
    UINT ibOff;
    void* vb[32];
    UINT vbStride[32];
    UINT vbOff[32];
    void* psSrv[128];
    void* vsCb[14];
    void* psCb[14];
    void* psSamp[16];
    void* layout;
    D3D11_PRIMITIVE_TOPOLOGY topo;
    void* rs;
    void* ds;
    UINT dsRef;
    void* blend;
    FLOAT blendFactor[4];
    UINT sampleMask;
    UINT vpCount;
    D3D11_VIEWPORT vp[16];

    ShadowState() { reset(); }

    void reset() {
        vs = kUnknown;
        ps = kUnknown;
        ib = kUnknown;
        ibFmt = DXGI_FORMAT_UNKNOWN;
        ibOff = 0;
        for (auto& p : vb) p = kUnknown;
        for (auto& v : vbStride) v = 0;
        for (auto& v : vbOff) v = 0;
        for (auto& p : psSrv) p = kUnknown;
        for (auto& p : vsCb) p = kUnknown;
        for (auto& p : psCb) p = kUnknown;
        for (auto& p : psSamp) p = kUnknown;
        layout = kUnknown;
        topo = D3D11_PRIMITIVE_TOPOLOGY_UNDEFINED;
        rs = kUnknown;
        ds = kUnknown;
        dsRef = 0;
        blend = kUnknown;
        for (auto& f : blendFactor) f = 0.0f;
        sampleMask = 0;
        vpCount = ~0u;
        std::memset(vp, 0, sizeof(vp));
    }
};

static mutex g_stateMutex;
static std::unordered_map<ID3D11DeviceContext*, ShadowState> g_states;
static std::unordered_map<ID3D11Resource*, UpdateSubresourceCache> g_updateCache;

struct MapShadow {
    std::vector<uint8_t> scratch;
    std::vector<uint8_t> last;
    bool valid = false;
    bool pending = false;
    UINT subresource = 0;
};
static std::unordered_map<ID3D11Resource*, MapShadow> g_mapShadows;

inline bool isShadowedConstantSize(UINT width) {
    return width == 16 || width == 2192 || width == 8400;
}

inline ShadowState* getState(ID3D11DeviceContext* pContext) {
    static thread_local ID3D11DeviceContext* lastContext = nullptr;
    static thread_local ShadowState* lastState = nullptr;
    if (pContext == lastContext) {
        return lastState;
    }
    std::lock_guard<mutex> lock(g_stateMutex);
    lastContext = pContext;
    lastState = &g_states[pContext];
    return lastState;
}

template<size_t N, typename T>
inline bool slotsRedundant(void* (&cache)[N], UINT start, UINT num, T* const* incoming) {
    if (!incoming || start >= N || num > N - start) {
        for (auto& p : cache) p = kUnknown;
        return false;
    }
    bool same = true;
    for (UINT i = 0; i < num; ++i) {
        if (cache[start + i] != static_cast<void*>(incoming[i])) {
            same = false;
            break;
        }
    }
    if (same) {
        return true;
    }
    for (UINT i = 0; i < num; ++i) {
        cache[start + i] = static_cast<void*>(incoming[i]);
    }
    return false;
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

    } /*else if (simd_equal(PlayerShader, hash) && (TextureVal == 0)) {

        if (!VSPlayerB) {
            VSPlayerB = true;
            log("VS Player found");
        }
        return procs->CreateVertexShader(pDevice, SIMPLIFIED_VS_PLAYER_SHADER.data(), SIMPLIFIED_VS_PLAYER_SHADER.size(), pClassLinkage, ppVertexShader);

    } */else if (simd_equal(DefaultShader, hash) && (QualityVal == 2)) {

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
uint32_t TextureVal = 0U;
uint32_t QualityVal = 0U;

HRESULT STDMETHODCALLTYPE ID3D11Device_CreatePixelShader(
    ID3D11Device* pDevice,
    const void* pShaderBytecode,
    SIZE_T                  BytecodeLength,
    ID3D11ClassLinkage* pClassLinkage,
    ID3D11PixelShader** ppPixelShader) {
    const auto* procs = getDeviceProcs(pDevice);

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

    } /*else if (simd_equal(SkyBoxAniShader, hash)) {

        return procs->CreatePixelShader(pDevice, FS_SKYBOX_ANI.data(), FS_SKYBOX_ANI.size(), pClassLinkage, ppPixelShader);

    } */else if (simd_equal(DiffSphericShader, hash) && (QualityVal == 2)) {

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

    if (pDstResource && pSrcData && !pDstBox && isImmediatecontext(pContext)) {
        D3D11_RESOURCE_DIMENSION dim;
        pDstResource->GetType(&dim);
        if (dim == D3D11_RESOURCE_DIMENSION_BUFFER) {
            D3D11_BUFFER_DESC bufferDesc;
            static_cast<ID3D11Buffer*>(pDstResource)->GetDesc(&bufferDesc);
            if ((bufferDesc.ByteWidth == 2192 || bufferDesc.ByteWidth == 16 || bufferDesc.ByteWidth == 8400) && (bufferDesc.BindFlags & D3D11_BIND_CONSTANT_BUFFER)) {
                auto& entry = g_updateCache[pDstResource];
                const auto* bytes = static_cast<const uint8_t*>(pSrcData);
                if (entry.resource == pDstResource && entry.subresource == DstSubresource &&
                    entry.data.size() == bufferDesc.ByteWidth &&
                    std::memcmp(entry.data.data(), bytes, bufferDesc.ByteWidth) == 0) {
                    return;
                }
                if (!entry.resource) {
                    pDstResource->AddRef();
                    entry.resource = pDstResource;
                }
                entry.subresource = DstSubresource;
                entry.data.assign(bytes, bytes + bufferDesc.ByteWidth);
            }
        }
    }

    procs->UpdateSubresource(pContext, pDstResource, DstSubresource, pDstBox, pSrcData, SrcRowPitch, SrcDepthPitch);

}

HRESULT STDMETHODCALLTYPE ID3D11DeviceContext_Map(ID3D11DeviceContext* pContext, ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource) {
    auto procs = getContextProcs(pContext);
    if (pResource && pMappedResource && isImmediatecontext(pContext)) {
        auto it = g_mapShadows.find(pResource);
        if (it == g_mapShadows.end() && MapType == D3D11_MAP_WRITE_DISCARD) {
            D3D11_RESOURCE_DIMENSION dim;
            pResource->GetType(&dim);
            if (dim == D3D11_RESOURCE_DIMENSION_BUFFER) {
                D3D11_BUFFER_DESC bufferDesc;
                static_cast<ID3D11Buffer*>(pResource)->GetDesc(&bufferDesc);
                if (bufferDesc.Usage == D3D11_USAGE_DYNAMIC && (bufferDesc.BindFlags & D3D11_BIND_CONSTANT_BUFFER) &&
                    isShadowedConstantSize(bufferDesc.ByteWidth)) {
                    pResource->AddRef();
                    it = g_mapShadows.emplace(pResource, MapShadow{}).first;
                    it->second.last.assign(bufferDesc.ByteWidth, 0);
                    it->second.scratch.assign(bufferDesc.ByteWidth, 0);
                }
            }
        }
        if (it != g_mapShadows.end()) {
            auto& entry = it->second;
            if (MapType == D3D11_MAP_WRITE_DISCARD && !entry.pending) {
                entry.scratch = entry.last;
                entry.pending = true;
                entry.subresource = Subresource;
                pMappedResource->pData = entry.scratch.data();
                pMappedResource->RowPitch = static_cast<UINT>(entry.scratch.size());
                pMappedResource->DepthPitch = static_cast<UINT>(entry.scratch.size());
                return S_OK;
            }
            entry.valid = false;
        }
    }
    return procs->Map(pContext, pResource, Subresource, MapType, MapFlags, pMappedResource);
}

void STDMETHODCALLTYPE ID3D11DeviceContext_Unmap(ID3D11DeviceContext* pContext, ID3D11Resource* pResource, UINT Subresource) {
    auto procs = getContextProcs(pContext);
    if (pResource && isImmediatecontext(pContext)) {
        auto it = g_mapShadows.find(pResource);
        if (it != g_mapShadows.end() && it->second.pending) {
            auto& entry = it->second;
            entry.pending = false;
            if (entry.valid && entry.scratch == entry.last) {
                return;
            }
            D3D11_MAPPED_SUBRESOURCE mapped;
            if (SUCCEEDED(procs->Map(pContext, pResource, entry.subresource, D3D11_MAP_WRITE_DISCARD, 0, &mapped))) {
                std::memcpy(mapped.pData, entry.scratch.data(), entry.scratch.size());
                procs->Unmap(pContext, pResource, entry.subresource);
                entry.last = entry.scratch;
                entry.valid = true;
            } else {
                entry.valid = false;
            }
            return;
        }
    }
    procs->Unmap(pContext, pResource, Subresource);
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

    auto* state = getState(pContext);
    if (state->ib == static_cast<void*>(pIndexBuffer) && state->ibFmt == Format && state->ibOff == Offset) {
        return;
    }
    state->ib = pIndexBuffer;
    state->ibFmt = Format;
    state->ibOff = Offset;

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
    auto* state = getState(pContext);
    if (NumClassInstances == 0 && state->ps == static_cast<void*>(pPixelShader)) {
        return;
    }
    state->ps = (NumClassInstances == 0) ? static_cast<void*>(pPixelShader) : kUnknown;
    procs->PSSetShader(pContext, pPixelShader, ppClassInstances, NumClassInstances);
}

void STDMETHODCALLTYPE ID3D11DeviceContext_VSSetShader(
        ID3D11DeviceContext* pContext,
        ID3D11VertexShader* pVertexShader,
        ID3D11ClassInstance* const* ppClassInstances,
        UINT NumClassInstances) {
    auto* state = getState(pContext);
    if (NumClassInstances == 0 && state->vs == static_cast<void*>(pVertexShader)) {
        return;
    }
    state->vs = (NumClassInstances == 0) ? static_cast<void*>(pVertexShader) : kUnknown;
    getContextProcs(pContext)->VSSetShader(pContext, pVertexShader, ppClassInstances, NumClassInstances);
}

void STDMETHODCALLTYPE ID3D11DeviceContext_VSSetConstantBuffers(
        ID3D11DeviceContext* pContext,
        UINT StartSlot,
        UINT NumBuffers,
        ID3D11Buffer* const* ppConstantBuffers) {
    auto* state = getState(pContext);
    if (slotsRedundant(state->vsCb, StartSlot, NumBuffers, ppConstantBuffers)) {
        return;
    }
    getContextProcs(pContext)->VSSetConstantBuffers(pContext, StartSlot, NumBuffers, ppConstantBuffers);
}

void STDMETHODCALLTYPE ID3D11DeviceContext_PSSetShaderResources(
        ID3D11DeviceContext* pContext,
        UINT StartSlot,
        UINT NumViews,
        ID3D11ShaderResourceView* const* ppShaderResourceViews) {
    auto* state = getState(pContext);
    if (slotsRedundant(state->psSrv, StartSlot, NumViews, ppShaderResourceViews)) {
        return;
    }
    getContextProcs(pContext)->PSSetShaderResources(pContext, StartSlot, NumViews, ppShaderResourceViews);
}

void STDMETHODCALLTYPE ID3D11DeviceContext_IASetVertexBuffers(
        ID3D11DeviceContext* pContext,
        UINT StartSlot,
        UINT NumBuffers,
        ID3D11Buffer* const* ppVertexBuffers,
        const UINT* pStrides,
        const UINT* pOffsets) {
    auto* state = getState(pContext);
    if (ppVertexBuffers && pStrides && pOffsets && StartSlot < 32 && NumBuffers <= 32 - StartSlot) {
        bool same = true;
        for (UINT i = 0; i < NumBuffers; ++i) {
            const UINT slot = StartSlot + i;
            if (state->vb[slot] != static_cast<void*>(ppVertexBuffers[i]) ||
                state->vbStride[slot] != pStrides[i] ||
                state->vbOff[slot] != pOffsets[i]) {
                same = false;
                break;
            }
        }
        if (same) {
            return;
        }
        for (UINT i = 0; i < NumBuffers; ++i) {
            const UINT slot = StartSlot + i;
            state->vb[slot] = static_cast<void*>(ppVertexBuffers[i]);
            state->vbStride[slot] = pStrides[i];
            state->vbOff[slot] = pOffsets[i];
        }
    } else {
        for (auto& p : state->vb) p = kUnknown;
    }
    getContextProcs(pContext)->IASetVertexBuffers(pContext, StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets);
}

void STDMETHODCALLTYPE ID3D11DeviceContext_PSSetConstantBuffers(
        ID3D11DeviceContext* pContext,
        UINT StartSlot,
        UINT NumBuffers,
        ID3D11Buffer* const* ppConstantBuffers) {
    auto* state = getState(pContext);
    if (slotsRedundant(state->psCb, StartSlot, NumBuffers, ppConstantBuffers)) {
        return;
    }
    getContextProcs(pContext)->PSSetConstantBuffers(pContext, StartSlot, NumBuffers, ppConstantBuffers);
}

void STDMETHODCALLTYPE ID3D11DeviceContext_PSSetSamplers(
        ID3D11DeviceContext* pContext,
        UINT StartSlot,
        UINT NumSamplers,
        ID3D11SamplerState* const* ppSamplers) {
    auto* state = getState(pContext);
    if (slotsRedundant(state->psSamp, StartSlot, NumSamplers, ppSamplers)) {
        return;
    }
    getContextProcs(pContext)->PSSetSamplers(pContext, StartSlot, NumSamplers, ppSamplers);
}

void STDMETHODCALLTYPE ID3D11DeviceContext_IASetInputLayout(
        ID3D11DeviceContext* pContext,
        ID3D11InputLayout* pInputLayout) {
    auto* state = getState(pContext);
    if (state->layout == static_cast<void*>(pInputLayout)) {
        return;
    }
    state->layout = pInputLayout;
    getContextProcs(pContext)->IASetInputLayout(pContext, pInputLayout);
}

void STDMETHODCALLTYPE ID3D11DeviceContext_IASetPrimitiveTopology(
        ID3D11DeviceContext* pContext,
        D3D11_PRIMITIVE_TOPOLOGY Topology) {
    auto* state = getState(pContext);
    if (state->topo == Topology) {
        return;
    }
    state->topo = Topology;
    getContextProcs(pContext)->IASetPrimitiveTopology(pContext, Topology);
}

void STDMETHODCALLTYPE ID3D11DeviceContext_OMSetBlendState(
        ID3D11DeviceContext* pContext,
        ID3D11BlendState* pBlendState,
        const FLOAT* BlendFactor,
        UINT SampleMask) {
    static const FLOAT defaultFactor[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    const FLOAT* factor = BlendFactor ? BlendFactor : defaultFactor;
    auto* state = getState(pContext);
    if (state->blend == static_cast<void*>(pBlendState) && state->sampleMask == SampleMask &&
        std::memcmp(state->blendFactor, factor, sizeof(state->blendFactor)) == 0) {
        return;
    }
    state->blend = pBlendState;
    state->sampleMask = SampleMask;
    std::memcpy(state->blendFactor, factor, sizeof(state->blendFactor));
    getContextProcs(pContext)->OMSetBlendState(pContext, pBlendState, BlendFactor, SampleMask);
}

void STDMETHODCALLTYPE ID3D11DeviceContext_OMSetDepthStencilState(
        ID3D11DeviceContext* pContext,
        ID3D11DepthStencilState* pDepthStencilState,
        UINT StencilRef) {
    auto* state = getState(pContext);
    if (state->ds == static_cast<void*>(pDepthStencilState) && state->dsRef == StencilRef) {
        return;
    }
    state->ds = pDepthStencilState;
    state->dsRef = StencilRef;
    getContextProcs(pContext)->OMSetDepthStencilState(pContext, pDepthStencilState, StencilRef);
}

void STDMETHODCALLTYPE ID3D11DeviceContext_RSSetState(
        ID3D11DeviceContext* pContext,
        ID3D11RasterizerState* pRasterizerState) {
    auto* state = getState(pContext);
    if (state->rs == static_cast<void*>(pRasterizerState)) {
        return;
    }
    state->rs = pRasterizerState;
    getContextProcs(pContext)->RSSetState(pContext, pRasterizerState);
}

void STDMETHODCALLTYPE ID3D11DeviceContext_RSSetViewports(
        ID3D11DeviceContext* pContext,
        UINT NumViewports,
        const D3D11_VIEWPORT* pViewports) {
    auto* state = getState(pContext);
    if (pViewports && NumViewports <= 16) {
        const size_t bytes = sizeof(D3D11_VIEWPORT) * NumViewports;
        if (state->vpCount == NumViewports && std::memcmp(state->vp, pViewports, bytes) == 0) {
            return;
        }
        state->vpCount = NumViewports;
        std::memcpy(state->vp, pViewports, bytes);
    } else {
        state->vpCount = ~0u;
    }
    getContextProcs(pContext)->RSSetViewports(pContext, NumViewports, pViewports);
}

void STDMETHODCALLTYPE ID3D11DeviceContext_OMSetRenderTargets(
        ID3D11DeviceContext* pContext,
        UINT NumViews,
        ID3D11RenderTargetView* const* ppRenderTargetViews,
        ID3D11DepthStencilView* pDepthStencilView) {
    for (auto& p : getState(pContext)->psSrv) p = kUnknown;
    getContextProcs(pContext)->OMSetRenderTargets(pContext, NumViews, ppRenderTargetViews, pDepthStencilView);
}

void STDMETHODCALLTYPE ID3D11DeviceContext_OMSetRenderTargetsAndUnorderedAccessViews(
        ID3D11DeviceContext* pContext,
        UINT NumRTVs,
        ID3D11RenderTargetView* const* ppRenderTargetViews,
        ID3D11DepthStencilView* pDepthStencilView,
        UINT UAVStartSlot,
        UINT NumUAVs,
        ID3D11UnorderedAccessView* const* ppUnorderedAccessViews,
        const UINT* pUAVInitialCounts) {
    for (auto& p : getState(pContext)->psSrv) p = kUnknown;
    getContextProcs(pContext)->OMSetRenderTargetsAndUnorderedAccessViews(pContext, NumRTVs, ppRenderTargetViews, pDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts);
}

void STDMETHODCALLTYPE ID3D11DeviceContext_ClearState(ID3D11DeviceContext* pContext) {
    getState(pContext)->reset();
    getContextProcs(pContext)->ClearState(pContext);
}

void STDMETHODCALLTYPE ID3D11DeviceContext_ExecuteCommandList(
        ID3D11DeviceContext* pContext,
        ID3D11CommandList* pCommandList,
        BOOL RestoreContextState) {
    getContextProcs(pContext)->ExecuteCommandList(pContext, pCommandList, RestoreContextState);
    getState(pContext)->reset();
}

HRESULT STDMETHODCALLTYPE ID3D11DeviceContext_FinishCommandList(
        ID3D11DeviceContext* pContext,
        BOOL RestoreDeferredContextState,
        ID3D11CommandList** ppCommandList) {
    HRESULT hr = getContextProcs(pContext)->FinishCommandList(pContext, RestoreDeferredContextState, ppCommandList);
    getState(pContext)->reset();
    return hr;
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

HRESULT STDMETHODCALLTYPE ID3D11Device_CreateQuery(ID3D11Device* pDevice, const D3D11_QUERY_DESC* pQueryDesc, ID3D11Query** ppQuery)  {
    const auto* procs = getDeviceProcs(pDevice);

    if (pQueryDesc->Query == D3D11_QUERY_TIMESTAMP ||
        /*pQueryDesc->Query == D3D11_QUERY_OCCLUSION ||*/ // dont disable it will cause graphical issue
        pQueryDesc->Query == D3D11_QUERY_TIMESTAMP_DISJOINT)
    {
        *ppQuery = nullptr;
        return S_OK;
    }

    return procs->CreateQuery(pDevice, pQueryDesc, ppQuery);
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
    // HOOK_PROC(ID3D11Device, pDevice, procs, 24,  CreateQuery);

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

  HOOK_PROC(ID3D11DeviceContext, pContext, procs, 9, PSSetShader);
//   HOOK_PROC(ID3D11DeviceContext, pContext, procs, 12, DrawIndexed);
//   HOOK_PROC(ID3D11DeviceContext, pContext, procs, 13, Draw);
  HOOK_PROC(ID3D11DeviceContext, pContext, procs, 19, IASetIndexBuffer);
  HOOK_PROC(ID3D11DeviceContext, pContext, procs, 14, Map);
  HOOK_PROC(ID3D11DeviceContext, pContext, procs, 48,  UpdateSubresource);
  HOOK_PROC(ID3D11DeviceContext, pContext, procs, 7, VSSetConstantBuffers);
  HOOK_PROC(ID3D11DeviceContext, pContext, procs, 8, PSSetShaderResources);
  HOOK_PROC(ID3D11DeviceContext, pContext, procs, 11, VSSetShader);
  HOOK_PROC(ID3D11DeviceContext, pContext, procs, 18, IASetVertexBuffers);
  HOOK_PROC(ID3D11DeviceContext, pContext, procs, 33, OMSetRenderTargets);
  HOOK_PROC(ID3D11DeviceContext, pContext, procs, 34, OMSetRenderTargetsAndUnorderedAccessViews);
  HOOK_PROC(ID3D11DeviceContext, pContext, procs, 58, ExecuteCommandList);
  HOOK_PROC(ID3D11DeviceContext, pContext, procs, 110, ClearState);
  HOOK_PROC(ID3D11DeviceContext, pContext, procs, 114, FinishCommandList);
  HOOK_PROC(ID3D11DeviceContext, pContext, procs, 10, PSSetSamplers);
  HOOK_PROC(ID3D11DeviceContext, pContext, procs, 15, Unmap);
  HOOK_PROC(ID3D11DeviceContext, pContext, procs, 16, PSSetConstantBuffers);
  HOOK_PROC(ID3D11DeviceContext, pContext, procs, 17, IASetInputLayout);
  HOOK_PROC(ID3D11DeviceContext, pContext, procs, 24, IASetPrimitiveTopology);
  HOOK_PROC(ID3D11DeviceContext, pContext, procs, 35, OMSetBlendState);
  HOOK_PROC(ID3D11DeviceContext, pContext, procs, 36, OMSetDepthStencilState);
  HOOK_PROC(ID3D11DeviceContext, pContext, procs, 43, RSSetState);
  HOOK_PROC(ID3D11DeviceContext, pContext, procs, 44, RSSetViewports);

  g_installedHooks |= flag;

  /* Immediate context and deferred context methods may share code */
  if (flag & HOOK_IMM_CTX)
    g_defContextProcs = g_immContextProcs;
}
}