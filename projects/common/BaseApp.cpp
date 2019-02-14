#include <WindowsX.h>
#include "BaseApp.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

BaseApp::BaseApp(HINSTANCE hInstance)
    : mhAppInst(hInstance)
{
    mApp = this;
}