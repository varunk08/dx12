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
    BOOL bRet = 1;
    while ((bRet = GetMessage(&msg, 0, 0, 0)) != 0)
    {
        if (bRet == -1)
        {
            MessageBoxW(0, L"GetMessage FAILED", L"Error", MB_OK);
            break;
        }
        else
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
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
        MessageBoxW(0, L"Hello World", L"Hello", MB_OK);
        break;
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
    return true;
}
