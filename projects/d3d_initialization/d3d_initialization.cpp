#include <iostream>
#include <Windows.h>
#include <DirectXColors.h>

#include "../common/BaseApp.h"

using namespace std;
using namespace DirectX;

class InitD3dApp : public BaseApp
{
public:
    InitD3dApp(HINSTANCE hInstance);
    ~InitD3dApp();

    virtual bool Initialize() override;
private:
    virtual void OnResize() override;
    virtual void Update(const BaseTimer& timer) override;
    virtual void Draw(const BaseTimer& timer) override;

};

InitD3dApp::InitD3dApp(
    HINSTANCE hInstance)
    :
    BaseApp(hInstance)
{
}

InitD3dApp::~InitD3dApp()
{
}

bool InitD3dApp::Initialize()
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

void InitD3dApp::OnResize()
{
}

void InitD3dApp::Update(const BaseTimer & timer)
{
}

void InitD3dApp::Draw(const BaseTimer & timer)
{
}

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   PSTR      pCmdLine,
                   int       nShowCmd)
{
    InitD3dApp theApp(hInstance);
    int retCode = 0;

    if (theApp.Initialize() == true)
    {
        retCode = theApp.Run();
    }
    else
    {
        cout << "Error Initializing the app" << endl;
    }

    return retCode;
}