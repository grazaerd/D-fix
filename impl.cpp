#include <array>
#include <cstring>
#include <immintrin.h>

#include "impl.h"
#include "Shaderbool.h"
#include "Shaders/Default.h"
#include "Shaders/Grass.h"
#include "Shaders/Particle1.h"
#include "Shaders/RadialBlur.h"
#include "Shaders/Shadow.h"
#include "Shaders/Terrain.h"
#include "Shaders/Tex.h"
#include "Shaders/VolumeFog.h"
#include "util.h"


namespace atfix {

extern "C" bool IsAMD();

/** Hooking-related stuff */
using PFN_ID3D11Device_CreateVertexShader = HRESULT(STDMETHODCALLTYPE*) (ID3D11Device*, const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11VertexShader**);
using PFN_ID3D11Device_CreatePixelShader = HRESULT(STDMETHODCALLTYPE*) (ID3D11Device*, const void*, SIZE_T, ID3D11ClassLinkage*, ID3D11PixelShader**);

struct DeviceProcs {
  PFN_ID3D11Device_CreateVertexShader                   CreateVertexShader            = nullptr;
  PFN_ID3D11Device_CreatePixelShader                    CreatePixelShader             = nullptr;
};

static mutex  g_hookMutex;
static mutex  g_globalMutex;

DeviceProcs   g_deviceProcs;

constexpr uint32_t HOOK_DEVICE  = (1u << 0);

uint32_t      g_installedHooks = 0u;

const DeviceProcs* getDeviceProcs(ID3D11Device* pDevice) {
  return &g_deviceProcs;
}

HRESULT STDMETHODCALLTYPE ID3D11Device_CreateVertexShader(
        ID3D11Device*           pDevice,
        const void*             pShaderBytecode,
        SIZE_T                  BytecodeLength,
        ID3D11ClassLinkage*     pClassLinkage,
        ID3D11VertexShader**    ppVertexShader) {
    auto procs = getDeviceProcs(pDevice);

    static constexpr std::array<uint32_t, 4> ParticleShader1 = { 0x231fb2e6, 0xc211f72b, 0x1a0b5fbb, 0xe9e36557 };
    static constexpr std::array<uint32_t, 4> ParticleShader2 = { 0x003ca944, 0x7fb09127, 0xed8e5b6e, 0x4cbdd6e9 };
    static constexpr std::array<uint32_t, 4> VolumeFogShader = { 0xdf94514a, 0xbe2cf252, 0xf86fcdba, 0x640e1563 };
    static constexpr std::array<uint32_t, 4> GrassShader = { 0x5272db3c, 0xdc7a397a, 0xb7bf11d5, 0x078d9485 };
    static constexpr std::array<uint32_t, 4> ShadowPlayerShader = { 0xe4c7cd57, 0xbc029e48, 0xabcb38c1, 0xeae68c10 };
    static constexpr std::array<uint32_t, 4> ShadowPropShader = { 0xefbe9f94, 0x5c300015, 0x29ab6626, 0xb640836c };
    static constexpr std::array<uint32_t, 4> TerrainShader = { 0xe0dfec90, 0xc8480b86, 0x20262b5d, 0xf0ace17e };
    static constexpr std::array<uint32_t, 4> DefaultShader = { 0x49d8396e, 0x5b9dfd57, 0xb4f45dba, 0xe6d8b741 };

    const uint32_t* hash = reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(pShaderBytecode) + 4);
    bool AMD = IsAMD();
    if (std::equal(ParticleShader1.begin(), ParticleShader1.end(), hash)) {

        if (!Particle1B) {
            Particle1B = true;
            log("Particle found");
        }
        if (AMD) {
            return procs->CreateVertexShader(pDevice, FIXED_PARTICLE_SHADER1, sizeof(FIXED_PARTICLE_SHADER1), pClassLinkage, ppVertexShader);
        } else {
            return procs->CreateVertexShader(pDevice, pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
        }

    } else if (std::equal(ParticleShader2.begin(), ParticleShader2.end(), hash)) {

        if (!Particle2B) {
            Particle2B = true;
            log("Particle Iterate found");
        }
        if (AMD) {
            return procs->CreateVertexShader(pDevice, FIXED_PARTICLE_SHADER2, sizeof(FIXED_PARTICLE_SHADER2), pClassLinkage, ppVertexShader);
        } else {
            return procs->CreateVertexShader(pDevice, pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
        }
    }
#ifndef RELEASELOW
    else if (std::equal(VolumeFogShader.begin(), VolumeFogShader.end(), hash)) {

        if (!VolumeFogB) {
            VolumeFogB = true;
            log("Volumefog found");
        }
        return procs->CreateVertexShader(pDevice, NO_VOLUMEFOG_SHADER, sizeof(NO_VOLUMEFOG_SHADER), pClassLinkage, ppVertexShader);

    } else if (std::equal(GrassShader.begin(), GrassShader.end(), hash)) {

        if (!GrassB) {
            GrassB = true;
            log("Grass found");
        }
        return procs->CreateVertexShader(pDevice, SIMPLIFIED_VS_GRASS_SHADER, sizeof(SIMPLIFIED_VS_GRASS_SHADER), pClassLinkage, ppVertexShader);

    } else if (std::equal(ShadowPlayerShader.begin(), ShadowPlayerShader.end(), hash)) {

        if (!ShadowPlayerB) {
            ShadowPlayerB = true;
            log("Shadow Player found");
        }
        return procs->CreateVertexShader(pDevice, FIXED_PLAYER_SHADOW_SHADER, sizeof(FIXED_PLAYER_SHADOW_SHADER), pClassLinkage, ppVertexShader);

    } else if (std::equal(ShadowPropShader.begin(), ShadowPropShader.end(), hash)) {

        if (!ShadowPropB) {
            ShadowPropB = true;
            log("Shadow Prop found");
        }
        return procs->CreateVertexShader(pDevice, FIXED_PROP_SHADOW_SHADER, sizeof(FIXED_PROP_SHADOW_SHADER), pClassLinkage, ppVertexShader);

    } 
#else
    else if (std::equal(TerrainShader.begin(), TerrainShader.end(), hash)) {

        if (!TerrainB) {
            TerrainB = true;
            log("Terrain found");
        }
        return procs->CreateVertexShader(pDevice, LOW_VS_TERRAIN_SHADER, sizeof(LOW_VS_TERRAIN_SHADER), pClassLinkage, ppVertexShader);

    } 
    else if (std::equal(DefaultShader.begin(), DefaultShader.end(), hash)) {

        if (!DefaultB) {
            DefaultB = true;
            log("Default found");
        }
        return procs->CreateVertexShader(pDevice, SIMPLIFIED_VS_DEFAULT_SHADER, sizeof(SIMPLIFIED_VS_DEFAULT_SHADER), pClassLinkage, ppVertexShader);

    }
#endif
    return procs->CreateVertexShader(pDevice, pShaderBytecode, BytecodeLength, pClassLinkage, ppVertexShader);
}

HRESULT STDMETHODCALLTYPE ID3D11Device_CreatePixelShader(
    ID3D11Device* pDevice,
    const void* pShaderBytecode,
    SIZE_T                  BytecodeLength,
    ID3D11ClassLinkage* pClassLinkage,
    ID3D11PixelShader** ppPixelShader) {
    auto procs = getDeviceProcs(pDevice);

    static constexpr std::array<uint32_t, 4> TexShader = { 0x4342435a, 0xd5824908, 0x23e6147a, 0x3ec4c9ea };
    static constexpr std::array<uint32_t, 4> RadialShader = { 0xcf3dfb4b, 0x6c82c337, 0xec6459ee, 0x0a2b4c01 };
    static constexpr std::array<uint32_t, 4> GrassShader = { 0xb2f29488, 0x210994ca, 0x07510660, 0x301d1575 };
    static constexpr std::array<uint32_t, 4> ShadowShader = { 0xbb5a2d0a, 0x29d139b7, 0x40992005, 0xf3b46588 };
    static constexpr std::array<uint32_t, 4> TerrainShader = { 0x74a9f538, 0x75cb0ce6, 0x3da09498, 0x7bc641bd };
    static constexpr std::array<uint32_t, 4> DefaultShader = { 0x5cbbb737, 0x265384da, 0x36d6d037, 0x1b052f54 };

    const uint32_t* hash = reinterpret_cast<const uint32_t*>(reinterpret_cast<const uint8_t*>(pShaderBytecode) + 4);


    if (std::equal(TexShader.begin(), TexShader.end(), hash)) {

        if (!DiffVolTexB) {
            DiffVolTexB = true;
            log("DiffVolTex found");
        }
        return procs->CreatePixelShader(pDevice, SIMPLIFIED_TEX_SHADER, sizeof(SIMPLIFIED_TEX_SHADER), pClassLinkage, ppPixelShader);

    }
#ifndef RELEASELOW
    else if (std::equal(RadialShader.begin(), RadialShader.end(), hash)) {

        if (!RadialBlurB) {
            RadialBlurB = true;
            log("RadialBlur found");
        }
        return procs->CreatePixelShader(pDevice, NO_RADIALBLUR_SHADER, sizeof(NO_RADIALBLUR_SHADER), pClassLinkage, ppPixelShader);

    } else if (std::equal(GrassShader.begin(), GrassShader.end(), hash)) {

        return procs->CreatePixelShader(pDevice, SIMPLIFIED_FS_GRASS_SHADER, sizeof(SIMPLIFIED_FS_GRASS_SHADER), pClassLinkage, ppPixelShader);

    } else if (std::equal(ShadowShader.begin(), ShadowShader.end(), hash)) {

        if (!FragmentShadowB) {
            FragmentShadowB = true;
            log("Fragment Shadow found");
        }
        return procs->CreatePixelShader(pDevice, SIMPLIFIED_FS_SHADOW_SHADER, sizeof(SIMPLIFIED_FS_SHADOW_SHADER), pClassLinkage, ppPixelShader);

    }
#else
    else if (std::equal(TerrainShader.begin(), TerrainShader.end(), hash)) {

        return procs->CreatePixelShader(pDevice, LOW_FS_TERRAIN_SHADER, sizeof(LOW_FS_TERRAIN_SHADER), pClassLinkage, ppPixelShader);
    }    
    else if (std::equal(DefaultShader.begin(), DefaultShader.end(), hash)) {

        return procs->CreatePixelShader(pDevice, SIMPLIFIED_FS_DEFAULT_SHADER, sizeof(SIMPLIFIED_FS_DEFAULT_SHADER), pClassLinkage, ppPixelShader);
    }
#endif
    return procs->CreatePixelShader(pDevice, pShaderBytecode, BytecodeLength, pClassLinkage, ppPixelShader);
}

#define HOOK_PROC(iface, object, table, index, proc) \
  hookProc(object, #iface "::" #proc, &table->proc, &iface ## _ ## proc, index)

template<typename T>
void hookProc(void* pObject, const char* pName, T** ppOrig, T* pHook, uint32_t index) {
  void** vtbl = *reinterpret_cast<void***>(pObject);

  MH_STATUS mh = MH_CreateHook(vtbl[index],
    reinterpret_cast<void*>(pHook),
    reinterpret_cast<void**>(ppOrig));

  if (mh) {
    if (mh != MH_ERROR_ALREADY_CREATED)
#ifndef NDEBUG
      log("Failed to create hook for ", pName, ": ", MH_StatusToString(mh));
#endif
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
  std::lock_guard lock(g_hookMutex);

  if (g_installedHooks & HOOK_DEVICE)
    return;

#ifndef NDEBUG
  log("Hooking device ", pDevice);
#endif

  DeviceProcs* procs = &g_deviceProcs;
  HOOK_PROC(ID3D11Device, pDevice, procs, 12,  CreateVertexShader);
  HOOK_PROC(ID3D11Device, pDevice, procs, 15,  CreatePixelShader);

  g_installedHooks |= HOOK_DEVICE;
}

class ContextWrapper final : public ID3D11DeviceContext {
  LONG refcnt;
  ID3D11DeviceContext* ctx;

public:
  ContextWrapper(ID3D11DeviceContext* ctx_) : refcnt(1), ctx(ctx_) {}

  // IUnknown
  HRESULT QueryInterface(REFIID riid, void** ppvObject) override {
    LPOLESTR iidstr;
    if (StringFromIID(riid, &iidstr) == S_OK) {
      char buf[64] = {};
      WideCharToMultiByte(CP_UTF8, 0, iidstr, -1, buf, sizeof(buf), nullptr, nullptr);
#ifndef NDEBUG
      log("ID3D11DeviceContext QueryInterface ", buf);
#endif
      CoTaskMemFree(iidstr);
    } else {
#ifndef NDEBUG
      log("ID3D11DeviceContext QueryInterface <failed to get iid str>");
#endif
    }
    return ctx->QueryInterface(riid, ppvObject);
  }
  ULONG AddRef() override { return InterlockedAdd(&refcnt, 1); }
  ULONG Release() override {
    ULONG res = InterlockedAdd(&refcnt, -1);
    if (res == 0) {
      ctx->Release();
      delete this;
    }
    return res;
  }

  // ID3D11DeviceChild
  void STDMETHODCALLTYPE GetDevice(ID3D11Device** ppDevice) override { ctx->GetDevice(ppDevice); }
  HRESULT STDMETHODCALLTYPE GetPrivateData(REFGUID guid, UINT* pDataSize, void* pData) override { return ctx->GetPrivateData(guid, pDataSize, pData); }
  HRESULT STDMETHODCALLTYPE SetPrivateData(REFGUID guid, UINT DataSize, const void* pData) override { return ctx->SetPrivateData(guid, DataSize, pData); }
  HRESULT STDMETHODCALLTYPE SetPrivateDataInterface(REFGUID guid, const IUnknown* pData) override { return ctx->SetPrivateDataInterface(guid, pData); }

  // ID3D11DeviceContext
  void VSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers) override { ctx->VSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
  void PSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews) override { ctx->PSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
  void PSSetShader(ID3D11PixelShader* pPixelShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances) override { ctx->PSSetShader(pPixelShader, ppClassInstances, NumClassInstances); }
  void PSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers) override { ctx->PSSetSamplers(StartSlot, NumSamplers, ppSamplers); }
  void VSSetShader(ID3D11VertexShader* pVertexShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances) override { ctx->VSSetShader(pVertexShader, ppClassInstances, NumClassInstances); }
  void DrawIndexed(UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation) override { ctx->DrawIndexed(IndexCount, StartIndexLocation, BaseVertexLocation); }
  void Draw(UINT VertexCount, UINT StartVertexLocation) override { ctx->Draw(VertexCount, StartVertexLocation); }
  HRESULT Map(ID3D11Resource* pResource, UINT Subresource, D3D11_MAP MapType, UINT MapFlags, D3D11_MAPPED_SUBRESOURCE* pMappedResource) override { return ctx->Map(pResource, Subresource, MapType, MapFlags, pMappedResource); }
  void Unmap(ID3D11Resource* pResource, UINT Subresource) override { ctx->Unmap(pResource, Subresource); }
  void PSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers) override { ctx->PSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
  void IASetInputLayout(ID3D11InputLayout* pInputLayout) override { ctx->IASetInputLayout(pInputLayout); }
  void IASetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppVertexBuffers, const UINT* pStrides, const UINT* pOffsets) override { ctx->IASetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets); }
  void IASetIndexBuffer(ID3D11Buffer* pIndexBuffer, DXGI_FORMAT Format, UINT Offset) override { ctx->IASetIndexBuffer(pIndexBuffer, Format, Offset); }
  void DrawIndexedInstanced(UINT IndexCountPerInstance, UINT InstanceCount, UINT StartIndexLocation, INT BaseVertexLocation, UINT StartInstanceLocation) override { ctx->DrawIndexedInstanced(IndexCountPerInstance, InstanceCount, StartIndexLocation, BaseVertexLocation, StartInstanceLocation); }
  void DrawInstanced(UINT VertexCountPerInstance, UINT InstanceCount, UINT StartVertexLocation, UINT StartInstanceLocation) override { ctx->DrawInstanced(VertexCountPerInstance, InstanceCount, StartVertexLocation, StartInstanceLocation); }
  void GSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers) override { ctx->GSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
  void GSSetShader(ID3D11GeometryShader* pShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances) override { ctx->GSSetShader(pShader, ppClassInstances, NumClassInstances); }
  void IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY Topology) override { ctx->IASetPrimitiveTopology(Topology); }
  void VSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews) override { ctx->VSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
  void VSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers) override { ctx->VSSetSamplers(StartSlot, NumSamplers, ppSamplers); }
  void Begin(ID3D11Asynchronous* pAsync) override { ctx->Begin(pAsync); }
  void End(ID3D11Asynchronous* pAsync) override { ctx->End(pAsync); }
  HRESULT GetData(ID3D11Asynchronous* pAsync, void* pData, UINT DataSize, UINT GetDataFlags) override { return ctx->GetData(pAsync, pData, DataSize, GetDataFlags); }
  void SetPredication(ID3D11Predicate* pPredicate, BOOL PredicateValue) override { ctx->SetPredication(pPredicate, PredicateValue); }
  void GSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews) override { ctx->GSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
  void GSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers) override { ctx->GSSetSamplers(StartSlot, NumSamplers, ppSamplers); }
  void OMSetBlendState(ID3D11BlendState* pBlendState, const FLOAT BlendFactor[4], UINT SampleMask) override { ctx->OMSetBlendState(pBlendState, BlendFactor, SampleMask); }
  void OMSetDepthStencilState(ID3D11DepthStencilState* pDepthStencilState, UINT StencilRef) override { ctx->OMSetDepthStencilState(pDepthStencilState, StencilRef); }
  void SOSetTargets(UINT NumBuffers, ID3D11Buffer* const* ppSOTargets, const UINT* pOffsets) override { ctx->SOSetTargets(NumBuffers, ppSOTargets, pOffsets); }
  void DrawAuto() override { ctx->DrawAuto(); }
  void DrawIndexedInstancedIndirect(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs) override { ctx->DrawIndexedInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs); }
  void DrawInstancedIndirect(ID3D11Buffer* pBufferForArgs, UINT AlignedByteOffsetForArgs) override { ctx->DrawInstancedIndirect(pBufferForArgs, AlignedByteOffsetForArgs); }
  void RSSetState(ID3D11RasterizerState* pRasterizerState) override { ctx->RSSetState(pRasterizerState); }
  void RSSetViewports(UINT NumViewports, const D3D11_VIEWPORT* pViewports) override { ctx->RSSetViewports(NumViewports, pViewports); }
  void RSSetScissorRects(UINT NumRects, const D3D11_RECT* pRects) override { ctx->RSSetScissorRects(NumRects, pRects); }
  void CopyStructureCount(ID3D11Buffer* pDstBuffer, UINT DstAlignedByteOffset, ID3D11UnorderedAccessView* pSrcView) override { ctx->CopyStructureCount(pDstBuffer, DstAlignedByteOffset, pSrcView); }
  void ClearDepthStencilView(ID3D11DepthStencilView* pDepthStencilView, UINT ClearFlags, FLOAT Depth, UINT8 Stencil) override { ctx->ClearDepthStencilView(pDepthStencilView, ClearFlags, Depth, Stencil); }
  void GenerateMips(ID3D11ShaderResourceView* pShaderResourceView) override { ctx->GenerateMips(pShaderResourceView); }
  void SetResourceMinLOD(ID3D11Resource* pResource, FLOAT MinLOD) override { ctx->SetResourceMinLOD(pResource, MinLOD); }
  FLOAT GetResourceMinLOD(ID3D11Resource* pResource) override { return ctx->GetResourceMinLOD(pResource); }
  void ResolveSubresource(ID3D11Resource* pDstResource, UINT DstSubresource, ID3D11Resource* pSrcResource, UINT SrcSubresource, DXGI_FORMAT Format) override { ctx->ResolveSubresource(pDstResource, DstSubresource, pSrcResource, SrcSubresource, Format); }
  void ExecuteCommandList(ID3D11CommandList* pCommandList, BOOL RestoreContextState) override { ctx->ExecuteCommandList(pCommandList, RestoreContextState); }
  void HSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews) override { ctx->HSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
  void HSSetShader(ID3D11HullShader* pHullShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances) override { ctx->HSSetShader(pHullShader, ppClassInstances, NumClassInstances); }
  void HSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers) override { ctx->HSSetSamplers(StartSlot, NumSamplers, ppSamplers); }
  void HSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers) override { ctx->HSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
  void DSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews) override { ctx->DSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
  void DSSetShader(ID3D11DomainShader* pDomainShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances) override { ctx->DSSetShader(pDomainShader, ppClassInstances, NumClassInstances); }
  void DSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers) override { ctx->DSSetSamplers(StartSlot, NumSamplers, ppSamplers); }
  void DSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers) override { ctx->DSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
  void CSSetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView* const* ppShaderResourceViews) override { ctx->CSSetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
  void CSSetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView* const* ppUnorderedAccessViews, const UINT* pUAVInitialCounts) override { ctx->CSSetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews, pUAVInitialCounts); }
  void CSSetShader(ID3D11ComputeShader* pComputeShader, ID3D11ClassInstance* const* ppClassInstances, UINT NumClassInstances) override { ctx->CSSetShader(pComputeShader, ppClassInstances, NumClassInstances); }
  void CSSetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState* const* ppSamplers) override { ctx->CSSetSamplers(StartSlot, NumSamplers, ppSamplers); }
  void CSSetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer* const* ppConstantBuffers) override { ctx->CSSetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
  void VSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers) override { ctx->VSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
  void PSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews) override { ctx->PSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
  void PSGetShader(ID3D11PixelShader** ppPixelShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances) override { ctx->PSGetShader(ppPixelShader, ppClassInstances, pNumClassInstances); }
  void PSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers) override { ctx->PSGetSamplers(StartSlot, NumSamplers, ppSamplers); }
  void VSGetShader(ID3D11VertexShader** ppVertexShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances) override { ctx->VSGetShader(ppVertexShader, ppClassInstances, pNumClassInstances); }
  void PSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers) override { ctx->PSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
  void IAGetInputLayout(ID3D11InputLayout** ppInputLayout) override { ctx->IAGetInputLayout(ppInputLayout); }
  void IAGetVertexBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppVertexBuffers, UINT* pStrides, UINT* pOffsets) override { ctx->IAGetVertexBuffers(StartSlot, NumBuffers, ppVertexBuffers, pStrides, pOffsets); }
  void IAGetIndexBuffer(ID3D11Buffer** pIndexBuffer, DXGI_FORMAT* Format, UINT* Offset) override { ctx->IAGetIndexBuffer(pIndexBuffer, Format, Offset); }
  void GSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers) override { ctx->GSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
  void GSGetShader(ID3D11GeometryShader** ppGeometryShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances) override { ctx->GSGetShader(ppGeometryShader, ppClassInstances, pNumClassInstances); }
  void IAGetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY* pTopology) override { ctx->IAGetPrimitiveTopology(pTopology); }
  void VSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews) override { ctx->VSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
  void VSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers) override { ctx->VSGetSamplers(StartSlot, NumSamplers, ppSamplers); }
  void GetPredication(ID3D11Predicate** ppPredicate, BOOL* pPredicateValue) override { ctx->GetPredication(ppPredicate, pPredicateValue); }
  void GSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews) override { ctx->GSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
  void GSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers) override { ctx->GSGetSamplers(StartSlot, NumSamplers, ppSamplers); }
  void OMGetRenderTargets(UINT NumViews, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView) override { ctx->OMGetRenderTargets(NumViews, ppRenderTargetViews, ppDepthStencilView); }
  void OMGetRenderTargetsAndUnorderedAccessViews(UINT NumRTVs, ID3D11RenderTargetView** ppRenderTargetViews, ID3D11DepthStencilView** ppDepthStencilView, UINT UAVStartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews) override { ctx->OMGetRenderTargetsAndUnorderedAccessViews(NumRTVs, ppRenderTargetViews, ppDepthStencilView, UAVStartSlot, NumUAVs, ppUnorderedAccessViews); }
  void OMGetBlendState(ID3D11BlendState** ppBlendState, FLOAT BlendFactor[4], UINT* pSampleMask) override { ctx->OMGetBlendState(ppBlendState, BlendFactor, pSampleMask); }
  void OMGetDepthStencilState(ID3D11DepthStencilState** ppDepthStencilState, UINT* pStencilRef) override { ctx->OMGetDepthStencilState(ppDepthStencilState, pStencilRef); }
  void SOGetTargets(UINT NumBuffers, ID3D11Buffer** ppSOTargets) override { ctx->SOGetTargets(NumBuffers, ppSOTargets); }
  void RSGetState(ID3D11RasterizerState** ppRasterizerState) override { ctx->RSGetState(ppRasterizerState); }
  void RSGetViewports(UINT* pNumViewports, D3D11_VIEWPORT* pViewports) override { ctx->RSGetViewports(pNumViewports, pViewports); }
  void RSGetScissorRects(UINT* pNumRects, D3D11_RECT* pRects) override { ctx->RSGetScissorRects(pNumRects, pRects); }
  void HSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews) override { ctx->HSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
  void HSGetShader(ID3D11HullShader** ppHullShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances) override { ctx->HSGetShader(ppHullShader, ppClassInstances, pNumClassInstances); }
  void HSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers) override { ctx->HSGetSamplers(StartSlot, NumSamplers, ppSamplers); }
  void HSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers) override { ctx->HSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
  void DSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews) override { ctx->DSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
  void DSGetShader(ID3D11DomainShader** ppDomainShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances) override { ctx->DSGetShader(ppDomainShader, ppClassInstances, pNumClassInstances); }
  void DSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers) override { ctx->DSGetSamplers(StartSlot, NumSamplers, ppSamplers); }
  void DSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers) override { ctx->DSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
  void CSGetShaderResources(UINT StartSlot, UINT NumViews, ID3D11ShaderResourceView** ppShaderResourceViews) override { ctx->CSGetShaderResources(StartSlot, NumViews, ppShaderResourceViews); }
  void CSGetUnorderedAccessViews(UINT StartSlot, UINT NumUAVs, ID3D11UnorderedAccessView** ppUnorderedAccessViews) override { ctx->CSGetUnorderedAccessViews(StartSlot, NumUAVs, ppUnorderedAccessViews); }
  void CSGetShader(ID3D11ComputeShader** ppComputeShader, ID3D11ClassInstance** ppClassInstances, UINT* pNumClassInstances) override { ctx->CSGetShader(ppComputeShader, ppClassInstances, pNumClassInstances); }
  void CSGetSamplers(UINT StartSlot, UINT NumSamplers, ID3D11SamplerState** ppSamplers) override { ctx->CSGetSamplers(StartSlot, NumSamplers, ppSamplers); }
  void CSGetConstantBuffers(UINT StartSlot, UINT NumBuffers, ID3D11Buffer** ppConstantBuffers) override { ctx->CSGetConstantBuffers(StartSlot, NumBuffers, ppConstantBuffers); }
  void ClearState() override { ctx->ClearState(); }
  void Flush() override { ctx->Flush(); }
  D3D11_DEVICE_CONTEXT_TYPE GetType() override { return ctx->GetType(); }
  UINT GetContextFlags() override { return ctx->GetContextFlags(); }
  HRESULT FinishCommandList(BOOL RestoreDeferredContextState, ID3D11CommandList** ppCommandList) override { return ctx->FinishCommandList(RestoreDeferredContextState, ppCommandList); }
  void ClearRenderTargetView(ID3D11RenderTargetView* pRTV, const FLOAT pColor[4]) override { ctx->ClearRenderTargetView(pRTV, pColor); }
  void ClearUnorderedAccessViewFloat(ID3D11UnorderedAccessView* pUAV, const FLOAT pColor[4]) override { ctx->ClearUnorderedAccessViewFloat(pUAV, pColor); }
  void ClearUnorderedAccessViewUint(ID3D11UnorderedAccessView* pUAV, const UINT pColor[4]) override { ctx->ClearUnorderedAccessViewUint(pUAV, pColor); }
  void CopyResource(ID3D11Resource* pDstResource, ID3D11Resource* pSrcResource) override { ctx->CopyResource(pDstResource, pSrcResource); }
  void CopySubresourceRegion(ID3D11Resource* pDstResource, UINT DstSubresource, UINT DstX, UINT DstY, UINT DstZ, ID3D11Resource* pSrcResource, UINT SrcSubresource, const D3D11_BOX* pSrcBox) override { ctx->CopySubresourceRegion(pDstResource, DstSubresource, DstX, DstY, DstZ, pSrcResource, SrcSubresource, pSrcBox); }
  void Dispatch(UINT X, UINT Y, UINT Z) override { ctx->Dispatch(X, Y, Z); }
  void DispatchIndirect(ID3D11Buffer* pParameterBuffer, UINT pParameterOffset) override { ctx->DispatchIndirect(pParameterBuffer, pParameterOffset); }
  void OMSetRenderTargets(UINT RTVCount, ID3D11RenderTargetView* const* ppRTVs, ID3D11DepthStencilView* pDSV) override { ctx->OMSetRenderTargets(RTVCount, ppRTVs, pDSV); }
  void OMSetRenderTargetsAndUnorderedAccessViews(UINT RTVCount, ID3D11RenderTargetView* const* ppRTVs, ID3D11DepthStencilView* pDSV, UINT UAVIndex, UINT UAVCount, ID3D11UnorderedAccessView* const* ppUAVs, const UINT* pUAVClearValues) override { ctx->OMSetRenderTargetsAndUnorderedAccessViews( RTVCount, ppRTVs, pDSV, UAVIndex, UAVCount, ppUAVs, pUAVClearValues); }
  void UpdateSubresource(ID3D11Resource* pResource, UINT Subresource, const D3D11_BOX* pBox, const void* pData, UINT RowPitch, UINT SlicePitch) override { ctx->UpdateSubresource( pResource, Subresource, pBox, pData, RowPitch, SlicePitch); }
};

ID3D11DeviceContext* hookContext(ID3D11DeviceContext* pContext) {
  return new ContextWrapper(pContext);
}

}