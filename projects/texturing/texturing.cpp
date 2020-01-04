#include <iostream>
#include "DirectXColors.h"
#include "windows.h"
#include "BaseApp.h"
#include "FrameResource.h"
#include "GeometryGenerator.h"

using namespace std;
using Microsoft::WRL::ComPtr;
using namespace DirectX;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")

const unsigned int NumFrameResources = 3;

// =====================================================================================================================
class RenderItem
{
public:
    RenderItem() = default;

    XMFLOAT4X4 world_ = MathHelper::Identity4x4();
    XMFLOAT4X4 texTransform_ = MathHelper::Identity4x4();

    int numFramesDirty_ = NumFrameResources;
    UINT objCbIndex_ = -1;
    Material* mat_ = nullptr;
    MeshGeometry* geo_ = nullptr;

    D3D12_PRIMITIVE_TOPOLOGY primitiveType_ = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    UINT indexCount_ = 0;
    UINT startIndexLocation_ = 0;
    int baseVertexLocation_ = 0;
};

// =====================================================================================================================
class TextureDemo : public BaseApp
{
public:
    TextureDemo(HINSTANCE hInstance)
        :
        BaseApp(hInstance),
        theta_(1.3f * XM_PI),
        phi_(0.4f * XM_PI),
        radius_(2.5f),
        currentFrameIndex_(0)
    {
        projMatrix_ = MathHelper::Identity4x4();
        viewMatrix_ = MathHelper::Identity4x4();
        eyePos_ = { 0.0f, 0.0f, 0.0f };
    }

    virtual void OnResize() override;
    virtual void Update(const BaseTimer& timer) override;
    virtual void Draw(const BaseTimer& timer) override;
    bool Initialize() override;

    virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y) override;

    void OnKeyboardInput(const BaseTimer& timer);
    void UpdateCamera(const BaseTimer& timer);
    void AnimateMaterials(const BaseTimer& timer);
    void UpdateObjectCBs(const BaseTimer& timer);
    void UpdateMaterialCBs(const BaseTimer& timer);
    void UpdateMainPassCBs(const BaseTimer& timer);

    std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();
    void LoadTextures();
    void BuildRootSignature();
    void BuildDescriptorHeaps();
    void BuildShadersAndInputLayout();
    void BuildShapeGeometry();
    void BuildMaterials();
    void BuildRenderItems();
    void BuildFrameResources();
    void BuildPipelines();

    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& items);

private:
    ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
    ComPtr<ID3D12DescriptorHeap> srvDescriptorHeap_ = nullptr;
    ComPtr<ID3D12PipelineState> opaqueGfxPipe_ = nullptr;

    std::unordered_map<std::string, std::unique_ptr<Texture>> textures_;
    std::unordered_map<std::string, ComPtr<ID3DBlob>> shaders_;
    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> geometries_;
    std::unordered_map<std::string, std::unique_ptr<Material>> materials_;

    std::vector<D3D12_INPUT_ELEMENT_DESC> inputLayout_;
    std::vector<std::unique_ptr<RenderItem>> allItems_;
    std::vector<RenderItem*> opaqueItems_;
    std::vector<std::unique_ptr<FrameResource::Resources>> frameResources_;
    FrameResource::Resources* currentFrameRes_ = nullptr;
    FrameResource::PassConstants mainPassCB_;

    XMFLOAT4X4 projMatrix_;
    XMFLOAT4X4 viewMatrix_;
    XMFLOAT3 eyePos_;

    float theta_;
    float phi_;
    float radius_;
    POINT lastMousePos_;

    unsigned int currentFrameIndex_;
};

// =====================================================================================================================
void TextureDemo::OnResize()
{
    BaseApp::OnResize();

    XMMATRIX proj = XMMatrixPerspectiveFovLH(0.25f * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&projMatrix_, proj);
}

// =====================================================================================================================
void TextureDemo::Update(const BaseTimer& timer)
{
    OnKeyboardInput(timer);
    UpdateCamera(timer);

    currentFrameIndex_ = (currentFrameIndex_ + 1) % NumFrameResources;
    currentFrameRes_ = frameResources_[currentFrameIndex_].get();

    if ((currentFrameRes_->m_fence != 0) && (m_fence->GetCompletedValue() < currentFrameRes_->m_fence))
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(m_fence->SetEventOnCompletion(currentFrameRes_->m_fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    AnimateMaterials(timer);
    UpdateObjectCBs(timer);
    UpdateMaterialCBs(timer);
    UpdateMainPassCBs(timer);

}

// ====================================================================================================================
void TextureDemo::Draw(const BaseTimer& timer)
{
    auto cmdAllocator = currentFrameRes_->m_cmdListAlloc;

    // Reset the allocator and the command list.
    ThrowIfFailed(cmdAllocator->Reset());
    ThrowIfFailed(m_commandList->Reset(cmdAllocator.Get(), opaqueGfxPipe_.Get()));

    // Reset the viewport
    m_commandList->RSSetViewports(1, &m_screenViewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Barrier transition ofcurrent back buffer from present image to render target
    m_commandList->ResourceBarrier(1,
                   &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
                                     D3D12_RESOURCE_STATE_PRESENT,
                                     D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Perform operations on the render target image.
    m_commandList->ClearRenderTargetView(CurrentBackBufferView(), DirectX::Colors::LightYellow, 0, nullptr);
    m_commandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
    m_commandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    ID3D12DescriptorHeap* descriptorHeaps[] = { srvDescriptorHeap_.Get() };
    m_commandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

    m_commandList->SetGraphicsRootSignature(rootSignature_.Get());

    auto passCB = currentFrameRes_->m_passCb->Resource();
    m_commandList->SetGraphicsRootConstantBufferView(2, passCB->GetGPUVirtualAddress());

    DrawRenderItems(m_commandList.Get(), opaqueItems_);

    m_commandList->ResourceBarrier(1,
                   &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
                                     D3D12_RESOURCE_STATE_RENDER_TARGET,
                                     D3D12_RESOURCE_STATE_PRESENT));

    // Close command list and execute on command queue.
    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    // Swap chain buffer swap
    ThrowIfFailed(m_swapChain->Present(0, 0));
    m_currBackBuffer = (m_currBackBuffer + 1) % SwapChainBufferCount;

    currentFrameRes_->m_fence = ++m_currentFence;
    m_commandQueue->Signal(m_fence.Get(), m_currentFence);
}

// ====================================================================================================================
bool TextureDemo::Initialize()
{
    auto success = BaseApp::Initialize();

    if (success == true)
    {
        ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), nullptr));

        LoadTextures();
        BuildRootSignature();
        BuildDescriptorHeaps();
        BuildShadersAndInputLayout();
        BuildShapeGeometry();
        BuildMaterials();
        BuildRenderItems();
        BuildFrameResources();
        BuildPipelines();

        ThrowIfFailed(m_commandList->Close());
        ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

        FlushCommandQueue();
    }

    return success;
}

// =====================================================================================================================
void TextureDemo::LoadTextures()
{
    auto woodCrateTex = std::make_unique<Texture>();
    woodCrateTex->name_ = "woodCrateTex";
    woodCrateTex->filename_ = L"..\\textures\\WoodCrate01.dds";

    ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(m_d3dDevice.Get(),
                                                      m_commandList.Get(),
                                                      woodCrateTex->filename_.c_str(),
                                                      woodCrateTex->resource_,
                                                      woodCrateTex->uploadHeap_));

    textures_[woodCrateTex->name_] = std::move(woodCrateTex);
}

// =====================================================================================================================
void TextureDemo::BuildRootSignature()
{
    CD3DX12_DESCRIPTOR_RANGE texTable;
    texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0);

    CD3DX12_ROOT_PARAMETER slotRootParameter[4];

    slotRootParameter[0].InitAsDescriptorTable(1, &texTable, D3D12_SHADER_VISIBILITY_PIXEL);
    slotRootParameter[1].InitAsConstantBufferView(0);
    slotRootParameter[2].InitAsConstantBufferView(1);
    slotRootParameter[3].InitAsConstantBufferView(2);

    auto staticSamplers = GetStaticSamplers();

    // a root signature consists of root parameters
    // root parameters can be (descriptor tables, descriptor, root constant)
    CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(4,
                        slotRootParameter,
                        static_cast<uint32_t>(staticSamplers.size()),
                        staticSamplers.data(),
                        D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc,
                         D3D_ROOT_SIGNATURE_VERSION_1,
                         serializedRootSig.GetAddressOf(),
                         errorBlob.GetAddressOf());

    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA(static_cast<char*>(errorBlob->GetBufferPointer()));
    }

    ThrowIfFailed(hr);

    ThrowIfFailed(m_d3dDevice->CreateRootSignature(0,
                           serializedRootSig->GetBufferPointer(),
                           serializedRootSig->GetBufferSize(),
                           IID_PPV_ARGS(rootSignature_.GetAddressOf())));
}

// ====================================================================================================================
void TextureDemo::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc =  {};
    srvHeapDesc.NumDescriptors = 1;
    srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&srvDescriptorHeap_)));

    CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(srvDescriptorHeap_->GetCPUDescriptorHandleForHeapStart());

    auto woodCrateTex = textures_["woodCrateTex"]->resource_;

    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = woodCrateTex->GetDesc().Format;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = woodCrateTex->GetDesc().MipLevels;
    srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

    m_d3dDevice->CreateShaderResourceView(woodCrateTex.Get(), &srvDesc, hDescriptor);
}

// =====================================================================================================================
void TextureDemo::BuildShadersAndInputLayout()
{
    shaders_["standardVS"] = BaseUtil::CompileShader(L"shaders\\textureDemo.hlsl", nullptr, "VS", "vs_5_0");
    shaders_["opaquePS"] = BaseUtil::CompileShader(L"shaders\\textureDemo.hlsl", nullptr, "PS", "ps_5_0");

    inputLayout_ =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        {"NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        {"TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
    };
}

// =====================================================================================================================
void TextureDemo::BuildShapeGeometry()
{
    GeometryGenerator geoGen;
    MeshData box = geoGen.CreateBox(1.0f, 1.0f, 1.0f, 3);

    SubmeshGeometry boxSubmesh;
    boxSubmesh.indexCount = static_cast<UINT>(box.m_indices32.size());
    boxSubmesh.startIndexLocation = 0;
    boxSubmesh.baseVertexLocation = 0;

    std::vector<FrameResource::Vertex> vertices(box.m_vertices.size());

    for (uint32_t i = 0; i < box.m_vertices.size(); ++i)
    {
        vertices[i].pos    = box.m_vertices[i].m_position;
        vertices[i].normal = box.m_vertices[i].m_normal;
        vertices[i].texC   = box.m_vertices[i].m_texC;
    }

    std::vector<std::uint16_t> indices = box.GetIndices16();

    const UINT vbByteSize = static_cast<UINT>(vertices.size() * sizeof(FrameResource::Vertex));
    const UINT ibBytesSize = static_cast<UINT>(indices.size() * sizeof(std::uint16_t));

    auto geo = std::make_unique<MeshGeometry>();
    geo->name = "boxGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->vertexBufferCPU));
    CopyMemory(geo->vertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibBytesSize, &geo->indexBufferCPU));
    CopyMemory(geo->indexBufferCPU->GetBufferPointer(), indices.data(), ibBytesSize);

    geo->vertexBufferGPU = BaseUtil::CreateDefaultBuffer(m_d3dDevice.Get(),
                             m_commandList.Get(),
                             vertices.data(),
                             vbByteSize,
                             geo->vertexBufferUploader);

    geo->indexBufferGPU = BaseUtil::CreateDefaultBuffer(m_d3dDevice.Get(),
                            m_commandList.Get(),
                            indices.data(),
                            ibBytesSize,
                            geo->indexBufferUploader);

    geo->vertexByteStride = sizeof(FrameResource::Vertex);
    geo->vertexBufferByteSize = vbByteSize;
    geo->indexFormat = DXGI_FORMAT_R16_UINT;
    geo->indexBufferByteSize = ibBytesSize;
    geo->drawArgs["box"] = boxSubmesh;

    geometries_[geo->name] = std::move(geo);
}

// ====================================================================================================================
void TextureDemo::BuildMaterials()
{
    auto woodCrate = std::make_unique<Material>();
    woodCrate->m_name = "woodCrate";
    woodCrate->m_matCbIndex = 0;
    woodCrate->m_diffuseSrvHeapIndex = 0;
    woodCrate->m_diffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
    woodCrate->m_fresnelR0 = XMFLOAT3(0.05f, 0.05f, 0.05f);
    woodCrate->m_roughness = 0.2f;

    materials_["woodCrate"] = std::move(woodCrate);
}

// ====================================================================================================================
void TextureDemo::BuildRenderItems()
{
    auto boxItem = std::make_unique<RenderItem>();
    boxItem->objCbIndex_ = 0;
    boxItem->mat_ = materials_["woodCrate"].get();
    boxItem->geo_ = geometries_["boxGeo"].get();
    boxItem->primitiveType_ = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    boxItem->indexCount_ = boxItem->geo_->drawArgs["box"].indexCount;
    boxItem->startIndexLocation_ = boxItem->geo_->drawArgs["box"].startIndexLocation;
    boxItem->baseVertexLocation_ = boxItem->geo_->drawArgs["box"].baseVertexLocation;
    allItems_.push_back(std::move(boxItem));

    for (auto& e : allItems_)
    {
        opaqueItems_.push_back(e.get());
    }
}

// =====================================================================================================================
void TextureDemo::BuildFrameResources()
{
    using namespace FrameResource;
    for (int i = 0; i < NumFrameResources; ++i)
    {
        frameResources_.push_back(std::make_unique<Resources>(m_d3dDevice.Get(), 1, static_cast<UINT>(allItems_.size()), static_cast<UINT>(materials_.size())));
    }
}

// =====================================================================================================================
void TextureDemo::BuildPipelines()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePipeDesc = {};
    ZeroMemory(&opaquePipeDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    opaquePipeDesc.InputLayout = { inputLayout_.data(), static_cast<UINT>(inputLayout_.size()) };
    opaquePipeDesc.pRootSignature = rootSignature_.Get();
    opaquePipeDesc.VS = { reinterpret_cast<BYTE*>(shaders_["standardVS"]->GetBufferPointer()), shaders_["standardVS"]->GetBufferSize() };
    opaquePipeDesc.PS = { reinterpret_cast<BYTE*>(shaders_["opaquePS"]->GetBufferPointer()), shaders_["opaquePS"]->GetBufferSize() };
    opaquePipeDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    opaquePipeDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    opaquePipeDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    opaquePipeDesc.SampleMask = UINT_MAX;
    opaquePipeDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    opaquePipeDesc.NumRenderTargets = 1;
    opaquePipeDesc.RTVFormats[0] = m_backBufferFormat;
    opaquePipeDesc.SampleDesc.Count = m_4xMsaaEn ? 4 : 1;
    opaquePipeDesc.SampleDesc.Quality = m_4xMsaaEn ? m_4xMsaaQuality - 1 : 0;
    opaquePipeDesc.DSVFormat = m_depthStencilFormat;
    ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&opaquePipeDesc, IID_PPV_ARGS(&opaqueGfxPipe_)));
}

// =====================================================================================================================
void TextureDemo::DrawRenderItems(ID3D12GraphicsCommandList * cmdList, const std::vector<RenderItem*>& items)
{
    UINT objCBByteSize = BaseUtil::CalcConstantBufferByteSize(sizeof(FrameResource::ObjectConstants));
    UINT matCBByteSize = BaseUtil::CalcConstantBufferByteSize(sizeof(FrameResource::MaterialConstants));

    auto objectCB = currentFrameRes_->m_objCb->Resource();
    auto matCB = currentFrameRes_->m_matCb->Resource();

    for (size_t i = 0; i < items.size(); ++i)
    {
        auto ri = items[i];

        cmdList->IASetVertexBuffers(0, 1, &ri->geo_->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->geo_->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->primitiveType_);

        CD3DX12_GPU_DESCRIPTOR_HANDLE tex(srvDescriptorHeap_->GetGPUDescriptorHandleForHeapStart());
        tex.Offset(ri->mat_->m_diffuseSrvHeapIndex, m_cbvSrvUavDescriptorSize);

        D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objectCB->GetGPUVirtualAddress() + ri->objCbIndex_ * objCBByteSize;
        D3D12_GPU_VIRTUAL_ADDRESS matCBAddress = matCB->GetGPUVirtualAddress() + ri->mat_->m_matCbIndex * matCBByteSize;

        cmdList->SetGraphicsRootDescriptorTable(0, tex);
        cmdList->SetGraphicsRootConstantBufferView(1, objCBAddress);
        cmdList->SetGraphicsRootConstantBufferView(3, matCBAddress);

        cmdList->DrawIndexedInstanced(ri->indexCount_, 1, ri->startIndexLocation_, ri->baseVertexLocation_, 0);
    }
}

// =====================================================================================================================
void TextureDemo::OnMouseDown(WPARAM btnState, int x, int y)
{
    lastMousePos_.x = x;
    lastMousePos_.y = y;

    SetCapture(mhMainWnd);
}

// =====================================================================================================================
void TextureDemo::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

// =====================================================================================================================
void TextureDemo::OnMouseMove(WPARAM btnState, int x, int y)
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

      radius_ += (dx - dy);

      radius_ = MathHelper::Clamp(radius_, 5.0f, 150.0f);
    }

    lastMousePos_.x = x;
    lastMousePos_.y = y;
}

// =====================================================================================================================
void TextureDemo::OnKeyboardInput(const BaseTimer & timer)
{
}

// =====================================================================================================================
void TextureDemo::UpdateCamera(const BaseTimer & timer)
{
    // Convert spherical to Cartesian Coordinates
    eyePos_.x = radius_ * sinf(phi_) * cosf(theta_);
    eyePos_.y = radius_ * cosf(phi_);
    eyePos_.z = radius_ * sinf(phi_) * sinf(theta_);

    XMVECTOR pos = XMVectorSet(eyePos_.x, eyePos_.y, eyePos_.z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&viewMatrix_, view);
}

// =====================================================================================================================
void TextureDemo::AnimateMaterials(const BaseTimer & timer)
{
}

// =====================================================================================================================
void TextureDemo::UpdateObjectCBs(const BaseTimer & timer)
{
    using namespace FrameResource;

    auto currObjCB = currentFrameRes_->m_objCb.get();
    for (auto& e : allItems_)
    {
        if (e->numFramesDirty_ > 0)
        {
            XMMATRIX world = DirectX::XMLoadFloat4x4(&e->world_);
            XMMATRIX texTransform = DirectX::XMLoadFloat4x4(&e->texTransform_);

            ObjectConstants objConstants;
            DirectX::XMStoreFloat4x4(&objConstants.m_world, XMMatrixTranspose(world));
            DirectX::XMStoreFloat4x4(&objConstants.m_texTransform, XMMatrixTranspose(texTransform));

            currObjCB->CopyData(e->objCbIndex_, objConstants);
            e->numFramesDirty_--;
        }
    }
}

// =====================================================================================================================
void TextureDemo::UpdateMaterialCBs(const BaseTimer & timer)
{
    using namespace FrameResource;

    auto currMaterialCB = currentFrameRes_->m_matCb.get();
    for (auto& e : materials_)
    {
        Material* pMat = e.second.get();
        if (pMat->m_numFramesDirty > 0)
        {
            XMMATRIX matTransform = XMLoadFloat4x4(&pMat->m_matTransform);

            MaterialConstants matConstants;
            matConstants.diffuseAlbedo = pMat->m_diffuseAlbedo;
            matConstants.fresnelR0 = pMat->m_fresnelR0;
            matConstants.roughness = pMat->m_roughness;
            XMStoreFloat4x4(&matConstants.matTransform, XMMatrixTranspose(matTransform));
            currMaterialCB->CopyData(pMat->m_matCbIndex, matConstants);

            pMat->m_numFramesDirty--;
        }
    }
}

// =====================================================================================================================
void TextureDemo::UpdateMainPassCBs(const BaseTimer & timer)
{
    XMMATRIX view = XMLoadFloat4x4(&viewMatrix_);
    XMMATRIX proj = XMLoadFloat4x4(&projMatrix_);

    XMMATRIX viewProj    = XMMatrixMultiply(view, proj);
    XMMATRIX invView     = XMMatrixInverse(&XMMatrixDeterminant(view), view);
    XMMATRIX invProj     = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
    XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

    XMStoreFloat4x4(&mainPassCB_.view, XMMatrixTranspose(view));
    XMStoreFloat4x4(&mainPassCB_.proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&mainPassCB_.viewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&mainPassCB_.invView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&mainPassCB_.invProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&mainPassCB_.invViewProj, XMMatrixTranspose(invViewProj));

    mainPassCB_.eyePosW = eyePos_;
    mainPassCB_.renderTargetSize = XMFLOAT2(static_cast<float>(m_clientWidth), static_cast<float>(m_clientHeight));
    mainPassCB_.invRenderTargetSize = XMFLOAT2(1.0f / m_clientWidth, 1.0f / m_clientHeight);

    // frustum
    mainPassCB_.nearZ = 1.0f;
    mainPassCB_.farZ = 1000.0f;

    // time
    mainPassCB_.totalTime = timer.TotalTimeInSecs();
    mainPassCB_.deltaTime = timer.DeltaTimeInSecs();

    // lights
    mainPassCB_.ambientLight = { 0.25f, 0.25f, 0.35f, 1.0f };
    mainPassCB_.lights[0].direction = { 0.57735f, -0.57735f, 0.57735f };
    mainPassCB_.lights[0].strength = { 0.6f, 0.6f, 0.6f };
    mainPassCB_.lights[1].direction = { -0.57735f, -0.57735f, 0.57735f };
    mainPassCB_.lights[1].strength = { 0.3f, 0.3f, 0.3f };
    mainPassCB_.lights[2].direction = { 0.0f, -0.707f, -0.707f };
    mainPassCB_.lights[2].strength = { 0.15f, 0.15f, 0.15f };

    auto currPassCB = currentFrameRes_->m_passCb.get();
    currPassCB->CopyData(0, mainPassCB_);
}

// =====================================================================================================================
std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> TextureDemo::GetStaticSamplers()
{
    const CD3DX12_STATIC_SAMPLER_DESC pointWrap(0,
                        D3D12_FILTER_MIN_MAG_MIP_POINT,
                        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                        D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                        D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    const CD3DX12_STATIC_SAMPLER_DESC pointClamp(1,
                         D3D12_FILTER_MIN_MAG_MIP_POINT,
                         D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                         D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                         D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    const CD3DX12_STATIC_SAMPLER_DESC linearWrap(2,
                         D3D12_FILTER_MIN_MAG_MIP_LINEAR,
                         D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                         D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                         D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    const CD3DX12_STATIC_SAMPLER_DESC linearClamp(3,
                          D3D12_FILTER_MIN_MAG_MIP_LINEAR,
                          D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                          D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                          D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(4,
                              D3D12_FILTER_ANISOTROPIC,
                              D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                              D3D12_TEXTURE_ADDRESS_MODE_WRAP,
                              D3D12_TEXTURE_ADDRESS_MODE_WRAP);
    const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(5,
                               D3D12_FILTER_ANISOTROPIC,
                               D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                               D3D12_TEXTURE_ADDRESS_MODE_CLAMP,
                               D3D12_TEXTURE_ADDRESS_MODE_CLAMP);

    return { pointWrap, pointClamp, linearWrap, linearClamp, anisotropicWrap, anisotropicClamp };
}

// =====================================================================================================================
// Write the win main func here
int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   PSTR      pCmdLine,
                   int       nShowCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
    auto retCode = 0;
    TextureDemo demoApp(hInstance);

    if (demoApp.Initialize() == true)
    {
        retCode = demoApp.Run();
    }

    return retCode;
}



