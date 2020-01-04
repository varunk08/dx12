#include <iostream>
#include <vector>
#include "DirectXColors.h"
#include "windows.h"
#include "BaseApp.h"

using namespace std;
using Microsoft::WRL::ComPtr;
using namespace DirectX;

struct FrameResource
{
  
};

// Represents all parameters of a vertex in DirectX compatible formats.
struct DxVertex
{
  DxVertex() {}
  DxVertex(const DirectX::XMFLOAT3& pos,
           const DirectX::XMFLOAT3& nor,
           const DirectX::XMFLOAT3& tan,
           const DirectX::XMFLOAT2& uv)
    :
    position_(pos),
    normal_(nor),
    tangentU_(tan),
    texC_(uv)
  { }
  DxVertex(float px, float py, float pz,
           float nx, float ny, float nz,
           float tx, float ty, float tz,
           float u, float v)
    :
    position_(px, py, pz),
    normal_(nx, ny, nz),
    tangentU_(tx, ty, tz),
    texC_(u, v)
  { }
  
  DirectX::XMFLOAT3 position_;
  DirectX::XMFLOAT3 normal_;
  DirectX::XMFLOAT3 tangentU_;
  DirectX::XMFLOAT2 texC_;
};

// The vertex shader expects vertex data in this layout.
struct ShaderVertex
{
  DirectX::XMFLOAT3 pos_;
  DirectX::XMFLOAT3 nor_;
  DirectX::XMFLOAT2 tex_;
};

// Represents a complete mesh that can be rendered.
struct MeshData
{

  MeshData() {}
  MeshData(const MeshData& other)
  {
    vertices_.resize(other.vertices_.size());
    indices32_.resize(other.indices32_.size());

    vertices_.assign(other.vertices_.cbegin(), other.vertices_.cend());
    indices32_.assign(other.indices32_.cbegin(), other.indices32_.cend());
  }

  // Gets indices as uint16
  std::vector<uint16_t> GetIndices16()
  {
    if (indices16_.empty()) {
        indices16_.resize(indices32_.size());

        for (int i = 0; i < indices16_.size(); i++) {
            indices16_[i] = static_cast<uint16_t>(indices32_[i]);
          }
      }

    return indices16_;
  }
  
  std::vector<DxVertex> vertices_;
  std::vector<uint32_t> indices32_;

private:  
  std::vector<uint16_t> indices16_;
};

// Class to generate geometry for the demo.
class GeometryGenerator
{
public:
  GeometryGenerator() {}
  MeshData CreateGrid(float width, float depth, uint32_t m, uint32_t n);
  
private:
};
  
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

  void BuildTerrainGeometry();
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

    BuildTerrainGeometry();
    
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
  if ((m_currentFence != 0) && (m_fence->GetCompletedValue() < m_currentFence)) {
      HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
      ThrowIfFailed(m_fence->SetEventOnCompletion(m_currentFence, eventHandle));
      WaitForSingleObject(eventHandle, INFINITE);
      CloseHandle(eventHandle);
  }
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
                                       DirectX::Colors::Indigo,
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

  ++m_currentFence;
  m_commandQueue->Signal(m_fence.Get(), m_currentFence);
}

// Function to generate terrain geometry
void BlendApp::BuildTerrainGeometry()
{
  GeometryGenerator geoGen;
  MeshData grid = geoGen.CreateGrid(160.0f, 160.0f, 50, 50);

  std::vector<ShaderVertex> vertices(grid.vertices_.size());
  for (size_t i = 0; i < grid.vertices_.size(); ++i) {
    auto& p = grid.vertices_[i].position_;
    vertices[i].pos_ = p;
    vertices[i].nor_ = grid.vertices_[i].normal_;
    vertices[i].tex_ = grid.vertices_[i].texC_;
  }

  const UINT vbByteSize = static_cast<UINT>(vertices.size() * sizeof(ShaderVertex));

  std::vector<std::uint16_t> indices = grid.GetIndices16();
  const UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(uint16_t));
  
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


// Implementations of helper classes and functions.
MeshData GeometryGenerator::CreateGrid(float width, float depth, uint32_t m, uint32_t n)
{
  MeshData meshData;
  uint32_t vertexCount = m * n;
  uint32_t faceCount = (m - 1) * (n - 1) * 2;

  float halfWidth = 0.5f * width;
  float halfDepth = 0.5f * depth;

  float dx = width / (n - 1);
  float dz = depth / (m - 1);

  float du = 1.0f / (n - 1);
  float dv = 1.0f / (m - 1);

  // Create the vertices.
  meshData.vertices_.resize(vertexCount);
  for (uint32_t i = 0; i < m; i++) {
    float z = halfDepth - i * dz;

    for (uint32_t j = 0; j < n; j++) {
      float x = -halfWidth  + j * dx;

      auto index = i * n + j;
      meshData.vertices_[index].position_ = XMFLOAT3(x, 0.0f, z);
      meshData.vertices_[index].normal_   = XMFLOAT3(0.0f, 1.0f, 0.0f);
      meshData.vertices_[index].tangentU_ = XMFLOAT3(1.0f, 0.0f, 0.0f);

      meshData.vertices_[index].texC_.x = j * du;
      meshData.vertices_[index].texC_.y = i * dv;
    }
  }

  // Create the indices.
  meshData.indices32_.resize(faceCount * 3);
  uint32_t k = 0;
  for (uint32_t i = 0; i < m - 1; i++) {
    for (uint32_t j = 0; j < n - 1; ++j) {

      // Triangle 1.
      meshData.indices32_[k]   = i * n + j;
      meshData.indices32_[++k] = i * n + j + 1;
      meshData.indices32_[++k] = (i + 1) * n + j;

      // Triangle 2.
      meshData.indices32_[++k] = (i + 1) * n + j;
      meshData.indices32_[++k] = i * n + j + 1;
      meshData.indices32_[++k] = (i + 1) * n + j + 1;

      ++k;
    }
  }

  return meshData;
}

/**

Blending demo agenda:
:- swapchain buffer already created in BaseApp
:- create render target and clear to color, buffers created by BaseApp
:- single fence for proper wait until reset
- Draw a single grid with mouse movements
  - create pipeline with shaders
  - create geometry
- Frame resources, for rendering one full frame
- triple buffering
- Draw a textured crate
- Draw animated water
- Draw Mountains
- water blends with other elements

**/
