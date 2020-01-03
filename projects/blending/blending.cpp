#include <iostream>
#include <vector>
#include "DirectXColors.h"
#include "windows.h"
#include "BaseApp.h"

using namespace std;
using Microsoft::WRL::ComPtr;
using namespace DirectX;

// Current demo class to handle user input and to setup demo-specific resource and commands.
class BlendApp : public BaseApp
{
public:
  BlendApp(HINSTANCE hInst);
  ~BlendApp();

  // Delete copy and assign.
  BlendApp(const BlendApp& app) = delete;
  BlendApp& operator=(const BlendApp& app) = delete;

  bool Initialize() override;

private:
  virtual void Update(const BaseTimer& timer) override;
  virtual void Draw(const BaseTimer& timer) override;

  virtual void OnMouseDown(WPARAM btnState, int x, int y) override { }
  virtual void OnMouseUp(WPARAM btnState, int x, int y) override { }
  virtual void OnMouseMove(WPARAM btnState, int x, int y) override { }
  virtual void OnKeyDown(WPARAM wparam) override {}
};

// Demo constructor.
BlendApp::BlendApp(HINSTANCE h_inst)
  :
  BaseApp(h_inst)
{

}

// Destructor flushes command queue
BlendApp::~BlendApp()
{
  if (m_d3dDevice != nullptr){
    FlushCommandQueue();
  }
}

// Calls base app to set up windows and d3d and this demo's resources
bool BlendApp::Initialize()
{
  bool ret = BaseApp::Initialize();

  if (ret == true) {
    ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), nullptr));
    m_cbvSrvUavDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    // Submit the initialization commands.
    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* cmdLists[] =  { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
    FlushCommandQueue();

  } else {
    ::OutputDebugStringA("Error! - failed blend app initialize\n");
  }

  return ret;
}

// Updates resource state for a frame.
void BlendApp::Update(const BaseTimer& timer)
{
}

// Writes draw commands for a frame.
void BlendApp::Draw(const BaseTimer& timer)
{
  ThrowIfFailed(m_directCmdListAlloc->Reset());
  ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), nullptr));

  m_commandList->RSSetViewports(1, &m_screenViewport);
  m_commandList->RSSetScissorRects(1, &m_scissorRect);

  // Transition the back buffer so we can render to it.
  m_commandList->ResourceBarrier(1,
                                 &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
                                                                       D3D12_RESOURCE_STATE_PRESENT,
                                                                       D3D12_RESOURCE_STATE_RENDER_TARGET));

  // Clear the render target and depth stencil buffers.
  m_commandList->ClearRenderTargetView(CurrentBackBufferView(),
                                       DirectX::Colors::Lavender,
                                       0,
                                       nullptr);
  m_commandList->ClearDepthStencilView(DepthStencilView(),
                                       D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                                       1.0f,
                                       0,
                                       0,
                                       nullptr);

  m_commandList->OMSetRenderTargets(1,                        // Num render target descriptors
                                    &CurrentBackBufferView(), // handle to rt
                                    true,                     // descriptors are contiguous
                                    &DepthStencilView());     // handle to ds

  // Transition the render target back to be presentable.
  m_commandList->ResourceBarrier(1,
                                 &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
                                                                       D3D12_RESOURCE_STATE_RENDER_TARGET,
                                                                       D3D12_RESOURCE_STATE_PRESENT));

  ThrowIfFailed(m_commandList->Close());
  ID3D12CommandList* command_lists[] = { m_commandList.Get() };
  m_commandQueue->ExecuteCommandLists(_countof(command_lists), command_lists);

  // Present current swap chain buffer and advance to next buffer.
  ThrowIfFailed(m_swapChain->Present(0, 0));
  m_currBackBuffer = (m_currBackBuffer + 1) % SwapChainBufferCount;

  //m_commandQueue->Signal(m_fence.Get(), m_currentFence);
}

// Windows main function to setup and go into Run() loop.
int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   PSTR      pCmdLine,
                   int       nShowCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

  auto ret_code = 0;
  BlendApp demo_app(hInstance);

  if (demo_app.Initialize() == true) {
    ret_code = demo_app.Run();
  }

  return ret_code;
}

/**

Blending demo agenda:
:- swapchain buffer already created in BaseApp
- create render target and clear to color
- Frame resources
- triple buffering
- Draw a textured crate
- Draw animated water
- Draw Mountains
- water blends with other elements

**/
