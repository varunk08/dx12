#include <iostream>
#include <vector>
#include <array>
#include "DirectXColors.h"
#include "../common/BaseUtil.h"
#include "../common/BaseApp.h"
#include "../common/GeometryGenerator.h"
#include "../common/UploadBuffer.h"
#include "../common/DDSTextureLoader.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

// Our stenciling demo app, derived from the BaseApp ofcourse.
class GSDemo final : public BaseApp
{
public:
  // Delete default and copy constructors, and copy operator.
  GSDemo& operator=(const GSDemo& other) = delete;
  GSDemo(const GSDemo& other) = delete;
  GSDemo() = delete;

  // Constructor.
  GSDemo(HINSTANCE hInstance)
    :
    BaseApp(hInstance)
  {
  }

  // Destructor.
  ~GSDemo()
  {
    if (m_d3dDevice != nullptr) {
        FlushCommandQueue();
    }
  }

  virtual void OnResize()override
  {
      BaseApp::OnResize();
  }

  void UpdateObjectCBs()
  {

  }
  void UpdateCamera(const BaseTimer& gt)
  {
  }

  void UpdatePassCB()
  {
  }

  virtual void Update(const BaseTimer& gt)override
  {
      if ((m_currentFence != 0) && (m_fence->GetCompletedValue() < m_currentFence)) {
          HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
          ThrowIfFailed(m_fence->SetEventOnCompletion(m_currentFence, eventHandle));
          WaitForSingleObject(eventHandle, INFINITE);
          CloseHandle(eventHandle);
      }
  }

  virtual void Draw(const BaseTimer& gt)override
  {
      ThrowIfFailed(m_directCmdListAlloc->Reset());
      ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), nullptr));

      m_commandList->RSSetViewports(1, &m_screenViewport);
      m_commandList->RSSetScissorRects(1, &m_scissorRect);

      // Transition the back buffer so we can render to it.
      m_commandList->ResourceBarrier(1,
        &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

      m_commandList->ClearRenderTargetView(CurrentBackBufferView(),
                                           DirectX::Colors::Gray,
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
                                     &CD3DX12_RESOURCE_BARRIER::Transition(
                                         CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

      ThrowIfFailed(m_commandList->Close());
      ID3D12CommandList* command_lists[] = { m_commandList.Get() };
      m_commandQueue->ExecuteCommandLists(_countof(command_lists), command_lists);

      // Present current swap chain buffer and advance to next buffer.
      ThrowIfFailed(m_swapChain->Present(0, 0));
      m_currBackBuffer = (m_currBackBuffer + 1) % SwapChainBufferCount;

      ++m_currentFence;
      m_commandQueue->Signal(m_fence.Get(), m_currentFence);
  }

  virtual void OnMouseDown(WPARAM btnState, int x, int y)override
  {
  }

  virtual void OnMouseUp(WPARAM btnState, int x, int y)override
  {
  }

  virtual void OnMouseMove(WPARAM btnState, int x, int y)override
  {
  }

  void LoadTextures()
  {
  }

  void BuildSceneGeometry()
  {

  }

  void BuildRenderObjects()
  {
  }

  void BuildMaterials()
  {
  }

  void BuildShadersAndInputLayout()
  {

  }

  void BuildRootSignature()
  {
  }

  // Builds the constant buffers etc needed in the shader program.
  void InitShaderResources()
  {

  }

  // Build descriptor heaps and shader resource views for our textures.
  void BuildShaderViews()
  {
  }

  // Builds shaders, descriptors and pipelines.
  void BuildPipelines()
  {

  }

  virtual bool Initialize() override
  {
      bool res = BaseApp::Initialize();

      if (res) {
          ThrowIfFailed(m_commandList.Get()->Reset(m_directCmdListAlloc.Get(), nullptr));

          BuildSceneGeometry();
          BuildPipelines();

          // After writing the commands needed to build resources, which involves uploading
          // some of them to GPU memory, submit the commands.
          ThrowIfFailed(m_commandList->Close());
          ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
          m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
          FlushCommandQueue(); // Queue-submit.
      }
      else {
          ::OutputDebugStringA("Error initializing the geometry shader demo!\n");
      }

      return res;
  }

  // Member variables.
}; // Class StencilDemo

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR cmdLine, int showCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

  try
    {
      GSDemo demoApp(hInstance);
      if (demoApp.Initialize() == false)
        {
          return 0;
        }
      return demoApp.Run();
    }
  catch (DxException& e)
    {
      MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
      return 0;
    }
}


/************************************************************************

Geometry shader app demo:
- simple first step: renders a set of points as quads

- build pipeline
- build scene geometry, of some points
- build command buffer, implement draw function

 ***********************************************************************/
