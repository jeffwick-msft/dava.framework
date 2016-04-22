/*==================================================================================
    Copyright (c) 2008, binaryzebra
    All rights reserved.

    Redistribution and use in source and binary forms, with or without
    modification, are permitted provided that the following conditions are met:

    * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
    notice, this list of conditions and the following disclaimer in the
    documentation and/or other materials provided with the distribution.
    * Neither the name of the binaryzebra nor the
    names of its contributors may be used to endorse or promote products
    derived from this software without specific prior written permission.

    THIS SOFTWARE IS PROVIDED BY THE binaryzebra AND CONTRIBUTORS "AS IS" AND
    ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
    WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
    DISCLAIMED. IN NO EVENT SHALL binaryzebra BE LIABLE FOR ANY
    DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
    (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
    LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
    ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
    (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
    SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
=====================================================================================*/

    #include "rhi_DX11.h"
    #include "../Common/rhi_Impl.h"
    
    #include "Debug/DVAssert.h"
    #include "Logger/Logger.h"
    #include "Core/Core.h"
using DAVA::Logger;

    #include "_dx11.h"
    #include "../rhi_Type.h"
    #include "../Common/dbg_StatSet.h"

    #include <vector>

#if defined(__DAVAENGINE_WIN_UAP__)
	#include "uap_dx11.h"
#endif

namespace rhi
{
//==============================================================================

static Dispatch DispatchDX11 = { 0 };

static RenderDeviceCaps _DeviceCapsDX11 = {};

//------------------------------------------------------------------------------

static Api
dx11_HostApi()
{
    return RHI_DX11;
}

//------------------------------------------------------------------------------

static const RenderDeviceCaps&
dx11_DeviceCaps()
{
    return _DeviceCapsDX11;
}

//------------------------------------------------------------------------------

static bool
dx11_NeedRestoreResources()
{
    return false;
}

//------------------------------------------------------------------------------

static bool
dx11_TextureFormatSupported(TextureFormat format)
{
    bool supported = false;

    switch (format)
    {
    case TEXTURE_FORMAT_R8G8B8A8:
    case TEXTURE_FORMAT_R5G5B5A1:
    case TEXTURE_FORMAT_R5G6B5:
    case TEXTURE_FORMAT_R4G4B4A4:
    case TEXTURE_FORMAT_R8:
    case TEXTURE_FORMAT_R16:
    case TEXTURE_FORMAT_DXT1:
    case TEXTURE_FORMAT_DXT3:
    case TEXTURE_FORMAT_DXT5:
        supported = true;
        break;
    }

    return supported;
}

//------------------------------------------------------------------------------

static bool
_IsValidIntelCardDX11(unsigned vendor_id, unsigned device_id)
{
    return ((vendor_id == 0x8086) && // Intel Architecture

            // These guys are prehistoric :)

            (device_id == 0x2572) || // 865G
            (device_id == 0x3582) || // 855GM
            (device_id == 0x2562) || // 845G
            (device_id == 0x3577) || // 830M

            // These are from 2005 and later

            (device_id == 0x2A02) || // GM965 Device 0
            (device_id == 0x2A03) || // GM965 Device 1
            (device_id == 0x29A2) || // G965 Device 0
            (device_id == 0x29A3) || // G965 Device 1
            (device_id == 0x27A2) || // 945GM Device 0
            (device_id == 0x27A6) || // 945GM Device 1
            (device_id == 0x2772) || // 945G Device 0
            (device_id == 0x2776) || // 945G Device 1
            (device_id == 0x2592) || // 915GM Device 0
            (device_id == 0x2792) || // 915GM Device 1
            (device_id == 0x2582) || // 915G Device 0
            (device_id == 0x2782) // 915G Device 1
            );
}

//------------------------------------------------------------------------------

static void
dx11_Uninitialize()
{
    UninitializeRenderThreadDX11();
}

//------------------------------------------------------------------------------

static void
dx11_Reset(const ResetParam& param)
{
    if (_DX11_InitParam.fullScreen != param.fullScreen)
    {
    }
    else
    {
#if defined(__DAVAENGINE_WIN_UAP__)
        resize_swapchain(param.width, param.height, param.scaleX, param.scaleY);
#else
//Not implemented
#endif
    }
}

//------------------------------------------------------------------------------

static void
dx11_SuspendRendering()
{
#if defined(__DAVAENGINE_WIN_UAP__)
    CommandBufferDX11::DiscardAll();

    IDXGIDevice3* dxgiDevice3 = NULL;

    if (SUCCEEDED(_D3D11_Device->QueryInterface(__uuidof(IDXGIDevice3), (void**)(&dxgiDevice3))))
    {
        _D3D11_ImmediateContext->ClearState();
        dxgiDevice3->Trim();
        dxgiDevice3->Release();
    }
#endif
}

//------------------------------------------------------------------------------

static void
dx11_ResumeRendering()
{
}

//------------------------------------------------------------------------------

static void
dx11_TakeScreenshot(ScreenShotCallback callback)
{
    _D3D11_ScreenshotCallbackSync.Lock();
    DVASSERT(!_D3D11_PendingScreenshotCallback);
    _D3D11_PendingScreenshotCallback = callback;
    _D3D11_ScreenshotCallbackSync.Unlock();
}

//------------------------------------------------------------------------------

void _InitDX11()
{
#if defined(__DAVAENGINE_WIN_UAP__)

    init_device_and_swapchain_uap(_DX11_InitParam.window);
    _D3D11_Device->CreateDeferredContext(0, &_D3D11_SecondaryContext);
    get_device_description(_DeviceCapsDX11.deviceDescription);

#else

    HRESULT hr;
    DWORD flags = 0;
    #if RHI_DX11__FORCE_9X_PROFILE
    D3D_FEATURE_LEVEL feature[] = { D3D_FEATURE_LEVEL_9_3, D3D_FEATURE_LEVEL_9_2, D3D_FEATURE_LEVEL_9_1 };
    #else
    D3D_FEATURE_LEVEL feature[] = { /*D3D_FEATURE_LEVEL_11_1, */ D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_9_1 };
    #endif
    DXGI_SWAP_CHAIN_DESC swapchain_desc = { 0 };
    IDXGIAdapter* defAdapter = NULL;

    // enumerate adapters
    {
        IDXGIFactory* factory = NULL;
        std::vector<IDXGIAdapter*> adapter;
        const uint32 preferredVendorID[] =
        {
          0x10DE, // nVIDIA
          0x1002 // ATI
        };

        if (SUCCEEDED(CreateDXGIFactory1(__uuidof(IDXGIFactory), (void**)&factory)))
        {
            IDXGIAdapter* a = NULL;

            for (UINT i = 0; factory->EnumAdapters(i, &a) != DXGI_ERROR_NOT_FOUND; ++i)
            {
                adapter.push_back(a);
            }
        }

        Logger::Info("detected GPUs (%u) :", adapter.size());
        for (unsigned i = 0; i != adapter.size(); ++i)
        {
            DXGI_ADAPTER_DESC desc = { 0 };

            if (SUCCEEDED(adapter[i]->GetDesc(&desc)))
            {
                char info[128];

                ::WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, desc.Description, -1, info, countof(info) - 1, NULL, NULL);

                Logger::Info("  adapter[%u]  \"%s\"  vendor= %04X  device= %04X", i, info, desc.VendorId, desc.DeviceId);

                if (!defAdapter)
                {
                    for (unsigned k = 0; k != countof(preferredVendorID); ++k)
                    {
                        if (desc.VendorId == preferredVendorID[k])
                        {
                            defAdapter = adapter[i];
                            break;
                        }
                    }
                }
                //if( desc.VendorId == 0x8086 )
                //    defAdapter = adapter[i];
            }
        }
    }


    #if 0
    flags |= D3D11_CREATE_DEVICE_DEBUG;
    flags |= D3D11_CREATE_DEVICE_PREVENT_INTERNAL_THREADING_OPTIMIZATIONS;
    #endif

    swapchain_desc.BufferDesc.Width = static_cast<UINT>(_DX11_InitParam.width);
    swapchain_desc.BufferDesc.Height = static_cast<UINT>(_DX11_InitParam.height);
    swapchain_desc.BufferDesc.Format = DXGI_FORMAT_B8G8R8A8_UNORM;
    swapchain_desc.BufferDesc.RefreshRate.Numerator = 0;
    swapchain_desc.BufferDesc.RefreshRate.Denominator = 0;
    swapchain_desc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_PROGRESSIVE;
    swapchain_desc.BufferDesc.Scaling = DXGI_MODE_SCALING_CENTERED;

    swapchain_desc.SampleDesc.Count = 1;
    swapchain_desc.SampleDesc.Quality = 0;

    swapchain_desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; //DXGI_USAGE_BACK_BUFFER;
    swapchain_desc.BufferCount = 2;

    swapchain_desc.OutputWindow = (HWND)_DX11_InitParam.window;
    swapchain_desc.Windowed = (_DX11_InitParam.fullScreen) ? FALSE : TRUE;

    swapchain_desc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
    swapchain_desc.Flags = 0;

    hr = D3D11CreateDeviceAndSwapChain(
    defAdapter, (defAdapter) ? D3D_DRIVER_TYPE_UNKNOWN : D3D_DRIVER_TYPE_HARDWARE, NULL,
    flags, feature, countof(feature), D3D11_SDK_VERSION, &swapchain_desc,
    &_D3D11_SwapChain, &_D3D11_Device, &_D3D11_FeatureLevel, &_D3D11_ImmediateContext);

    if (FAILED(hr))
    {
        // fall back to 'default' adapter
        hr = D3D11CreateDeviceAndSwapChain(
        NULL, D3D_DRIVER_TYPE_HARDWARE, NULL,
        flags, feature, countof(feature), D3D11_SDK_VERSION, &swapchain_desc,
        &_D3D11_SwapChain, &_D3D11_Device, &_D3D11_FeatureLevel, &_D3D11_ImmediateContext);
    }

    if (SUCCEEDED(hr))
    {
        hr = _D3D11_SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)(&_D3D11_SwapChainBuffer));

        if (SUCCEEDED(hr))
        {
            IDXGIDevice* dxgiDevice = NULL;
            IDXGIAdapter* dxgiAdapter = NULL;

            if (SUCCEEDED(_D3D11_Device->QueryInterface(__uuidof(IDXGIDevice), (void**)(&dxgiDevice))))
            {
                if (SUCCEEDED(dxgiDevice->GetAdapter(&dxgiAdapter)))
                {
                    DXGI_ADAPTER_DESC desc = { 0 };

                    if (SUCCEEDED(dxgiAdapter->GetDesc(&desc)))
                    {
                        ::WideCharToMultiByte(CP_ACP, WC_NO_BEST_FIT_CHARS, desc.Description, -1, _DeviceCapsDX11.deviceDescription, countof(_DeviceCapsDX11.deviceDescription), NULL, NULL);

                        Logger::Info("using adapter  \"%s\"  vendor= %04X  device= %04X", _DeviceCapsDX11.deviceDescription, desc.VendorId, desc.DeviceId);
                    }
                }
            }

            hr = _D3D11_Device->QueryInterface(__uuidof(ID3D11Debug), (void**)(&_D3D11_Debug));

            hr = _D3D11_ImmediateContext->QueryInterface(__uuidof(ID3DUserDefinedAnnotation), (void**)(&_D3D11_UserAnnotation));
        }

        hr = _D3D11_Device->CreateRenderTargetView(_D3D11_SwapChainBuffer, 0, &_D3D11_RenderTargetView);

        _D3D11_Device->CreateDeferredContext(0, &_D3D11_SecondaryContext);

        D3D11_TEXTURE2D_DESC ds_desc = { 0 };

        ds_desc.Width = static_cast<UINT>(_DX11_InitParam.width);
        ds_desc.Height = static_cast<UINT>(_DX11_InitParam.height);
        ds_desc.MipLevels = 1;
        ds_desc.ArraySize = 1;
        ds_desc.Format = (_D3D11_FeatureLevel == D3D_FEATURE_LEVEL_11_0) ? DXGI_FORMAT_D32_FLOAT : DXGI_FORMAT_D24_UNORM_S8_UINT;
        ds_desc.SampleDesc.Count = 1;
        ds_desc.SampleDesc.Quality = 0;
        ds_desc.Usage = D3D11_USAGE_DEFAULT;
        ds_desc.BindFlags = D3D11_BIND_DEPTH_STENCIL;
        ds_desc.CPUAccessFlags = 0;
        ds_desc.MiscFlags = 0;

        hr = _D3D11_Device->CreateTexture2D(&ds_desc, 0, &_D3D11_DepthStencilBuffer);
        hr = _D3D11_Device->CreateDepthStencilView(_D3D11_DepthStencilBuffer, 0, &_D3D11_DepthStencilView);
    }

#endif

    #if !RHI_DX11__USE_DEFERRED_CONTEXTS
    ConstBufferDX11::InitializeRingBuffer(_DX11_InitParam.shaderConstRingBufferSize);
    #endif
}

//------------------------------------------------------------------------------

void dx11_Initialize(const InitParam& param)
{
    _DX11_InitParam = param;
    InitializeRenderThreadDX11((param.threadedRenderEnabled) ? param.threadedRenderFrameCount : 0);

    VertexBufferDX11::SetupDispatch(&DispatchDX11);
    IndexBufferDX11::SetupDispatch(&DispatchDX11);
    QueryBufferDX11::SetupDispatch(&DispatchDX11);
    PerfQuerySetDX11::SetupDispatch(&DispatchDX11);
    TextureDX11::SetupDispatch(&DispatchDX11);
    PipelineStateDX11::SetupDispatch(&DispatchDX11);
    ConstBufferDX11::SetupDispatch(&DispatchDX11);
    DepthStencilStateDX11::SetupDispatch(&DispatchDX11);
    SamplerStateDX11::SetupDispatch(&DispatchDX11);
    RenderPassDX11::SetupDispatch(&DispatchDX11);
    CommandBufferDX11::SetupDispatch(&DispatchDX11);

    DispatchDX11.impl_Uninitialize = &dx11_Uninitialize;
    DispatchDX11.impl_Reset = &dx11_Reset;
    DispatchDX11.impl_HostApi = &dx11_HostApi;
    DispatchDX11.impl_TextureFormatSupported = &dx11_TextureFormatSupported;
    DispatchDX11.impl_DeviceCaps = &dx11_DeviceCaps;
    DispatchDX11.impl_NeedRestoreResources = &dx11_NeedRestoreResources;
    DispatchDX11.impl_TakeScreenshot = &dx11_TakeScreenshot;
    DispatchDX11.impl_SuspendRendering = &dx11_SuspendRendering;
    DispatchDX11.impl_ResumeRendering = &dx11_ResumeRendering;

    SetDispatchTable(DispatchDX11);

    if (param.maxVertexBufferCount)
        VertexBufferDX11::Init(param.maxVertexBufferCount);
    if (param.maxIndexBufferCount)
        IndexBufferDX11::Init(param.maxIndexBufferCount);
    if (param.maxConstBufferCount)
        ConstBufferDX11::Init(param.maxConstBufferCount);
    if (param.maxTextureCount)
        TextureDX11::Init(param.maxTextureCount);
    //    ConstBufferDX11::InitializeRingBuffer( param.ringBufferSize );

    stat_DIP = StatSet::AddStat("rhi'dip", "dip");
    stat_DP = StatSet::AddStat("rhi'dp", "dp");
    stat_DTL = StatSet::AddStat("rhi'dtl", "dtl");
    stat_DTS = StatSet::AddStat("rhi'dts", "dts");
    stat_DLL = StatSet::AddStat("rhi'dll", "dll");
    stat_SET_PS = StatSet::AddStat("rhi'set-ps", "set-ps");
    stat_SET_SS = StatSet::AddStat("rhi'set-ss", "set-ss");
    stat_SET_TEX = StatSet::AddStat("rhi'set-tex", "set-tex");
    stat_SET_CB = StatSet::AddStat("rhi'set-cb", "set-cb");
    stat_SET_VB = StatSet::AddStat("rhi'set-vb", "set-vb");
    stat_SET_IB = StatSet::AddStat("rhi'set-ib", "set-ib");

    _DeviceCapsDX11.is32BitIndicesSupported = true;
    _DeviceCapsDX11.isFramebufferFetchSupported = true;
    _DeviceCapsDX11.isVertexTextureUnitsSupported = (_D3D11_FeatureLevel >= D3D_FEATURE_LEVEL_11_0);
    _DeviceCapsDX11.isUpperLeftRTOrigin = true;
    _DeviceCapsDX11.isZeroBaseClipRange = true;
    _DeviceCapsDX11.isCenterPixelMapping = false;
    _DeviceCapsDX11.instancingSupported = (_D3D11_FeatureLevel >= D3D_FEATURE_LEVEL_9_2);
}

//==============================================================================
} // namespace rhi
