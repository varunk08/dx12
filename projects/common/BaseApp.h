#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl.h>
#include <DirectXMath.h>

#include <iostream>
#include <string>
#include <vector>

#include "BaseUtil.h"
#include "BaseTimer.h"

// Link necessary d3d12 libraries.
#pragma comment(lib,"d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")

class BaseApp
{
public:
    static BaseApp* GetApp();

    HINSTANCE AppInst() const;
    HWND      MainWnd() const;
    float     AspectRatio() const;

    int Run();

    virtual bool Initialize();
    virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);

protected:
    BaseApp(HINSTANCE hInstance);
    BaseApp(const BaseApp& rhs) = delete;

    virtual void OnResize();
    virtual void Update(const BaseTimer& gt) = 0;
    virtual void Draw(const BaseTimer& gt) = 0;

    bool InitMainWindow();
    bool InitDirect3D();
    void CreateSwapChain();
    void FlushCommandQueue();

    static BaseApp* mApp;

    HINSTANCE mhAppInst = nullptr;
    HWND      mhMainWnd = nullptr; // main window handle
    Microsoft::WRL::ComPtr<IDXGIFactory4> m_dxgiFactory;

private:
    void LogAdapters();
    void LogDisplays(IDXGIAdapter* pAdapter);
    void LogDisplayModes(IDXGIOutput* pOutput, DXGI_FORMAT format);
    void CheckFeatureLevels();
};