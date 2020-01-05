#include <array>
#include <vector>
#include <unordered_map>
#include <string>

#include <Windows.h>

#include <DirectXCollision.h>
#include <DirectXColors.h>
#include <DirectXMath.h>

#include "../common/BaseApp.h"
#include "../common/BaseUtil.h"
#include "../common/d3dx12.h"
#include "../common/GeometryGenerator.h"
#include "../common/MathHelper.h"

#include "FrameResource.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

// ====================================================================================================================
const unsigned int NumFrameResources = 3;

// ====================================================================================================================
struct RenderItem
{
        RenderItem() = default;

        XMFLOAT4X4               m_world = MathHelper::Identity4x4();
        UINT                     m_numFramesDirty = NumFrameResources;
        UINT                     m_objCbIndex = -1;
        MeshGeometry*            m_pGeo = nullptr;
        D3D12_PRIMITIVE_TOPOLOGY m_primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
        UINT                     m_indexCount = 0;
        UINT                     m_startIndexLocation = 0;
        INT                      m_baseVertexLocation = 0;
        Material*                m_pMat = nullptr;
        XMFLOAT4X4               m_texTransform = MathHelper::Identity4x4();
};

// ====================================================================================================================
class ShapesDemo : public BaseApp
{
public:
    ShapesDemo(HINSTANCE hInstance);
    bool Initialize();
protected:
    enum Shapes : uint32
    {
        Box,
        Grid,
        Cylinder,
        Sphere,
        Count
    };
private:
    void OnResize() override;
    void Update(const BaseTimer& gt);
    void Draw(const BaseTimer& gt);

    virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
    virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
    virtual void OnMouseMove(WPARAM btnState, int x, int y) override;
    virtual void OnKeyDown(WPARAM wparam) override;

    void OnKeyboardInput(const BaseTimer& gt);
    void UpdateCamera(const BaseTimer& gt);
    void UpdateObjectCBs(const BaseTimer& gt);
    void UpdateMainPassCB(const BaseTimer& timer);
    void UpdateMaterialCBs(const BaseTimer& timer);

    void BuildRootSignature();
    void BuildShadersAndInputLayout();
    void BuildShapeGeometry();
    void BuildRenderItems();
    void BuildFrameResources();
    void BuildDescriptorHeaps();
    void BuildConstBufferViews();
    void BuildPsos();
    void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& rItems);
    void UpdateRenderItems();
    void BuildMaterials();

    FrameResource::PassConstants                                   m_mainPassCB;
    bool                                                           m_isWireFrame = false;
    std::vector<std::unique_ptr<FrameResource::FrameResource>>     m_frameResources;
    std::vector<RenderItem*>									   m_opaqueItems;
    std::vector<std::unique_ptr<RenderItem>>                       m_allRenderItems;
    std::vector<D3D12_INPUT_ELEMENT_DESC>                          m_inputLayout;
    std::unordered_map<std::string, ComPtr<ID3DBlob>>              m_shaders;
    std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> m_geometries;
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>>   m_psos;
    ComPtr<ID3D12RootSignature>                                    m_rootSignature = nullptr;
    ComPtr<ID3D12DescriptorHeap>                                   m_cbvHeap = nullptr;
    UINT                                                           m_passCbvOffset = 0;
    XMFLOAT3                                                       m_eyePos = {0.0f, 0.0f, 0.0f};
    XMFLOAT4X4                                                     m_view = MathHelper::Identity4x4();
    XMFLOAT4X4                                                     m_proj = MathHelper::Identity4x4();
    float                                                          m_theta = 1.5f * XM_PI;
    float                                                          m_phi = 0.2f * XM_PI;
    float                                                          m_radius = 15.0f;
    POINT                                                          m_lastMousePos;
    int                                                            m_currFrameResourceIndex = 0;
    FrameResource::FrameResource*                                  m_currFrameResource = nullptr;
    Shapes                                                         m_currentShape = Shapes::Box;
    std::unordered_map<std::string, std::unique_ptr<Material>>     m_materials;
};

// ====================================================================================================================
bool ShapesDemo::Initialize()
{
    bool result = BaseApp::Initialize();

    if (result == true)
    {
        ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), nullptr));

        BuildRootSignature();
        BuildShadersAndInputLayout();
        BuildShapeGeometry();
        BuildMaterials();
        BuildRenderItems();
        BuildFrameResources();
        BuildDescriptorHeaps();
        BuildConstBufferViews();
        BuildPsos();

        ThrowIfFailed(m_commandList->Close());

        ID3D12CommandList* cmdLists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);
        FlushCommandQueue();
    }

    return result;
}

// ====================================================================================================================
ShapesDemo::ShapesDemo(HINSTANCE hInstance)
:
BaseApp(hInstance)
{
}

// ====================================================================================================================
void ShapesDemo::OnResize()
{
    BaseApp::OnResize();

    XMMATRIX P = XMMatrixPerspectiveFovLH(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
    XMStoreFloat4x4(&m_proj, P);
}

// ====================================================================================================================
void ShapesDemo::OnMouseDown(WPARAM btnState, int x, int y)
{
    m_lastMousePos.x = x;
    m_lastMousePos.y = y;
    SetCapture(mhMainWnd);
}

// ====================================================================================================================
void ShapesDemo::OnMouseUp(WPARAM btnState, int x, int y)
{
    ReleaseCapture();
}

// ====================================================================================================================
void ShapesDemo::OnMouseMove(WPARAM btnState, int x, int y)
{
    if ((btnState & MK_LBUTTON) != 0)
    {
        float dx = XMConvertToRadians(0.25f * static_cast<float>(x - m_lastMousePos.x));
        float dy = XMConvertToRadians(0.25f * static_cast<float>(y - m_lastMousePos.y));

        m_theta += dx;
        m_phi   += dy;

        m_phi = MathHelper::Clamp(m_phi, 0.1f, MathHelper::Pi - 0.1f);
    }
    else if ((btnState & MK_RBUTTON) != 0)
    {
        float dx = 0.05f * static_cast<float>(x - m_lastMousePos.x);
        float dy = 0.05f * static_cast<float>(y - m_lastMousePos.y);

        m_radius += dx - dy;
        m_radius = MathHelper::Clamp(m_radius, 5.0f, 150.0f);
    }

    m_lastMousePos.x = x;
    m_lastMousePos.y = y;
}

// ====================================================================================================================
void ShapesDemo::OnKeyDown(WPARAM wparam)
{
    if (wparam == VK_RIGHT)
    {
        uint32 currIdx = static_cast<uint32>(m_currentShape);
        currIdx  = (currIdx + 1) % Shapes::Count;
        m_currentShape = static_cast<Shapes>(currIdx);
    }

    //char buf[256];
    //itoa(static_cast<uint32>(m_currentShape), &buf[0], 10);
    //wchar_t wtext[20];
    //mbstowcs(wtext, buf, strlen(buf) + 1);//Plus null
    //LPWSTR ptr = wtext;
    //MessageBoxW(0, ptr, 0, 0);
}


// ====================================================================================================================
void ShapesDemo::Update(const BaseTimer& gt)
{
    OnKeyboardInput(gt);
    UpdateCamera(gt);

    m_currFrameResourceIndex = (m_currFrameResourceIndex + 1) % NumFrameResources;
    m_currFrameResource = m_frameResources[m_currFrameResourceIndex].get();

    if ((m_currFrameResource->m_fence != 0) && (m_fence->GetCompletedValue() < m_currFrameResource->m_fence))
    {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(m_fence->SetEventOnCompletion(m_currFrameResource->m_fence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }

    UpdateRenderItems();

    UpdateObjectCBs(gt);
    UpdateMaterialCBs(gt);
    UpdateMainPassCB(gt);
}

// ====================================================================================================================
void ShapesDemo::Draw(const BaseTimer& gt)
{
    auto cmdListAlloc = m_currFrameResource->m_cmdListAlloc;

    ThrowIfFailed(cmdListAlloc->Reset());
    ThrowIfFailed(m_commandList->Reset(cmdListAlloc.Get(), m_psos["opaque"].Get()));

    m_commandList->RSSetViewports(1, &m_screenViewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);
    m_commandList->ResourceBarrier(1,
                                   &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
                                   D3D12_RESOURCE_STATE_PRESENT,
                                   D3D12_RESOURCE_STATE_RENDER_TARGET));

    m_commandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::AntiqueWhite, 0, nullptr);
    m_commandList->ClearDepthStencilView(DepthStencilView(),
                                         D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,
                                         1.0f,
                                         0,
                                         0,
                                         nullptr);

    m_commandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    m_commandList->SetGraphicsRootSignature(m_rootSignature.Get());
    
    auto passCb = m_currFrameResource->m_passCb->Resource();
    m_commandList->SetGraphicsRootConstantBufferView(2, passCb->GetGPUVirtualAddress());

    DrawRenderItems(m_commandList.Get(), m_opaqueItems);

    m_commandList->ResourceBarrier(1,
                                   &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
                                   D3D12_RESOURCE_STATE_RENDER_TARGET,
                                   D3D12_RESOURCE_STATE_PRESENT));

    ThrowIfFailed(m_commandList->Close());
    ID3D12CommandList* cmdLists[] = {m_commandList.Get() };
    m_commandQueue->ExecuteCommandLists(_countof(cmdLists), cmdLists);

    ThrowIfFailed(m_swapChain->Present(0, 0));
    m_currBackBuffer = (m_currBackBuffer + 1) % SwapChainBufferCount;

    m_currFrameResource->m_fence = ++ m_currentFence;
    m_commandQueue->Signal(m_fence.Get(), m_currentFence);
}

// ====================================================================================================================
void ShapesDemo::OnKeyboardInput(const BaseTimer& gt)
{
    if (GetAsyncKeyState('1') & 0x8000)
    {
        m_isWireFrame = true;
    }
    else
    {
        m_isWireFrame = false;
    }
}


// ====================================================================================================================
void ShapesDemo::UpdateCamera(const BaseTimer& gt)
{
    m_eyePos.x = m_radius * sinf(m_phi) * cosf(m_theta);
    m_eyePos.z = m_radius * sinf(m_phi) * sinf(m_theta);
    m_eyePos.y = m_radius * cosf(m_phi);

    XMVECTOR pos    = XMVectorSet(m_eyePos.x, m_eyePos.y, m_eyePos.z, 1.0f);
    XMVECTOR target = XMVectorZero();
    XMVECTOR up     = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

    XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
    XMStoreFloat4x4(&m_view, view);
}

// ====================================================================================================================
void ShapesDemo::BuildRootSignature()
{
    // A "root parameter" can be a descriptor table, root constant or descriptor.
    CD3DX12_ROOT_PARAMETER slotRootParameter[3];
    slotRootParameter[0].InitAsConstantBufferView(0);
    slotRootParameter[1].InitAsConstantBufferView(1);
    slotRootParameter[2].InitAsConstantBufferView(2);

    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC rootSignDesc(3,
                                             slotRootParameter,
                                             0,
                                             nullptr,
                                             D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

    ComPtr<ID3DBlob> serializeRootSign = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;

    HRESULT hr = D3D12SerializeRootSignature(&rootSignDesc,
                                             D3D_ROOT_SIGNATURE_VERSION_1,
                                             serializeRootSign.GetAddressOf(),
                                             errorBlob.GetAddressOf());
    if (errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(m_d3dDevice->CreateRootSignature(0,
                                                   serializeRootSign->GetBufferPointer(),
                                                   serializeRootSign->GetBufferSize(),
                                                   IID_PPV_ARGS(m_rootSignature.GetAddressOf())));
}

// ====================================================================================================================
void ShapesDemo::BuildShadersAndInputLayout()
{
    
    m_shaders["standardVS"] = BaseUtil::CompileShader(L"shaders\\color.hlsl", nullptr, "VS", "vs_5_1");
    m_shaders["opaquePS"]   = BaseUtil::CompileShader(L"shaders\\color.hlsl", nullptr, "PS", "ps_5_1");

    m_inputLayout =
    {
        { "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0,  D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        { "NORMAL",   0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        { "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT,    0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
    };
}

// ====================================================================================================================
void ShapesDemo::BuildShapeGeometry()
{
    GeometryGenerator geoGen;
    MeshData box    = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 5);
    MeshData grid   = geoGen.CreateGrid(2.0f, 2.0f, 40, 40);
    MeshData sphere = geoGen.CreateSphere(0.5f, 20, 20);
    MeshData cyl    = geoGen.CreateCylinder(0.5f, 0.5f, 3.0f, 20, 20);

    UINT boxVertexOffset    = 0;
    UINT gridVertexOffset   = static_cast<UINT>(box.m_vertices.size());
    UINT sphereVertexOffset = gridVertexOffset + static_cast<UINT>(grid.m_vertices.size());
    UINT cylVertexOffset    = sphereVertexOffset + static_cast<UINT>(sphere.m_vertices.size());

    UINT boxIndexOffset    = 0;
    UINT gridIndexOffset   = static_cast<UINT>(box.m_indices32.size());
    UINT sphereIndexOffset = gridIndexOffset + static_cast<UINT>(grid.m_indices32.size());
    UINT cylIndexOffset    = sphereIndexOffset + static_cast<UINT>(sphere.m_indices32.size());

    SubmeshGeometry boxSubmesh    = {};
    boxSubmesh.indexCount         = static_cast<UINT>(box.m_indices32.size());
    boxSubmesh.startIndexLocation = boxIndexOffset;
    boxSubmesh.baseVertexLocation = boxVertexOffset;

    SubmeshGeometry gridSubMesh    = {};
    gridSubMesh.indexCount         = static_cast<UINT>(grid.m_indices32.size());
    gridSubMesh.startIndexLocation = gridIndexOffset;
    gridSubMesh.baseVertexLocation = gridVertexOffset;

    SubmeshGeometry sphereSubMesh    = {};
    sphereSubMesh.indexCount         = static_cast<UINT>(sphere.m_indices32.size());
    sphereSubMesh.startIndexLocation = sphereIndexOffset;
    sphereSubMesh.baseVertexLocation = sphereVertexOffset;

    SubmeshGeometry cylSubMesh    = {};
    cylSubMesh.indexCount         = static_cast<UINT>(cyl.m_indices32.size());
    cylSubMesh.startIndexLocation = cylIndexOffset;
    cylSubMesh.baseVertexLocation = cylVertexOffset;

    auto totalVertexCount = box.m_vertices.size()
                          + grid.m_vertices.size()
                          + sphere.m_vertices.size()
                          + cyl.m_vertices.size();

    std::vector<FrameResource::Vertex> vertices(totalVertexCount);

    UINT k = 0;
    for (size_t i = 0; i < box.m_vertices.size(); ++i, ++k)
    {
        vertices[k].pos    = box.m_vertices[i].m_position;
        vertices[k].normal = box.m_vertices[i].m_normal;
    }

    for (size_t i = 0; i < grid.m_vertices.size(); ++i, ++k)
    {
        vertices[k].pos    = grid.m_vertices[i].m_position;
        vertices[k].normal = grid.m_vertices[i].m_normal;
    }

    for (size_t i = 0; i < sphere.m_vertices.size(); ++i, ++k)
    {
        vertices[k].pos    = sphere.m_vertices[i].m_position;
        vertices[k].normal = sphere.m_vertices[i].m_normal;
    }

    for (size_t i = 0; i < cyl.m_vertices.size(); ++i, ++k)
    {
        vertices[k].pos    = cyl.m_vertices[i].m_position;
        vertices[k].normal = cyl.m_vertices[i].m_normal;
    }

    std::vector<std::uint32_t> indices;
    indices.insert(indices.end(), std::cbegin(box.m_indices32),    std::cend(box.m_indices32));
    indices.insert(indices.end(), std::cbegin(grid.m_indices32),   std::cend(grid.m_indices32));
    indices.insert(indices.end(), std::cbegin(sphere.m_indices32), std::cend(sphere.m_indices32));
    indices.insert(indices.end(), std::cbegin(cyl.m_indices32),    std::cend(cyl.m_indices32));

    const UINT vbByteSize = static_cast<UINT>(vertices.size()) * sizeof(FrameResource::Vertex);
    const UINT ibByteSize = static_cast<UINT>(indices.size())  * sizeof(std::uint32_t);

    auto geo  = std::make_unique<MeshGeometry>();
    geo->name = "shapeGeo";

    ThrowIfFailed(D3DCreateBlob(vbByteSize, &geo->vertexBufferCPU));
    CopyMemory(geo->vertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);

    ThrowIfFailed(D3DCreateBlob(ibByteSize, &geo->indexBufferCPU));
    CopyMemory(geo->indexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);

    geo->vertexBufferGPU = BaseUtil::CreateDefaultBuffer(m_d3dDevice.Get(),
                                                         m_commandList.Get(),
                                                         vertices.data(),
                                                         vbByteSize,
                                                         geo->vertexBufferUploader);

    geo->indexBufferGPU = BaseUtil::CreateDefaultBuffer(m_d3dDevice.Get(),
                                                         m_commandList.Get(),
                                                         indices.data(),
                                                         ibByteSize,
                                                         geo->indexBufferUploader);

    geo->vertexByteStride     = sizeof(FrameResource::Vertex);
    geo->vertexBufferByteSize = vbByteSize;
    geo->indexFormat          = DXGI_FORMAT_R32_UINT;
    geo->indexBufferByteSize  = ibByteSize;

    geo->drawArgs["box"]    = boxSubmesh;
    geo->drawArgs["grid"]   = gridSubMesh;
    geo->drawArgs["sphere"] = sphereSubMesh;
    geo->drawArgs["cyl"]    =  cylSubMesh;

    m_geometries[geo->name] = std::move(geo);
}

// ====================================================================================================================
void ShapesDemo::BuildMaterials()
{
    uint32 matIndex = 0;

    auto brickMat                   = std::make_unique<Material>();
    brickMat->m_name                = "bricksmat";
    brickMat->m_matCbIndex          = matIndex++;
    brickMat->m_diffuseSrvHeapIndex = 0;
    brickMat->m_diffuseAlbedo       = XMFLOAT4(Colors::ForestGreen);
    brickMat->m_fresnelR0           = XMFLOAT3(0.02f, 0.02f, 0.02f);
    brickMat->m_roughness           = 0.1f;

    auto stoneMat                   = std::make_unique<Material>();
    stoneMat->m_name                = "stonemat";
    stoneMat->m_matCbIndex          = matIndex++;
    stoneMat->m_diffuseSrvHeapIndex = 1;
    stoneMat->m_diffuseAlbedo       = XMFLOAT4(Colors::LightSteelBlue);
    stoneMat->m_fresnelR0           = XMFLOAT3(0.05f, 0.05f, 0.05f);
    stoneMat->m_roughness           = 0.3f;

    auto tilemat                   = std::make_unique<Material>();
    tilemat->m_name                = "tilemat";
    tilemat->m_matCbIndex          = matIndex++;
    tilemat->m_diffuseSrvHeapIndex = 2;
    tilemat->m_diffuseAlbedo       = XMFLOAT4(Colors::LightGray);
    tilemat->m_fresnelR0           = XMFLOAT3(0.02f, 0.02f, 0.02f);
    tilemat->m_roughness           = 0.2f;

    m_materials["bricksmat"] = std::move(brickMat);
    m_materials["stonemat"]  = std::move(stoneMat);
    m_materials["tilemat"]   = std::move(tilemat);
}

// ====================================================================================================================
void ShapesDemo::BuildRenderItems()
{
    uint32 objectCbIndex = 0;

    auto boxItem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&boxItem->m_world, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.0f, 0.0f));
    XMStoreFloat4x4(&boxItem->m_texTransform, XMMatrixScaling(1.0f, 1.0f, 1.0f));
    boxItem->m_objCbIndex         = objectCbIndex++;
    boxItem->m_pGeo               = m_geometries["shapeGeo"].get();
    boxItem->m_pMat               = m_materials["bricksmat"].get();
    boxItem->m_primitiveType      = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    boxItem->m_indexCount         = boxItem->m_pGeo->drawArgs["box"].indexCount;
    boxItem->m_startIndexLocation = boxItem->m_pGeo->drawArgs["box"].startIndexLocation;
    boxItem->m_baseVertexLocation = boxItem->m_pGeo->drawArgs["box"].baseVertexLocation;
    m_allRenderItems.push_back(std::move(boxItem));

    auto gridItem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&gridItem->m_world, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f));
    gridItem->m_objCbIndex         = objectCbIndex++;
    gridItem->m_pGeo               = m_geometries["shapeGeo"].get();
    gridItem->m_pMat               = m_materials["tilemat"].get();
    gridItem->m_primitiveType      = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    gridItem->m_indexCount         = gridItem->m_pGeo->drawArgs["grid"].indexCount;
    gridItem->m_startIndexLocation = gridItem->m_pGeo->drawArgs["grid"].startIndexLocation;
    gridItem->m_baseVertexLocation = gridItem->m_pGeo->drawArgs["grid"].baseVertexLocation;
    m_allRenderItems.push_back(std::move(gridItem));

    auto sphereItem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&sphereItem->m_world, XMMatrixScaling(3.0f, 3.0f, 3.0f) * XMMatrixTranslation(0.0f, 0.0f, 0.0f));
    sphereItem->m_objCbIndex         = objectCbIndex++;
    sphereItem->m_pGeo               = m_geometries["shapeGeo"].get();
    sphereItem->m_pMat               = m_materials["stonemat"].get();
    sphereItem->m_primitiveType      = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    sphereItem->m_indexCount         = sphereItem->m_pGeo->drawArgs["sphere"].indexCount;
    sphereItem->m_startIndexLocation = sphereItem->m_pGeo->drawArgs["sphere"].startIndexLocation;
    sphereItem->m_baseVertexLocation = sphereItem->m_pGeo->drawArgs["sphere"].baseVertexLocation;
    m_allRenderItems.push_back(std::move(sphereItem));

    auto cylItem = std::make_unique<RenderItem>();
    XMStoreFloat4x4(&cylItem->m_world, XMMatrixScaling(2.0f, 1.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.0f, 0.0f));
    cylItem->m_objCbIndex         = objectCbIndex++;
    cylItem->m_pGeo               = m_geometries["shapeGeo"].get();
    cylItem->m_pMat               = m_materials["stonemat"].get();
    cylItem->m_primitiveType      = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
    cylItem->m_indexCount         = cylItem->m_pGeo->drawArgs["cyl"].indexCount;
    cylItem->m_startIndexLocation = cylItem->m_pGeo->drawArgs["cyl"].startIndexLocation;
    cylItem->m_baseVertexLocation = cylItem->m_pGeo->drawArgs["cyl"].baseVertexLocation;
    m_allRenderItems.push_back(std::move(cylItem));

    // Start with a box
    m_opaqueItems.push_back(m_allRenderItems[static_cast<uint32>(Shapes::Box)].get());
}

// ====================================================================================================================
void ShapesDemo::BuildFrameResources()
{
    for (int i = 0; i < NumFrameResources; i++)
    {
        m_frameResources.push_back(
            std::make_unique<FrameResource::FrameResource>(m_d3dDevice.Get(),
                                                           1,
                                                           static_cast<UINT>(m_allRenderItems.size()),
                                                           static_cast<UINT>(m_materials.size())));
    }
}

// ====================================================================================================================
void ShapesDemo::BuildDescriptorHeaps()
{
    UINT objCount       = (UINT)m_allRenderItems.size();
    UINT numDescriptors = (objCount + 1) * NumFrameResources;
    m_passCbvOffset     = objCount * NumFrameResources;

    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = { };
    cbvHeapDesc.NumDescriptors             = numDescriptors;
    cbvHeapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask                   = 0;
    ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap)));
}

// ====================================================================================================================
void ShapesDemo::BuildConstBufferViews()
{
    UINT objCbByteSize = BaseUtil::CalcConstantBufferByteSize(sizeof(FrameResource::ObjectConstants));
    UINT objCount      = (UINT) m_allRenderItems.size();

    // Create const buffers for each object that is going to be rendered.
    for (int frameIndex = 0; frameIndex < NumFrameResources; ++frameIndex)
    {
        auto objectCb = m_frameResources[frameIndex]->m_objCb->Resource();
        for (UINT i = 0; i < objCount; ++i)
        {
            D3D12_GPU_VIRTUAL_ADDRESS cbAddress = objectCb->GetGPUVirtualAddress();
            cbAddress += i * static_cast<UINT64>(objCbByteSize);

            int heapIndex = frameIndex * objCount + i;
            auto handle = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
            handle.Offset(heapIndex, static_cast<UINT>(m_cbvSrvUavDescriptorSize));

            D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
            cbvDesc.BufferLocation                  = cbAddress;
            cbvDesc.SizeInBytes                     = objCbByteSize;

            m_d3dDevice->CreateConstantBufferView(&cbvDesc, handle);
        }
    }

    UINT passCbByteSize = BaseUtil::CalcConstantBufferByteSize(sizeof(FrameResource::PassConstants));

    // Create const buffers for each frame resource for view proj matrices etc.
    for (int frameIndex = 0; frameIndex < NumFrameResources; ++frameIndex)
    {
        auto passCb = m_frameResources[frameIndex]->m_passCb->Resource();
        D3D12_GPU_VIRTUAL_ADDRESS cbAddress = passCb->GetGPUVirtualAddress();

        int heapIndex = m_passCbvOffset + frameIndex;
        auto handle   = CD3DX12_CPU_DESCRIPTOR_HANDLE(m_cbvHeap->GetCPUDescriptorHandleForHeapStart());
        handle.Offset(heapIndex, static_cast<UINT>(m_cbvSrvUavDescriptorSize));

        D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};
        cbvDesc.BufferLocation                  = cbAddress;
        cbvDesc.SizeInBytes                     = passCbByteSize;

        m_d3dDevice->CreateConstantBufferView(&cbvDesc, handle);
    }
}

// ====================================================================================================================
void ShapesDemo::BuildPsos()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc = { };

    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    opaquePsoDesc.InputLayout = { m_inputLayout.data(), (UINT) m_inputLayout.size() };
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
    opaquePsoDesc.RasterizerState          = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    opaquePsoDesc.BlendState               = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    opaquePsoDesc.DepthStencilState        = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    opaquePsoDesc.SampleMask               = UINT_MAX;
    opaquePsoDesc.PrimitiveTopologyType    = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    opaquePsoDesc.NumRenderTargets         = 1;
    opaquePsoDesc.RTVFormats[0]            = m_backBufferFormat;
    opaquePsoDesc.SampleDesc.Count         = m_4xMsaaEn ? 4 : 1;
    opaquePsoDesc.SampleDesc.Quality       = m_4xMsaaEn ? (m_4xMsaaQuality - 1) : 0;
    opaquePsoDesc.DSVFormat                = m_depthStencilFormat;

    ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&m_psos["opaque"])));
}

// ====================================================================================================================
void ShapesDemo::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& rItems)
{
    UINT objCbByteSize = BaseUtil::CalcConstantBufferByteSize(sizeof(FrameResource::ObjectConstants));
    UINT matCbByteSize = BaseUtil::CalcConstantBufferByteSize(sizeof(FrameResource::MaterialConstants));

    auto matCb = m_currFrameResource->m_matCb->Resource();
    auto objectCb = m_currFrameResource->m_objCb->Resource();

    for (size_t i = 0; i < rItems.size(); ++i)
    {
        auto ri = rItems[i];
        cmdList->IASetVertexBuffers(0, 1, &ri->m_pGeo->VertexBufferView());
        cmdList->IASetIndexBuffer(&ri->m_pGeo->IndexBufferView());
        cmdList->IASetPrimitiveTopology(ri->m_primitiveType);

        D3D12_GPU_VIRTUAL_ADDRESS objCbAddress = objectCb->GetGPUVirtualAddress() + ri->m_objCbIndex * objCbByteSize;
        D3D12_GPU_VIRTUAL_ADDRESS matCbAddress= matCb->GetGPUVirtualAddress() + ri->m_pMat->m_matCbIndex * matCbByteSize;
        cmdList->SetGraphicsRootConstantBufferView(0, objCbAddress);
        cmdList->SetGraphicsRootConstantBufferView(1, matCbAddress);

        // The actual draw call
        cmdList->DrawIndexedInstanced(ri->m_indexCount, 1, ri->m_startIndexLocation, ri->m_baseVertexLocation, 0);
    }
}

// ====================================================================================================================
void ShapesDemo::UpdateRenderItems()
{
    m_opaqueItems.clear();
    m_opaqueItems.push_back(m_allRenderItems[static_cast<uint32>(m_currentShape)].get());
}

// ====================================================================================================================
void ShapesDemo::UpdateObjectCBs(const BaseTimer& timer)
{
    auto currObjectCB = m_currFrameResource->m_objCb.get();
    for (auto& e : m_allRenderItems)
    {
        if (e->m_numFramesDirty > 0)
        {
            XMMATRIX world = XMLoadFloat4x4(&e->m_world);
            XMMATRIX texTransform = XMLoadFloat4x4(&e->m_texTransform);

            FrameResource::ObjectConstants objConstants;
            XMStoreFloat4x4(&objConstants.m_world, XMMatrixTranspose(world));
            XMStoreFloat4x4(&objConstants.m_texTransform, XMMatrixTranspose(texTransform));

            currObjectCB->CopyData(e->m_objCbIndex, objConstants);

            e->m_numFramesDirty--;
        }
    }
}

// ====================================================================================================================
void ShapesDemo::UpdateMaterialCBs(const BaseTimer& timer)
{
    auto currMaterialCB = m_currFrameResource->m_matCb.get();
    for (auto& e : m_materials)
    {
        // Only update the cbuffer data if the constants have changed.  If the cbuffer
        // data changes, it needs to be updated for each FrameResource.
        Material* mat = e.second.get();
        if (mat->m_numFramesDirty > 0)
        {
            XMMATRIX matTransform = XMLoadFloat4x4(&mat->m_matTransform);

            FrameResource::MaterialConstants matConstants;
            matConstants.diffuseAlbedo = mat->m_diffuseAlbedo;
            matConstants.fresnelR0 = mat->m_fresnelR0;
            matConstants.roughness = mat->m_roughness;
            XMStoreFloat4x4(&matConstants.matTransform, XMMatrixTranspose(matTransform));

            currMaterialCB->CopyData(mat->m_matCbIndex, matConstants);

            // Next FrameResource need to be updated too.
            mat->m_numFramesDirty--;
        }
    }
}

// ====================================================================================================================
void ShapesDemo::UpdateMainPassCB(const BaseTimer& timer)
{
    XMMATRIX view = XMLoadFloat4x4(&m_view);
    XMMATRIX proj = XMLoadFloat4x4(&m_proj);

    XMMATRIX viewProj    = XMMatrixMultiply(view, proj);
    XMMATRIX invView     = XMMatrixInverse(&XMMatrixDeterminant(view), view);
    XMMATRIX invProj     = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
    XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

    XMStoreFloat4x4(&m_mainPassCB.view, XMMatrixTranspose(view));
    XMStoreFloat4x4(&m_mainPassCB.invView, XMMatrixTranspose(invView));
    XMStoreFloat4x4(&m_mainPassCB.proj, XMMatrixTranspose(proj));
    XMStoreFloat4x4(&m_mainPassCB.invProj, XMMatrixTranspose(invProj));
    XMStoreFloat4x4(&m_mainPassCB.viewProj, XMMatrixTranspose(viewProj));
    XMStoreFloat4x4(&m_mainPassCB.invViewProj, XMMatrixTranspose(invViewProj));

    m_mainPassCB.eyePosW             = m_eyePos;
    m_mainPassCB.renderTargetSize    = XMFLOAT2(static_cast<float>(m_clientWidth), static_cast<float>(m_clientHeight));
    m_mainPassCB.invRenderTargetSize = XMFLOAT2(1.0f / m_clientWidth, 1.0f / m_clientHeight);
    m_mainPassCB.nearZ               = 1.0f;
    m_mainPassCB.farZ                = 1000.0f;
    m_mainPassCB.totalTime           = timer.TotalTimeInSecs();
    m_mainPassCB.deltaTime           = timer.DeltaTimeInSecs();

    m_mainPassCB.ambientLight        = { 0.25f, 0.25f, 0.35f, 1.0f };

    m_mainPassCB.lights[0].direction = { 0.57735f, -0.57735f, 0.57735f };
    m_mainPassCB.lights[0].strength  = { 0.6f, 0.6f, 0.6f };

    m_mainPassCB.lights[1].direction = { -0.57735f, -0.57735f, 0.57735f };
    m_mainPassCB.lights[1].strength  = { 0.3f, 0.3f, 0.3f };

    m_mainPassCB.lights[2].direction = { 0.0f, -0.707f, -0.707f };
    m_mainPassCB.lights[2].strength  = { 0.15f, 0.15f, 0.15f };

    auto currPassCB = m_currFrameResource->m_passCb.get();
    currPassCB->CopyData(0, m_mainPassCB);
}

// ====================================================================================================================
// Write the win main func here
int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   PSTR      pCmdLine,
                   int       nShowCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    ShapesDemo demoApp(hInstance);

    int retCode = 0;
    if (demoApp.Initialize() == true)
    {
        retCode = demoApp.Run();
    }
    else
    {
        cout << "Error initializing the shapes app...\n";
    }

    return retCode;
}

