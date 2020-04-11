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

// Common types used in this demo.
struct ShaderVertex
{
    XMFLOAT3 position;
};

constexpr std::array<ShaderVertex, 10> points = {
          ShaderVertex({XMFLOAT3(+0.2f, -1.0f, +1.0f)}), ShaderVertex({XMFLOAT3(-0.2f, -1.0f, +1.0f)}),
          ShaderVertex({XMFLOAT3(+0.4f, -1.0f, +1.0f)}), ShaderVertex({XMFLOAT3(-0.4f, -1.0f, +1.0f)}),
          ShaderVertex({XMFLOAT3(+0.6f, -1.0f, +1.0f)}), ShaderVertex({XMFLOAT3(-0.6f, -1.0f, +1.0f)}),
          ShaderVertex({XMFLOAT3(+0.8f, -1.0f, +1.0f)}), ShaderVertex({XMFLOAT3(-0.8f, -1.0f, +1.0f)}),
          ShaderVertex({XMFLOAT3(+1.0f, -1.0f, +1.0f)}), ShaderVertex({XMFLOAT3(-1.0f, -1.0f, +1.0f)}),
      };



struct ShaderCbForMvp
{
  XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};

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

// Windows resize proc. Update our perspective projection matrix.
  virtual void OnResize()override
  {
      BaseApp::OnResize();
      XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 100.0f);
      XMStoreFloat4x4(&proj_, proj);
  }

  void UpdateObjectCBs()
  {

  }
  void UpdateCamera(const BaseTimer& gt)
  {
    eyePos_.x = radius_ * sinf(phi_) * cosf(theta_);
    eyePos_.z = radius_ * sinf(phi_) * sinf(theta_);
    eyePos_.y = radius_ * cosf(phi_);

    XMVECTOR pos = XMVectorSet(eyePos_.x, eyePos_.y, eyePos_.z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&view_, view);
  }

  void UpdatePassCB()
  {
    XMMATRIX worldViewProj = XMLoadFloat4x4(&world_) * XMLoadFloat4x4(&view_) * XMLoadFloat4x4(&proj_);

    ShaderCbForMvp shaderMvpCb = { };
    XMStoreFloat4x4(&shaderMvpCb.WorldViewProj, XMMatrixTranspose(worldViewProj));
    shaderMvpConstBuf_->CopyData(0, shaderMvpCb);
  }

  virtual void Update(const BaseTimer& gt)override
  {
      UpdateCamera(gt);

      if ((m_currentFence != 0) && (m_fence->GetCompletedValue() < m_currentFence)) {
          HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
          ThrowIfFailed(m_fence->SetEventOnCompletion(m_currentFence, eventHandle));
          WaitForSingleObject(eventHandle, INFINITE);
          CloseHandle(eventHandle);
      }

      UpdatePassCB();
  }

  virtual void Draw(const BaseTimer& gt)override
  {
      ThrowIfFailed(m_directCmdListAlloc->Reset());
      ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), pso_.Get()));

      m_commandList->RSSetViewports(1, &m_screenViewport);
      m_commandList->RSSetScissorRects(1, &m_scissorRect);

      // Transition the back buffer so we can render to it.
      m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
                                                                              D3D12_RESOURCE_STATE_PRESENT,
                                                                              D3D12_RESOURCE_STATE_RENDER_TARGET));

      m_commandList->ClearRenderTargetView(CurrentBackBufferView(), DirectX::Colors::Black, 0, nullptr);

      m_commandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

      m_commandList->OMSetRenderTargets(1,                        // Num render target descriptors
                                        &CurrentBackBufferView(), // handle to rt
                                        true,                     // descriptors are contiguous
                                        &DepthStencilView());     // handle to ds


      ID3D12DescriptorHeap* descriptorHeaps[] = { cbvHeap_.Get() };
      m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
      m_commandList->SetGraphicsRootSignature(rootSignature_.Get());

      m_commandList->IASetVertexBuffers(0, 1, &pGeometry_->VertexBufferView());
      m_commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);

      m_commandList->SetGraphicsRootDescriptorTable(0, cbvHeap_->GetGPUDescriptorHandleForHeapStart());

      m_commandList->DrawInstanced((UINT)points.size(), 1, 0, 0 );

      // Transition the render target back to be presentable.
      m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
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

  virtual void OnMouseDown(WPARAM btnState, int x, int y)override
  {
    lastMousePos_.x = x;
    lastMousePos_.y = y;
    SetCapture(mhMainWnd);
  }

  virtual void OnMouseUp(WPARAM btnState, int x, int y)override
  {
    ReleaseCapture();
  }

  virtual void OnMouseMove(WPARAM btnState, int x, int y)override
  {
    if ((btnState & MK_LBUTTON) != 0)
    {
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - lastMousePos_.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - lastMousePos_.y));

        theta_ += dx;
        phi_   += dy;

        phi_ = MathHelper::Clamp(phi_, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if ((btnState & MK_RBUTTON) != 0)
    {
        float dx = 0.05f * static_cast<float>(x - lastMousePos_.x);
        float dy = 0.05f * static_cast<float>(y - lastMousePos_.y);

        radius_ += dx - dy;
        radius_ = MathHelper::Clamp(radius_, 5.0f, 150.0f);
    }

    lastMousePos_.x = x;
    lastMousePos_.y = y;
  }

  void LoadTextures()
  {
  }

  void BuildSceneGeometry()
  {
      const UINT vbByteSize = (UINT)points.size() * sizeof(ShaderVertex);
      pGeometry_ = std::make_unique<MeshGeometry>();
      pGeometry_->name = "points";

      ThrowIfFailed(D3DCreateBlob(vbByteSize, &pGeometry_->vertexBufferCPU));
      CopyMemory(pGeometry_->vertexBufferCPU->GetBufferPointer(), points.data(), vbByteSize);

      pGeometry_->vertexBufferGPU = BaseUtil::CreateDefaultBuffer(m_d3dDevice.Get(), m_commandList.Get(), points.data(), vbByteSize, pGeometry_->vertexBufferUploader);

      pGeometry_->vertexByteStride = sizeof(ShaderVertex);
      pGeometry_->vertexBufferByteSize = vbByteSize;

      SubmeshGeometry subMesh = {};
      subMesh.baseVertexLocation = 0;

      pGeometry_->drawArgs["points"] = subMesh;
  }

  void BuildRenderObjects()
  {
  }

  void BuildMaterials()
  {
  }

// Builds a vertex, geometry and pixel shader needed for the demo.
  void BuildShadersAndInputLayout()
  {
      HRESULT hr = S_OK;

      shaders_["vertexShader"] = BaseUtil::CompileShader(L"shaders\\pointToQuad.hlsl", nullptr, "VS", "vs_5_0");
      //shaders_["geomShader"] = BaseUtil::CompileShader(L"shaders\\pointToQuad.hlsl", nullptr, "GS", "gs_5_0");
      shaders_["pixelShader"] = BaseUtil::CompileShader(L"shaders\\pointToQuad.hlsl", nullptr, "PS", "ps_5_0");

      inputLayout_ =
      {
          { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      };
  }

// Builds the root signature, const buffer heap, view for this demo.
  void BuildRootSignature()
  {
    CD3DX12_ROOT_PARAMETER rootParameter[1];

    CD3DX12_DESCRIPTOR_RANGE cbvTable = { };
    cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    rootParameter[0].InitAsDescriptorTable(1, &cbvTable);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignDesc(1, rootParameter, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob         = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(&rootSignDesc,
                                             D3D_ROOT_SIGNATURE_VERSION_1,
                                             serializedRootSig.GetAddressOf(),
                                             errorBlob.GetAddressOf());

    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }

    ThrowIfFailed(hr);

    // Create our root signature with the single root parameter.
    ThrowIfFailed(m_d3dDevice->CreateRootSignature(0,
                                                   serializedRootSig->GetBufferPointer(),
                                                   serializedRootSig->GetBufferSize(),
                                                   IID_PPV_ARGS(&rootSignature_)));

    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask       = 0;

    // Build the descriptor heap for the lone MVP const buffer.
    ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&cbvHeap_)));

    shaderMvpConstBuf_ = std::make_unique<UploadBuffer<ShaderCbForMvp>>(m_d3dDevice.Get(), 1, true);

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = { };
    cbvDesc.BufferLocation                  = shaderMvpConstBuf_->Resource()->GetGPUVirtualAddress();;
    cbvDesc.SizeInBytes                     = BaseUtil::CalcConstantBufferByteSize(sizeof(ShaderCbForMvp));

    // Build the const buffer view for shader access.
    m_d3dDevice->CreateConstantBufferView(&cbvDesc, cbvHeap_->GetCPUDescriptorHandleForHeapStart());
  }

  // Builds the constant buffers etc needed in the shader program.
  void InitShaderResources()
  {

  }

  // Build descriptor heaps and shader resource views for our textures.
  void BuildShaderViews()
  {
  }

  // Builds descriptors and pipelines.
  void BuildPipelines()
  {
    D3D12_GRAPHICS_PIPELINE_STATE_DESC pipeState = { };

    ZeroMemory(&pipeState, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    pipeState.InputLayout = { inputLayout_.data(), (UINT) inputLayout_.size() };
    pipeState.pRootSignature = rootSignature_.Get();
    pipeState.VS = { reinterpret_cast<BYTE*>(shaders_["vertexShader"]->GetBufferPointer()), shaders_["vertexShader"]->GetBufferSize() };
    pipeState.PS = { reinterpret_cast<BYTE*>(shaders_["pixelShader"]->GetBufferPointer()), shaders_["pixelShader"]->GetBufferSize() };
    //pipeState.GS = { reinterpret_cast<BYTE*>(shaders_["geomShader"]->GetBufferPointer()), shaders_["geomShader"]->GetBufferSize() };

    pipeState.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    //pipeState.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    pipeState.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    pipeState.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    pipeState.SampleMask = UINT_MAX;
    pipeState.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_POINT;
    pipeState.NumRenderTargets = 1;
    pipeState.RTVFormats[0] = m_backBufferFormat;
    pipeState.SampleDesc.Count = m_4xMsaaEn ? 4 : 1;
    pipeState.SampleDesc.Quality = m_4xMsaaEn ? (m_4xMsaaQuality - 1) : 0;
    pipeState.DSVFormat = m_depthStencilFormat;
    ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&pipeState,  IID_PPV_ARGS(&pso_)));
  }

// First step. Setup everything and submit command buffers to do so.
  virtual bool Initialize() override
  {
      bool res = BaseApp::Initialize();

      if (res) {
          ThrowIfFailed(m_commandList.Get()->Reset(m_directCmdListAlloc.Get(), nullptr));

          BuildSceneGeometry();
          BuildShadersAndInputLayout();
          BuildRootSignature();
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
  std::unique_ptr<MeshGeometry> pGeometry_ = nullptr;
  std::unordered_map<std::string, ComPtr<ID3DBlob>> shaders_;
  ComPtr<ID3D12PipelineState> pso_         = nullptr;
  ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
  std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout_;
  POINT lastMousePos_;
  float theta_ = 1.5f * XM_PI;
  float phi_ = 0.2f * XM_PI;
  float radius_ = 15.0f;
  XMFLOAT3 eyePos_ = {0.0f, 0.0f, 0.0f };
  XMFLOAT4X4 view_ = MathHelper::Identity4x4();
  XMFLOAT4X4 proj_ = MathHelper::Identity4x4();
  XMFLOAT4X4 world_ = MathHelper::Identity4x4();
  ComPtr<ID3D12DescriptorHeap> cbvHeap_ = nullptr;
  std::unique_ptr<UploadBuffer<ShaderCbForMvp>> shaderMvpConstBuf_ = nullptr;

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

- build command buffer, implement draw function
- build shaders stubs
- build pipeline
- build root signature.

- render a set of points
- renders a set of points as quads
- build scene geometry, of some points


 ***********************************************************************/
