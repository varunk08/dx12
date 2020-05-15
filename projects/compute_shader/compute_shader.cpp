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

// ====================================================================================================================
class BlurFilter
{
public:
  BlurFilter(ID3D12Device* pDevice, UINT width, UINT height, DXGI_FORMAT format)
  : pDevice_(pDevice), width_(width), height_(height), format_(format)
  {
    BuildResources();
  }

  ~BlurFilter() {}

  ID3D12Resource* Output() { return blurMap0_.Get();}

  void BuildDescriptors(CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDesc, CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDesc, UINT descSize)
  {
      blur0CpuSrv_ = hCpuDesc;
      blur0CpuUav_ = hCpuDesc.Offset(1, descSize);
      blur1CpuSrv_ = hCpuDesc.Offset(1, descSize);
      blur1CpuUav_ = hCpuDesc.Offset(1, descSize);

      blur0GpuSrv_ = hGpuDesc;
      blur0GpuUav_ = hGpuDesc.Offset(1, descSize);
      blur1GpuSrv_ = hGpuDesc.Offset(1, descSize);
      blur1GpuUav_ = hGpuDesc.Offset(1, descSize);

      BuildDescriptorsInternal();
  }

  void OnResize(UINT newWidth, UINT newHeight)
  {
      if ((width_ != newWidth) || (height_ != newHeight))
      {
          width_ = newWidth;
          height_ = newHeight;

          BuildResources();
          BuildDescriptorsInternal();
      }
  }

  void Execute(ID3D12GraphicsCommandList* pCmdList, ID3D12RootSignature* pRootSig, ID3D12PipelineState* pHorBlurPso, ID3D12PipelineState* pVertBlurPso, ID3D12Resource* pInput, int blurCount)
  {
      auto weights = CalcGaussWeights(2.5f);
      int blurRadius = static_cast<int>(weights.size() / 2);

      pCmdList->SetComputeRootSignature(pRootSig);
      pCmdList->SetComputeRoot32BitConstants(0, 1, &blurRadius, 0);
      pCmdList->SetComputeRoot32BitConstants(0, static_cast<unsigned>(weights.size()), weights.data(), 1);

      pCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(pInput, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_COPY_SOURCE));
      pCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(blurMap0_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_DEST));

      pCmdList->CopyResource(blurMap0_.Get(), pInput);

      pCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(blurMap0_.Get(), D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_GENERIC_READ));
      pCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(blurMap1_.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

      for (int i = 0; i < blurCount; ++i)
      {
          // Horizontal Blur pass.
          pCmdList->SetPipelineState(pHorBlurPso);

          pCmdList->SetComputeRootDescriptorTable(1, blur0GpuSrv_);
          pCmdList->SetComputeRootDescriptorTable(2, blur1GpuUav_);

          // How many groups do we need to dispatch to cover a row of pixels, where each
          // group covers 256 pixels (the 256 is defined in the ComputeShader).
          UINT numGroupsX = (UINT)ceilf(width_ / 256.0f);
          pCmdList->Dispatch(numGroupsX, height_, 1);

          pCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(blurMap0_.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
          pCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(blurMap1_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));

          // Vertical Blur pass.
          pCmdList->SetPipelineState(pVertBlurPso);

          pCmdList->SetComputeRootDescriptorTable(1, blur1GpuSrv_);
          pCmdList->SetComputeRootDescriptorTable(2, blur0GpuUav_);

          // How many groups do we need to dispatch to cover a column of pixels, where each
          // group covers 256 pixels  (the 256 is defined in the ComputeShader).
          UINT numGroupsY = (UINT)ceilf(height_ / 256.0f);
          pCmdList->Dispatch(width_, numGroupsY, 1);

          pCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(blurMap0_.Get(), D3D12_RESOURCE_STATE_UNORDERED_ACCESS, D3D12_RESOURCE_STATE_GENERIC_READ));
          pCmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(blurMap1_.Get(), D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_UNORDERED_ACCESS));
      }

  }

private:
    std::vector<float> CalcGaussWeights(float sigma)
    {
        float twoSigma2 = 2.0f * sigma * sigma;
        int blurRadius = static_cast<int>(ceil(2.0f * sigma));

        std::vector<float> weights;
        weights.resize(2 * blurRadius + 1);

        float weightSum = 0.0f;

        for (int i = -blurRadius; i <= blurRadius; i++) {
            float x = static_cast<float>(i);
            weights[i + blurRadius] = expf(-x * x / twoSigma2);
            weightSum += weights[i + blurRadius];
        }

        for (auto& w : weights) {
            w /= weightSum;
        }

        return weights;
    }

  void BuildDescriptorsInternal()
  {
      D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
      srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      srvDesc.Format                  = format_;
      srvDesc.ViewDimension           = D3D12_SRV_DIMENSION_TEXTURE2D;
      srvDesc.Texture2D.MostDetailedMip = 0;
      srvDesc.Texture2D.MipLevels       = 1;

      D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
      uavDesc.Format                = format_;
      uavDesc.ViewDimension         = D3D12_UAV_DIMENSION_TEXTURE2D;
      uavDesc.Texture2D.MipSlice    = 0;


      pDevice_->CreateShaderResourceView(blurMap0_.Get(), &srvDesc, blur0CpuSrv_);
      pDevice_->CreateUnorderedAccessView(blurMap0_.Get(), nullptr, &uavDesc, blur0CpuUav_);

      pDevice_->CreateShaderResourceView(blurMap1_.Get(), &srvDesc, blur1CpuSrv_);
      pDevice_->CreateUnorderedAccessView(blurMap1_.Get(), nullptr, &uavDesc, blur1CpuUav_);
  }

  void BuildResources()
  {
      D3D12_RESOURCE_DESC texDesc;
      ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
      texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
      texDesc.Alignment = 0;
      texDesc.Width = width_;
      texDesc.Height = height_;
      texDesc.DepthOrArraySize = 1;
      texDesc.MipLevels = 1;
      texDesc.Format = format_;
      texDesc.SampleDesc.Count = 1;
      texDesc.SampleDesc.Quality = 0;
      texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
      texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

      ThrowIfFailed(pDevice_->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                                      D3D12_HEAP_FLAG_NONE,
                                                      &texDesc,
                                                      D3D12_RESOURCE_STATE_COMMON,
                                                      nullptr,
                                                      IID_PPV_ARGS(&blurMap0_)));

      ThrowIfFailed(pDevice_->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
                                                      D3D12_HEAP_FLAG_NONE,
                                                      &texDesc,
                                                      D3D12_RESOURCE_STATE_COMMON,
                                                      nullptr,
                                                      IID_PPV_ARGS(&blurMap1_)));

  }

  const int MaxBlurRadius = 5;
  ID3D12Device* pDevice_ = nullptr;
  UINT width_ = 0;
  UINT height_=0;
  DXGI_FORMAT format_=DXGI_FORMAT_R8G8B8A8_UNORM;
  CD3DX12_CPU_DESCRIPTOR_HANDLE blur0CpuSrv_;
  CD3DX12_CPU_DESCRIPTOR_HANDLE blur0CpuUav_;
  CD3DX12_CPU_DESCRIPTOR_HANDLE blur1CpuSrv_;
  CD3DX12_CPU_DESCRIPTOR_HANDLE blur1CpuUav_;
  CD3DX12_GPU_DESCRIPTOR_HANDLE blur0GpuSrv_;
  CD3DX12_GPU_DESCRIPTOR_HANDLE blur0GpuUav_;
  CD3DX12_GPU_DESCRIPTOR_HANDLE blur1GpuSrv_;
  CD3DX12_GPU_DESCRIPTOR_HANDLE blur1GpuUav_;
  ComPtr<ID3D12Resource> blurMap0_ = nullptr;
  ComPtr<ID3D12Resource> blurMap1_ = nullptr;
};

// ====================================================================================================================
// The vertex shader expects vertex data in this layout.
struct ShaderVertex
{
    XMFLOAT3 pos_;
    XMFLOAT3 nor_;
    XMFLOAT2 tex_;
};

// The shader expects lights to be in this format.
struct ShaderLight
{
  XMFLOAT3 strength;
  float    fallOffStart;
  XMFLOAT3 direction;
  float    fallOffEnd;
  XMFLOAT3 position;
  float    spotPower;
};

// Per object const buffer.
struct ObjectConstants
{
  XMFLOAT4X4 worldMatrix = MathHelper::Identity4x4();
};

// This will be referenced by the shader as a const buffer.
struct PassConstants
{
  XMFLOAT4X4   viewProj = MathHelper::Identity4x4();
  XMFLOAT3     eyePos = { 0.0f, 0.0f, 0.0f };
  float        padding0;
  XMFLOAT4     ambientLight;
  XMFLOAT4     fogColor;
  float        fogStart;
  float        fogRange;
  XMFLOAT2     padding1;
  ShaderLight  lights[MaxLights];
};

// Represents const buffer that will be used for materials in the shader.
struct ShaderMaterialCb
{
  XMFLOAT4X4 materialTransform  = MathHelper::Identity4x4();
  XMFLOAT4   diffuseAlbedo  = { 1.0f, 1.0f, 1.0f, 1.0f };
  XMFLOAT3   fresnelR0 = { 0.01f, 0.01f, 0.01f };
  float      roughness = 0.25f;
};

// Represents all parameters of a vertex in DirectX compatible formats.
struct VertexInfo
{
  VertexInfo() {}
  VertexInfo(const DirectX::XMFLOAT3& pos,
             const DirectX::XMFLOAT3& nor,
             const DirectX::XMFLOAT3& tan,
             const DirectX::XMFLOAT2& uv)
    :
    position_(pos),
    normal_(nor),
    tangentU_(tan),
    texC_(uv)
  { }
  VertexInfo(float px, float py, float pz,
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

// ====================================================================================================================
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

  std::vector<VertexInfo> vertices_;
  std::vector<uint32_t> indices32_;

private:
  std::vector<uint16_t> indices16_;
};

// ====================================================================================================================
// Class to generate geometry for the demo.
class GeometryGenerator
{
public:
  GeometryGenerator() {}
  MeshData CreateGrid(float width, float depth, uint32_t m, uint32_t n);
};

// Represents material properties for a single object to be rendered.
struct MaterialInfo
{
  string     name;
  int        materialCbIndex = -1;
  XMFLOAT4X4 materialTransform = MathHelper::Identity4x4();
  XMFLOAT4   diffuseAlbedo  = { 1.0f, 1.0f, 1.0f, 1.0f };
};

// Stores parameters for each item in the scene that will be rendered with a draw call.
struct RenderObject
{
  RenderObject() = default;
  XMFLOAT4X4               worldTransform = MathHelper::Identity4x4();
  XMFLOAT4X4               texTransform = MathHelper::Identity4x4();
  MeshGeometry*            pGeo = nullptr;
  MaterialInfo*            pMat = nullptr;
  D3D12_PRIMITIVE_TOPOLOGY primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
  UINT                     indexCount = 0;
  UINT                     startIndexLocation = 0;
  UINT                     baseVertexLocation = 0;
  int                      objectCbIndex = -1;
};

// ====================================================================================================================
// Current demo class to handle user input and to setup demo-specific resource and commands.
class BlurDemo : public BaseApp
{
public:
  BlurDemo(HINSTANCE hInst);
  ~BlurDemo();

  // Delete copy and assign.
  BlurDemo(const BlurDemo& app) = delete;
  BlurDemo& operator=(const BlurDemo& app) = delete;

  bool Initialize() override;

private:
  void OnResize() override;
  void Update(const BaseTimer& timer) override;
  void Draw(const BaseTimer& timer) override;

  virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
  virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
  virtual void OnMouseMove(WPARAM btnState, int x, int y) override;
  virtual void OnKeyDown(WPARAM wparam) override;

  void AnimateMaterials(const BaseTimer& timer);
  void UpdateMaterials(const BaseTimer& timer);
  void BuildMaterials();
  void BuildRenderObjects();
  void BuildRootSignature();
  void BuildPostProcessRootSignature();
  void BuildTerrainGeometry();
  void BuildWaterGeometry();
  void BuildInputLayout();
  void BuildShaders();
  void BuildPipelines();
  void BuildDescriptorHeaps();
  void BuildBufferViews();
  void LoadTextures();
  void DrawRenderObjects();
  void UpdateObjectConstants();

  float GetHillsHeight(float x, float z) const;
  XMFLOAT3 GetHillsNormal(float x, float z) const;

  std::vector<D3D12_INPUT_ELEMENT_DESC>                          inputLayout_;
  std::unordered_map<std::string, ComPtr<ID3DBlob>>              shaders_;
  ComPtr<ID3D12RootSignature>                                    rootSign_ = nullptr;
  ComPtr<ID3D12RootSignature>                                    postProcessRootSign_ = nullptr;
  std::unordered_map<std::string, ComPtr<ID3D12PipelineState>>   pipelines_;
  ComPtr<ID3D12DescriptorHeap>                                   descriptorHeap_;
  std::unique_ptr<UploadBuffer<PassConstants>>                   passCb_ = nullptr;     // Stores the MVP matrices etc.
  std::unique_ptr<UploadBuffer<ShaderMaterialCb>>                materialCb_ = nullptr;
  std::unique_ptr<UploadBuffer<ObjectConstants>>                 objectCb_ = nullptr;
  std::unordered_map<std::string, std::unique_ptr<Texture>>      textures_;
  std::unordered_map<std::string, std::unique_ptr<MaterialInfo>> materials;
  std::unique_ptr<BlurFilter>                                    blurFilter_;

  // Matrices
  XMFLOAT4X4                                                     view_ = MathHelper::Identity4x4();
  XMFLOAT4X4                                                     proj_ = MathHelper::Identity4x4();

  // Geometries to draw.
  std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> geometries;

  // Objects to be rendered in a scene.
  std::vector<std::unique_ptr<RenderObject>>                     allRenderObjects;

  // Mouse parameters.
  POINT                                                          lastMousePos_ = { 0, 0 };
  float                                                          radius_ = 100.0f;
  float                                                          phi_    = 0.3f * XM_PI;
  float                                                          theta_  = 1.5f * XM_PI;
};

// Demo constructor.
BlurDemo::BlurDemo(HINSTANCE h_inst)
  :
  BaseApp(h_inst)
{

}

// Destructor flushes command queue
BlurDemo::~BlurDemo()
{
  if (m_d3dDevice != nullptr){
    FlushCommandQueue();
  }
}

// Calls base app to set up windows and d3d and this demo's resources
bool BlurDemo::Initialize()
{
  bool ret = BaseApp::Initialize();

  if (ret == true) {
    ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), nullptr));
    m_cbvSrvUavDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

    blurFilter_ = std::make_unique<BlurFilter>(m_d3dDevice.Get(), m_clientWidth, m_clientHeight, DXGI_FORMAT_R8G8B8A8_UNORM);

    LoadTextures();
    BuildTerrainGeometry();
    BuildWaterGeometry();
    BuildMaterials();
    BuildRenderObjects();
    BuildInputLayout();
    BuildShaders();
    BuildDescriptorHeaps();
    BuildBufferViews();
    BuildRootSignature();
    BuildPostProcessRootSignature();
    BuildPipelines();

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
void BlurDemo::Update(const BaseTimer& timer)
{
  // Update the pass constants that will be written to the const buffer.
  // Update the MVP matrix.
   XMFLOAT3 eye_pos = XMFLOAT3(radius_ * sinf(phi_) * cosf(theta_),
                               radius_ * cosf(phi_),
                               radius_ * sinf(phi_) * sinf(theta_));

  XMVECTOR pos    = XMVectorSet(eye_pos.x, eye_pos.y, eye_pos.z, 1.0f);
  XMVECTOR target = XMVectorZero();
  XMVECTOR up     = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
  XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
  XMStoreFloat4x4(&view_, view);

  XMMATRIX newViewProj = XMMatrixMultiply(XMLoadFloat4x4(&view_), XMLoadFloat4x4(&proj_));

  PassConstants newPassConsts = {};
  newPassConsts.eyePos = eye_pos;
  XMStoreFloat4x4(&newPassConsts.viewProj, XMMatrixTranspose(newViewProj));

  // Update the light info.
  newPassConsts.ambientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
  XMStoreFloat4(&newPassConsts.fogColor, DirectX::Colors::LightGray);
  newPassConsts.fogStart = 100.0f;
  newPassConsts.fogRange = 300.0f;

  // The scene has only one directional light now.
  newPassConsts.lights[0].direction = { 0.57735f, -0.57735f, 0.57735f };
  newPassConsts.lights[0].strength  = { 0.9f, 0.9f, 0.8f };
  newPassConsts.lights[1].direction = { -0.57735f, -0.57735f, 0.57735f };
  newPassConsts.lights[1].strength  = { 0.3f, 0.3f, 0.3f };
  newPassConsts.lights[2].direction = { 0.0f, -0.707f, -0.707f };
  newPassConsts.lights[2].strength  = { 0.15f, 0.15f, 0.15f };

  // Copy the data into the constant buffer.
  passCb_->CopyData(0, newPassConsts);

  if ((m_currentFence != 0) && (m_fence->GetCompletedValue() < m_currentFence)) {
      HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
      ThrowIfFailed(m_fence->SetEventOnCompletion(m_currentFence, eventHandle));
      WaitForSingleObject(eventHandle, INFINITE);
      CloseHandle(eventHandle);
  }

  AnimateMaterials(timer);
  UpdateMaterials(timer);
  UpdateObjectConstants();
}

void BlurDemo::UpdateObjectConstants()
{
  for (auto& renderObj : allRenderObjects) {
      ObjectConstants newObjConsts = {};
      XMMATRIX worldTransform = XMLoadFloat4x4(&renderObj->worldTransform);
      XMStoreFloat4x4(&newObjConsts.worldMatrix, XMMatrixTranspose(worldTransform));

      objectCb_->CopyData(renderObj->objectCbIndex, newObjConsts);
  }
}

// Animates each of the dynamic materials in this demo.
void BlurDemo::AnimateMaterials(const BaseTimer& timer)
{
  auto pWater = materials["water"].get();
  float& tu = pWater->materialTransform(3, 0);
  float& tv = pWater->materialTransform(3, 1);

  tu += 0.1f * timer.DeltaTimeInSecs();
  tv += 0.02f * timer.DeltaTimeInSecs();

  if (tu >= 1.0f) {
    tu -= 1.0f;
  }

  if (tv  >= 1.0f) {
    tv -= 1.0f;
  }

  pWater->materialTransform(3, 0) = tu;
  pWater->materialTransform(3, 1) = tv;
}

// Updates material transforms in the material const buffers with the latest transforms.
void BlurDemo::UpdateMaterials(const BaseTimer& timer)
{
  for (auto& mat : materials) {
    MaterialInfo* pMat = mat.second.get();
    XMMATRIX mat_transform = XMLoadFloat4x4(&pMat->materialTransform);

    ShaderMaterialCb mat_cb = {};
    mat_cb.diffuseAlbedo = pMat->diffuseAlbedo;
    XMStoreFloat4x4(&mat_cb.materialTransform, XMMatrixTranspose(mat_transform));
    materialCb_->CopyData(pMat->materialCbIndex, mat_cb);
  }
}

// Must update the projection matrix on resizing.
void BlurDemo::OnResize()
{
  // Base app updates swap chain buffers, RT, DS buffer sizes, viewport and scissor rects etc.
  BaseApp::OnResize();

  // Also update this demo's projection matrix.
  XMMATRIX p = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
  XMStoreFloat4x4(&proj_, p);
}

// Writes draw commands for a frame.
void BlurDemo::Draw(const BaseTimer& timer)
{
  ThrowIfFailed(m_directCmdListAlloc->Reset());
  ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), pipelines_["std_gfx_pipe"].Get()));

  m_commandList->RSSetViewports(1, &m_screenViewport);
  m_commandList->RSSetScissorRects(1, &m_scissorRect);

  // Transition the back buffer so we can render to it.
  m_commandList->ResourceBarrier(1,
                                 &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
                                                                       D3D12_RESOURCE_STATE_PRESENT,
                                                                       D3D12_RESOURCE_STATE_RENDER_TARGET));

  // Clear the render target and depth stencil buffers.
  m_commandList->ClearRenderTargetView(CurrentBackBufferView(),
                                       DirectX::Colors::LightGray,
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

  // Bind the desc heap to the graphics root signature.
  m_commandList->SetGraphicsRootSignature(rootSign_.Get());
  ID3D12DescriptorHeap* descriptorHeaps[] = { descriptorHeap_.Get() };
  m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

  m_commandList->SetGraphicsRootConstantBufferView(0, passCb_->Resource()->GetGPUVirtualAddress());

  DrawRenderObjects();

  // Execute the blur on the back buffer which has our scene rendered.
  blurFilter_->Execute(m_commandList.Get(),
                       postProcessRootSign_.Get(),
                       pipelines_["horzBlur"].Get(),
                       pipelines_["vertBlur"].Get(),
                       CurrentBackBuffer(),
                       4);

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

// Submits the draw calls to render each object in the scene.
void BlurDemo::DrawRenderObjects()
{
  UINT mat_cb_byte_size = BaseUtil::CalcConstantBufferByteSize(sizeof(ShaderMaterialCb));
  UINT objectCbByteSize = BaseUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

  for (auto& render_obj : allRenderObjects) {
    // Set the vertex/index buffers required for rendering this object.
    m_commandList->IASetVertexBuffers(0, 1, &render_obj->pGeo->VertexBufferView());
    m_commandList->IASetIndexBuffer(&render_obj->pGeo->IndexBufferView());
    m_commandList->IASetPrimitiveTopology(render_obj->primitiveType);

    // Set the material and texture required for this render object.
    CD3DX12_GPU_DESCRIPTOR_HANDLE tex(descriptorHeap_->GetGPUDescriptorHandleForHeapStart());
    tex.Offset(render_obj->pMat->materialCbIndex, m_cbvSrvUavDescriptorSize);
    m_commandList->SetGraphicsRootDescriptorTable(1, tex);

    D3D12_GPU_VIRTUAL_ADDRESS mat_cb_address = materialCb_->Resource()->GetGPUVirtualAddress() +
                                               (render_obj->pMat->materialCbIndex * mat_cb_byte_size);
    m_commandList->SetGraphicsRootConstantBufferView(2, mat_cb_address);

    D3D12_GPU_VIRTUAL_ADDRESS objCbAddress = objectCb_->Resource()->GetGPUVirtualAddress() +
                                             (render_obj->objectCbIndex * objectCbByteSize);
    m_commandList->SetGraphicsRootConstantBufferView(3, objCbAddress);

    m_commandList->DrawIndexedInstanced(render_obj->indexCount,
                                        1,
                                        render_obj->startIndexLocation,
                                        render_obj->baseVertexLocation,
                                        0);

    m_commandList->SetPipelineState(pipelines_["transparent_gfx_pipe"].Get());
  }
}

// Applies y=f(x,z) to the values provided.
float BlurDemo::GetHillsHeight(float x, float z) const
{
  return 0.3f * (z * sinf(0.1f * x) + x * cosf(0.1f * z));
}

// Builds all materials used in this demo.
void BlurDemo::BuildMaterials()
{
  auto grass = std::make_unique<MaterialInfo>();
  grass->name = "grass";
  grass->materialCbIndex = 0;
  grass->diffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);

  auto water = std::make_unique<MaterialInfo>();
  water->name = "water";
  water->materialCbIndex =  1;
  water->diffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 0.5f);

  materials["grass"] = std::move(grass);
  materials["water"] = std::move(water);
}

// Function to generate terrain geometry
void BlurDemo::BuildTerrainGeometry()
{
  GeometryGenerator geoGen;

  MeshData grid = geoGen.CreateGrid(200.0f, 200.0f, 50, 50);

  const size_t totalVertices = grid.vertices_.size();
  const size_t totalIndices  = grid.indices32_.size();

  std::vector<ShaderVertex> vertices(totalVertices);

  size_t i = 0;
  for (; i < grid.vertices_.size(); i++) {
    auto& p = grid.vertices_[i].position_;
    vertices[i].pos_ = p;
    vertices[i].pos_.y = GetHillsHeight(p.x, p.z);
    vertices[i].nor_ = GetHillsNormal(p.x, p.z);
    vertices[i].tex_ = grid.vertices_[i].texC_;
  }

  const UINT vbByteSize = static_cast<UINT>(vertices.size() * sizeof(ShaderVertex));

  std::vector<std::uint16_t> indices = grid.GetIndices16();

  const UINT ibByteSize = static_cast<UINT>(indices.size() * sizeof(uint16_t));

  auto geo_ = std::make_unique<MeshGeometry>();
  geo_->name = "terrain";

  ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo_->vertexBufferCPU));
  CopyMemory(geo_->vertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

  ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo_->indexBufferCPU));
  CopyMemory(geo_->indexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

  geo_->vertexBufferGPU = BaseUtil::CreateDefaultBuffer(m_d3dDevice.Get(),
                                                        m_commandList.Get(),
                                                        vertices.data(),
                                                        vbByteSize,
                                                        geo_->vertexBufferUploader);

  geo_->indexBufferGPU = BaseUtil::CreateDefaultBuffer(m_d3dDevice.Get(),
                                                       m_commandList.Get(),
                                                       indices.data(),
                                                       ibByteSize,
                                                       geo_->indexBufferUploader);

  geo_->vertexByteStride = sizeof(ShaderVertex);
  geo_->vertexBufferByteSize = vbByteSize;
  geo_->indexFormat = DXGI_FORMAT_R16_UINT;
  geo_->indexBufferByteSize = ibByteSize;

  SubmeshGeometry sub_mesh;
  sub_mesh.indexCount = static_cast<UINT>(indices.size());
  sub_mesh.startIndexLocation = 0;
  sub_mesh.baseVertexLocation = 0;

  geo_->drawArgs["terrain"] = sub_mesh;

  geometries["terrain"] = std::move(geo_);
}

//Builds a grid representing water and adds it to the global geometries map.
void BlurDemo::BuildWaterGeometry()
{
  GeometryGenerator geoGen;
  MeshData water_grid = geoGen.CreateGrid(200.0f, 200.0f, 50, 50);

  std::vector<ShaderVertex> vert_buffer(water_grid.vertices_.size());
  for (size_t i = 0; i < water_grid.vertices_.size(); ++i) {
    vert_buffer[i].pos_ = water_grid.vertices_[i].position_;
    vert_buffer[i].nor_ = water_grid.vertices_[i].normal_;
    vert_buffer[i].tex_ = water_grid.vertices_[i].texC_;
  }

  std::vector<uint16_t> index_buffer = water_grid.GetIndices16();
  const UINT vb_byte_size = static_cast<UINT>(sizeof(ShaderVertex) * vert_buffer.size());
  const UINT ib_byte_size = static_cast<UINT>(sizeof(uint16_t) * index_buffer.size());

  auto mesh_geo = std::make_unique<MeshGeometry>();
  mesh_geo->name = "water";

  ThrowIfFailed(D3DCreateBlob(vb_byte_size, &mesh_geo->vertexBufferCPU));
  CopyMemory(mesh_geo->vertexBufferCPU->GetBufferPointer(), vert_buffer.data(), vb_byte_size);

  ThrowIfFailed(D3DCreateBlob(ib_byte_size, &mesh_geo->indexBufferCPU));
  CopyMemory(mesh_geo->indexBufferCPU->GetBufferPointer(), index_buffer.data(), ib_byte_size);

  mesh_geo->vertexBufferGPU = BaseUtil::CreateDefaultBuffer(m_d3dDevice.Get(),
                                                            m_commandList.Get(),
                                                            vert_buffer.data(),
                                                            vb_byte_size,
                                                            mesh_geo->vertexBufferUploader);

  mesh_geo->indexBufferGPU = BaseUtil::CreateDefaultBuffer(m_d3dDevice.Get(),
                                                           m_commandList.Get(),
                                                           index_buffer.data(),
                                                           ib_byte_size,
                                                           mesh_geo->indexBufferUploader);
  mesh_geo->vertexByteStride = sizeof(ShaderVertex);
  mesh_geo->vertexBufferByteSize = vb_byte_size;
  mesh_geo->indexFormat = DXGI_FORMAT_R16_UINT;
  mesh_geo->indexBufferByteSize = ib_byte_size;

  SubmeshGeometry sub_mesh;
  sub_mesh.indexCount = static_cast<UINT>(index_buffer.size());
  sub_mesh.startIndexLocation = 0;
  sub_mesh.baseVertexLocation = 0;

  mesh_geo->drawArgs["water"] = sub_mesh;

  geometries["water"] = std::move(mesh_geo);
}

// Builds list of objects to rendered as a scene.
void BlurDemo::BuildRenderObjects()
{
  auto water = std::make_unique<RenderObject>();
  water->worldTransform = MathHelper::Identity4x4(); // We're not transforming the object.
  water->pGeo = geometries["water"].get();
  water->indexCount = water->pGeo->drawArgs["water"].indexCount;
  water->startIndexLocation = water->pGeo->drawArgs["water"].startIndexLocation;
  water->baseVertexLocation = water->pGeo->drawArgs["water"].baseVertexLocation;
  water->pMat = materials["water"].get();
  water->objectCbIndex = 1;

  auto land = std::make_unique<RenderObject>();
  land->worldTransform = MathHelper::Identity4x4();
  land->pGeo = geometries["terrain"].get();
  land->indexCount = land->pGeo->drawArgs["terrain"].indexCount;
  land->startIndexLocation = land->pGeo->drawArgs["terrain"].startIndexLocation;
  land->baseVertexLocation = land->pGeo->drawArgs["terrain"].baseVertexLocation;
  land->pMat = materials["grass"].get();
  land->objectCbIndex = 0;

  allRenderObjects.push_back(std::move(land));
  allRenderObjects.push_back(std::move(water));
}

// Builds input layout.
void BlurDemo::BuildInputLayout()
{
  inputLayout_ =
    {
     {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
     {"NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
     {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
}

// Builds shaders needed for this demo.
void BlurDemo::BuildShaders()
{
  // Shader define for fog.
  const D3D_SHADER_MACRO defines[] =
    {
     "FOG", "1",
      NULL, NULL
    };

  shaders_["std_vs"] = BaseUtil::CompileShader(L"shaders\\blending.hlsl", nullptr, "VS", "vs_5_1");
  shaders_["std_ps"] = BaseUtil::CompileShader(L"shaders\\blending.hlsl", defines, "PS", "ps_5_1");

  shaders_["horzBlurCS"] = BaseUtil::CompileShader(L"shaders\\blur.hlsl", defines, "HorzBlurCS", "cs_5_0");
  shaders_["vertBlurCS"] = BaseUtil::CompileShader(L"shaders\\blur.hlsl", defines, "VertBlurCS", "cs_5_0");
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
void BlurDemo::BuildPipelines()
{
  D3D12_GRAPHICS_PIPELINE_STATE_DESC std_gfx_pipe = {};
  ZeroMemory(&std_gfx_pipe, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
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
  //  std_gfx_pipe.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
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

  // Create a pipeline for rendering transparent objects.
  D3D12_GRAPHICS_PIPELINE_STATE_DESC transparent_gfx_pipe = std_gfx_pipe;
  // transparent_gfx_pipe.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
  D3D12_RENDER_TARGET_BLEND_DESC blend_desc;
  blend_desc.BlendEnable = true;
  blend_desc.LogicOpEnable = false;
  blend_desc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
  blend_desc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
  blend_desc.BlendOp = D3D12_BLEND_OP_ADD;
  blend_desc.SrcBlendAlpha = D3D12_BLEND_ONE;
  blend_desc.DestBlendAlpha = D3D12_BLEND_ZERO;
  blend_desc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
  blend_desc.LogicOp = D3D12_LOGIC_OP_NOOP;
  blend_desc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

  transparent_gfx_pipe.BlendState.RenderTarget[0] = blend_desc;
  ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&transparent_gfx_pipe,
                                                         IID_PPV_ARGS(&pipelines_["transparent_gfx_pipe"])));

  // Blur compute pipelines.
  // Horizontal blur.
  D3D12_COMPUTE_PIPELINE_STATE_DESC horzBlurPSO = {};
  horzBlurPSO.pRootSignature = postProcessRootSign_.Get();
  horzBlurPSO.CS =
  {
      reinterpret_cast<BYTE*>(shaders_["horzBlurCS"]->GetBufferPointer()), shaders_["horzBlurCS"]->GetBufferSize()
  };
  horzBlurPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
  ThrowIfFailed(m_d3dDevice->CreateComputePipelineState(&horzBlurPSO, IID_PPV_ARGS(&pipelines_["horzBlur"])));

  // Vertical blur.
  D3D12_COMPUTE_PIPELINE_STATE_DESC vertBlurPSO = {};
  vertBlurPSO.pRootSignature = postProcessRootSign_.Get();
  vertBlurPSO.CS =
  {
      reinterpret_cast<BYTE*>(shaders_["vertBlurCS"]->GetBufferPointer()), shaders_["vertBlurCS"]->GetBufferSize()
  };
  vertBlurPSO.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
  ThrowIfFailed(m_d3dDevice->CreateComputePipelineState(&vertBlurPSO, IID_PPV_ARGS(&pipelines_["vertBlur"])));
}

// Creates descriptor heaps for the resources used by this demo's pipelines.
void BlurDemo::BuildDescriptorHeaps()
{
  const unsigned NumDescriptors = 2 +  // 2 SRVs for textures so far in this demo.
                                  4;   // Blur descriptors.
  // Create heap for SRVs for textures used in this demo.
  D3D12_DESCRIPTOR_HEAP_DESC srv_heap_desc;
  srv_heap_desc.NumDescriptors = NumDescriptors;
  srv_heap_desc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
  srv_heap_desc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  srv_heap_desc.NodeMask       = 0;

  ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&srv_heap_desc, IID_PPV_ARGS(&descriptorHeap_)));
}

// Creates CBV for the world view projection matrix and SRVs for the textures used for this demo.
void BlurDemo::BuildBufferViews()
{
  // Create a const buffer for the world view projection matrix.
  passCb_ = std::make_unique<UploadBuffer<PassConstants>>(m_d3dDevice.Get(),
                                                          1,      // Element count.
                                                          true);  // Is a const buffer?

  const uint32_t NumObjects = 2; // There are only these many objects in this demo.

  materialCb_ = std::make_unique<UploadBuffer<ShaderMaterialCb>>(m_d3dDevice.Get(),
                                                                 NumObjects,     // 2 materials in this demo so far.
                                                                 true);
  objectCb_ = make_unique<UploadBuffer<ObjectConstants>>(m_d3dDevice.Get(),
                                                         NumObjects,
                                                         true);

  auto heap_handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeap_->GetCPUDescriptorHandleForHeapStart());

  // Create a SRV for the texture used in this demo. The texture should have been created by now.
  assert(textures_.size() && textures_["grass_tex"] != nullptr);
  auto grass_tex = textures_["grass_tex"]->resource_;

  D3D12_SHADER_RESOURCE_VIEW_DESC srv_desc = {};
  srv_desc.Shader4ComponentMapping   = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
  srv_desc.Format                    = grass_tex->GetDesc().Format;
  srv_desc.ViewDimension             = D3D12_SRV_DIMENSION_TEXTURE2D;
  srv_desc.Texture2D.MostDetailedMip = 0;
  srv_desc.Texture2D.MipLevels       = -1;

  m_d3dDevice->CreateShaderResourceView(grass_tex.Get(), &srv_desc, heap_handle);

  // Create the next view after offsetting by 1, since the grass tex SRV is at offset 0.
  heap_handle.Offset(1, m_cbvSrvUavDescriptorSize);

  assert(textures_["water_tex"] != nullptr);
  auto water_tex = textures_["water_tex"]->resource_;
  srv_desc.Format = water_tex->GetDesc().Format;
  m_d3dDevice->CreateShaderResourceView(water_tex.Get(), &srv_desc, heap_handle);

  // Fill out the heap with the descriptors to the BlurFilter resources.
  blurFilter_->BuildDescriptors(
      CD3DX12_CPU_DESCRIPTOR_HANDLE(descriptorHeap_->GetCPUDescriptorHandleForHeapStart(), 2, m_cbvSrvUavDescriptorSize),
      CD3DX12_GPU_DESCRIPTOR_HANDLE(descriptorHeap_->GetGPUDescriptorHandleForHeapStart(), 2, m_cbvSrvUavDescriptorSize),
      m_cbvSrvUavDescriptorSize);
}

// Builds the root signature for this demo.
void BlurDemo::BuildRootSignature()
{
  /* My root signature:

    [0] - CBV for pass constants (view-projection matrix, eye-pos etc).
    [1] - Descriptor table with, only one range for SRVs for the textures.
    [2] - CBV for materials (diffuseAlbedo, roughness, material transform etc).
    [3] - CBV for per-object properties (world transform).

   */
  const uint32_t NumRootParams = 4;

  CD3DX12_DESCRIPTOR_RANGE tex_table;
  tex_table.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, // Descriptor type.
                 1,                               // Num descriptors.
                 0,                               // Base shader register.
                 0,                               // Register space.
                 0);                              // Offset from the start of the table.

  CD3DX12_ROOT_PARAMETER root_param[NumRootParams];
  root_param[0].InitAsConstantBufferView(0); // cbuf register 0
  root_param[1].InitAsDescriptorTable(1, &tex_table, D3D12_SHADER_VISIBILITY_PIXEL);
  root_param[2].InitAsConstantBufferView(1); // cbuf register 1
  root_param[3].InitAsConstantBufferView(2); // cbuf register 2

  CD3DX12_STATIC_SAMPLER_DESC linear_sampler = CD3DX12_STATIC_SAMPLER_DESC(0,
                                                                          D3D12_FILTER_MIN_MAG_MIP_LINEAR,
                                                                          D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                                                                          D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                                                                          D3D12_TEXTURE_ADDRESS_MODE_WRAP);
  std::array<CD3DX12_STATIC_SAMPLER_DESC, 1> static_samplers = { linear_sampler };

  CD3DX12_ROOT_SIGNATURE_DESC root_sign_desc(NumRootParams,
                                             root_param,
                                             1,
                                             static_samplers.data(),
                                             D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

  ComPtr<ID3DBlob> serialized_root_sign = nullptr;
  ComPtr<ID3DBlob> error_blob           = nullptr;

  HRESULT hr = D3D12SerializeRootSignature(&root_sign_desc,
                                           D3D_ROOT_SIGNATURE_VERSION_1,
                                           serialized_root_sign.GetAddressOf(),
                                           error_blob.GetAddressOf());

  if (error_blob != nullptr) {
    ::OutputDebugStringA((char*)error_blob->GetBufferPointer());
  }
  ThrowIfFailed(hr);

  ThrowIfFailed(m_d3dDevice->CreateRootSignature(0,
                                                 serialized_root_sign->GetBufferPointer(),
                                                 serialized_root_sign->GetBufferSize(),
                                                 IID_PPV_ARGS(rootSign_.GetAddressOf())));
}

// Creates the root signature for the post processing compute pipeline.
void BlurDemo::BuildPostProcessRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE srvTable;
    srvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_DESCRIPTOR_RANGE uavTable;
    uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

    CD3DX12_ROOT_PARAMETER slotRootParameter[3];

    slotRootParameter[0].InitAsConstants(12, 0);
    slotRootParameter[1].InitAsDescriptorTable(1, &srvTable);
    slotRootParameter[2].InitAsDescriptorTable(1, &uavTable);

    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter,
                                            0, nullptr,
                                            D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(m_d3dDevice->CreateRootSignature(0,
                                                   serializedRootSig->GetBufferPointer(),
                                                   serializedRootSig->GetBufferSize(),
                                                   IID_PPV_ARGS(postProcessRootSign_.GetAddressOf())));
}

// When the mouse button is pressed down.
void BlurDemo::OnMouseDown(WPARAM btnState, int x, int y)
{
  lastMousePos_.x = x;
  lastMousePos_.y = y;

  // All mouse movement is captured and reported to this application.
  SetCapture(mhMainWnd);
}

// When the pressed mouse button is released.
void BlurDemo::OnMouseUp(WPARAM btnState, int x, int y)
{
  // Release the captured mouse to the OS.
  ReleaseCapture();
}

// When the mouse is moved.
void BlurDemo::OnMouseMove(WPARAM btn_state, int x, int y)
{
  if ((btn_state & MK_LBUTTON) != 0) { // Left mouse button changes angle of view.
    float dx = XMConvertToRadians(0.25f * static_cast<float>(x - lastMousePos_.x));
    float dy = XMConvertToRadians(0.25f * static_cast<float>(y - lastMousePos_.y));

    theta_ += dx;
    phi_   += dy;

    phi_ = MathHelper::Clamp(phi_, 0.1f, MathHelper::Pi - 0.1f);
  } else if ((btn_state & MK_RBUTTON) != 0) { // right mouse button changes zoom.
    float dx = 0.2f * static_cast<float>(x - lastMousePos_.x);
    float dy = 0.2f * static_cast<float>(y - lastMousePos_.y);
    radius_ += dx - dy;
    radius_ = MathHelper::Clamp(radius_, 5.0f, 600.0f);
  }

  lastMousePos_.x = x;
  lastMousePos_.y = y;
}

// When any key board key is pressed.
void BlurDemo::OnKeyDown(WPARAM wparam)
{
}

void BlurDemo::LoadTextures()
{
  auto grass_tex = std::make_unique<Texture>();
  grass_tex->name_ = "grass_tex";
  grass_tex->filename_ = L"..\\textures\\grass.dds";
  ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(m_d3dDevice.Get(),
                                                    m_commandList.Get(),
                                                    grass_tex->filename_.c_str(),
                                                    grass_tex->resource_,
                                                    grass_tex->uploadHeap_));
  textures_[grass_tex->name_] = std::move(grass_tex);

  auto water_tex = std::make_unique<Texture>();
  water_tex->name_ = "water_tex";
  water_tex->filename_ = L"..\\textures\\water1.dds";
  ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(m_d3dDevice.Get(),
                                                    m_commandList.Get(),
                                                    water_tex->filename_.c_str(),
                                                    water_tex->resource_,
                                                    water_tex->uploadHeap_));
  textures_[water_tex->name_] = std::move(water_tex);
}

// Calculates the updated normals for the generated hill terrain.
XMFLOAT3 BlurDemo::GetHillsNormal(float x, float z) const
{
  XMFLOAT3 n( -0.03f * z * cosf(0.1f * x) - 0.3f * cosf(0.1f * z),
              1.0f,
              -0.3f * sinf(0.1f * x) + 0.03f * x * sinf(0.1f * z));

  XMVECTOR unitNormal = XMVector3Normalize(XMLoadFloat3(&n));
  XMStoreFloat3(&n, unitNormal);

  return n;
}

// ====================================================================================================================
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
  BlurDemo demo_app(hInstance);

  if (demo_app.Initialize() == true) {
    ret_code = demo_app.Run();
  }

  return ret_code;
}


// ====================================================================================================================
/*
BLUR COMPUTE SHADER DEMO

TODO:
- present blurred texture
- write blur shader
- execute blur

DONE:
- render scene to texture
- build post process root signature
*/