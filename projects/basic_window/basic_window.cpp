#pragma comment(linker, "/subsystem:windows")
#include <iostream>
#include <Windows.h>

using namespace std;

LRESULT CALLBACK WndProc(HWND hWnd,
                         UINT msg,
                         WPARAM wParam,
                         LPARAM lParam)
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
            DestroyWindow(hWnd);
        }
        break;
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        result = DefWindowProc(hWnd, msg, wParam, lParam);
        break;
    }

    return result;
}

bool InitWindowsApp(HINSTANCE hInstance,
                    int       nShowCmd,
                    HWND&     hWnd)
{
    bool result = false;

    WNDCLASSW wc = {};
    wc.style = CS_HREDRAW | CS_VREDRAW;
    wc.lpfnWndProc = WndProc;
    wc.hInstance = hInstance;
    wc.hIcon = LoadIcon(0, IDI_APPLICATION);
    wc.hCursor = LoadCursor(0, IDC_ARROW);
    wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);
    wc.lpszMenuName = 0;
    wc.lpszClassName = L"BasicWndClass";

    result = RegisterClassW(&wc);
    if (result == true) {
        hWnd = CreateWindowW(L"BasicWndClass",
                             L"Win32Basic",
                             WS_OVERLAPPEDWINDOW,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             CW_USEDEFAULT,
                             0,
                             0,
                             hInstance,
                             0);

        if (hWnd == 0)
        {
            MessageBoxW(0, L"CreateWindow FAILED", 0, 0);
            result = false;
        }
        else
        {
            ShowWindow(hWnd, nShowCmd);
            UpdateWindow(hWnd);
        }

    }
    else
    {
        MessageBoxW(0, L"RegsiterClass FAILED", 0, 0);
    }

    return result;
}

int Run(HWND& hWnd)
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

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   PSTR      pCmdLine,
                   int       nShowCmd)
{
    int retCode = 0;
    HWND hMainWnd = 0;
    if (InitWindowsApp(hInstance, nShowCmd, hMainWnd) == true) {
        Run(hMainWnd);
    }

    return retCode;
}