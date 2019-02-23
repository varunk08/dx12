#include <Windows.h>
#include "BaseApp.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

BaseApp::BaseApp(HINSTANCE hInstance)
    : mhAppInst(hInstance)
{
    //mApp = this;
}

void BaseApp::OnResize()
{
}

int BaseApp::Run()
{
    return 0;
}

bool BaseApp::Initialize()
{
    return false;
}

LRESULT BaseApp::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
    return LRESULT();
}
