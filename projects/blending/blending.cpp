#include <iostream>
#include <vector>
#include <unordered_map>
#include "DirectXColors.h"
#include "windows.h"
#include "BaseApp.h"
#include "../common/BaseUtil.h"
#include "../common/MathHelper.h"
#include "../common/UploadBuffer.h"

using namespace std;
using Microsoft::WRL::ComPtr;
using namespace DirectX;

// This will be referenced by the shader as a const buffer.
struct PassConstants
{
  DirectX::XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
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
  void OnResize() override;
  void Update(const BaseTimer& timer) override;
  void Draw(const BaseTimer& timer) override;

  virtual void OnMouseDown(WPARAM btnState, int x, int y) override { }
  virtual void OnMouseUp(WPARAM btnState, int x, int y) override { }
  virtual void OnMouseMove(WPARAM btnState, int x, int y) override { }
  virtual void OnKeyDown(WPARAM wparam) override {}

  void BuildTerrainGeometry();
  void BuildInputLayout();
  void BuildShaders();
  void BuildPipelines();
  void BuildDescriptorHeaps();
  void BuildConstBufferViews();
  
  std::vector<D3D12_INPUT_ELEMENT_DESC>                        inputLayout_;
  std::unordered_map<std::string, ComPtr<ID3DBlob>>            shaders_;
  ComPtr<ID3D12RootSignature>                                  rootSign_ = nullptr;
  std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> pipelines_;
  ComPtr<ID3D12DescriptorHeap>                                 cbvHeap_;
  std::unique_ptr<UploadBuffer<PassConstants>>                 passCb_ = nullptr;     // Stores the MVP matrices etc.

  // Matrices
  XMFLOAT4X4 view_ = MathHelper::Identity4x4();
  XMFLOAT4X4 proj_ = MathHelper::Identity4x4();

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
    BuildInputLayout();
    BuildShaders();
    BuildDescriptorHeaps();
    BuildConstBufferViews();
  
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
  // Update view matrix
  float phi = 0.2f * XM_PI;
  float theta = 1.5f * XM_PI;
  float radius =  15.0f;
  
  XMFLOAT3 eye_pos = XMFLOAT3(radius * sinf(phi) * cosf(theta),
                              radius * cosf(phi),
                              radius * sinf(phi) * sinf(theta));
  
  XMVECTOR pos = XMVectorSet(eye_pos.x, eye_pos.y, eye_pos.z, 1.0f);
  XMVECTOR target = XMVectorZero();
  XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
  
  XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
  XMStoreFloat4x4(&view_, view);

  XMMATRIX view_proj = XMMatrixMultiply(XMLoadFloat4x4(&view_), XMLoadFloat4x4(&proj_));
  PassConstants pass_const = {};
  XMStoreFloat4x4(&pass_const.ViewProj, XMMatrixTranspose(view_proj));
  passCb_->CopyData(0, pass_const);
  
  if ((m_currentFence != 0) && (m_fence->GetCompletedValue() < m_currentFence)) {
      HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
      ThrowIfFailed(m_fence->SetEventOnCompletion(m_currentFence, eventHandle));
      WaitForSingleObject(eventHandle, INFINITE);
      CloseHandle(eventHandle);
  }
}

// Must update the projection matrix on resizing.
void BlendApp::OnResize()
{
  // Base app updates swap chain buffers, RT, DS buffer sizes, viewport and scissor rects etc.
  BaseApp::OnResize();

  // Also update this demo's projection matrix.
  XMMATRIX p = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
  XMStoreFloat4x4(&proj_, p);
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

// Builds input layout.
void BlendApp::BuildInputLayout()
{
  inputLayout_ =
    {
     {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
     {"COLOR",    0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
     {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
}

// Builds shaders needed for this demo.
void BlendApp::BuildShaders()
{
  shaders_["std_vs"] = BaseUtil::CompileShader(L"shaders\\blending.hlsl", nullptr, "VS", "vs_5_1");
  shaders_["std_ps"] = BaseUtil::CompileShader(L"shaders\\blending.hlsl", nullptr, "PS", "ps_5_1");
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

// Builds the pipelines required for this demo.
void BlendApp::BuildPipelines()
{
  D3D12_GRAPHICS_PIPELINE_STATE_DESC std_gfx_pipe = {};

  std_gfx_pipe.InputLayout    = { inputLayout_.data(), static_cast<UINT>(inputLayout_.size()) };
  std_gfx_pipe.pRootSignature = rootSign_.Get();
  std_gfx_pipe.VS             = {
                                 reinterpret_cast<BYTE*>(shaders_["std_vs"]->GetBufferPointer()),
                                 shaders_["std_vs"]->GetBufferSize()
  };
  std_gfx_pipe.PS             = {
                                 reinterpret_cast<BYTE*>(shaders_["std_ps"]->GetBufferPointer()),
                                 shaders_["std_ps"]->GetBufferSize()
  };
  std_gfx_pipe.RasterizerState         = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
  // std_gfx_pipe.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
  std_gfx_pipe.BlendState              = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
  std_gfx_pipe.DepthStencilState       = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
  std_gfx_pipe.SampleMask              = UINT_MAX;
  std_gfx_pipe.PrimitiveTopologyType   = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
  std_gfx_pipe.NumRenderTargets        = 1;
  std_gfx_pipe.RTVFormats[0]           = m_backBufferFormat;
  std_gfx_pipe.SampleDesc.Count        = m_4xMsaaEn ? 4 : 1;
  std_gfx_pipe.SampleDesc.Quality      = m_4xMsaaEn ? (m_4xMsaaQuality - 1) : 0;
  std_gfx_pipe.DSVFormat               = m_depthStencilFormat;

  ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&std_gfx_pipe, IID_PPV_ARGS(&pipelines_["std_gfx_pipe"])));
}

// Creates descriptor heaps for the resources used by this demo's pipelines.
void BlendApp::BuildDescriptorHeaps()
{
  // Create a single const-buf-view heap for the world view proj matrix for the simple case.
  D3D12_DESCRIPTOR_HEAP_DESC cbv_heap_desc;
  cbv_heap_desc.NumDescriptors = 1;
  cbv_heap_desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  cbv_heap_desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  cbv_heap_desc.NodeMask       = 0;

  ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&cbv_heap_desc, IID_PPV_ARGS(&cbvHeap_)));
}

// Creates a const buffer and also bBuilds the const buffer views for the world view projection matrix used for this demo
void BlendApp::BuildConstBufferViews()
{
  passCb_ = std::make_unique<UploadBuffer<PassConstants>>(m_d3dDevice.Get(),
                                                          1,      // Element count.
                                                          true);  // Is a const buffer?

  auto cb_address  = passCb_->Resource()->GetGPUVirtualAddress();
  auto heap_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(cbvHeap_->GetCPUDescriptorHandleForHeapStart());

  D3D12_CONSTANT_BUFFER_VIEW_DESC cbv_desc = {};
  cbv_desc.BufferLocation = cb_address;
  cbv_desc.SizeInBytes    = BaseUtil::CalcConstantBufferByteSize(sizeof(PassConstants));

  m_d3dDevice->CreateConstantBufferView(&cbv_desc, heap_handle);
}




/**

Blending demo agenda:
:- swapchain buffer already created in BaseApp
:- create render target and clear to color, buffers created by BaseApp
:- single fence for proper wait until reset
- Draw a single grid with mouse movements
  :- generate mesh for grid.
  :- create pipeline
  :- write vertex and pixel shaders
  :- create descriptors
  - create const buffer view for the world view proj matrix
  - upload vertex/index buffers
  - upload constant buffers
  - create root signature
  - load texture
  - write drawing commands
- Frame resources, for rendering one full frame
- triple buffering
- Draw a textured crate
- Draw animated water
- Draw Mountains
- water blends with other elements

**/
