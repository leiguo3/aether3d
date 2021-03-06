#include "GfxDevice.hpp"
#define WIN32_LEAN_AND_MEAN
#define VC_EXTRALEAN
#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>
#include <d3dx12.h>
#include <pix.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <sstream>
#include <cmath>
#include "ComputeShader.hpp"
#include "DescriptorHeapManager.hpp"
#include "Macros.hpp"
#include "LightTiler.hpp"
#include "RenderTexture.hpp"
#include "System.hpp"
#include "Shader.hpp"
#include "Statistics.hpp"
#include "TextureBase.hpp"
#include "RenderTexture.hpp"
#include "Texture2D.hpp"
#include "TextureCube.hpp"
#include "VertexBuffer.hpp"

void DestroyShaders(); // Defined in ShaderD3D12.cpp
void DestroyComputeShaders(); // Defined in ComputeShaderD3D12.cpp
float GetFloatAnisotropy( ae3d::Anisotropy anisotropy );

namespace WindowGlobal
{
    extern HWND hwnd;
    extern int windowWidth;
    extern int windowHeight;
}

namespace MathUtil
{
    unsigned GetHash( const char* s, unsigned length );
}

namespace ae3d
{
    namespace System
    {
        namespace Statistics
        {
            std::string GetStatistics()
            {
                std::stringstream stm;
                stm << "frame time: " << ::Statistics::GetFrameTimeMS() << "ms\n";
                stm << "shadow pass time: " << ::Statistics::GetShadowMapTimeMS() << "ms\n";
                stm << "depth pass time: " << ::Statistics::GetDepthNormalsTimeMS() << "ms\n";
                stm << "draw calls: " << ::Statistics::GetDrawCalls() << "\n";
                stm << "barrier calls: " << ::Statistics::GetBarrierCalls() << "\n";
                stm << "triangles: " << ::Statistics::GetTriangleCount() << "\n";
                stm << "create constant buffer calls: " << ::Statistics::GetCreateConstantBufferCalls() << "\n";

                return stm.str();
	    }
        }
    }
}

enum SamplerIndexByAnisotropy : int
{
    One = 0,
    Two,
    Four,
    Eight
};

int GetSamplerIndexByAnisotropy( ae3d::Anisotropy anisotropy )
{
    const int intAnisotropy = static_cast< int >( GetFloatAnisotropy( anisotropy ) );

    if (intAnisotropy == 1)
    {
        return SamplerIndexByAnisotropy::One;
    }
    if (intAnisotropy == 2)
    {
        return SamplerIndexByAnisotropy::Two;
    }
    if (intAnisotropy == 4)
    {
        return SamplerIndexByAnisotropy::Four;
    }
    if (intAnisotropy == 8)
    {
        return SamplerIndexByAnisotropy::Eight;
    }

    return SamplerIndexByAnisotropy::One;
}

namespace GfxDeviceGlobal
{
    // Indexed by SamplerIndexByAnisotropy
    struct Samplers
    {
        D3D12_CPU_DESCRIPTOR_HANDLE linearRepeatCPU = {};
        D3D12_CPU_DESCRIPTOR_HANDLE linearClampCPU = {};
        D3D12_CPU_DESCRIPTOR_HANDLE pointRepeatCPU = {};
        D3D12_CPU_DESCRIPTOR_HANDLE pointClampCPU = {};

        D3D12_GPU_DESCRIPTOR_HANDLE linearRepeatGPU = {};
        D3D12_GPU_DESCRIPTOR_HANDLE linearClampGPU = {};
        D3D12_GPU_DESCRIPTOR_HANDLE pointRepeatGPU = {};
        D3D12_GPU_DESCRIPTOR_HANDLE pointClampGPU = {};
    } samplers[ 4 ];
 
    const unsigned BufferCount = 2;
    int backBufferWidth = 640;
    int backBufferHeight = 400;

    ID3D12Device* device = nullptr;
    IDXGISwapChain3* swapChain = nullptr;
    IDXGIAdapter3* adapter = nullptr;

    // Not backbuffer.
    ae3d::RenderTexture* currentRenderTarget = nullptr;
    unsigned currentRenderTargetCubeMapFace = 0;

    ID3D12Resource* renderTargets[ 2 ] = { nullptr, nullptr };
    GpuResource rtvResources[ 2 ];

    ID3D12Resource* depthTexture = nullptr;
    ID3D12CommandAllocator* commandListAllocator = nullptr;
    ID3D12RootSignature* rootSignatureGraphics = nullptr;
    ID3D12RootSignature* rootSignatureTileCuller = nullptr;
    ID3D12PipelineState* lightTilerPSO = nullptr;
    ID3D12InfoQueue* infoQueue = nullptr;
    float clearColor[ 4 ] = { 0, 0, 0, 1 };
    std::unordered_map< unsigned, ID3D12PipelineState* > psoCache;
    ae3d::TextureBase* texture0 = nullptr;
    ae3d::TextureBase* texture1 = nullptr;
    std::vector< ID3D12DescriptorHeap* > frameHeaps;
    std::vector< ID3D12Resource* > constantBuffers;
    std::vector< ID3D12Resource* > mappedConstantBuffers;
    int currentConstantBufferIndex = 0;
    ae3d::LightTiler lightTiler;

    ID3D12GraphicsCommandList* graphicsCommandList = nullptr;
    ID3D12CommandQueue* commandQueue = nullptr;
    ID3D12Fence* fence = nullptr;
    UINT64 fenceValue = 1;
    HANDLE fenceEvent;
    int sampleCount = 1;
    ID3D12Resource* msaaColor = nullptr;
    ID3D12Resource* msaaDepth = nullptr;
    D3D12_CPU_DESCRIPTOR_HANDLE msaaColorHandle = {};
    D3D12_CPU_DESCRIPTOR_HANDLE msaaDepthHandle = {};
}

namespace Global
{
    extern std::vector< ID3D12Resource* > frameVBUploads; // Defined in VertexBufferD3D12.cpp
}

namespace ae3d
{
    void CreateRenderer( int samples );
}

void WaitForPreviousFrame()
{
    const UINT64 fenceValue = GfxDeviceGlobal::fenceValue;
    HRESULT hr = GfxDeviceGlobal::commandQueue->Signal( GfxDeviceGlobal::fence, fenceValue );
    AE3D_CHECK_D3D( hr, "command queue signal" );
    ++GfxDeviceGlobal::fenceValue;
    Statistics::IncFenceCalls();

    if (GfxDeviceGlobal::fence->GetCompletedValue() < fenceValue)
    {
        hr = GfxDeviceGlobal::fence->SetEventOnCompletion( fenceValue, GfxDeviceGlobal::fenceEvent );
        AE3D_CHECK_D3D( hr, "fence event" );
        WaitForSingleObject( GfxDeviceGlobal::fenceEvent, INFINITE );
    }
}

void TransitionResource( GpuResource& gpuResource, D3D12_RESOURCE_STATES newState )
{
    D3D12_RESOURCE_STATES oldState = gpuResource.usageState;

    if (oldState == newState)
    {
        return;
    }

    D3D12_RESOURCE_BARRIER BarrierDesc = {};

    BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
    BarrierDesc.Transition.pResource = gpuResource.resource;
    BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
    BarrierDesc.Transition.StateBefore = oldState;
    BarrierDesc.Transition.StateAfter = newState;

    // Check to see if we already started the transition
    if (newState == gpuResource.transitioningState)
    {
        BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_END_ONLY;
        gpuResource.transitioningState = (D3D12_RESOURCE_STATES)-1;
    }
    else
    {
        BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
    }

    gpuResource.usageState = newState;

    Statistics::IncBarrierCalls();
    GfxDeviceGlobal::graphicsCommandList->ResourceBarrier( 1, &BarrierDesc );
}

void CreateBackBuffer()
{
    for (int i = 0; i < GfxDeviceGlobal::BufferCount; ++i)
    {
        const HRESULT hr = GfxDeviceGlobal::swapChain->GetBuffer( i, IID_PPV_ARGS( &GfxDeviceGlobal::renderTargets[ i ] ) );
        AE3D_CHECK_D3D( hr, "Failed to create RTV" );

        GfxDeviceGlobal::renderTargets[ i ]->SetName( L"SwapChain_Buffer" );
        GfxDeviceGlobal::rtvResources[ i ].usageState = D3D12_RESOURCE_STATE_COMMON;
        GfxDeviceGlobal::rtvResources[ i ].resource = GfxDeviceGlobal::renderTargets[ i ];
        GfxDeviceGlobal::backBufferWidth = int( GfxDeviceGlobal::renderTargets[ i ]->GetDesc().Width );
        GfxDeviceGlobal::backBufferHeight = int( GfxDeviceGlobal::renderTargets[ i ]->GetDesc().Height );
    }

    for (auto i = 0u; i < GfxDeviceGlobal::BufferCount; ++i)
    {
        D3D12_CPU_DESCRIPTOR_HANDLE handle = DescriptorHeapManager::AllocateDescriptor( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );

        D3D12_RENDER_TARGET_VIEW_DESC descRtv = {};
        descRtv.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
        descRtv.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
        GfxDeviceGlobal::device->CreateRenderTargetView( GfxDeviceGlobal::renderTargets[ i ], &descRtv, handle );
    }
}

void CreateMSAA()
{
    if (GfxDeviceGlobal::sampleCount == 1)
    {
        return;
    }

    // MSAA color

    D3D12_CLEAR_VALUE clearValue = {};
    clearValue.Format = DXGI_FORMAT_R8G8B8A8_UNORM;

    D3D12_HEAP_PROPERTIES heapProp = {};
    heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

    D3D12_RESOURCE_DESC rtDesc = {};
    rtDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rtDesc.Width = GfxDeviceGlobal::backBufferWidth;
    rtDesc.Height = GfxDeviceGlobal::backBufferHeight;
    rtDesc.DepthOrArraySize = 1;
    rtDesc.MipLevels = 1;
    rtDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    rtDesc.SampleDesc.Count = GfxDeviceGlobal::sampleCount;
    rtDesc.SampleDesc.Quality = DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN;
    rtDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

    HRESULT hr = GfxDeviceGlobal::device->CreateCommittedResource( &heapProp, D3D12_HEAP_FLAG_NONE, &rtDesc, D3D12_RESOURCE_STATE_RENDER_TARGET, &clearValue, IID_PPV_ARGS( &GfxDeviceGlobal::msaaColor ) );
    AE3D_CHECK_D3D( hr, "Failed to create MSAA color" );
    GfxDeviceGlobal::msaaColor->SetName( L"MSAA color" );

    GfxDeviceGlobal::msaaColorHandle = DescriptorHeapManager::AllocateDescriptor( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );
    GfxDeviceGlobal::device->CreateRenderTargetView( GfxDeviceGlobal::msaaColor, nullptr, GfxDeviceGlobal::msaaColorHandle );

    // MSAA depth

    clearValue.Format = DXGI_FORMAT_D32_FLOAT;
    clearValue.DepthStencil.Depth = 1;

    heapProp.Type = D3D12_HEAP_TYPE_DEFAULT;

    rtDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    rtDesc.Width = GfxDeviceGlobal::backBufferWidth;
    rtDesc.Height = GfxDeviceGlobal::backBufferHeight;
    rtDesc.DepthOrArraySize = 1;
    rtDesc.MipLevels = 1;
    rtDesc.Format = DXGI_FORMAT_D32_FLOAT;
    rtDesc.SampleDesc.Count = GfxDeviceGlobal::sampleCount;
    rtDesc.SampleDesc.Quality = DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN;
    rtDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    hr = GfxDeviceGlobal::device->CreateCommittedResource( &heapProp, D3D12_HEAP_FLAG_NONE, &rtDesc, D3D12_RESOURCE_STATE_DEPTH_WRITE, &clearValue, IID_PPV_ARGS( &GfxDeviceGlobal::msaaDepth ) );
    AE3D_CHECK_D3D( hr, "Failed to create MSAA depth" );
    GfxDeviceGlobal::msaaDepth->SetName( L"MSAA depth" );

    GfxDeviceGlobal::msaaDepthHandle = DescriptorHeapManager::AllocateDescriptor( D3D12_DESCRIPTOR_HEAP_TYPE_DSV );
    GfxDeviceGlobal::device->CreateDepthStencilView( GfxDeviceGlobal::msaaDepth, nullptr, GfxDeviceGlobal::msaaDepthHandle );
}

void CreateSamplers()
{
    for (int samplerIndex = 0; samplerIndex < 4; ++samplerIndex)
    {
        D3D12_SAMPLER_DESC descSampler = {};
        descSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        descSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        descSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        descSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        descSampler.MinLOD = 0;
        descSampler.MaxLOD = FLT_MAX;
        descSampler.MipLODBias = 0;
        descSampler.MaxAnisotropy = UINT( std::pow( 2, samplerIndex ) );
        descSampler.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;
        
        D3D12_CPU_DESCRIPTOR_HANDLE handleCPU = DescriptorHeapManager::GetSamplerHeap()->GetCPUDescriptorHandleForHeapStart();
        D3D12_GPU_DESCRIPTOR_HANDLE handleGPU = DescriptorHeapManager::GetSamplerHeap()->GetGPUDescriptorHandleForHeapStart();

        GfxDeviceGlobal::samplers[ samplerIndex ].linearRepeatCPU = handleCPU;
        GfxDeviceGlobal::samplers[ samplerIndex ].linearRepeatGPU = handleGPU;
        GfxDeviceGlobal::device->CreateSampler( &descSampler, GfxDeviceGlobal::samplers[ samplerIndex ].linearRepeatCPU );
        handleCPU.ptr += GfxDeviceGlobal::device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER );
        handleGPU.ptr += GfxDeviceGlobal::device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER );

        descSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;
        descSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        descSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        descSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;

        GfxDeviceGlobal::samplers[ samplerIndex ].linearClampCPU = handleCPU;
        GfxDeviceGlobal::samplers[ samplerIndex ].linearClampGPU = handleGPU;
        GfxDeviceGlobal::device->CreateSampler( &descSampler, GfxDeviceGlobal::samplers[ samplerIndex ].linearClampCPU );
        handleCPU.ptr += GfxDeviceGlobal::device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER );
        handleGPU.ptr += GfxDeviceGlobal::device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER );

        descSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        descSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        descSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        descSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;
        GfxDeviceGlobal::samplers[ samplerIndex ].pointRepeatCPU = handleCPU;
        GfxDeviceGlobal::samplers[ samplerIndex ].pointRepeatGPU = handleGPU;
        GfxDeviceGlobal::device->CreateSampler( &descSampler, GfxDeviceGlobal::samplers[ samplerIndex ].pointRepeatCPU );
        handleCPU.ptr += GfxDeviceGlobal::device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER );
        handleGPU.ptr += GfxDeviceGlobal::device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER );

        descSampler.Filter = D3D12_FILTER_MIN_MAG_MIP_POINT;
        descSampler.AddressU = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        descSampler.AddressV = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        descSampler.AddressW = D3D12_TEXTURE_ADDRESS_MODE_CLAMP;
        GfxDeviceGlobal::samplers[ samplerIndex ].pointClampCPU = handleCPU;
        GfxDeviceGlobal::samplers[ samplerIndex ].pointClampGPU = handleGPU;
        GfxDeviceGlobal::device->CreateSampler( &descSampler, GfxDeviceGlobal::samplers[ samplerIndex ].pointClampCPU );
    }
}

void CreateRootSignature()
{
    // Graphics
    {
        CD3DX12_DESCRIPTOR_RANGE descRange1[ 2 ];
        descRange1[ 0 ].Init( D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0 );
        descRange1[ 1 ].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0 );

        CD3DX12_DESCRIPTOR_RANGE descRange2[ 1 ];
        descRange2[ 0 ].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SAMPLER, 2, 0 );

        CD3DX12_ROOT_PARAMETER rootParam[ 2 ];
        rootParam[ 0 ].InitAsDescriptorTable( 2, descRange1 );
        rootParam[ 1 ].InitAsDescriptorTable( 1, descRange2, D3D12_SHADER_VISIBILITY_PIXEL );

        ID3DBlob* pOutBlob = nullptr;
        ID3DBlob* pErrorBlob = nullptr;
        D3D12_ROOT_SIGNATURE_DESC descRootSignature;
        descRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        descRootSignature.NumParameters = 2;
        descRootSignature.NumStaticSamplers = 0;
        descRootSignature.pParameters = rootParam;
        descRootSignature.pStaticSamplers = nullptr;

        HRESULT hr = D3D12SerializeRootSignature( &descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &pOutBlob, &pErrorBlob );
        AE3D_CHECK_D3D( hr, "Failed to serialize root signature" );

        hr = GfxDeviceGlobal::device->CreateRootSignature( 0, pOutBlob->GetBufferPointer(), pOutBlob->GetBufferSize(), IID_PPV_ARGS( &GfxDeviceGlobal::rootSignatureGraphics ) );
        AE3D_CHECK_D3D( hr, "Failed to create root signature" );
        GfxDeviceGlobal::rootSignatureGraphics->SetName( L"Graphics Root Signature" );
    }

    // Tile Culler
    {
        CD3DX12_DESCRIPTOR_RANGE descRange1[ 3 ];
        descRange1[ 0 ].Init( D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0 );
        descRange1[ 1 ].Init( D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 2, 0 );
        descRange1[ 2 ].Init( D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0 );

        CD3DX12_ROOT_PARAMETER rootParam[ 1 ];
        rootParam[ 0 ].InitAsDescriptorTable( 3, descRange1 );

        ID3DBlob* pOutBlob = nullptr;
        ID3DBlob* pErrorBlob = nullptr;
        D3D12_ROOT_SIGNATURE_DESC descRootSignature;
        descRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
        descRootSignature.NumParameters = 1;
        descRootSignature.NumStaticSamplers = 0;
        descRootSignature.pParameters = rootParam;
        descRootSignature.pStaticSamplers = nullptr;

        HRESULT hr = D3D12SerializeRootSignature( &descRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &pOutBlob, &pErrorBlob );
        AE3D_CHECK_D3D( hr, "Failed to serialize root signature" );

        hr = GfxDeviceGlobal::device->CreateRootSignature( 0, pOutBlob->GetBufferPointer(), pOutBlob->GetBufferSize(), IID_PPV_ARGS( &GfxDeviceGlobal::rootSignatureTileCuller ) );
        AE3D_CHECK_D3D( hr, "Failed to create root signature" );
        GfxDeviceGlobal::rootSignatureTileCuller->SetName( L"Tile Culler Root Signature" );
    }
}

unsigned GetPSOHash( ae3d::VertexBuffer::VertexFormat vertexFormat, ae3d::Shader& shader, ae3d::GfxDevice::BlendMode blendMode,
                     ae3d::GfxDevice::DepthFunc depthFunc, ae3d::GfxDevice::CullMode cullMode, ae3d::GfxDevice::FillMode fillMode, DXGI_FORMAT rtvFormat, int sampleCount )
{
    std::string hashString;
    hashString += std::to_string( (unsigned)vertexFormat );
    hashString += std::to_string( (ptrdiff_t)&shader.blobShaderVertex );
    hashString += std::to_string( (ptrdiff_t)&shader.blobShaderPixel );
    hashString += std::to_string( (unsigned)blendMode );
    hashString += std::to_string( ((unsigned)depthFunc) );
    hashString += std::to_string( ((unsigned)cullMode) );
    hashString += std::to_string( ((unsigned)fillMode) );
    hashString += std::to_string( ((unsigned)rtvFormat) );
    hashString += std::to_string( ((unsigned)sampleCount) );

    return MathUtil::GetHash( hashString.c_str(), static_cast< unsigned >( hashString.length() ) );
}

void CreateComputePSO( ae3d::ComputeShader& shader )
{
    D3D12_COMPUTE_PIPELINE_STATE_DESC descPso = {};
    descPso.CS = { reinterpret_cast<BYTE*>( shader.blobShader->GetBufferPointer() ), shader.blobShader->GetBufferSize() };
    descPso.pRootSignature = GfxDeviceGlobal::rootSignatureTileCuller;

    HRESULT hr = GfxDeviceGlobal::device->CreateComputePipelineState( &descPso, IID_PPV_ARGS( &GfxDeviceGlobal::lightTilerPSO ) );
    AE3D_CHECK_D3D( hr, "Failed to create compute PSO" );
    GfxDeviceGlobal::lightTilerPSO->SetName( L"PSO Tile Culler" );
}

void CreatePSO( ae3d::VertexBuffer::VertexFormat vertexFormat, ae3d::Shader& shader, ae3d::GfxDevice::BlendMode blendMode, ae3d::GfxDevice::DepthFunc depthFunc,
                ae3d::GfxDevice::CullMode cullMode, ae3d::GfxDevice::FillMode fillMode, DXGI_FORMAT rtvFormat, int sampleCount )
{
    D3D12_RASTERIZER_DESC descRaster = {};

    if (cullMode == ae3d::GfxDevice::CullMode::Off)
    {
        descRaster.CullMode = D3D12_CULL_MODE_NONE;
    }
    else if (cullMode == ae3d::GfxDevice::CullMode::Front)
    {
        descRaster.CullMode = D3D12_CULL_MODE_FRONT;
    }
    else if (cullMode == ae3d::GfxDevice::CullMode::Back)
    {
        descRaster.CullMode = D3D12_CULL_MODE_BACK;
    }
    else
    {
        ae3d::System::Assert( false, "unhandled cull mode" );
    }

    descRaster.DepthBias = 0;
    descRaster.DepthBiasClamp = 0;
    descRaster.DepthClipEnable = TRUE;
    descRaster.FillMode = fillMode == ae3d::GfxDevice::FillMode::Solid ? D3D12_FILL_MODE_SOLID : D3D12_FILL_MODE_WIREFRAME;
    descRaster.FrontCounterClockwise = TRUE;
    descRaster.MultisampleEnable = sampleCount > 1;
    descRaster.SlopeScaledDepthBias = 0;

    D3D12_BLEND_DESC descBlend = {};
    descBlend.AlphaToCoverageEnable = FALSE;
    descBlend.IndependentBlendEnable = FALSE;

    const D3D12_RENDER_TARGET_BLEND_DESC blendOff =
    {
        FALSE, FALSE,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_BLEND_ONE, D3D12_BLEND_ZERO, D3D12_BLEND_OP_ADD,
        D3D12_LOGIC_OP_NOOP,
        D3D12_COLOR_WRITE_ENABLE_ALL,
    };

    D3D12_RENDER_TARGET_BLEND_DESC blendAlpha;
    ZeroMemory( &blendAlpha, sizeof( D3D12_RENDER_TARGET_BLEND_DESC ) );
    blendAlpha.BlendEnable = TRUE;
    blendAlpha.SrcBlend = D3D12_BLEND_SRC_ALPHA;
    blendAlpha.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
    blendAlpha.SrcBlendAlpha = D3D12_BLEND_ONE;
    blendAlpha.DestBlendAlpha = D3D12_BLEND_ZERO;
    blendAlpha.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendAlpha.BlendOp = D3D12_BLEND_OP_ADD;
    blendAlpha.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    D3D12_RENDER_TARGET_BLEND_DESC blendAdd;
    ZeroMemory( &blendAdd, sizeof( D3D12_RENDER_TARGET_BLEND_DESC ) );
    blendAdd.BlendEnable = TRUE;
    blendAdd.SrcBlend = D3D12_BLEND_ONE;
    blendAdd.DestBlend = D3D12_BLEND_ONE;
    blendAdd.SrcBlendAlpha = D3D12_BLEND_ONE;
    blendAdd.DestBlendAlpha = D3D12_BLEND_ONE;
    blendAdd.BlendOpAlpha = D3D12_BLEND_OP_ADD;
    blendAdd.BlendOp = D3D12_BLEND_OP_ADD;
    blendAdd.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

    if (blendMode == ae3d::GfxDevice::BlendMode::Off)
    {
        descBlend.RenderTarget[ 0 ] = blendOff;
    }
    else if (blendMode == ae3d::GfxDevice::BlendMode::AlphaBlend)
    {
        descBlend.RenderTarget[ 0 ] = blendAlpha;
    }
    else if (blendMode == ae3d::GfxDevice::BlendMode::Additive)
    {
        descBlend.RenderTarget[ 0 ] = blendAdd;
    }
    else
    {
        ae3d::System::Assert( false, "unhandled blend mode" );
    }

    D3D12_INPUT_ELEMENT_DESC layoutPTC[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_INPUT_ELEMENT_DESC layoutPTN[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    D3D12_INPUT_ELEMENT_DESC layoutPTNTC[] =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 20, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "TANGENT", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        { "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 48, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };

    UINT numElements = 0;
    D3D12_INPUT_ELEMENT_DESC* layout = nullptr;
    if (vertexFormat == ae3d::VertexBuffer::VertexFormat::PTC)
    {
        layout = layoutPTC;
        numElements = 3;
    }
    else if (vertexFormat == ae3d::VertexBuffer::VertexFormat::PTN)
    {
        layout = layoutPTN;
        numElements = 3;
    }
    else if (vertexFormat == ae3d::VertexBuffer::VertexFormat::PTNTC)
    {
        layout = layoutPTNTC;
        numElements = 5;
    }
    else
    {
        ae3d::System::Assert( false, "unhandled vertex format" );
        layout = layoutPTC;
        numElements = 3;
    }

    D3D12_GRAPHICS_PIPELINE_STATE_DESC descPso = {};
    descPso.InputLayout = { layout, numElements };
    descPso.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;
    descPso.pRootSignature = GfxDeviceGlobal::rootSignatureGraphics;
    descPso.VS = { reinterpret_cast<BYTE*>(shader.blobShaderVertex->GetBufferPointer()), shader.blobShaderVertex->GetBufferSize() };
    descPso.PS = { reinterpret_cast<BYTE*>(shader.blobShaderPixel->GetBufferPointer()), shader.blobShaderPixel->GetBufferSize() };
    descPso.RasterizerState = descRaster;
    descPso.BlendState = descBlend;
    descPso.DepthStencilState.StencilEnable = FALSE;
    descPso.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL;
    
    if (depthFunc == ae3d::GfxDevice::DepthFunc::LessOrEqualWriteOff)
    {
        descPso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        descPso.DepthStencilState.DepthEnable = TRUE;
    }
    else if (depthFunc == ae3d::GfxDevice::DepthFunc::LessOrEqualWriteOn)
    {
        descPso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
        descPso.DepthStencilState.DepthEnable = TRUE;
    }
    else if (depthFunc == ae3d::GfxDevice::DepthFunc::NoneWriteOff)
    {
        descPso.DepthStencilState.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ZERO;
        descPso.DepthStencilState.DepthEnable = FALSE;
    }
    else
    {
        ae3d::System::Assert( false, "unhandled depth mode" );
    }

    descPso.SampleMask = UINT_MAX;
    descPso.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    descPso.NumRenderTargets = 1;
    descPso.RTVFormats[ 0 ] = rtvFormat;
    descPso.DSVFormat = DXGI_FORMAT_D32_FLOAT;
    descPso.SampleDesc.Count = sampleCount;
    descPso.SampleDesc.Quality = sampleCount > 1 ? DXGI_STANDARD_MULTISAMPLE_QUALITY_PATTERN : 0;

    ID3D12PipelineState* pso;
    HRESULT hr = GfxDeviceGlobal::device->CreateGraphicsPipelineState( &descPso, IID_PPV_ARGS( &pso ) );
    AE3D_CHECK_D3D( hr, "Failed to create PSO" );
    pso->SetName( L"PSO Graphics" );

    const unsigned hash = GetPSOHash( vertexFormat, shader, blendMode, depthFunc, cullMode, fillMode, rtvFormat, sampleCount );
    GfxDeviceGlobal::psoCache[ hash ] = pso;
}

D3D12_GPU_DESCRIPTOR_HANDLE GetSampler( ae3d::Mipmaps /*mipmaps*/, ae3d::TextureWrap wrap, ae3d::TextureFilter filter, ae3d::Anisotropy anisotropy )
{
    int samplerIndex = GetSamplerIndexByAnisotropy( anisotropy );

    if (wrap == ae3d::TextureWrap::Clamp && filter == ae3d::TextureFilter::Linear)
    {
        return GfxDeviceGlobal::samplers[ samplerIndex ].linearClampGPU;
    }
    if (wrap == ae3d::TextureWrap::Clamp && filter == ae3d::TextureFilter::Nearest)
    {
        return GfxDeviceGlobal::samplers[ samplerIndex ].pointClampGPU;
    }
    if (wrap == ae3d::TextureWrap::Repeat && filter == ae3d::TextureFilter::Linear)
    {
        return GfxDeviceGlobal::samplers[ samplerIndex ].linearRepeatGPU;
    }
    if (wrap == ae3d::TextureWrap::Repeat && filter == ae3d::TextureFilter::Nearest)
    {
        return GfxDeviceGlobal::samplers[ samplerIndex ].pointRepeatGPU;
    }

    return GfxDeviceGlobal::samplers[ samplerIndex ].linearClampGPU;
}

void CreateDepthStencil()
{
    auto descResource = CD3DX12_RESOURCE_DESC::Tex2D(
        DXGI_FORMAT_R32_TYPELESS, GfxDeviceGlobal::backBufferWidth, GfxDeviceGlobal::backBufferHeight, 1, 1, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL,
        D3D12_TEXTURE_LAYOUT_UNKNOWN, 0 );

    auto prop = CD3DX12_HEAP_PROPERTIES( D3D12_HEAP_TYPE_DEFAULT );

    D3D12_CLEAR_VALUE dsvClearValue;
    dsvClearValue.Format = DXGI_FORMAT_D32_FLOAT;
    dsvClearValue.DepthStencil.Depth = 1.0f;
    dsvClearValue.DepthStencil.Stencil = 0;
    HRESULT hr = GfxDeviceGlobal::device->CreateCommittedResource(
        &prop, // No need to read/write by CPU
        D3D12_HEAP_FLAG_NONE,
        &descResource,
        D3D12_RESOURCE_STATE_DEPTH_WRITE,
        &dsvClearValue,
        IID_PPV_ARGS( &GfxDeviceGlobal::depthTexture ) );
    AE3D_CHECK_D3D( hr, "Failed to create depth texture" );

    GfxDeviceGlobal::depthTexture->SetName( L"DepthTexture" );

    D3D12_DEPTH_STENCIL_VIEW_DESC descDsv = {};
    descDsv.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
    descDsv.Format = DXGI_FORMAT_D32_FLOAT;
    descDsv.Texture2D.MipSlice = 0;
    descDsv.Flags = D3D12_DSV_FLAG_NONE;
    GfxDeviceGlobal::device->CreateDepthStencilView( GfxDeviceGlobal::depthTexture, &descDsv, DescriptorHeapManager::GetDSVHeap()->GetCPUDescriptorHandleForHeapStart() );
}

void CreateConstantBuffers()
{
    GfxDeviceGlobal::constantBuffers.resize( 300 );
    GfxDeviceGlobal::mappedConstantBuffers.resize( GfxDeviceGlobal::constantBuffers.size() );
    ae3d::System::Assert( DescriptorHeapManager::numDescriptors > GfxDeviceGlobal::constantBuffers.size(), "There are more constant buffers than descriptors" );

    for (std::size_t bufferIndex = 0; bufferIndex < GfxDeviceGlobal::constantBuffers.size(); ++bufferIndex)
    {
        D3D12_HEAP_PROPERTIES prop = {};
        prop.Type = D3D12_HEAP_TYPE_UPLOAD;
        prop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
        prop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;
        prop.CreationNodeMask = 1;
        prop.VisibleNodeMask = 1;

        D3D12_RESOURCE_DESC buf = {};
        buf.Alignment = 0;
        buf.DepthOrArraySize = 1;
        buf.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
        buf.Flags = D3D12_RESOURCE_FLAG_NONE;
        buf.Format = DXGI_FORMAT_UNKNOWN;
        buf.Height = 1;
        buf.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;
        buf.MipLevels = 1;
        buf.SampleDesc.Count = 1;
        buf.SampleDesc.Quality = 0;
        buf.Width = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT;

        ID3D12Resource* constantBuffer;
        HRESULT hr = GfxDeviceGlobal::device->CreateCommittedResource(
            &prop,
            D3D12_HEAP_FLAG_NONE,
            &buf,
            D3D12_RESOURCE_STATE_GENERIC_READ,
            nullptr,
            IID_PPV_ARGS( &constantBuffer ) );
        if (FAILED( hr ))
        {
            ae3d::System::Print( "Unable to create shader constant buffer!" );
            return;
        }

        constantBuffer->SetName( L"ConstantBuffer" );

        auto handle = DescriptorHeapManager::AllocateDescriptor( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );// DescriptorHeapManager::GetCbvSrvUavHeap()->GetCPUDescriptorHandleForHeapStart();
        //handle.ptr += GfxDeviceGlobal::device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV ) * (bufferIndex);

        GfxDeviceGlobal::constantBuffers[ bufferIndex ] = constantBuffer;

        hr = constantBuffer->Map( 0, nullptr, reinterpret_cast<void**>( &GfxDeviceGlobal::mappedConstantBuffers[ bufferIndex ] ) );
        if (FAILED( hr ))
        {
            ae3d::System::Print( "Unable to map shader constant buffer!" );
        }
    }
}

void ae3d::CreateRenderer( int samples )
{
    if (samples > 0 && samples < 17)
    {
        GfxDeviceGlobal::sampleCount = samples;
    }

#ifdef DEBUG
    ID3D12Debug* debugController;
    HRESULT dhr = D3D12GetDebugInterface( IID_PPV_ARGS( &debugController ) );
    if (dhr == S_OK)
    {
        debugController->EnableDebugLayer();
#if 0
        ID3D12Debug1* debugController1;
        dhr = debugController->QueryInterface( IID_PPV_ARGS( &debugController1 ) );

        if (dhr == S_OK)
        {
            debugController1->SetEnableGPUBasedValidation( true );
        }
#endif
        debugController->Release();
    }
    else
    {
        OutputDebugStringA( "Failed to create debug layer!\n" );
    }
#endif
    IDXGIFactory4* factory = nullptr;
    HRESULT hr = CreateDXGIFactory1( IID_PPV_ARGS( &factory ) );
    AE3D_CHECK_D3D( hr, "Failed to create D3D12 WARP factory" );

    factory->EnumAdapters( 0, reinterpret_cast< IDXGIAdapter** >( &GfxDeviceGlobal::adapter ) );

    bool useWarp = false;

    if (useWarp)
    {
        hr = factory->EnumWarpAdapter( IID_PPV_ARGS( &GfxDeviceGlobal::adapter ) );
        AE3D_CHECK_D3D( hr, "Failed to create D3D12 WARP adapter" );
    }

    hr = D3D12CreateDevice( GfxDeviceGlobal::adapter, D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS( &GfxDeviceGlobal::device ) );
    AE3D_CHECK_D3D( hr, "Failed to create D3D12 device with feature level 11.0" );
    GfxDeviceGlobal::device->SetName( L"D3D12 device" );
#ifdef DEBUG
    hr = GfxDeviceGlobal::device->QueryInterface( IID_PPV_ARGS( &GfxDeviceGlobal::infoQueue ) );
    AE3D_CHECK_D3D( hr, "Infoqueue failed" );
    GfxDeviceGlobal::infoQueue->SetBreakOnSeverity( D3D12_MESSAGE_SEVERITY_ERROR, TRUE );
#endif

    hr = GfxDeviceGlobal::device->CreateCommandAllocator( D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS( &GfxDeviceGlobal::commandListAllocator ) );
    AE3D_CHECK_D3D( hr, "Failed to create command allocator" );
    GfxDeviceGlobal::commandListAllocator->SetName( L"Command Allocator" );

    hr = GfxDeviceGlobal::device->CreateCommandList( 1, D3D12_COMMAND_LIST_TYPE_DIRECT, GfxDeviceGlobal::commandListAllocator, nullptr, IID_PPV_ARGS( &GfxDeviceGlobal::graphicsCommandList ) );
    AE3D_CHECK_D3D( hr, "Failed to create command list" );
    GfxDeviceGlobal::graphicsCommandList->SetName( L"Graphics Command List" );

    hr = GfxDeviceGlobal::graphicsCommandList->Close();
    AE3D_CHECK_D3D( hr, "command list close" );

    D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
    commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
    hr = GfxDeviceGlobal::device->CreateCommandQueue( &commandQueueDesc, IID_PPV_ARGS( &GfxDeviceGlobal::commandQueue ) );
    AE3D_CHECK_D3D( hr, "Failed to create command queue" );
    GfxDeviceGlobal::commandQueue->SetName( L"Command Queue" );

    hr = GfxDeviceGlobal::device->CreateFence( 0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS( &GfxDeviceGlobal::fence ) );
    AE3D_CHECK_D3D( hr, "Failed to create fence" );
    GfxDeviceGlobal::fence->SetName( L"Fence" );
    GfxDeviceGlobal::fence->Signal( 0 );

    GfxDeviceGlobal::fenceEvent = CreateEvent( nullptr, FALSE, FALSE, nullptr );
    ae3d::System::Assert( GfxDeviceGlobal::fenceEvent != INVALID_HANDLE_VALUE, "Invalid fence event value!" );

    DXGI_SWAP_CHAIN_DESC1 swapChainDesc1 = {};
    swapChainDesc1.BufferCount = 2;
    swapChainDesc1.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    swapChainDesc1.Flags = 0;
    swapChainDesc1.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
    swapChainDesc1.Width = WindowGlobal::windowWidth;
    swapChainDesc1.Height = WindowGlobal::windowHeight;
    swapChainDesc1.SampleDesc.Count = 1;
    swapChainDesc1.SampleDesc.Quality = 0;
    swapChainDesc1.SwapEffect = samples > 1 ? DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL : DXGI_SWAP_EFFECT_FLIP_DISCARD;

    hr = factory->CreateSwapChainForHwnd( GfxDeviceGlobal::commandQueue, WindowGlobal::hwnd, &swapChainDesc1, nullptr, nullptr, (IDXGISwapChain1**)&GfxDeviceGlobal::swapChain );
    AE3D_CHECK_D3D( hr, "Failed to create swap chain" );
    factory->Release();

    D3D12_CPU_DESCRIPTOR_HANDLE initSamplerHeapTemp = DescriptorHeapManager::AllocateDescriptor( D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER );
    D3D12_CPU_DESCRIPTOR_HANDLE initDsvHeapTemp = DescriptorHeapManager::AllocateDescriptor( D3D12_DESCRIPTOR_HEAP_TYPE_DSV );
    D3D12_CPU_DESCRIPTOR_HANDLE initCbvSrvUavHeapTemp = DescriptorHeapManager::AllocateDescriptor( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );

    CreateBackBuffer();
    CreateMSAA();
    CreateRootSignature();
    CreateDepthStencil();
    CreateSamplers();
    CreateConstantBuffers();

    GfxDeviceGlobal::lightTiler.Init();
}

void ae3d::GfxDevice::SetPolygonOffset( bool enable, float, float )
{
    if (enable)
    {
        ae3d::System::Print( "SetPolygonOffset is unimplemented on D3D12 renderer" );
    }
}

void ae3d::GfxDevice::CreateNewUniformBuffer()
{
    ++GfxDeviceGlobal::currentConstantBufferIndex;

    if (GfxDeviceGlobal::currentConstantBufferIndex == GfxDeviceGlobal::mappedConstantBuffers.size())
    {
        GfxDeviceGlobal::currentConstantBufferIndex = 0;
    }
}

void* ae3d::GfxDevice::GetCurrentUniformBuffer()
{
    return GfxDeviceGlobal::mappedConstantBuffers[ GfxDeviceGlobal::currentConstantBufferIndex ];
}

void ae3d::GfxDevice::PushGroupMarker( const char* name )
{
    wchar_t wstr[ 128 ];
    std::mbstowcs( wstr, name, 128 );
    const UINT size = static_cast<UINT>((wcslen( wstr ) + 1) * sizeof( wstr[ 0 ] ));
    GfxDeviceGlobal::graphicsCommandList->BeginEvent( DirectX::Detail::PIX_EVENT_UNICODE_VERSION, wstr, size );
}

void ae3d::GfxDevice::PopGroupMarker()
{
    GfxDeviceGlobal::graphicsCommandList->EndEvent();
}

int ae3d::GfxDevice::CreateLineBuffer( const std::vector< Vec3 >& lines, const Vec3& color )
{
    return 0;
}

void ae3d::GfxDevice::DrawLines( int handle )
{

}

void ae3d::GfxDevice::Draw( VertexBuffer& vertexBuffer, int startFace, int endFace, Shader& shader, BlendMode blendMode, DepthFunc depthFunc,
                            CullMode cullMode, FillMode fillMode )
{
    // Prevents feedback. Currently disabled because it also prevents drawing a sprite that uses render texture.
    /*if (GfxDeviceGlobal::renderTexture0 && GfxDeviceGlobal::currentRenderTargetRTV.ptr == GfxDeviceGlobal::renderTexture0->GetRTV().ptr)
    {
        return;
    }*/

    DXGI_FORMAT rtvFormat = GfxDeviceGlobal::currentRenderTarget ? GfxDeviceGlobal::currentRenderTarget->GetDXGIFormat() : DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
    
    if (GfxDeviceGlobal::sampleCount > 1)
    {
        rtvFormat = (GfxDeviceGlobal::currentRenderTarget ? GfxDeviceGlobal::currentRenderTarget->GetDXGIFormat() : DXGI_FORMAT_R8G8B8A8_UNORM);
    }

    const unsigned psoHash = GetPSOHash( vertexBuffer.GetVertexFormat(), shader, blendMode, depthFunc, cullMode, fillMode, rtvFormat, GfxDeviceGlobal::currentRenderTarget ? 1 : GfxDeviceGlobal::sampleCount );

    if (GfxDeviceGlobal::psoCache.find( psoHash ) == std::end( GfxDeviceGlobal::psoCache ))
    {
        CreatePSO( vertexBuffer.GetVertexFormat(), shader, blendMode, depthFunc, cullMode, fillMode, rtvFormat, GfxDeviceGlobal::currentRenderTarget ? 1 : GfxDeviceGlobal::sampleCount );
    }
    
    D3D12_DESCRIPTOR_HEAP_DESC desc = {};
    desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    desc.NumDescriptors = DescriptorHeapManager::numDescriptors;
    desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    desc.NodeMask = 1;

    ID3D12DescriptorHeap* tempHeap = nullptr;
    HRESULT hr = GfxDeviceGlobal::device->CreateDescriptorHeap( &desc, IID_PPV_ARGS( &tempHeap ) );
    AE3D_CHECK_D3D( hr, "Failed to create CBV_SRV_UAV descriptor heap" );
    GfxDeviceGlobal::frameHeaps.push_back( tempHeap );
    wchar_t wname[ 128 ];
    std::string heapName( "tempHeap_faces_" );
    heapName += std::to_string( vertexBuffer.GetFaceCount() );
    std::mbstowcs( wname, heapName.c_str(), 128 );
    tempHeap->SetName( wname );

    D3D12_CPU_DESCRIPTOR_HANDLE handle = tempHeap->GetCPUDescriptorHandleForHeapStart();

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
    cbvDesc.BufferLocation = GfxDeviceGlobal::constantBuffers[ GfxDeviceGlobal::currentConstantBufferIndex ]->GetGPUVirtualAddress();
    cbvDesc.SizeInBytes = D3D12_CONSTANT_BUFFER_DATA_PLACEMENT_ALIGNMENT; // must be a multiple of 256
    GfxDeviceGlobal::device->CreateConstantBufferView( &cbvDesc, handle );
    
    handle.ptr += GfxDeviceGlobal::device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );
    
    const DXGI_FORMAT format0 = GfxDeviceGlobal::texture0->GetDXGIFormat();
    const int mipLevelCount0 = GfxDeviceGlobal::texture0->GetMipLevelCount();
    const D3D12_SRV_DIMENSION viewDimension0 = GfxDeviceGlobal::texture0->IsCube() ? D3D12_SRV_DIMENSION_TEXTURECUBE : D3D12_SRV_DIMENSION_TEXTURE2D;

    // TODO: Get from texture object
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc0 = {};
    srvDesc0.Format = format0;
    srvDesc0.ViewDimension = viewDimension0;
    srvDesc0.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    if (srvDesc0.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2D)
    {
        srvDesc0.Texture2D.MipLevels = mipLevelCount0;
        srvDesc0.Texture2D.MostDetailedMip = 0;
        srvDesc0.Texture2D.PlaneSlice = 0;
        srvDesc0.Texture2D.ResourceMinLODClamp = 0.0f;
    }
    else if (srvDesc0.ViewDimension == D3D12_SRV_DIMENSION_TEXTURECUBE)
    {
        srvDesc0.TextureCube.MipLevels = mipLevelCount0;
        srvDesc0.TextureCube.MostDetailedMip = 0;
        srvDesc0.TextureCube.ResourceMinLODClamp = 0.0f;
    }
    else
    {
        System::Assert( false, "unhandled texture dimension" );
    }

    const DXGI_FORMAT format1 = GfxDeviceGlobal::texture1->GetDXGIFormat();
    const int mipLevelCount1 = GfxDeviceGlobal::texture1->GetMipLevelCount();
    const D3D12_SRV_DIMENSION viewDimension1 = GfxDeviceGlobal::texture1->IsCube() ? D3D12_SRV_DIMENSION_TEXTURECUBE : D3D12_SRV_DIMENSION_TEXTURE2D;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc1 = {};
    srvDesc1.Format = format1;
    srvDesc1.ViewDimension = viewDimension1;
    srvDesc1.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;

    if (srvDesc1.ViewDimension == D3D12_SRV_DIMENSION_TEXTURE2D)
    {
        srvDesc1.Texture2D.MipLevels = mipLevelCount1;
        srvDesc1.Texture2D.MostDetailedMip = 0;
        srvDesc1.Texture2D.PlaneSlice = 0;
        srvDesc1.Texture2D.ResourceMinLODClamp = 0.0f;
    }
    else if (srvDesc1.ViewDimension == D3D12_SRV_DIMENSION_TEXTURECUBE)
    {
        srvDesc1.TextureCube.MipLevels = mipLevelCount1;
        srvDesc1.TextureCube.MostDetailedMip = 0;
        srvDesc1.TextureCube.ResourceMinLODClamp = 0.0f;
    }
    else
    {
        //System::Assert( false, "unhandled texture dimension" );
    }

    ID3D12Resource* texResource0 = GfxDeviceGlobal::texture0->GetGpuResource()->resource;

    GfxDeviceGlobal::device->CreateShaderResourceView( texResource0, &srvDesc0, handle );

    handle.ptr += GfxDeviceGlobal::device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV );

    ID3D12Resource* texResource1 = GfxDeviceGlobal::texture1->GetGpuResource()->resource;

    GfxDeviceGlobal::device->CreateShaderResourceView( texResource1, &srvDesc1, handle );

    D3D12_VERTEX_BUFFER_VIEW vertexBufferView;
    vertexBufferView.BufferLocation = vertexBuffer.GetVBResource()->GetGPUVirtualAddress();
    vertexBufferView.StrideInBytes = vertexBuffer.GetStride();
    vertexBufferView.SizeInBytes = vertexBuffer.GetIBOffset();

    D3D12_INDEX_BUFFER_VIEW indexBufferView;
    indexBufferView.BufferLocation = vertexBuffer.GetVBResource()->GetGPUVirtualAddress() + vertexBuffer.GetIBOffset();
    indexBufferView.SizeInBytes = vertexBuffer.GetIBSize();
    indexBufferView.Format = DXGI_FORMAT_R16_UINT;

    D3D12_GPU_DESCRIPTOR_HANDLE samplerHandle = GetSampler( GfxDeviceGlobal::texture0->GetMipmaps(), GfxDeviceGlobal::texture0->GetWrap(),
        GfxDeviceGlobal::texture0->GetFilter(), GfxDeviceGlobal::texture0->GetAnisotropy() );

    ID3D12DescriptorHeap* descHeaps[] = { tempHeap, DescriptorHeapManager::GetSamplerHeap() };
    GfxDeviceGlobal::graphicsCommandList->SetDescriptorHeaps( 2, &descHeaps[ 0 ] );
    GfxDeviceGlobal::graphicsCommandList->SetGraphicsRootSignature( GfxDeviceGlobal::rootSignatureGraphics );
    GfxDeviceGlobal::graphicsCommandList->SetGraphicsRootDescriptorTable( 0, tempHeap->GetGPUDescriptorHandleForHeapStart() );
    GfxDeviceGlobal::graphicsCommandList->SetGraphicsRootDescriptorTable( 1, samplerHandle );
    GfxDeviceGlobal::graphicsCommandList->SetPipelineState( GfxDeviceGlobal::psoCache[ psoHash ] );
    GfxDeviceGlobal::graphicsCommandList->IASetPrimitiveTopology( D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST );
    GfxDeviceGlobal::graphicsCommandList->IASetVertexBuffers( 0, 1, &vertexBufferView );
    GfxDeviceGlobal::graphicsCommandList->IASetIndexBuffer( &indexBufferView );
    GfxDeviceGlobal::graphicsCommandList->DrawIndexedInstanced( endFace * 3 - startFace * 3, 1, startFace * 3, 0, 0 );

    Statistics::IncTriangleCount( endFace - startFace );
    Statistics::IncDrawCalls();
}

void ae3d::GfxDevice::Init( int /*width*/, int /*height*/ )
{
}

void ae3d::GfxDevice::SetMultiSampling( bool /*enable*/ )
{
}

void ae3d::GfxDevice::ResetCommandList()
{
    HRESULT hr = GfxDeviceGlobal::graphicsCommandList->Reset( GfxDeviceGlobal::commandListAllocator, nullptr );
    AE3D_CHECK_D3D( hr, "graphicsCommandList Reset" );

    GfxDeviceGlobal::texture0 = Texture2D::GetDefaultTexture();
    GfxDeviceGlobal::texture1 = Texture2D::GetDefaultTexture();
}

void ae3d::GfxDevice::GetGpuMemoryUsage( unsigned& outUsedMBytes, unsigned& outBudgetMBytes )
{
    DXGI_QUERY_VIDEO_MEMORY_INFO videoMemoryInfo;
    GfxDeviceGlobal::adapter->QueryVideoMemoryInfo( 0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &videoMemoryInfo );

    outUsedMBytes = (unsigned)videoMemoryInfo.CurrentUsage / (1024 * 1024);
    outBudgetMBytes = (unsigned)videoMemoryInfo.Budget / (1024 * 1024);
}

void ae3d::GfxDevice::ReleaseGPUObjects()
{
    VertexBuffer::DestroyBuffers();
    DestroyShaders();
    DestroyComputeShaders();
    Texture2D::DestroyTextures();
    TextureCube::DestroyTextures();
    RenderTexture::DestroyTextures();
    AE3D_SAFE_RELEASE( GfxDeviceGlobal::msaaColor );
    AE3D_SAFE_RELEASE( GfxDeviceGlobal::msaaDepth );
    AE3D_SAFE_RELEASE( GfxDeviceGlobal::depthTexture );
    AE3D_SAFE_RELEASE( GfxDeviceGlobal::commandListAllocator );
    DescriptorHeapManager::Deinit();

    for (auto& pso : GfxDeviceGlobal::psoCache)
    {
        AE3D_SAFE_RELEASE( pso.second );
    }

    AE3D_SAFE_RELEASE( GfxDeviceGlobal::infoQueue );
    AE3D_SAFE_RELEASE( GfxDeviceGlobal::renderTargets[ 0 ] );
    AE3D_SAFE_RELEASE( GfxDeviceGlobal::renderTargets[ 1 ] );
    GfxDeviceGlobal::swapChain->SetFullscreenState( FALSE, nullptr );
    AE3D_SAFE_RELEASE( GfxDeviceGlobal::swapChain );
    AE3D_SAFE_RELEASE( GfxDeviceGlobal::rootSignatureGraphics );
    AE3D_SAFE_RELEASE( GfxDeviceGlobal::rootSignatureTileCuller );

    AE3D_SAFE_RELEASE( GfxDeviceGlobal::fence );
    AE3D_SAFE_RELEASE( GfxDeviceGlobal::graphicsCommandList );
    AE3D_SAFE_RELEASE( GfxDeviceGlobal::commandQueue );
    AE3D_SAFE_RELEASE( GfxDeviceGlobal::lightTilerPSO );

    GfxDeviceGlobal::lightTiler.DestroyBuffers();

    for (std::size_t cbInd = 0; cbInd < GfxDeviceGlobal::constantBuffers.size(); ++cbInd)
    {
        AE3D_SAFE_RELEASE( GfxDeviceGlobal::constantBuffers[ cbInd ] );
    }
    /*ID3D12DebugDevice* d3dDebug = nullptr;
    GfxDeviceGlobal::device->QueryInterface(__uuidof(ID3D12DebugDevice), reinterpret_cast<void**>(&d3dDebug));
    AE3D_SAFE_RELEASE( GfxDeviceGlobal::device );
    d3dDebug->ReportLiveDeviceObjects( D3D12_RLDO_DETAIL );
    AE3D_SAFE_RELEASE( d3dDebug );*/

    AE3D_SAFE_RELEASE( GfxDeviceGlobal::device );
}

void ae3d::GfxDevice::ClearScreen( unsigned clearFlags )
{
    const auto backBufferIndex = GfxDeviceGlobal::swapChain->GetCurrentBackBufferIndex();
    GfxDeviceGlobal::rtvResources[ backBufferIndex ].resource = GfxDeviceGlobal::renderTargets[ backBufferIndex ];

    GpuResource* resource = GfxDeviceGlobal::currentRenderTarget ? GfxDeviceGlobal::currentRenderTarget->GetGpuResource() : &GfxDeviceGlobal::rtvResources[ backBufferIndex ];

    TransitionResource( *resource, D3D12_RESOURCE_STATE_RENDER_TARGET );
    
    FLOAT vpWidth = static_cast< FLOAT >( GfxDeviceGlobal::backBufferWidth );
    FLOAT vpHeight = static_cast< FLOAT >( GfxDeviceGlobal::backBufferHeight );

    if (GfxDeviceGlobal::currentRenderTarget)
    {
        vpWidth = static_cast< FLOAT >( GfxDeviceGlobal::currentRenderTarget->GetWidth() );
        vpHeight = static_cast< FLOAT >( GfxDeviceGlobal::currentRenderTarget->GetHeight() );
    }

    D3D12_VIEWPORT viewPort{ 0, 0, vpWidth, vpHeight, 0, 1 };
    GfxDeviceGlobal::graphicsCommandList->RSSetViewports( 1, &viewPort );

    D3D12_RECT scissor = {};
    scissor.right = static_cast< LONG >( vpWidth );
    scissor.bottom = static_cast< LONG >( vpHeight );
    GfxDeviceGlobal::graphicsCommandList->RSSetScissorRects( 1, &scissor );

    auto descHandleRtvStep = GfxDeviceGlobal::device->GetDescriptorHandleIncrementSize( D3D12_DESCRIPTOR_HEAP_TYPE_RTV );

    D3D12_CPU_DESCRIPTOR_HANDLE descHandleRtv;

    if (GfxDeviceGlobal::currentRenderTarget)
    {
        descHandleRtv = GfxDeviceGlobal::currentRenderTarget->IsCube() ? GfxDeviceGlobal::currentRenderTarget->GetCubeRTV( GfxDeviceGlobal::currentRenderTargetCubeMapFace ) : GfxDeviceGlobal::currentRenderTarget->GetRTV();
    }
    else if (GfxDeviceGlobal::sampleCount > 1)
    {
        descHandleRtv = GfxDeviceGlobal::msaaColorHandle;
    }
    else
    {
        descHandleRtv = DescriptorHeapManager::GetRTVHeap()->GetCPUDescriptorHandleForHeapStart();
        descHandleRtv.ptr += GfxDeviceGlobal::swapChain->GetCurrentBackBufferIndex() * descHandleRtvStep;
    }

    if ((clearFlags & ClearFlags::Color) != 0)
    {
        GfxDeviceGlobal::graphicsCommandList->ClearRenderTargetView( descHandleRtv, GfxDeviceGlobal::clearColor, 0, nullptr );
    }

    D3D12_CPU_DESCRIPTOR_HANDLE descHandleDsv;
    if (GfxDeviceGlobal::currentRenderTarget && GfxDeviceGlobal::currentRenderTarget->IsCube())
    {
        descHandleDsv = GfxDeviceGlobal::currentRenderTarget->GetCubeDSV( GfxDeviceGlobal::currentRenderTargetCubeMapFace );
    }
    else if (GfxDeviceGlobal::currentRenderTarget && !GfxDeviceGlobal::currentRenderTarget->IsCube())
    {
        descHandleDsv = GfxDeviceGlobal::currentRenderTarget->GetDSV();
    }
    else
    {
        descHandleDsv = DescriptorHeapManager::GetDSVHeap()->GetCPUDescriptorHandleForHeapStart();
    }

    if (GfxDeviceGlobal::sampleCount > 1 && !GfxDeviceGlobal::currentRenderTarget)
    {
        descHandleDsv = GfxDeviceGlobal::msaaDepthHandle;
    }

    if ((clearFlags & ClearFlags::Depth) != 0)
    {
        GfxDeviceGlobal::graphicsCommandList->ClearDepthStencilView( descHandleDsv, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr );
    }

    GfxDeviceGlobal::graphicsCommandList->OMSetRenderTargets( 1, &descHandleRtv, TRUE, &descHandleDsv );
}

void ae3d::GfxDevice::Present()
{
    TransitionResource( GfxDeviceGlobal::rtvResources[ GfxDeviceGlobal::swapChain->GetCurrentBackBufferIndex() ], D3D12_RESOURCE_STATE_PRESENT );

    if (GfxDeviceGlobal::sampleCount > 1)
    {
        GpuResource msaaColorGpuResource;
        msaaColorGpuResource.resource = GfxDeviceGlobal::msaaColor;
        msaaColorGpuResource.usageState = D3D12_RESOURCE_STATE_RENDER_TARGET;
		
        auto backBufferRT = GfxDeviceGlobal::rtvResources[ GfxDeviceGlobal::swapChain->GetCurrentBackBufferIndex() ];

        TransitionResource( msaaColorGpuResource, D3D12_RESOURCE_STATE_RESOLVE_SOURCE );
        TransitionResource( backBufferRT, D3D12_RESOURCE_STATE_RESOLVE_DEST );
        GfxDeviceGlobal::graphicsCommandList->ResolveSubresource( backBufferRT.resource, 0, GfxDeviceGlobal::msaaColor, 0, DXGI_FORMAT_R8G8B8A8_UNORM );
        TransitionResource( backBufferRT, D3D12_RESOURCE_STATE_PRESENT );
        TransitionResource( msaaColorGpuResource, D3D12_RESOURCE_STATE_RENDER_TARGET );
    }

    HRESULT hr = GfxDeviceGlobal::graphicsCommandList->Close();
    AE3D_CHECK_D3D( hr, "command list close" );

    ID3D12CommandList* ppCommandLists[] = { GfxDeviceGlobal::graphicsCommandList };
    GfxDeviceGlobal::commandQueue->ExecuteCommandLists( 1, &ppCommandLists[ 0 ] );

    hr = GfxDeviceGlobal::swapChain->Present( 1, 0 );

    if (FAILED( hr ))
    {
        if (hr == DXGI_ERROR_DEVICE_REMOVED)
        {
            ae3d::System::Assert( false, "Present failed. Reason: device removed." );
        }
        else if (hr == DXGI_ERROR_DEVICE_RESET)
        {
            ae3d::System::Assert( false, "Present failed. Reason: device reset." );
        }
        else if (hr == DXGI_ERROR_DEVICE_HUNG)
        {
            ae3d::System::Assert( false, "Present failed. Reason: device hung." );
        }
        else
        {
            ae3d::System::Assert( false, "Present failed. Reason: unknown." );
        }
    }

    WaitForPreviousFrame();

    hr = GfxDeviceGlobal::commandListAllocator->Reset();
    AE3D_CHECK_D3D( hr, "commandListAllocator Reset" );

    for (std::size_t i = 0; i < GfxDeviceGlobal::frameHeaps.size(); ++i)
    {
        AE3D_SAFE_RELEASE( GfxDeviceGlobal::frameHeaps[ i ] );
    }
    
    GfxDeviceGlobal::frameHeaps.clear();

    for (std::size_t i = 0; i < Global::frameVBUploads.size(); ++i)
    {
       AE3D_SAFE_RELEASE( Global::frameVBUploads[ i ] );
    }
    
    Global::frameVBUploads.clear();
    
    GfxDeviceGlobal::currentRenderTarget = nullptr;

    Statistics::EndFrameTimeProfiling();
}

void ae3d::GfxDevice::SetClearColor( float red, float green, float blue )
{
    GfxDeviceGlobal::clearColor[ 0 ] = red;
    GfxDeviceGlobal::clearColor[ 1 ] = green;
    GfxDeviceGlobal::clearColor[ 2 ] = blue;
}

void ae3d::GfxDevice::ErrorCheck( const char* /*info*/ )
{
}

void ae3d::GfxDevice::SetRenderTarget( RenderTexture* target, unsigned cubeMapFace )
{
    System::Assert( !target || target->IsRenderTexture(), "target must be render texture" );
    System::Assert( cubeMapFace < 6, "invalid cube map face" );

    GfxDeviceGlobal::currentRenderTarget = target;
    GfxDeviceGlobal::currentRenderTargetCubeMapFace = cubeMapFace;

    if (target && target->IsCube())
    {
        TransitionResource( *target->GetGpuResource(), D3D12_RESOURCE_STATE_RENDER_TARGET );
    }
    else if (target)
    {
        System::Assert( target->GetGpuResource()->resource != nullptr, "no GPU resource's resource in render target!" );
        TransitionResource( *target->GetGpuResource(), D3D12_RESOURCE_STATE_RENDER_TARGET );
    }
}
