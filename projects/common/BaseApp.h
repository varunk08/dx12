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
    void CalculateFrameStats();
    void CreateCommandObjects();
    void CreateRtvAndDsvDescriptorHeaps();

    static BaseApp* mApp;
    static const int SwapChainBufferCount = 2;

    // App properties
    BaseTimer m_timer;
    bool m_appPaused = false;

    // Windows and d3d app stuff.
    HINSTANCE mhAppInst = nullptr;
    HWND      mhMainWnd = nullptr; // main window handle
    Microsoft::WRL::ComPtr<IDXGIFactory4>             m_dxgiFactory;
    Microsoft::WRL::ComPtr<ID3D12Device>              m_d3dDevice;
    Microsoft::WRL::ComPtr<ID3D12Fence>               m_fence;
    Microsoft::WRL::ComPtr<ID3D12CommandQueue>        m_commandQueue;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator>    m_directCmdListAlloc;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> m_commandList;
    Microsoft::WRL::ComPtr<IDXGISwapChain>            m_swapChain;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>      m_rtvHeap;
    Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>      m_dsvHeap;

    UINT64 m_cbvSrvUavDescriptorSize = 0;
    UINT64 m_dsvDescriptorSize   = 0;
    UINT64 m_rtvDescriptorSize   = 0;
    UINT64 m_currentFence = 0;
    DXGI_FORMAT m_backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    UINT       m_4xMsaaQuality = 0;      // quality level of 4X MSAA
    bool      m_4xMsaaEn = false;    // 4X MSAA enabled

    int m_clientWidth = 800;
    int m_clientHeight = 600;

private:
    void LogAdapters();
    void LogDisplays(IDXGIAdapter* pAdapter);
    void LogDisplayModes(IDXGIOutput* pOutput, DXGI_FORMAT format);
    void CheckFeatureLevels();
};