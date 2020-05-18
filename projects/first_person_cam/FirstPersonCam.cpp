/*
Create a first person camera demo with a basic scene
- create and render scene
    - create grid geometry
    - write simple vs/ ps
    - buid pipelines
    - build descriptor heaps and shader views
    - write draw commands
    -
- implement first person camera
    - load textures
    - write shader to apply texture to grid

*/
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

#pragma comment (lib, "d3dcompiler.lib")
#pragma comment (lib, "D3D12.lib")

// ======================================================================
struct ShaderVertex {
    DirectX::XMFLOAT3 pos;
    DirectX::XMFLOAT4 color;
};

struct SceneConstants {
    XMFLOAT4X4 mvpMatrix = MathHelper::Identity4x4();
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
class FpsCamDemo : public BaseApp
{
public:
    FpsCamDemo(HINSTANCE hInst)
    : BaseApp(hInst){
    }
    ~FpsCamDemo() {
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
    void LoadTextures();
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
        ID3D12DescriptorHeap* descriptorHeaps[] = {
            mpCbvHeap.Get()
        };
        m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);
        m_commandList->SetGraphicsRootSignature(mRootSignature.Get());
        m_commandList->IASetVertexBuffers(0, 1, &mGeometries["grid"]->VertexBufferView());
        m_commandList->IASetIndexBuffer(&mGeometries["grid"]->IndexBufferView());
        m_commandList->IASetPrimitiveTopology(D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
        m_commandList->SetGraphicsRootDescriptorTable(0, mpCbvHeap->GetGPUDescriptorHandleForHeapStart());
        m_commandList->DrawIndexedInstanced(mGeometries["grid"]->drawArgs["grid"].indexCount, 1, 0, 0, 0);
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
        float x = mRadius * sinf(mPhi) * cosf(mTheta);
        float z = mRadius * sinf(mPhi) * sinf(mTheta);
        float y = mRadius * cosf(mPhi);
        XMMATRIX view = mCamera.GetView();
        XMMATRIX proj = mCamera.GetProj();
        XMMATRIX world = XMLoadFloat4x4(&mWorld);
        XMMATRIX worldViewProj = world * view * proj;
        SceneConstants mvpMatrix = { };
        XMStoreFloat4x4(&mvpMatrix.mvpMatrix, XMMatrixTranspose(worldViewProj));
        mSceneConstants->CopyData(0, mvpMatrix);
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
        /*XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
        XMStoreFloat4x4(&mProj, proj);*/
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
        psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
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
        CD3DX12_ROOT_PARAMETER slotRootParams[1];
        CD3DX12_DESCRIPTOR_RANGE cbvTable = {};
        cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
        slotRootParams[0].InitAsDescriptorTable(1, &cbvTable);
        CD3DX12_ROOT_SIGNATURE_DESC rootSignDesc(1, slotRootParams, 0, nullptr, D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
        ComPtr<ID3DBlob> serializedRootSig = nullptr;
        ComPtr<ID3DBlob> errorBlob = nullptr;
        HRESULT hr = D3D12SerializeRootSignature(&rootSignDesc, D3D_ROOT_SIGNATURE_VERSION_1, serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());
        if (errorBlob != nullptr) {
            ::OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
        }
        ThrowIfFailed(hr);
        ThrowIfFailed(m_d3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(), serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
    }
    void BuildDescriptorsAndViews() {
        // We need one const buffer view just for the MVP matrix.
        D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
        cbvHeapDesc.NumDescriptors = 1;
        cbvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        cbvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&mpCbvHeap)));
        mSceneConstants = make_unique<UploadBuffer<SceneConstants>>(m_d3dDevice.Get(), 1, true);
        UINT sceneConstBytes = BaseUtil::CalcConstantBufferByteSize(sizeof(SceneConstants));
        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = { };
        cbvDesc.BufferLocation = mSceneConstants->Resource()->GetGPUVirtualAddress();
        cbvDesc.SizeInBytes = BaseUtil::CalcConstantBufferByteSize(sizeof(SceneConstants));
        m_d3dDevice->CreateConstantBufferView(&cbvDesc, mpCbvHeap->GetCPUDescriptorHandleForHeapStart());
    }
    void BuildShaders() {
        mShaders["simpleVS"] = BaseUtil::CompileShader(L"shaders\\simpleRender.hlsl", nullptr, "SimpleVS", "vs_5_1");
        mShaders["simplePS"] = BaseUtil::CompileShader(L"shaders\\simpleRender.hlsl", nullptr, "SimplePS", "ps_5_1");
        mInputLayout = {
            {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
            {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
        };
    }
    void BuildGeometry() {
        GeometryGenerator generator;
        MeshData grid                  = generator.CreateGrid(20.0f, 20.0f, 40, 40);
        UINT gridVertexOffset          = 0;
        UINT gridIndexOffset           = 0;
        SubmeshGeometry gridSubmesh    = {};
        gridSubmesh.indexCount         = static_cast<UINT>(grid.m_indices32.size());
        gridSubmesh.startIndexLocation = gridIndexOffset;
        gridSubmesh.baseVertexLocation = gridVertexOffset;
        vector<ShaderVertex> vertices(grid.m_vertices.size());
        vector<uint32_t> indices;
        for (size_t i = 0; i < grid.m_vertices.size(); ++i) {
            vertices[i].pos = grid.m_vertices[i].m_position;
            vertices[i].color = XMFLOAT4(DirectX::Colors::DarkGreen);
        }
        indices.insert(indices.end(), cbegin(grid.m_indices32), cend(grid.m_indices32));
        const UINT vbNumBytes             = static_cast<UINT>(vertices.size()) * sizeof(ShaderVertex);
        const UINT ibNumBytes             = static_cast<UINT>(indices.size()) * sizeof(uint32_t);
        unique_ptr<MeshGeometry> geometry = make_unique<MeshGeometry>();
        geometry->name                    = "grid";
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
        mGeometries[geometry->name]    = move(geometry);
    }

private:
    Camera                                          mCamera;
    unordered_map<string, unique_ptr<MeshGeometry>> mGeometries;
    unordered_map<string, ComPtr<ID3DBlob>>         mShaders;
    vector<D3D12_INPUT_ELEMENT_DESC>                mInputLayout;
    ComPtr<ID3D12DescriptorHeap>                    mpCbvHeap = nullptr;
    unique_ptr<UploadBuffer<SceneConstants>>        mSceneConstants = nullptr;
    ComPtr<ID3D12RootSignature>                     mRootSignature = nullptr;
    ComPtr<ID3D12PipelineState>                     mSimplePipeline = nullptr;
    float mRadius = 5.0f;
    float mPhi = XM_PIDIV4;
    float mTheta = 2.0f * XM_PI;
    XMFLOAT4X4 mWorld = MathHelper::Identity4x4();
    XMFLOAT4X4 mView = MathHelper::Identity4x4();
    XMFLOAT4X4 mProj = MathHelper::Identity4x4();
    POINT mLastMousePos;
};

// ======================================================================
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PSTR pCmdLine, int nShowCmd) {
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    auto ret = 0;
    FpsCamDemo demoApp(hInstance);
    if (demoApp.Initialize()) {
        ret = demoApp.Run();
    }
    return ret;
}