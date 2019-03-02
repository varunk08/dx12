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
    return false;
}

LRESULT BaseApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    LRESULT result = 0;
    switch (msg)
    {
    case WM_LBUTTONDOWN:
    {
        std::wstring time = std::to_wstring(m_timer.TotalTimeInSecs());
        time += std::wstring(L" seconds since start of app.");
        MessageBoxW(0, time.c_str(), L"Hello", MB_OK);
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

    WNDCLASSW wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = MainWndProc;
    wc.hInstance = mhAppInst;
    wc.hIcon = LoadIcon(0, IDI_APPLICATION);
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = L"BaseAppWindow";

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

    HRESULT result = CreateDXGIFactory(IID_PPV_ARGS(&m_dxgiFactory));

    if (SUCCEEDED(result))
    {
        LogAdapters();
        // Try to create hardware device.
        HRESULT hardwareResult = D3D12CreateDevice(
            nullptr,             // default adapter
            D3D_FEATURE_LEVEL_11_0,
            IID_PPV_ARGS(&m_d3dDevice));

        if (SUCCEEDED(hardwareResult))
        {

        }
        else
        {
            MessageBoxW(0, L"Device create failed!", 0, 0);
        }
    }
    else
    {
        MessageBoxW(0, L"Dxgi factory create failed!", 0, 0);
    }
    
    return ret;
}

void BaseApp::CreateSwapChain()
{
}

void BaseApp::FlushCommandQueue()
{
}

void BaseApp::CalculateFrameStats()
{
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