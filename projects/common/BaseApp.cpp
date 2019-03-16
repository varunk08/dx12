#pragma comment(linker, "/subsystem:windows")

#include <windows.h>
#include "BaseApp.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

LRESULT CALLBACK
MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    // Forward hwnd on because we can get messages (e.g., WM_CREATE)
    // before CreateWindow returns, and thus before mhMainWnd is valid.
    return BaseApp::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

BaseApp* BaseApp::mApp = nullptr;
BaseApp * BaseApp::GetApp()
{
    return mApp;
}

BaseApp::BaseApp(
    HINSTANCE hInstance)
    :
    mhAppInst(hInstance)
{
    assert(mApp == nullptr);
    mApp = this;
}

void BaseApp::OnResize()
{
    assert(m_d3dDevice);
    assert(m_swapChain);
    assert(m_directCmdListAlloc);

    // Flush before changing any resources.
    FlushCommandQueue();

    ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), nullptr));

    // Release the previous resources we will be recreating.
    for (int i = 0; i < SwapChainBufferCount; ++i)
    {
        m_swapChainBuffer[i].Reset();
    }
    m_depthStencilBuffer.Reset();

    // Resize the swap chain.
    ThrowIfFailed(m_swapChain->ResizeBuffers(SwapChainBufferCount,
                                             m_clientWidth, m_clientHeight,
                                             m_backBufferFormat,
                                             DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

    m_currBackBuffer = 0;
    
    CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
    for (UINT i = 0; i < SwapChainBufferCount; i++)
    {
        ThrowIfFailed(m_swapChain->GetBuffer(i, IID_PPV_ARGS(&m_swapChainBuffer[i])));
        m_d3dDevice->CreateRenderTargetView(m_swapChainBuffer[i].Get(), nullptr, rtvHeapHandle);
        rtvHeapHandle.Offset(1, m_rtvDescriptorSize);
    }

    // Create the depth/stencil buffer and view.
    D3D12_RESOURCE_DESC depthStencilDesc;
    depthStencilDesc.Dimension        = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    depthStencilDesc.Alignment        = 0;
    depthStencilDesc.Width            = m_clientWidth;
    depthStencilDesc.Height           = m_clientHeight;
    depthStencilDesc.DepthOrArraySize = 1;
    depthStencilDesc.MipLevels        = 1;

    // Correction 11/12/2016: SSAO chapter requires an SRV to the depth buffer to read from 
    // the depth buffer.  Therefore, because we need to create two views to the same resource:
    //   1. SRV format: DXGI_FORMAT_R24_UNORM_X8_TYPELESS
    //   2. DSV Format: DXGI_FORMAT_D24_UNORM_S8_UINT
    // we need to create the depth buffer resource with a typeless format.  
    depthStencilDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;

    depthStencilDesc.SampleDesc.Count   = m_4xMsaaEn ? 4 : 1;
    depthStencilDesc.SampleDesc.Quality = m_4xMsaaEn ? (m_4xMsaaQuality - 1) : 0;
    depthStencilDesc.Layout             = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    depthStencilDesc.Flags              = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

    D3D12_CLEAR_VALUE optClear;
    optClear.Format = m_depthStencilFormat;
    optClear.DepthStencil.Depth   = 1.0f;
    optClear.DepthStencil.Stencil = 0;
    ThrowIfFailed(m_d3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                                       D3D12_HEAP_FLAG_NONE,
                                                       &depthStencilDesc,
                                                       D3D12_RESOURCE_STATE_COMMON,
                                                       &optClear,
                                                       IID_PPV_ARGS(m_depthStencilBuffer.GetAddressOf())));

    // Create descriptor to mip level 0 of entire resource using the format of the resource.
    D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
    dsvDesc.Flags              = D3D12_DSV_FLAG_NONE;
    dsvDesc.ViewDimension      = D3D12_DSV_DIMENSION_TEXTURE2D;
    dsvDesc.Format             = m_depthStencilFormat;
    dsvDesc.Texture2D.MipSlice = 0;
    m_d3dDevice->CreateDepthStencilView(m_depthStencilBuffer.Get(), &dsvDesc, DepthStencilView());

    // Transition the resource from its initial state to be used as a depth buffer.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(m_depthStencilBuffer.Get(),
                                   D3D12_RESOURCE_STATE_COMMON,
                                   D3D12_RESOURCE_STATE_DEPTH_WRITE));
    
    // Execute the resize commands.
    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // Wait until resize is complete.
    FlushCommandQueue();

    // Update the viewport transform to cover the client area.
    m_screenViewport.TopLeftX = 0;
    m_screenViewport.TopLeftY = 0;
    m_screenViewport.Width    = static_cast<float>(m_clientWidth);
    m_screenViewport.Height   = static_cast<float>(m_clientHeight);
    m_screenViewport.MinDepth = 0.0f;
    m_screenViewport.MaxDepth = 1.0f;

    m_scissorRect = { 0, 0, m_clientWidth, m_clientHeight };
}

int BaseApp::Run()
{
    MSG msg = {};
    m_timer.Reset();

    while (msg.message != WM_QUIT)
    {
        if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
        else
        {
            m_timer.Tick();

            if (m_appPaused == false)
            {
                CalculateFrameStats();
                Update(m_timer);
                Draw(m_timer);
            }
            else
            {
                Sleep(100);
            }
        }
    }

    return static_cast<int>(msg.wParam);
}

bool BaseApp::Initialize()
{
    bool result = true;
    
    result = InitMainWindow();
    if (result == true)
    {
        result = InitDirect3D();
    }

    if (result == true)
    {
        OnResize();
    }

    return result;
}

LRESULT BaseApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (msg)
    {
    case WM_ACTIVATE:
    {   if (LOWORD(wParam) == WA_INACTIVE)
        {
            m_appPaused = true;
            m_timer.Stop();
        }
        else
        {
            m_appPaused = false;
            m_timer.Start();
        }
        break;
    }
    case WM_SIZE:
    {
        // Save the new client area dimensions.
        m_clientWidth = LOWORD(lParam);
        m_clientHeight = HIWORD(lParam);
        if (m_d3dDevice)
        {
            if (wParam == SIZE_MINIMIZED)
            {
                m_appPaused = true;
                m_minimized = true;
                m_maximized = false;
            }
            else if (wParam == SIZE_MAXIMIZED)
            {
                m_appPaused = false;
                m_minimized = false;
                m_maximized = true;
                OnResize();
            }
            else if (wParam == SIZE_RESTORED)
            {

                // Restoring from minimized state?
                if (m_minimized)
                {
                    m_appPaused = false;
                    m_minimized = false;
                    OnResize();
                }
                else if (m_maximized)
                {
                    // Restoring from maximized state?
                    m_appPaused = false;
                    m_maximized = false;
                    OnResize();
                }
                else if (m_resizing)
                {
                    // If user is dragging the resize bars, we do not resize 
                    // the buffers here because as the user continuously 
                    // drags the resize bars, a stream of WM_SIZE messages are
                    // sent to the window, and it would be pointless (and slow)
                    // to resize for each WM_SIZE message received from dragging
                    // the resize bars.  So instead, we reset after the user is 
                    // done resizing the window and releases the resize bars, which 
                    // sends a WM_EXITSIZEMOVE message.
                }
                else // API call such as SetWindowPos or mSwapChain->SetFullscreenState.
                {
                    OnResize();
                }
            }
        }
        break;
    }
    case WM_ENTERSIZEMOVE:
    {
        break;
    }
    case WM_EXITSIZEMOVE:
    {
        break;
    }
    case WM_MENUCHAR:
    {
        break;
    }
    case WM_GETMINMAXINFO:
    {
        break;
    }
    case WM_MBUTTONDOWN:
    case WM_RBUTTONDOWN:
    case WM_LBUTTONDOWN:
    {
        OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        break;
    }
    case WM_LBUTTONUP:
    case WM_MBUTTONUP:
    case WM_RBUTTONUP:
    {
        OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        break;
    }
    case WM_MOUSEMOVE:
    {
        OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
        break;
    }
    case WM_KEYDOWN:
        if (wParam == VK_ESCAPE)
        {
            DestroyWindow(hwnd);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        result = DefWindowProcW(hwnd, msg, wParam, lParam);
        break;
    }

    return result;
}

bool BaseApp::InitMainWindow()
{
    bool result = false;

    WNDCLASSW wc        = {};
    wc.style            = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc      = MainWndProc;
    wc.hInstance        = mhAppInst;
    wc.hIcon            = LoadIcon(0, IDI_APPLICATION);
    wc.hCursor          = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground    = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName     = 0;
    wc.lpszClassName    = L"BaseAppWindow";

    result = RegisterClassW(&wc);
    if (result == true) {
        mhMainWnd = CreateWindowW(L"BaseAppWindow",
                                  L"D3D init",
                                  WS_OVERLAPPEDWINDOW,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  CW_USEDEFAULT,
                                  0,
                                  0,
                                  mhAppInst,
                                  0);

        if (mhMainWnd == 0)
        {
            MessageBoxW(0, L"CreateWindow FAILED", 0, 0);
            result = false;
        }
        else
        {
            ShowWindow(mhMainWnd, SW_SHOW);
            UpdateWindow(mhMainWnd);
        }
    }
    else
    {
        MessageBoxW(0, L"RegsiterClass FAILED", 0, 0);
    }

    return result;
}

bool BaseApp::InitDirect3D()
{
    bool ret = true;
#if defined(DEBUG) || defined(_DEBUG) 
    {
        // Enable the D3D12 debug layer.
        ComPtr<ID3D12Debug> debugController;
        ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
        debugController->EnableDebugLayer();
    }
#endif

    HRESULT result = CreateDXGIFactory(IID_PPV_ARGS(&m_dxgiFactory));

    if (SUCCEEDED(result))
    {
#if _DEBUG
        LogAdapters();
#endif
        // Try to create hardware device.
        HRESULT hardwareResult = D3D12CreateDevice(nullptr,                      // default adapter
                                                   D3D_FEATURE_LEVEL_11_0,
                                                   IID_PPV_ARGS(&m_d3dDevice));

        if (SUCCEEDED(hardwareResult))
        {
            ThrowIfFailed(m_d3dDevice->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));

            m_rtvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
            m_dsvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);
            m_cbvSrvUavDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

            D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
            msQualityLevels.Format           = m_backBufferFormat;
            msQualityLevels.SampleCount      = 4;
            msQualityLevels.Flags            = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
            msQualityLevels.NumQualityLevels = 0;
            ThrowIfFailed(m_d3dDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS,
                                                           &msQualityLevels,
                                                           sizeof(msQualityLevels)));

            m_4xMsaaQuality = msQualityLevels.NumQualityLevels;
            assert(m_4xMsaaQuality > 0 && "Unexpected MSAA quality level.");

            CreateCommandObjects();
            CreateSwapChain();
            CreateRtvAndDsvDescriptorHeaps();
        }
        else
        {
            // No fallback to WARD adapter.
            MessageBoxW(0, L"Device create failed!", 0, 0);
            ret = false;
        }
    }
    else
    {
        MessageBoxW(0, L"Dxgi factory create failed!", 0, 0);
        ret = false;
    }
    
    return ret;
}

void BaseApp::CreateSwapChain()
{
    // Release the previous swapchain we will be recreating.
    m_swapChain.Reset();

    DXGI_SWAP_CHAIN_DESC sd;
    sd.BufferDesc.Width                   =  m_clientWidth;
    sd.BufferDesc.Height                  = m_clientHeight;
    sd.BufferDesc.RefreshRate.Numerator   = 60;
    sd.BufferDesc.RefreshRate.Denominator = 1;
    sd.BufferDesc.Format                  = m_backBufferFormat;
    sd.BufferDesc.ScanlineOrdering        = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
    sd.BufferDesc.Scaling                 = DXGI_MODE_SCALING_UNSPECIFIED;
    sd.SampleDesc.Count                   = m_4xMsaaEn ? 4 : 1;
    sd.SampleDesc.Quality                 = m_4xMsaaEn ? (m_4xMsaaQuality - 1) : 0;
    sd.BufferUsage                        = DXGI_USAGE_RENDER_TARGET_OUTPUT;
    sd.BufferCount                        = SwapChainBufferCount;
    sd.OutputWindow                       = mhMainWnd;
    sd.Windowed                           = true;
    sd.SwapEffect                         = DXGI_SWAP_EFFECT_FLIP_DISCARD;
    sd.Flags                              = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

    // Note: Swap chain uses queue to perform flush.
    ThrowIfFailed(m_dxgiFactory->CreateSwapChain(m_commandQueue.Get(),
                                                 &sd,
                                                 m_swapChain.GetAddressOf()));
}

void BaseApp::FlushCommandQueue()
{
    m_currentFence++;

    ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), m_currentFence));
    
    // Wait until the GPU has completed commands up to this fence point.
    if (m_fence->GetCompletedValue() < m_currentFence)
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);

        // Fire event when GPU hits current fence.  
        ThrowIfFailed(m_fence->SetEventOnCompletion(m_currentFence, eventHandle));

        // Wait until the GPU hits current fence event is fired.
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

void BaseApp::CalculateFrameStats()
{
}

void BaseApp::CreateCommandObjects()
{
    D3D12_COMMAND_QUEUE_DESC queueDesc = {};
    queueDesc.Type                     = D3D12_COMMAND_LIST_TYPE_DIRECT;
    queueDesc.Flags                    = D3D12_COMMAND_QUEUE_FLAG_NONE;

    ThrowIfFailed(m_d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));

    ThrowIfFailed(m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                      IID_PPV_ARGS(m_directCmdListAlloc.GetAddressOf())));

    ThrowIfFailed(m_d3dDevice->CreateCommandList(0,
                                                 D3D12_COMMAND_LIST_TYPE_DIRECT,
                                                 m_directCmdListAlloc.Get(), // Associated command allocator
                                                 nullptr,                   // Initial PipelineStateObject
                                                 IID_PPV_ARGS(m_commandList.GetAddressOf())));

    // Start off in a closed state.  This is because the first time we refer 
    // to the command list we will Reset it, and it needs to be closed before
    // calling Reset.
    m_commandList->Close();
}

void BaseApp::CreateRtvAndDsvDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc;
    rtvHeapDesc.NumDescriptors = SwapChainBufferCount;
    rtvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
    rtvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    rtvHeapDesc.NodeMask       = 0;
    ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(m_rtvHeap.GetAddressOf())));

    D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc;
    dsvHeapDesc.NumDescriptors = 1;
    dsvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
    dsvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
    dsvHeapDesc.NodeMask       = 0;
    ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(m_dsvHeap.GetAddressOf())));
}

ID3D12Resource * BaseApp::CurrentBackBuffer() const
{
    return m_swapChainBuffer[m_currBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE BaseApp::CurrentBackBufferView() const
{
    return CD3DX12_CPU_DESCRIPTOR_HANDLE(m_rtvHeap->GetCPUDescriptorHandleForHeapStart(),
                                         m_currBackBuffer,
                                         m_rtvDescriptorSize);
}

D3D12_CPU_DESCRIPTOR_HANDLE BaseApp::DepthStencilView() const
{
    return m_dsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void BaseApp::LogAdapters()
{
    UINT i = 0;
    IDXGIAdapter* pAdapter = nullptr;
    std::vector<IDXGIAdapter*> adapterList;

    std::wstring text = L"***Adapter(s):\n";
    while (m_dxgiFactory->EnumAdapters(i, &pAdapter) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_ADAPTER_DESC desc;
        pAdapter->GetDesc(&desc);

        text += desc.Description;
        text += L"\n";

        adapterList.push_back(pAdapter);
        ++i;
    }
    //MessageBoxW(0, text.c_str(), 0, 0);
    OutputDebugStringW(text.c_str());

    for (auto pAdp : adapterList)
    {
        if (pAdp)
        {
            LogDisplays(pAdp);
            util::ReleaseComObj(pAdp);
        }
    }
}

void BaseApp::LogDisplays(IDXGIAdapter* pAdapter)
{
    UINT i = 0;
    IDXGIOutput* pOutput = nullptr;
    while (pAdapter->EnumOutputs(i, &pOutput) != DXGI_ERROR_NOT_FOUND)
    {
        DXGI_OUTPUT_DESC desc;
        pOutput->GetDesc(&desc);
        std::wstring text = L"***Display: ";
        text += desc.DeviceName;
        text += L"\n";
        OutputDebugStringW(text.c_str());
        LogDisplayModes(pOutput, DXGI_FORMAT_B8G8R8A8_UNORM);
        util::ReleaseComObj(pOutput);
        ++i;
    }
}

void BaseApp::LogDisplayModes(IDXGIOutput* pOutput, DXGI_FORMAT format)
{
    UINT count = 0;
    UINT flags = 0;
    pOutput->GetDisplayModeList(format, flags, &count, nullptr);
    std::vector<DXGI_MODE_DESC> modeListVec(count);
    pOutput->GetDisplayModeList(format, flags, &count, &modeListVec[0]);

    for (auto& desc : modeListVec)
    {
        UINT n = desc.RefreshRate.Numerator;
        UINT d = desc.RefreshRate.Denominator;
        std::wstring text =
            L"Width = " + std::to_wstring(desc.Width) + L" " +
            L"Height = " + std::to_wstring(desc.Height) + L" " +
            L"Refresh = " + std::to_wstring(n) + L"/" + std::to_wstring(d) +
            L"\n";
        OutputDebugStringW(text.c_str());
    }
}

void BaseApp::CheckFeatureLevels()
{
    D3D_FEATURE_LEVEL featureLevels[3] =
    {
        D3D_FEATURE_LEVEL_12_1,
        D3D_FEATURE_LEVEL_12_0,
        D3D_FEATURE_LEVEL_11_0
    };

    D3D12_FEATURE_DATA_FEATURE_LEVELS featureLevelsInfo;
    featureLevelsInfo.NumFeatureLevels = 3;
    featureLevelsInfo.pFeatureLevelsRequested = featureLevels;
    // needs a device
}