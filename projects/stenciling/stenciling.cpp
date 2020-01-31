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

const uint32_t NumObjects = 4;

enum class RenderLayer : int
{
    Opaque = 0,
    Mirrors,
    Reflected,
    Transparent,
    Shadow,
    Count
};

struct ShaderVertex
{
    ShaderVertex() = default;
    ShaderVertex(float x, float y, float z, float nx, float ny, float nz, float u, float v) :
        Pos(x, y, z),
        Normal(nx, ny, nz),
        TexC(u, v) {}

    DirectX::XMFLOAT3 Pos;
    DirectX::XMFLOAT3 Normal;
    DirectX::XMFLOAT2 TexC;
};

const std::array<ShaderVertex, 20> vertices =
{
    // Floor: Observe we tile texture coordinates.
    ShaderVertex(-3.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 0.0f, 4.0f), // 0
    ShaderVertex(-3.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 0.0f),
    ShaderVertex(7.5f, 0.0f,   0.0f, 0.0f, 1.0f, 0.0f, 4.0f, 0.0f),
    ShaderVertex(7.5f, 0.0f, -10.0f, 0.0f, 1.0f, 0.0f, 4.0f, 4.0f),

    // Wall: Observe we tile texture coordinates, and that we
    // leave a gap in the middle for the mirror.
    ShaderVertex(-3.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 4
    ShaderVertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
    ShaderVertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 0.0f),
    ShaderVertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.5f, 2.0f),

    ShaderVertex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 2.0f), // 8
    ShaderVertex(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
    ShaderVertex(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 0.0f),
    ShaderVertex(7.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 2.0f, 2.0f),

    ShaderVertex(-3.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 12
    ShaderVertex(-3.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
    ShaderVertex(7.5f, 6.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 0.0f),
    ShaderVertex(7.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 6.0f, 1.0f),

    // Mirror
    ShaderVertex(-2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 1.0f), // 16
    ShaderVertex(-2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 0.0f, 0.0f),
    ShaderVertex(2.5f, 4.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 0.0f),
    ShaderVertex(2.5f, 0.0f, 0.0f, 0.0f, 0.0f, -1.0f, 1.0f, 1.0f)
};

const std::array<std::int16_t, 30> indices =
{
    // Floor
    0, 1, 2,
    0, 2, 3,

    // Walls
    4, 5, 6,
    4, 6, 7,

    8, 9, 10,
    8, 10, 11,

    12, 13, 14,
    12, 14, 15,

    // Mirror
    16, 17, 18,
    16, 18, 19
};

// Objects used by the demo to manage resources.
struct MaterialInfo
{
    UINT textureIndex = 0;

};

// Stores parameters for each item in the scene that will be rendered with a draw call.
struct RenderObject
{
    RenderObject() = default;
    XMFLOAT4X4               worldTransform = MathHelper::Identity4x4();
    XMFLOAT4X4               texTransform = MathHelper::Identity4x4();
    MeshGeometry* pGeo = nullptr;
    MaterialInfo* pMat = nullptr;
    D3D12_PRIMITIVE_TOPOLOGY primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    UINT                     indexCount = 0;
    UINT                     startIndexLocation = 0;
    UINT                     baseVertexLocation = 0;
    int                      objectCbIndex = -1;
};

// CBs that the shaders need.
struct PassCb
{
    XMFLOAT4X4 viewProjTransform;
};

struct MaterialCb
{
};

struct ObjectCb
{
    XMFLOAT4X4 worldTransform;
};

// Our stenciling demo app, derived from the BaseApp ofcourse.
class StencilDemo final : public BaseApp
{
public:
  // Delete default and copy constructors, and copy operator.
  StencilDemo& operator=(const StencilDemo& other) = delete;
  StencilDemo(const StencilDemo& other) = delete;
  StencilDemo() = delete;

  // Constructor.
  StencilDemo(HINSTANCE hInstance)
    :
    BaseApp(hInstance)
  {
  }

  // Destructor.
  ~StencilDemo()
  {
    if (m_d3dDevice != nullptr) {
        FlushCommandQueue();
    }
  }

  virtual void OnResize()override
  {
      BaseApp::OnResize();

      // The window resized, so update the aspect ratio and recompute the projection matrix.
      XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
      XMStoreFloat4x4(&m_proj, P);
  }

  void UpdateObjectCBs()
  {
      for (auto& rObj : m_allRenderObjects) {
        ObjectCb newObjConsts = {};
        XMMATRIX worldTransform = XMLoadFloat4x4(&rObj.get()->worldTransform);
        XMStoreFloat4x4(&newObjConsts.worldTransform, XMMatrixTranspose(worldTransform));

        m_objectCB->CopyData(rObj->objectCbIndex, newObjConsts);
      }
  }
  void UpdateCamera(const BaseTimer& gt)
  {
      // Convert Spherical to Cartesian coordinates.
      m_eyePos.x = m_radius * sinf(m_phi) * cosf(m_theta);
      m_eyePos.z = m_radius * sinf(m_phi) * sinf(m_theta);
      m_eyePos.y = m_radius * cosf(m_phi);

      // Build the view matrix.
      XMVECTOR pos = XMVectorSet(m_eyePos.x, m_eyePos.y, m_eyePos.z, 1.0f);
      XMVECTOR target = XMVectorZero();
      XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

      XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
      XMStoreFloat4x4(&m_view, view);
  }

  void UpdatePassCB()
  {
      XMMATRIX view = XMLoadFloat4x4(&m_view);
      XMMATRIX proj = XMLoadFloat4x4(&m_proj);
      XMMATRIX viewProj = XMMatrixMultiply(view, proj);
      PassCb newPassCb = {};
      XMStoreFloat4x4(&newPassCb.viewProjTransform, XMMatrixTranspose(viewProj));

      m_passCB->CopyData(0, newPassCb);
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
      UpdateObjectCBs();
  }

  virtual void Draw(const BaseTimer& gt)override
  {
      ThrowIfFailed(m_directCmdListAlloc->Reset());
      ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(),
                                         m_pipelines["opaque"].Get()));

      m_commandList->RSSetViewports(1, &m_screenViewport);
      m_commandList->RSSetScissorRects(1, &m_scissorRect);

      // Transition the back buffer so we can render to it.
      m_commandList->ResourceBarrier(1,
        &CD3DX12_RESOURCE_BARRIER::Transition(
            CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));


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

      ID3D12DescriptorHeap* ppDescriptorHeaps[] = { m_srvDescriptorHeap.Get() };
      m_commandList->SetDescriptorHeaps(_countof(ppDescriptorHeaps), ppDescriptorHeaps);
      m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
      m_commandList->SetGraphicsRootConstantBufferView(1, m_passCB->Resource()->GetGPUVirtualAddress());

      const unsigned int objCbByteSize = BaseUtil::CalcConstantBufferByteSize(sizeof(ObjectCb));
      for (size_t i = 0; i < m_allRenderObjects.size(); ++i)
      {
          auto ri =  m_allRenderObjects[i].get();

          m_commandList->IASetVertexBuffers(0, 1, &ri->pGeo->VertexBufferView());
          m_commandList->IASetIndexBuffer(&ri->pGeo->IndexBufferView());
          m_commandList->IASetPrimitiveTopology(ri->primitiveType);

          CD3DX12_GPU_DESCRIPTOR_HANDLE hTex(m_srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
          hTex.Offset(ri->pMat->textureIndex, m_cbvSrvUavDescriptorSize);
          m_commandList->SetGraphicsRootDescriptorTable(2, hTex);
          
          D3D12_GPU_VIRTUAL_ADDRESS objCbAddr = m_objectCB->Resource()->GetGPUVirtualAddress() +
                                                (ri->objectCbIndex * objCbByteSize);
          m_commandList->SetGraphicsRootConstantBufferView(0, objCbAddr);

          m_commandList->DrawIndexedInstanced(ri->indexCount,
                                              1,
                                              ri->startIndexLocation,
                                              ri->baseVertexLocation,
                                              0);
      }

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
      m_lastMousePos.x = x;
      m_lastMousePos.y = y;


      SetCapture(mhMainWnd);
  }

  virtual void OnMouseUp(WPARAM btnState, int x, int y)override
  {
      ReleaseCapture();
  }

  virtual void OnMouseMove(WPARAM btnState, int x, int y)override
  {
      if ((btnState & MK_LBUTTON) != 0) {
          float dx = XMConvertToRadians(0.25f * static_cast<float>(x - m_lastMousePos.x));
          float dy = XMConvertToRadians(0.25f * static_cast<float>(y - m_lastMousePos.y));

          m_theta += dx;
          m_phi   += dy;
          m_phi = MathHelper::Clamp(m_phi, 0.1f, MathHelper::Pi - 0.1f);
      }
      else if ((btnState & MK_RBUTTON) != 0) {
          float dx = 0.2f * static_cast<float>(x - m_lastMousePos.x);
          float dy = 0.2f * static_cast<float>(y - m_lastMousePos.y);
          m_radius += (dx - dy);
          m_radius = MathHelper::Clamp(m_radius, 5.0f, 600.0f);
      }

      m_lastMousePos.x = x;
      m_lastMousePos.y = y;
  }

  void LoadTextures()
  {
      unique_ptr<Texture> bricksTex = std::make_unique<Texture>();
      bricksTex->name_              = "bricksTex";
      bricksTex->filename_          = L"..\\textures\\bricks3.dds";
      ThrowIfFailed(CreateDDSTextureFromFile12(m_d3dDevice.Get(),
                                               m_commandList.Get(),
                                               bricksTex->filename_.c_str(),
                                               bricksTex->resource_,
                                               bricksTex->uploadHeap_));

      unique_ptr<Texture> checkboardTex = std::make_unique<Texture>();
      checkboardTex->name_              = "checkerboardTex";
      checkboardTex->filename_          = L"..\\textures\\checkboard.dds";
      ThrowIfFailed(CreateDDSTextureFromFile12(m_d3dDevice.Get(),
                                               m_commandList.Get(),
                                               checkboardTex->filename_.c_str(),
                                               checkboardTex->resource_,
                                               checkboardTex->uploadHeap_));

      m_textures[bricksTex->name_] = std::move(bricksTex);
      m_textures[checkboardTex->name_] = std::move(checkboardTex);
  }

  void BuildSceneGeometry()
  {
      SubmeshGeometry floorSubmesh;
      floorSubmesh.indexCount = 6;
      floorSubmesh.startIndexLocation = 0;
      floorSubmesh.baseVertexLocation = 0;

      SubmeshGeometry wallSubmesh;
      wallSubmesh.indexCount = 18;
      wallSubmesh.startIndexLocation = 6;
      wallSubmesh.baseVertexLocation = 0;

      SubmeshGeometry mirrorSubmesh;
      mirrorSubmesh.indexCount = 6;
      mirrorSubmesh.startIndexLocation = 24;
      mirrorSubmesh.baseVertexLocation = 0;

      GeometryGenerator generator;
      MeshData boxMesh = generator.CreateBox(5, 5, 5, 0);
      std::vector<ShaderVertex> boxVertices(boxMesh.m_vertices.size());

      for (UINT i = 0; i < boxVertices.size(); i++) {
          boxVertices[i].Pos = boxMesh.m_vertices[i].m_position;
          boxVertices[i].Normal = boxMesh.m_vertices[i].m_normal;
          boxVertices[i].TexC = boxMesh.m_vertices[i].m_texC;
      }

      std::vector<uint16_t> boxIndices = boxMesh.GetIndices16();
      
      const unsigned int boxVbByteSize = static_cast<unsigned int>(boxVertices.size()) * sizeof(ShaderVertex);
      const unsigned int boxIbByteSize = static_cast<unsigned int>(boxIndices.size()) * sizeof(uint16_t);

      const UINT vbByteSize = ((UINT)vertices.size()) * sizeof(ShaderVertex);
      const UINT ibByteSize = ((UINT)indices.size()) * sizeof(std::uint16_t);
     
      auto geo = std::make_unique<MeshGeometry>();
      geo->name = "roomGeo";

      ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->vertexBufferCPU));
      CopyMemory(geo->vertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

      ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->indexBufferCPU));
      CopyMemory(geo->indexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

      geo->vertexBufferGPU = BaseUtil::CreateDefaultBuffer(m_d3dDevice.Get(),
          m_commandList.Get(), vertices.data(), vbByteSize, geo->vertexBufferUploader);

      geo->indexBufferGPU = BaseUtil::CreateDefaultBuffer(m_d3dDevice.Get(),
          m_commandList.Get(), indices.data(), ibByteSize, geo->indexBufferUploader);

      geo->vertexByteStride     = sizeof(ShaderVertex);
      geo->vertexBufferByteSize = vbByteSize;
      geo->indexFormat          = DXGI_FORMAT_R16_UINT;
      geo->indexBufferByteSize  = ibByteSize;

      geo->drawArgs["floor"]  = floorSubmesh;
      geo->drawArgs["wall"]   = wallSubmesh;
      geo->drawArgs["mirror"] = mirrorSubmesh;
      
      SubmeshGeometry boxSubmesh;
      boxSubmesh.indexCount = static_cast<unsigned int>(boxIndices.size());
      boxSubmesh.baseVertexLocation = 0;
      boxSubmesh.startIndexLocation = 0;

      unique_ptr<MeshGeometry> boxGeo = make_unique<MeshGeometry>();
      boxGeo->name = "box";
      ThrowIfFailed(D3DCreateBlob(boxVbByteSize, &boxGeo->vertexBufferCPU));
      CopyMemory(boxGeo->vertexBufferCPU->GetBufferPointer(), boxVertices.data(), vbByteSize);

      ThrowIfFailed(D3DCreateBlob(boxIbByteSize, &boxGeo->indexBufferCPU));
      CopyMemory(boxGeo->indexBufferCPU->GetBufferPointer(), boxIndices.data(), ibByteSize);

      boxGeo->vertexBufferGPU = BaseUtil::CreateDefaultBuffer(m_d3dDevice.Get(),
          m_commandList.Get(), boxVertices.data(), boxVbByteSize, boxGeo->vertexBufferUploader);

      boxGeo->indexBufferGPU = BaseUtil::CreateDefaultBuffer(m_d3dDevice.Get(),
          m_commandList.Get(), boxIndices.data(), boxIbByteSize, boxGeo->indexBufferUploader);
      boxGeo->vertexByteStride = sizeof(ShaderVertex);
      boxGeo->vertexBufferByteSize = boxVbByteSize;
      boxGeo->indexFormat = DXGI_FORMAT_R16_UINT;
      boxGeo->indexBufferByteSize = boxIbByteSize;
      boxGeo->drawArgs["box"] = boxSubmesh;


      m_geometries[boxGeo->name] = std::move(boxGeo); // box
      m_geometries[geo->name] = std::move(geo); // room
  }

  void BuildRenderItems()
  {
      auto floorRitem = std::make_unique<RenderObject>();
      floorRitem->worldTransform = MathHelper::Identity4x4();
      floorRitem->texTransform = MathHelper::Identity4x4();
      floorRitem->objectCbIndex = 0;
      floorRitem->pMat = m_materials["checker"].get();
      floorRitem->pGeo = m_geometries["roomGeo"].get();
      floorRitem->primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
      floorRitem->indexCount = floorRitem->pGeo->drawArgs["floor"].indexCount;
      floorRitem->startIndexLocation = floorRitem->pGeo->drawArgs["floor"].startIndexLocation;
      floorRitem->baseVertexLocation = floorRitem->pGeo->drawArgs["floor"].baseVertexLocation;
      //m_renderLayer[(int)RenderLayer::Opaque].push_back(floorRitem.get());

      auto wallsRitem = std::make_unique<RenderObject>();
      wallsRitem->worldTransform = MathHelper::Identity4x4();
      wallsRitem->texTransform = MathHelper::Identity4x4();
      wallsRitem->objectCbIndex = 1;
      wallsRitem->pMat = m_materials["bricks"].get();
      wallsRitem->pGeo = m_geometries["roomGeo"].get();
      wallsRitem->primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
      wallsRitem->indexCount = wallsRitem->pGeo->drawArgs["wall"].indexCount;
      wallsRitem->startIndexLocation = wallsRitem->pGeo->drawArgs["wall"].startIndexLocation;
      wallsRitem->baseVertexLocation = wallsRitem->pGeo->drawArgs["wall"].baseVertexLocation;
      //m_renderLayer[(int)RenderLayer::Opaque].push_back(wallsRitem.get());

      auto mirrorRitem = std::make_unique<RenderObject>();
      mirrorRitem->worldTransform = MathHelper::Identity4x4();
      mirrorRitem->texTransform = MathHelper::Identity4x4();
      mirrorRitem->objectCbIndex = 2;
      mirrorRitem->pMat = m_materials["checker"].get();
      mirrorRitem->pGeo = m_geometries["roomGeo"].get();
      mirrorRitem->primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
      mirrorRitem->indexCount = mirrorRitem->pGeo->drawArgs["mirror"].indexCount;
      mirrorRitem->startIndexLocation = mirrorRitem->pGeo->drawArgs["mirror"].startIndexLocation;
      mirrorRitem->baseVertexLocation = mirrorRitem->pGeo->drawArgs["mirror"].baseVertexLocation;
      //m_renderLayer[(int)RenderLayer::Mirrors].push_back(mirrorRitem.get());
      //m_renderLayer[(int)RenderLayer::Transparent].push_back(mirrorRitem.get());
      //m_renderLayer[(int)RenderLayer::Opaque].push_back(wallsRitem.get());

      unique_ptr<RenderObject> boxItem = make_unique<RenderObject>();
      XMStoreFloat4x4(&boxItem->worldTransform, (XMMatrixMultiply(XMMatrixScaling(0.5f, 0.5f, 0.5f),
                                                                  XMMatrixTranslation(0.0f, 2.0f, -4.0f))));
      boxItem->texTransform = MathHelper::Identity4x4();
      boxItem->objectCbIndex = 3;
      boxItem->pMat = m_materials["bricks"].get();
      boxItem->pGeo = m_geometries["box"].get();
      boxItem->primitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
      boxItem->indexCount = boxItem->pGeo->drawArgs["box"].indexCount;
      boxItem->startIndexLocation = boxItem->pGeo->drawArgs["box"].startIndexLocation;
      boxItem->baseVertexLocation = boxItem->pGeo->drawArgs["box"].baseVertexLocation;

      m_allRenderObjects.push_back(std::move(floorRitem));
      m_allRenderObjects.push_back(std::move(wallsRitem));
      m_allRenderObjects.push_back(std::move(mirrorRitem));
      m_allRenderObjects.push_back(std::move(boxItem));
  }

  void BuildMaterials()
  {
    unique_ptr<MaterialInfo> bricks = make_unique<MaterialInfo>();
    bricks->textureIndex = 0;

    unique_ptr<MaterialInfo> checker = make_unique<MaterialInfo>();
    checker->textureIndex = 1;

    m_materials["bricks"] = std::move(bricks);
    m_materials["checker"] = std::move(checker);
  }

  void BuildShadersAndInputLayout()
  {
      m_shaders["standardVS"] = BaseUtil::CompileShader(L"shaders\\stenciling.hlsl", nullptr, "VS", "vs_5_0");
      m_shaders["opaquePS"] = BaseUtil::CompileShader(L"shaders\\stenciling.hlsl", nullptr, "PS", "ps_5_0");

      m_inputLayout =
      {
          { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
          { "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
          { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
      };
  }

  void BuildRootSignature()
  {
      CD3DX12_DESCRIPTOR_RANGE texTable;
      texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0); // t0

      CD3DX12_ROOT_PARAMETER rootParam[3];
      rootParam[0].InitAsConstantBufferView(0);
      rootParam[1].InitAsConstantBufferView(1);
      rootParam[2].InitAsDescriptorTable(1, &texTable);

      CD3DX12_ROOT_SIGNATURE_DESC rootSignDesc(3, rootParam, (UINT)m_staticSamplers.size(),
        m_staticSamplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

      ComPtr<ID3DBlob> serializedRootSig = nullptr;
      ComPtr<ID3DBlob> errorBlob = nullptr;
      HRESULT hr = D3D12SerializeRootSignature(&rootSignDesc, D3D_ROOT_SIGNATURE_VERSION_1,
          serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

      if (errorBlob != nullptr)
      {
          ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
      }
      ThrowIfFailed(hr);

      ThrowIfFailed(m_d3dDevice->CreateRootSignature(0,
                                                     serializedRootSig->GetBufferPointer(),
                                                     serializedRootSig->GetBufferSize(),
                                                     IID_PPV_ARGS(m_rootSignature.GetAddressOf())));
  }

  // Builds the constant buffers etc needed in the shader program.
  void InitShaderResources()
  {

      m_objectCB = std::make_unique<UploadBuffer<ObjectCb>>(m_d3dDevice.Get(), NumObjects, true);
      m_passCB   = std::make_unique<UploadBuffer<PassCb>>(m_d3dDevice.Get(), 1, true);
  }

  // Build descriptor heaps and shader resource views for our textures.
  void BuildShaderViews()
  {
      D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
      srvHeapDesc.NumDescriptors = 2; // Two textures for now.
      srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
      srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
      ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&m_srvDescriptorHeap)));

      CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(m_srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());

      auto bricksRes = m_textures["bricksTex"]->resource_;
      auto checkerRes = m_textures["checkerboardTex"]->resource_;

      D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
      srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
      srvDesc.Format                          = bricksRes->GetDesc().Format;
      srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURE2D;
      srvDesc.Texture2D.MostDetailedMip       = 0;
      srvDesc.Texture2D.MipLevels             = -1;
      m_d3dDevice->CreateShaderResourceView(bricksRes.Get(), &srvDesc, hDescriptor);

      hDescriptor.Offset(1, m_cbvSrvUavDescriptorSize);
      srvDesc.Format = checkerRes->GetDesc().Format;
      m_d3dDevice->CreateShaderResourceView(checkerRes.Get(), &srvDesc, hDescriptor);
  }

  // Builds shaders, descriptors and pipelines.
  void BuildPipelines()
  {
    InitShaderResources();
    BuildRootSignature();
    BuildShadersAndInputLayout();

    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc;

    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    opaquePsoDesc.InputLayout = { m_inputLayout.data(), (UINT)m_inputLayout.size() };
    opaquePsoDesc.pRootSignature = m_rootSignature.Get();
    opaquePsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(m_shaders["standardVS"]->GetBufferPointer()),
        m_shaders["standardVS"]->GetBufferSize()
    };
    opaquePsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(m_shaders["opaquePS"]->GetBufferPointer()),
        m_shaders["opaquePS"]->GetBufferSize()
    };
    opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    //opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    opaquePsoDesc.SampleMask = UINT_MAX;
    opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    opaquePsoDesc.NumRenderTargets = 1;
    opaquePsoDesc.RTVFormats[0] = m_backBufferFormat;
    opaquePsoDesc.SampleDesc.Count = m_4xMsaaEn ? 4 : 1;
    opaquePsoDesc.SampleDesc.Quality = m_4xMsaaEn ? (m_4xMsaaQuality - 1) : 0;
    opaquePsoDesc.DSVFormat = m_depthStencilFormat;
    ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc,
                                                           IID_PPV_ARGS(&m_pipelines["opaque"])));
  }


  virtual bool Initialize() override
  {
    // Initializes main window and d3d.
    bool result = BaseApp::Initialize();

    if (result == true) {
        ThrowIfFailed(m_commandList.Get()->Reset(m_directCmdListAlloc.Get(), nullptr));

        LoadTextures();
        BuildSceneGeometry();
        BuildMaterials();
        BuildShaderViews();
        BuildPipelines();
        BuildRenderItems();

        // After writing the commands needed to build resources, which involves uploading
        // some of them to GPU memory, submit the commands.
        ThrowIfFailed(m_commandList->Close());
        ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
        FlushCommandQueue(); // Queue-submit.
    }
    else {
        ::OutputDebugStringA("Error initializing stencil demo\n");
    }

    return result;
  }

  // Member variables.
  ComPtr<ID3D12RootSignature>                             m_rootSignature   = nullptr;
  std::unique_ptr<UploadBuffer<ObjectCb>>                 m_objectCB        = nullptr;
  std::unique_ptr<UploadBuffer<PassCb>>                   m_passCB          = nullptr;
  std::vector<std::unique_ptr<RenderObject>>              m_allRenderObjects;
  std::unordered_map<string, unique_ptr<MeshGeometry>>    m_geometries;
  std::vector<RenderObject*>                              m_renderLayer[(int)RenderLayer::Count];
  std::unordered_map<std::string, ComPtr<ID3DBlob>>       m_shaders;
  std::unordered_map<std::string, unique_ptr<Texture>>    m_textures;
  std::vector<D3D12_INPUT_ELEMENT_DESC>                   m_inputLayout;
  std::unordered_map<string, ComPtr<ID3D12PipelineState>> m_pipelines;
  XMFLOAT3                                                m_eyePos = { 0.0f, 0.0f, 0.0f };
  XMFLOAT4X4                                              m_view = MathHelper::Identity4x4();
  XMFLOAT4X4                                              m_proj = MathHelper::Identity4x4();
  float                                                   m_theta = 1.24f * XM_PI;
  float                                                   m_phi = 0.42f * XM_PI;
  float                                                   m_radius = 20.0f;
  POINT                                                   m_lastMousePos;
  ComPtr<ID3D12DescriptorHeap>                            m_srvDescriptorHeap = nullptr;
  std::unordered_map<string, unique_ptr<MaterialInfo>>    m_materials;


  const std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6>  m_staticSamplers =
  {
      // Applications usually only need a handful of samplers.  So just define them all up front
      // and keep them available as part of the root signature.

      CD3DX12_STATIC_SAMPLER_DESC(// pointWrap
          0, // shaderRegister
          D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
          D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
          D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
          D3D12_TEXTURE_ADDRESS_MODE_WRAP), // addressW

      CD3DX12_STATIC_SAMPLER_DESC( // pointClamp
          1, // shaderRegister
          D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
          D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
          D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
          D3D12_TEXTURE_ADDRESS_MODE_CLAMP), // addressW

      CD3DX12_STATIC_SAMPLER_DESC( // linearWrap
          2, // shaderRegister
          D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
          D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
          D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
          D3D12_TEXTURE_ADDRESS_MODE_WRAP), // addressW

      CD3DX12_STATIC_SAMPLER_DESC( // linearClamp
          3, // shaderRegister
          D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
          D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
          D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
          D3D12_TEXTURE_ADDRESS_MODE_CLAMP), // addressW

      CD3DX12_STATIC_SAMPLER_DESC( // anisotropicWrap
          4, // shaderRegister
          D3D12_FILTER_ANISOTROPIC, // filter
          D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
          D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
          D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
          0.0f,                             // mipLODBias
          8),                               // maxAnisotropy

      CD3DX12_STATIC_SAMPLER_DESC( // anisotropicClamp
          5, // shaderRegister
          D3D12_FILTER_ANISOTROPIC, // filter
          D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
          D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
          D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
          0.0f,                              // mipLODBias
          8),                                // maxAnisotropy
  };

}; // Class StencilDemo

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR cmdLine, int showCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
  _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

  try
    {
      StencilDemo demoApp(hInstance);
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

Stenciling demo app.
TODO:
- Write WinMain
- Demo class
- Initialize function
- Clear the screen with some color.

- Camera controls
- Load basic 3D geometry
    - write 3d model loader.
- Implement lighting
- Implement texturing
- Implement shadows
- Implement stenciling mirror.
- Move common types to vkCommon.
 ***********************************************************************/
