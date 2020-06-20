/*
Instancing and frustum culling demo.
- texture the objects in the scene


*/
#include <array>
#include <unordered_map>
#include <string>
#include <memory>
#include <vector>
#include <DirectXColors.h>
#include "BaseApp.h"
#include "BaseUtil.h"
#include "BaseTimer.h"
#include "UploadBuffer.h"
#include "../common/GeometryGenerator.h"
#include "../common/d3dx12.h"

using namespace std;
using namespace DirectX;
using Microsoft::WRL::ComPtr;
using uint = UINT;

#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "D3D12.lib")

// ======================================================================
struct ShaderVertex {
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT4 color;
    DirectX::XMFLOAT2 tex;
};

struct SceneConstants {
    XMFLOAT4X4 viewMatrix = MathHelper::Identity4x4();
    XMFLOAT4X4 projMatrix = MathHelper::Identity4x4();
};

struct InstanceData {
    XMFLOAT4X4 worldMatrix = MathHelper::Identity4x4();
    uint materialIndex;
    uint padding0;
    uint padding1;
    uint padding2;
};

struct ShaderMaterialData {
    XMFLOAT4 color;
};

struct ShaderPerObjectData {
    uint materialIndex;
    uint padding0;
    uint padding1;
    uint padding2;
};

// ======================================================================
class Camera
{
public:
    Camera() {}
    ~Camera() {
    }
    void SetPosition(float x, float y, float z) {
        mPosition  = XMFLOAT3(x, y, z);
        mViewDirty = true;
    }
    void SetLens(float fovY, float aspect, float zn, float zf) {
        mFovY             = fovY;
        mAspect           = aspect;
        mNearZ            = zn;
        mFarZ             = zf;
        mNearWindowHeight = 2.0f * mNearZ * tanf(0.5f*mFovY);
        mFarWindowHeight  = 2.0f * mFarZ * tanf(0.5f*mFovY);
        XMMATRIX P        = XMMatrixPerspectiveFovLH(mFovY, mAspect, mNearZ, mFarZ);
        XMStoreFloat4x4(&mProj, P);
    }
    XMMATRIX GetView() const {
        assert(!mViewDirty);
        return XMLoadFloat4x4(&mView);
    }
    XMMATRIX GetProj() const { return XMLoadFloat4x4(&mProj); }
    void Walk(float d) {
        XMVECTOR s = XMVectorReplicate(d);
        XMVECTOR l = XMLoadFloat3(&mLook);
        XMVECTOR p = XMLoadFloat3(&mPosition);
        XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, l, p));
        mViewDirty = true;
    }
    void Strafe(float d) {
        // mPosition += d*mRight
        XMVECTOR s = XMVectorReplicate(d);
        XMVECTOR r = XMLoadFloat3(&mRight);
        XMVECTOR p = XMLoadFloat3(&mPosition);
        XMStoreFloat3(&mPosition, XMVectorMultiplyAdd(s, r, p));
        mViewDirty = true;
    }
    void UpdateViewMatrix() {
        if (mViewDirty)
        {
            XMVECTOR R = XMLoadFloat3(&mRight);
            XMVECTOR U = XMLoadFloat3(&mUp);
            XMVECTOR L = XMLoadFloat3(&mLook);
            XMVECTOR P = XMLoadFloat3(&mPosition);
            // Keep camera's axes orthogonal to each other and of unit length.
            L = XMVector3Normalize(L);
            U = XMVector3Normalize(XMVector3Cross(L, R));
            // U, L already ortho-normal, so no need to normalize cross product.
            R = XMVector3Cross(U, L);
            // Fill in the view matrix entries.
            float x = -XMVectorGetX(XMVector3Dot(P, R));
            float y = -XMVectorGetX(XMVector3Dot(P, U));
            float z = -XMVectorGetX(XMVector3Dot(P, L));
            XMStoreFloat3(&mRight, R);
            XMStoreFloat3(&mUp, U);
            XMStoreFloat3(&mLook, L);
            mView(0, 0) = mRight.x;
            mView(1, 0) = mRight.y;
            mView(2, 0) = mRight.z;
            mView(3, 0) = x;
            mView(0, 1) = mUp.x;
            mView(1, 1) = mUp.y;
            mView(2, 1) = mUp.z;
            mView(3, 1) = y;
            mView(0, 2) = mLook.x;
            mView(1, 2) = mLook.y;
            mView(2, 2) = mLook.z;
            mView(3, 2) = z;
            mView(0, 3) = 0.0f;
            mView(1, 3) = 0.0f;
            mView(2, 3) = 0.0f;
            mView(3, 3) = 1.0f;
            mViewDirty = false;
        }
    }
    void Camera::Pitch(float angle) {
        // Rotate up and look vector about the right vector.
        XMMATRIX R = XMMatrixRotationAxis(XMLoadFloat3(&mRight), angle);
        XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
        XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));
        mViewDirty = true;
    }
    void Camera::RotateY(float angle) {
        // Rotate the basis vectors about the world y-axis.
        XMMATRIX R = XMMatrixRotationY(angle);
        XMStoreFloat3(&mRight, XMVector3TransformNormal(XMLoadFloat3(&mRight), R));
        XMStoreFloat3(&mUp, XMVector3TransformNormal(XMLoadFloat3(&mUp), R));
        XMStoreFloat3(&mLook, XMVector3TransformNormal(XMLoadFloat3(&mLook), R));
        mViewDirty = true;
    }
private:
    // Camera coordinate system with coordinates relative to world space.
    DirectX::XMFLOAT3 mPosition = { 0.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 mRight    = { 1.0f, 0.0f, 0.0f };
    DirectX::XMFLOAT3 mUp       = { 0.0f, 1.0f, 0.0f };
    DirectX::XMFLOAT3 mLook     = { 0.0f, 0.0f, 1.0f };
    // Cache frustum properties.
    float mNearZ            = 0.0f;
    float mFarZ             = 0.0f;
    float mAspect           = 0.0f;
    float mFovY             = 0.0f;
    float mNearWindowHeight = 0.0f;
    float mFarWindowHeight  = 0.0f;
    bool mViewDirty         = true;
    // Cache View/Proj matrices.
    DirectX::XMFLOAT4X4 mView = MathHelper::Identity4x4();
    DirectX::XMFLOAT4X4 mProj = MathHelper::Identity4x4();
};

// ======================================================================
class InstancingCullingDemo : public BaseApp
{
public:
    InstancingCullingDemo(HINSTANCE hInst)
    : BaseApp(hInst){
    }
    ~InstancingCullingDemo() {
        if (m_d3dDevice != nullptr) {
            FlushCommandQueue();
        }
    }
    bool Initialize() {
        bool ret = BaseApp::Initialize();
        if (ret) {
            ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), nullptr));
            m_cbvSrvUavDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
            mCamera.SetPosition(0.0f, 2.0f, -15.0f);
            LoadTextures();
            BuildMaterials();
            BuildGeometry();
            BuildDescriptorsAndViews();
            BuildShaders();
            BuildRootSignature();
            BuildPipelines();
            ThrowIfFailed(m_commandList->Close());
            ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
            m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
            FlushCommandQueue();
        }
        else {
            ::OutputDebugStringA("Error! - Initialize() failed!");
        }
        return ret;
    }

protected:
    void LoadTextures() {
        OutputDebugStringA("Loading textures from: ..\\..\\..\\projects\\Textures\\\n");
        auto bricks1 = make_unique<Texture>();
        bricks1->name_ = "bricks1";
        bricks1->filename_ = L"..\\..\\..\\projects\\Textures\\bricks.dds";
        ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(m_d3dDevice.Get(), m_commandList.Get(), bricks1->filename_.c_str(), bricks1->resource_, bricks1->uploadHeap_));
        auto bricks2 = make_unique<Texture>();
        bricks2->name_ = "bricks2";
        bricks2->filename_ = L"..\\..\\..\\projects\\Textures\\bricks2.dds";
        ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(m_d3dDevice.Get(), m_commandList.Get(), bricks2->filename_.c_str(), bricks2->resource_, bricks2->uploadHeap_));
        auto bricks3 = make_unique<Texture>();
        bricks3->name_ = "bricks3";
        bricks3->filename_ = L"..\\..\\..\\projects\\Textures\\bricks3.dds";
        ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(m_d3dDevice.Get(), m_commandList.Get(), bricks3->filename_.c_str(), bricks3->resource_, bricks3->uploadHeap_));
        mTextures[bricks1->name_] = move(bricks1);
        mTextures[bricks2->name_] = move(bricks2);
        mTextures[bricks3->name_] = move(bricks3);
    }
    void OnKeyboardInput(const BaseTimer& gt) {
        const float dt = gt.DeltaTimeInSecs();
        if (GetAsyncKeyState('W') & 0x8000) {
            mCamera.Walk(10.0f*dt);
        }
        if (GetAsyncKeyState('S') & 0x8000) {
            mCamera.Walk(-10.0f*dt);
        }
        if (GetAsyncKeyState('A') & 0x8000) {
            mCamera.Strafe(-10.0f*dt);
        }
        if (GetAsyncKeyState('D') & 0x8000) {
            mCamera.Strafe(10.0f*dt);
        }
        mCamera.UpdateViewMatrix();
    }
    void OnMouseDown(WPARAM btnState, int x, int y)override {
        mLastMousePos.x = x;
        mLastMousePos.y = y;
        SetCapture(mhMainWnd);
    }
    void OnMouseUp(WPARAM btnState, int x, int y)override {
        ReleaseCapture();
    }
    void OnMouseMove(WPARAM btnState, int x, int y)override {
        if ((btnState & MK_LBUTTON) != 0) {
            float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
            float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));
            mCamera.Pitch(dy);
            mCamera.RotateY(dx);
        }
        mLastMousePos.x = x;
        mLastMousePos.y = y;
    }
    void Draw(const BaseTimer& gt) {
        ThrowIfFailed(m_directCmdListAlloc->Reset());
        ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), mSimplePipeline.Get()));
        m_commandList->RSSetViewports(1, &m_screenViewport);
        m_commandList->RSSetScissorRects(1, &m_scissorRect);
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));
        m_commandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSlateGray, 0, nullptr);
        m_commandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
        m_commandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());
        m_commandList->IASetVertexBuffers(0, 1, &mGeometries["scene"]->VertexBufferView());
        m_commandList->IASetIndexBuffer(&mGeometries["scene"]->IndexBufferView());
        m_commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

        ID3D12DescriptorHeap* descriptorHeaps[] = { mTextureSrvHeap.Get() };
        m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
        m_commandList->SetGraphicsRootSignature(mRootSignature.Get());
        m_commandList->SetGraphicsRootConstantBufferView(0, mSceneConstants->Resource()->GetGPUVirtualAddress());
        m_commandList->SetGraphicsRootConstantBufferView(1, mObjectBuffer->Resource()->GetGPUVirtualAddress());
        m_commandList->SetGraphicsRootShaderResourceView(2, mMatBuffer->Resource()->GetGPUVirtualAddress());
        m_commandList->SetGraphicsRootShaderResourceView(3, mInstDataBuffer->Resource()->GetGPUVirtualAddress());
        m_commandList->SetGraphicsRootDescriptorTable(4, mTextureSrvHeap->GetGPUDescriptorHandleForHeapStart());

        //m_commandList->DrawIndexedInstanced(mGeometries["scene"]->drawArgs["grid"].indexCount, 1, 0, 0, 0);
        uint objCBByteSize = BaseUtil::CalcConstantBufferByteSize(sizeof(ShaderPerObjectData));
        m_commandList->SetGraphicsRootConstantBufferView(1, mObjectBuffer->Resource()->GetGPUVirtualAddress() + objCBByteSize);
        const auto& boxDrawArgs = mGeometries["scene"]->drawArgs["box"];
        m_commandList->DrawIndexedInstanced(boxDrawArgs.indexCount, static_cast<UINT>(mBoxInstances.size()), boxDrawArgs.startIndexLocation, boxDrawArgs.baseVertexLocation, 0);
        m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
        ThrowIfFailed(m_commandList->Close());
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
    void Update(const BaseTimer& gt) {
        OnKeyboardInput(gt);
        XMMATRIX view = mCamera.GetView();
        XMMATRIX proj = mCamera.GetProj();
        //XMMATRIX world = XMLoadFloat4x4(&mWorld);
        //XMMATRIX worldViewProj = world * view * proj;
        SceneConstants vpMatrix = { };
        XMStoreFloat4x4(&vpMatrix.viewMatrix, XMMatrixTranspose(view));
        XMStoreFloat4x4(&vpMatrix.projMatrix, XMMatrixTranspose(proj));
        mSceneConstants->CopyData(0, vpMatrix);
        if ((m_currentFence != 0) && (m_fence->GetCompletedValue() < m_currentFence)) {
            HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
            ThrowIfFailed(m_fence->SetEventOnCompletion(m_currentFence, eventHandle));
            WaitForSingleObject(eventHandle, INFINITE);
            CloseHandle(eventHandle);
        }
    }
    void OnResize() {
        BaseApp::OnResize();
        mCamera.SetLens(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.f);
    }
    void BuildPipelines() {
        D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = { };
        ZeroMemory(&psoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
        psoDesc.InputLayout = {
            mInputLayout.data(), (UINT)mInputLayout.size()
        };
        psoDesc.pRootSignature = mRootSignature.Get();
        psoDesc.VS = {
            reinterpret_cast<BYTE*>(mShaders["simpleVS"]->GetBufferPointer()),
            mShaders["simpleVS"]->GetBufferSize()
        };
        psoDesc.PS = {
            reinterpret_cast<BYTE*>(mShaders["simplePS"]->GetBufferPointer()),
            mShaders["simplePS"]->GetBufferSize()
        };
        psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
        //psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
        psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;
        psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
        psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
        psoDesc.SampleMask = UINT_MAX;
        psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
        psoDesc.NumRenderTargets = 1;
        psoDesc.RTVFormats[0] = m_backBufferFormat;
        psoDesc.SampleDesc.Count = m_4xMsaaEn ? 4 : 1;
        psoDesc.SampleDesc.Quality = m_4xMsaaEn ? (m_4xMsaaQuality - 1) : 0;
        psoDesc.DSVFormat = m_depthStencilFormat;
        HRESULT hr = m_d3dDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&mSimplePipeline));
        ThrowIfFailed(hr);
    }
    void BuildRootSignature() {
        assert(mTextures.size() > 0);
        CD3DX12_DESCRIPTOR_RANGE texTable;
        texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, static_cast<uint>(mTextures.size()), 0, 0); // t0, space0
        CD3DX12_ROOT_PARAMETER slotRootParams[5];
        slotRootParams[0].InitAsConstantBufferView(0); // MVP matrix
        slotRootParams[1].InitAsConstantBufferView(1);  // per-object data
        slotRootParams[2].InitAsShaderResourceView(0, 1); // all material data (t0, space1)
        slotRootParams[3].InitAsShaderResourceView(1, 1); // instance data (t1, space1)
        slotRootParams[4].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);  // all the textures
        auto samplers = GetStaticSamplers();
        CD3DX12_ROOT_SIGNATURE_DESC rootSignDesc((sizeof(slotRootParams) / sizeof(CD3DX12_ROOT_PARAMETER)), slotRootParams, (uint)samplers.size(), samplers.data(), D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
        ComPtr<ID3DBlob> serializedRootSig = nullptr;
        ComPtr<ID3DBlob> errorBlob         = nullptr;
        HRESULT hr                         = D3D12SerializeRootSignature(&rootSignDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
        if (errorBlob != nullptr) {
            ::OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
        }
        ThrowIfFailed(hr);
        ThrowIfFailed(m_d3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
    }
    void BuildDescriptorsAndViews() {
        assert(mMaterials.size() > 0);
        assert(mGeometries.size() > 0);
        assert(mGeometries["scene"]->drawArgs.size() > 0);
        assert(mTextures.size() > 0);

        mSceneConstants           = make_unique<UploadBuffer<SceneConstants>>(m_d3dDevice.Get(), 1, true);
        mMatBuffer                = make_unique<UploadBuffer<ShaderMaterialData>>(m_d3dDevice.Get(), mMaterials.size(), false);
        int mIndex                = 0;
        for (auto& mat : mMaterials) {
            mMatBuffer->CopyData(mIndex++, *mat.second.get());
        }
        const uint NumObjects = static_cast<uint>(mGeometries["scene"]->drawArgs.size());
        mObjectBuffer = make_unique<UploadBuffer<ShaderPerObjectData>>(m_d3dDevice.Get(), NumObjects, true);
        for (uint i = 0; i < NumObjects; i++) {
            ShaderPerObjectData objData = {};
            objData.materialIndex = i;
            mObjectBuffer->CopyData(i, objData);
        }
        D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
        heapDesc.NumDescriptors             = static_cast<uint>(mTextures.size());
        heapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        heapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&mTextureSrvHeap)));
        CD3DX12_CPU_DESCRIPTOR_HANDLE hDesc(mTextureSrvHeap->GetCPUDescriptorHandleForHeapStart());
        auto bricks1Res                         = mTextures["bricks1"]->resource_;
        D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
        srvDesc.Shader4ComponentMapping         = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
        srvDesc.Format                          = bricks1Res->GetDesc().Format;
        srvDesc.ViewDimension                   = D3D12_SRV_DIMENSION_TEXTURE2D;
        srvDesc.Texture2D.MostDetailedMip       = 0;
        srvDesc.Texture2D.MipLevels             = bricks1Res->GetDesc().MipLevels;
        srvDesc.Texture2D.ResourceMinLODClamp   = 0.0f;
        m_d3dDevice->CreateShaderResourceView(bricks1Res.Get(), &srvDesc, hDesc);
        auto bricks2Res             = mTextures["bricks2"]->resource_;
        srvDesc.Format              = bricks2Res->GetDesc().Format;
        srvDesc.Texture2D.MipLevels = bricks2Res->GetDesc().MipLevels;
        hDesc.Offset(1, m_cbvSrvUavDescriptorSize);
        m_d3dDevice->CreateShaderResourceView(bricks2Res.Get(), &srvDesc, hDesc);
        auto bricks3Res             = mTextures["bricks3"]->resource_;
        srvDesc.Format              = bricks3Res->GetDesc().Format;
        srvDesc.Texture2D.MipLevels = bricks3Res->GetDesc().MipLevels;
        hDesc.Offset(1, m_cbvSrvUavDescriptorSize);
        m_d3dDevice->CreateShaderResourceView(bricks3Res.Get(), &srvDesc, hDesc);
    }
    void BuildShaders() {
        OutputDebugStringA("Building shader - ..\\..\\..\\projects\\instancing_culling\\shaders\\simpleRender.hlsl\n");
        mShaders["simpleVS"] = BaseUtil::CompileShader(L"..\\..\\..\\projects\\instancing_culling\\shaders\\simpleRender.hlsl", nullptr, "SimpleVS", "vs_5_1");
        mShaders["simplePS"] = BaseUtil::CompileShader(L"..\\..\\..\\projects\\instancing_culling\\shaders\\simpleRender.hlsl", nullptr, "SimplePS", "ps_5_1");
        mInputLayout = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 28, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        };
    }
    void BuildGeometry() {
        GeometryGenerator generator;
        MeshData grid                  = generator.CreateGrid(20.0f, 20.0f, 40, 40);
        MeshData box                   = generator.CreateBox(2.0f, 2.0f, 2.0f, 1); // box comes after grid.
        uint gridVertexOffset          = static_cast<uint>(0);
        uint gridIndexOffset           = static_cast<uint>(0);
        uint boxVertexOffset           = static_cast<uint>(gridVertexOffset + grid.m_vertices.size());
        uint boxIndexOffset            = static_cast<uint>(gridIndexOffset + grid.m_indices32.size());
        SubmeshGeometry gridSubmesh    = {};
        gridSubmesh.indexCount         = static_cast<uint>(grid.m_indices32.size());
        gridSubmesh.startIndexLocation = gridIndexOffset;
        gridSubmesh.baseVertexLocation = gridVertexOffset;
        SubmeshGeometry boxSubmesh     = {};
        boxSubmesh.indexCount          = static_cast<uint>(box.m_indices32.size());
        boxSubmesh.startIndexLocation  = boxIndexOffset;
        boxSubmesh.baseVertexLocation  = boxVertexOffset;
        const uint NumVertices         = static_cast<uint>(grid.m_vertices.size() + box.m_vertices.size());
        const uint NumIndices          = static_cast<uint>(grid.m_indices32.size() + box.m_indices32.size());
        vector<ShaderVertex> vertices(NumVertices);
        uint k = 0;
        for (size_t i = 0; i < grid.m_vertices.size(); ++i, k++) {
            vertices[k].pos = grid.m_vertices[i].m_position;
            vertices[k].color = XMFLOAT4(DirectX::Colors::DarkGreen);
            vertices[k].tex = grid.m_vertices[i].m_texC;
        }
        for (size_t i = 0; i < box.m_vertices.size(); ++i, k++) {
            vertices[k].pos = box.m_vertices[i].m_position;
            vertices[k].color = XMFLOAT4(DirectX::Colors::Maroon);
            vertices[k].tex = box.m_vertices[i].m_texC;
        }
        vector<uint32_t> indices;
        indices.insert(indices.end(), cbegin(grid.m_indices32), cend(grid.m_indices32));
        indices.insert(indices.end(), cbegin(box.m_indices32), cend(box.m_indices32));
        const UINT vbNumBytes             = NumVertices * sizeof(ShaderVertex);
        const UINT ibNumBytes             = NumIndices * sizeof(uint32_t);
        unique_ptr<MeshGeometry> geometry = make_unique<MeshGeometry>();
        geometry->name                    = "scene";
        ThrowIfFailed(D3DCreateBlob(vbNumBytes, &geometry->vertexBufferCPU));
        CopyMemory(geometry->vertexBufferCPU->GetBufferPointer(), vertices.data(), vbNumBytes);
        ThrowIfFailed(D3DCreateBlob(ibNumBytes, &geometry->indexBufferCPU));
        CopyMemory(geometry->indexBufferCPU->GetBufferPointer(), indices.data(), ibNumBytes);
        geometry->vertexBufferGPU      = BaseUtil::CreateDefaultBuffer(m_d3dDevice.Get(), m_commandList.Get(), vertices.data(), vbNumBytes, geometry->vertexBufferUploader);
        geometry->indexBufferGPU       = BaseUtil::CreateDefaultBuffer(m_d3dDevice.Get(), m_commandList.Get(), indices.data(), ibNumBytes, geometry->indexBufferUploader);
        geometry->vertexByteStride     = sizeof(ShaderVertex);
        geometry->vertexBufferByteSize = vbNumBytes;
        geometry->indexFormat          = DXGI_FORMAT_R32_UINT;
        geometry->indexBufferByteSize  = ibNumBytes;
        geometry->drawArgs["grid"]     = gridSubmesh;
        geometry->drawArgs["box"]      = boxSubmesh;
        mGeometries[geometry->name]    = move(geometry);

        assert(mTextures.size() > 0);
        int matIndex = 0;
        float width = 20.0f;
        float height = 20.0f;
        float depth = 20.0f;
        float sX = -0.5f * width;
        float sY = -0.5f * height;
        float sZ = -0.5f * height;
        float dx = width / 4;
        float dy = height / 4;
        float dz = depth / 4;
        for (int x = 0; x < 5; x++) {
            for (int y = 0; y < 5; y++) {
                for (int z = 0; z < 5; z++) {
                    InstanceData instData = {};
                    instData.worldMatrix = XMFLOAT4X4(
                        1.0f, 0.0f, 0.0f, sX + x * dx,
                        0.0f, 1.0f, 0.0f, sY + y * dy,
                        0.0f, 0.0f, 1.0f, sZ + z * dz,
                        0.0f, 0.0f, 0.0f, 1.0f);
                    instData.materialIndex = (++matIndex % mTextures.size());
                    mBoxInstances.push_back(instData);
                }
            }
        }
        mInstDataBuffer = make_unique<UploadBuffer<InstanceData>>(m_d3dDevice.Get(), mBoxInstances.size(), false);
        for (int i = 0; i < mBoxInstances.size(); i++) {
            mInstDataBuffer->CopyData(i, mBoxInstances[i]);
        }
    }
    void BuildMaterials() {
        mMaterials["darkGreen"]              = make_unique<ShaderMaterialData>();
        mMaterials["maroon"]                 = make_unique<ShaderMaterialData>();
        mMaterials["darkGreen"].get()->color = XMFLOAT4(DirectX::Colors::DarkGreen);
        mMaterials["maroon"].get()->color    = XMFLOAT4(DirectX::Colors::Maroon);
    }

private:
    array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers() {
        const CD3DX12_STATIC_SAMPLER_DESC pointWrap(0, // shaderRegister
                                                    D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
                                                    D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
                                                    D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
                                                    D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW
        const CD3DX12_STATIC_SAMPLER_DESC pointClamp(1, // shaderRegister
                                                     D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
                                                     D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
                                                     D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
                                                     D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW
        const CD3DX12_STATIC_SAMPLER_DESC linearWrap(2, // shaderRegister
                                                     D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
                                                     D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
                                                     D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
                                                     D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW
        const CD3DX12_STATIC_SAMPLER_DESC linearClamp(3, // shaderRegister
                                                      D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
                                                      D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
                                                      D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
                                                      D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW
        const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(4, // shaderRegister
                                                          D3D12_FILTER_ANISOTROPIC, // filter
                                                          D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
                                                          D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
                                                          D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
                                                          0.0f,                             // mipLODBias
                                                          8);                               // maxAnisotropy
        const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(5, // shaderRegister
                                                           D3D12_FILTER_ANISOTROPIC, // filter
                                                           D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
                                                           D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
                                                           D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
                                                           0.0f,                              // mipLODBias
                                                           8);                                // maxAnisotropy
        return { pointWrap, pointClamp, linearWrap, linearClamp, anisotropicWrap, anisotropicClamp };
    }

    Camera                                                mCamera;
    unordered_map<string, unique_ptr<MeshGeometry>>       mGeometries;
    unordered_map<string, ComPtr<ID3DBlob>>               mShaders;
    unordered_map<string, unique_ptr<ShaderMaterialData>> mMaterials;
    unordered_map<string, unique_ptr<Texture>>            mTextures;
    vector<InstanceData>                                  mBoxInstances;
    vector<D3D12_INPUT_ELEMENT_DESC>                      mInputLayout;
    unique_ptr<UploadBuffer<SceneConstants>>              mSceneConstants = nullptr;
    unique_ptr<UploadBuffer<ShaderMaterialData>>          mMatBuffer = nullptr;
    unique_ptr<UploadBuffer<ShaderPerObjectData>>         mObjectBuffer = nullptr;
    unique_ptr<UploadBuffer<InstanceData>>                mInstDataBuffer = nullptr;
    ComPtr<ID3D12RootSignature>                           mRootSignature = nullptr;
    ComPtr<ID3D12PipelineState>                           mSimplePipeline = nullptr;
    ComPtr<ID3D12DescriptorHeap>                          mTextureSrvHeap = nullptr;
    float mRadius     = 5.0f;
    float mPhi        = XM_PIDIV4;
    float mTheta      = 2.0f * XM_PI;
    //XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
    XMFLOAT4X4 mView  = MathHelper::Identity4x4();
    XMFLOAT4X4 mProj  = MathHelper::Identity4x4();
    POINT mLastMousePos;
};

// ======================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nShowCmd) {
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    auto ret = 0;
    InstancingCullingDemo demoApp(hInstance);
    if (demoApp.Initialize()) {
        ret = demoApp.Run();
    }
    return ret;
}