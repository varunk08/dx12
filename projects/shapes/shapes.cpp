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
constexpr unsigned int NumFrameResources = 3;

// ====================================================================================================================
struct RenderItem
{
		RenderItem() = default;
		
		XMFLOAT4X4               m_world = MathHelper::Identity4x4();
		int                      m_numFramesDirty = NumFrameResources;
		UINT                     m_objCbIndex = -1;
		MeshGeometry*            m_pGeo = nullptr;
		D3D12_PRIMITIVE_TOPOLOGY m_primitiveType = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
		UINT                     m_indexCount                        = 0;
		UINT                     m_startIndexLocation                = 0;
		INT                      m_baseVertexLocation                 = 0;
};

// ====================================================================================================================
class ShapesDemo : public BaseApp
{
public:
    ShapesDemo(HINSTANCE hInstance);
    bool Initialize();
protected:
private:
    void Update(const BaseTimer& gt);
    void Draw(const BaseTimer& gt);

    void ShapesBuildRootSignature();
    void ShapesBuildShadersAndInputLayout();
    void ShapesBuildShapeGeometry();
	void ShapesBuildRenderItems();
    void ShapesBuildFrameResources();
    void ShapesBuildDescriptorHeaps();
    void ShapesBuildConstBufferViews();
    void ShapesBuildPsos();

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
};

// ====================================================================================================================
bool ShapesDemo::Initialize()
{
    bool result = BaseApp::Initialize();

    if (result == true)
    {
        ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), nullptr));

        ShapesBuildRootSignature();
		ShapesBuildShadersAndInputLayout();
		ShapesBuildShapeGeometry();
        ShapesBuildFrameResources();
        ShapesBuildDescriptorHeaps();
        ShapesBuildConstBufferViews();
        ShapesBuildPsos();

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
void ShapesDemo::Update(const BaseTimer& gt)
{

}

// ====================================================================================================================
void ShapesDemo::Draw(const BaseTimer& gt)
{

}

// ====================================================================================================================
void ShapesDemo::ShapesBuildRootSignature()
{
    // Create a couple of "descriptor range"s. These map registers in the shader to the
    // resources here.
    CD3DX12_DESCRIPTOR_RANGE cbvTable0;
    cbvTable0.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);

    CD3DX12_DESCRIPTOR_RANGE cbvTable1;
    cbvTable1.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 1);

    // A "root parameter" can be a descriptor table, root constant or descriptor.
    CD3DX12_ROOT_PARAMETER slotRootParameter[2];
    slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable0);
    slotRootParameter[1].InitAsDescriptorTable(1, &cbvTable1);

    // A root signature is an array of root parameters.
    CD3DX12_ROOT_SIGNATURE_DESC rootSignDesc(2,
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
void ShapesDemo::ShapesBuildShadersAndInputLayout()
{
    m_shaders["standardVS"] = BaseUtil::CompileShader(L"shaders\\colors.hlsl", nullptr, "VS", "vs_5_1");
    m_shaders["opaquePS"]   = BaseUtil::CompileShader(L"shaders\\colors.hlsl", nullptr, "PS", "ps_5_1");

    m_inputLayout = 
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0},
        {"COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0}
    };
}

// ====================================================================================================================
void ShapesDemo::ShapesBuildShapeGeometry()
{
    GeometryGenerator geoGen;
	MeshData box = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);
	
	UINT boxVertexOffset = 0;
	UINT boxIndexOffset  = 0;
	
	SubmeshGeometry boxSubmesh;
	boxSubmesh.indexCount         = static_cast<UINT>(box.m_indices32.size());
	boxSubmesh.startIndexLocation = boxIndexOffset;
	boxSubmesh.baseVertexLocation = boxVertexOffset;
	
	auto totalVertexCount = box.m_vertices.size();
	
	std::vector<FrameResource::Vertex> vertices(totalVertexCount);
	
	UINT k = 0;
	for (size_t i = 0; i < box.m_vertices.size(); ++i, ++k)
	{
		vertices[k].pos = box.m_vertices[i].m_position;
		vertices[k].color = XMFLOAT4(DirectX::Colors::DarkGreen);
	}
	
	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), std::begin(box.GetIndices16()), std::end(box.GetIndices16()));
	
	const UINT vbByteSize = static_cast<UINT>(vertices.size());
	const UINT ibByteSize = static_cast<UINT>(indices.size());
	
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
	
	geo->vertexByteStride     = sizeof(Vertex);
	geo->vertexBufferByteSize = vbByteSize;
	geo->indexFormat          = DXGI_FORMAT_R16_UINT;
	geo->indexBufferByteSize  = ibByteSize;	
	geo->drawArgs["box"]      = boxSubmesh;
	
	m_geometries[geo->name] = std::move(geo);
}

// ====================================================================================================================
void ShapesDemo::ShapesBuildRenderItems()
{
	auto boxItem = std::make_unique<RenderItem>();
	XMStoreFloat4x4(&boxItem->m_world, XMMatrixScaling(2.0f, 2.0f, 2.0f) * XMMatrixTranslation(0.0f, 0.5f, 0.0f));
	boxItem->m_objCbIndex         = 0;
	boxItem->m_pGeo               = m_geometries["shapeGeo"].get();
	boxItem->m_primitiveType      = D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST;
	boxItem->m_indexCount         = boxItem->m_pGeo->drawArgs["box"].indexCount;
	boxItem->m_startIndexLocation = boxItem->m_pGeo->drawArgs["box"].startIndexLocation;
	boxItem->m_baseVertexLocation = boxItem->m_pGeo->drawArgs["box"].baseVertexLocation;
	m_allRenderItems.push_back(std::move(boxItem));
	
	for(auto& e : m_allRenderItems)
		m_opaqueItems.push_back(e.get());
}

// ====================================================================================================================
void ShapesDemo::ShapesBuildFrameResources()
{
    for (int i = 0; i < NumFrameResources; i++)
    {
        m_frameResources.push_back(
            std::make_unique<FrameResource::FrameResource>(m_d3dDevice.Get(),
                                                           1,
                                                           static_cast<UINT>(m_allRenderItems.size())));
    }
}

// ====================================================================================================================
void ShapesDemo::ShapesBuildDescriptorHeaps()
{
    UINT objCount = (UINT)m_opaqueItems.size();
    UINT numDescriptors = (objCount + 1) * NumFrameResources;
    m_passCbvOffset = objCount * NumFrameResources;

    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = { };
    cbvHeapDesc.NumDescriptors             = numDescriptors;
    cbvHeapDesc.Type                       = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags                      = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask                   = 0;
    ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_cbvHeap)));
}


// ====================================================================================================================
void ShapesDemo::ShapesBuildConstBufferViews()
{
    UINT objCbByteSize = BaseUtil::CalcConstantBufferByteSize(sizeof(FrameResource::ObjectConstants));
    UINT objCount      = (UINT) m_opaqueItems.size();

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
void ShapesDemo::ShapesBuildPsos()
{
    D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc = { };

    ZeroMemory(&opaquePsoDesc, sizeof(D3D12_GRAPHICS_PIPELINE_STATE_DESC));
    opaquePsoDesc.InputLayout = { m_inputLayout.data(), (UINT) m_inputLayout.size() };
    opaquePsoDesc.pRootSignature = m_rootSignature.Get();
    opaquePsoDesc.VS =
    {
        reinterpret_cast<BYTE*>(m_shaders["standardVS"]->GetBufferPointer()), m_shaders["standardVS"]->GetBufferSize()
    };
    opaquePsoDesc.PS =
    {
        reinterpret_cast<BYTE*>(m_shaders["opaquePS"]->GetBufferPointer(), m_shaders["opaquePS"]->GetBufferSize())
    };
    opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
    opaquePsoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_WIREFRAME;
    opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
    opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
    opaquePsoDesc.SampleMask = UINT_MAX;
    opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
    opaquePsoDesc.NumRenderTargets = 1;
    opaquePsoDesc.RTVFormats[0] = m_backBufferFormat;
    opaquePsoDesc.SampleDesc.Count = m_4xMsaaEn ? 4 : 1;
    opaquePsoDesc.SampleDesc.Quality = m_4xMsaaEn ? (m_4xMsaaQuality - 1) : 0;
    opaquePsoDesc.DSVFormat = m_depthStencilFormat;
    ThrowIfFailed(m_d3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&m_psos["opaque"])));
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
    if (demoApp.Initialize() == 0)
    {
        retCode = demoApp.Run();
    }
    else
    {
        cout << "Error initializing the shapes app...\n";
    }

    return retCode;
}

