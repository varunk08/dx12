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
    virtual void OnMouseDown(WPARAM btnState, int x, int y);
    virtual void OnMouseUp(WPARAM btnState, int x, int y);
    virtual void OnMouseMove(WPARAM btnState, int x, int y);

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
    BaseApp::OnResize();
}

void InitD3dApp::Update(const BaseTimer & timer)
{
}

void InitD3dApp::Draw(const BaseTimer & timer)
{
    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(m_directCmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), nullptr));

    // Indicate a state transition on the resource usage.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
                                   D3D12_RESOURCE_STATE_PRESENT,
                                   D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Set the viewport and scissor rect.  This needs to be reset whenever the command list is reset.
    m_commandList->RSSetViewports(1, &m_screenViewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Clear the back buffer and depth buffer.
    m_commandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::MediumVioletRed, 0, nullptr);
    m_commandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    m_commandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    // Indicate a state transition on the resource usage.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    ThrowIfFailed(m_commandList->Close());

    // Add the command list to the queue for execution.
    ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

    // swap the back and front buffers
    ThrowIfFailed(m_swapChain->Present(0, 0));
    m_currBackBuffer = (m_currBackBuffer + 1) % SwapChainBufferCount;

    // Wait until frame commands are complete.  This waiting is inefficient and is
    // done for simplicity.  Later we will show how to organize our rendering code
    // so we do not have to wait per frame.
    FlushCommandQueue();
}

void InitD3dApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    std::wstring time = std::to_wstring(m_timer.TotalTimeInSecs());
    time += std::wstring(L" seconds since start of app.");
    MessageBoxW(0, time.c_str(), L"Hello", MB_OK);
}

void InitD3dApp::OnMouseUp(WPARAM btnState, int x, int y)
{
}

void InitD3dApp::OnMouseMove(WPARAM btnState, int x, int y)
{
}

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   PSTR      pCmdLine,
                   int       nShowCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

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