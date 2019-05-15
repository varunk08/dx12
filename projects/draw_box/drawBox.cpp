#include <iostream>
#include <Windows.h>
#include <DirectXColors.h>

#include "../common/BaseApp.h"
#include "../common/BaseUtil.h"
#include "../common/MathHelper.h"
#include "../common/UploadBuffer.h"

using Microsoft::WRL::ComPtr;
using namespace std;
using namespace DirectX;

// ==========================================================================================
// Types used by this demo
struct Vertex
{
    XMFLOAT3 Pos;
    XMFLOAT4 Color;
};

struct ObjectConstants
{
    XMFLOAT4X4 WorldViewProj = MathHelper::Identity4x4();
};

// ==========================================================================================
// The demo app
class BasicBox : public BaseApp
{
public:
    BasicBox(HINSTANCE hInstance);

    // tell mr. compiler not to generate these default functions
    BasicBox(const BasicBox& box) = delete;
    BasicBox& operator=(const BasicBox& box) = delete;

    ~BasicBox();

    virtual bool Initialize() override;
private:
    virtual void OnResize() override;
    virtual void Update(const BaseTimer& timer) override;
    virtual void Draw(const BaseTimer& timer) override;
    virtual void OnMouseDown(WPARAM btnState, int x, int y);
    virtual void OnMouseUp(WPARAM btnState, int x, int y);
    virtual void OnMouseMove(WPARAM btnState, int x, int y);

    void BuildPSO();
    void BuildBoxGeometry();
    void BuildShadersAndInputLayout();
    void BuildRootSignature();
    void BuildDescriptorHeaps();
    void BuildConstantBuffers();

    ComPtr<ID3D12DescriptorHeap> m_pCbvHeap                   = nullptr;
    std::unique_ptr<UploadBuffer<ObjectConstants>> m_objectCb = nullptr;
    ComPtr<ID3D12RootSignature> m_rootSignature               = nullptr;
    ComPtr<ID3DBlob> m_vsByteCode                             = nullptr;
    ComPtr<ID3DBlob> m_psByteCode                             = nullptr;
    
    std::vector<D3D12_INPUT_ELEMENT_DESC> m_inputLayout;
};

// ==========================================================================================
BasicBox::BasicBox(
    HINSTANCE hInstance)
    :
    BaseApp(hInstance)
{
}

// ==========================================================================================
BasicBox::~BasicBox()
{
}

// ==========================================================================================
bool BasicBox::Initialize()
{
    bool result = BaseApp::Initialize();
    if (result == true) {
        ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), nullptr));

        BuildDescriptorHeaps();
        BuildConstantBuffers();
        BuildRootSignature();
        BuildShadersAndInputLayout();
        BuildBoxGeometry();
        BuildPSO();

        ThrowIfFailed(m_commandList->Close());
        ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
    
        FlushCommandQueue();
    }
}

// ==========================================================================================
void BasicBox::OnResize()
{
    BaseApp::OnResize();
}

// ==========================================================================================
void BasicBox::Update(const BaseTimer & timer)
{
}

// ==========================================================================================
void BasicBox::Draw(const BaseTimer & timer)
{
    // Reuse the memory associated with command recording.
    // We can only reset when the associated command lists have finished execution on the GPU.
    ThrowIfFailed(m_directCmdListAlloc->Reset());

    // A command list can be reset after it has been added to the command queue via ExecuteCommandList.
    // Reusing the command list reuses memory.
    ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), nullptr));

    // Indicate a state transition on the resource usage.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
                                   D3D12_RESOURCE_STATE_PRESENT,
                                   D3D12_RESOURCE_STATE_RENDER_TARGET));

    // Set the viewport and scissor rect.  This needs to be reset whenever the command list is reset.
    m_commandList->RSSetViewports(1, &m_screenViewport);
    m_commandList->RSSetScissorRects(1, &m_scissorRect);

    // Clear the back buffer and depth buffer.
    m_commandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::MediumVioletRed, 0, nullptr);
    m_commandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

    // Specify the buffers we are going to render to.
    m_commandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

    // Indicate a state transition on the resource usage.
    m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
        D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

    // Done recording commands.
    ThrowIfFailed(m_commandList->Close());

    // Add the command list to the queue for execution.
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

// ==========================================================================================
void BasicBox::OnMouseDown(WPARAM btnState, int x, int y)
{
    std::wstring time = std::to_wstring(m_timer.TotalTimeInSecs());
    time += std::wstring(L" seconds since start of app.");
    MessageBoxW(0, time.c_str(), L"Hello", MB_OK);
}

// ==========================================================================================
void BasicBox::OnMouseUp(WPARAM btnState, int x, int y)
{
}

// ==========================================================================================
void BasicBox::OnMouseMove(WPARAM btnState, int x, int y)
{
}

void BasicBox::BuildPSO()
{
}

void BasicBox::BuildBoxGeometry()
{
}

// ====================================================================================================================
void BasicBox::BuildShadersAndInputLayout()
{
    HRESULT hr = S_OK;

    m_vsByteCode = BaseUtil::CompileShader(L"shaders\\color.hlsl", nullptr, "VS", "vs_5_0");
    m_psByteCode = BaseUtil::CompileShader(L"shaders\\color.hlsl", nullptr, "PS", "ps_5_0");

    m_inputLayout =
    {
        {"POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
        {"COLOR", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
    };
}

// ====================================================================================================================
// Root signature defines inputs required by the shader.
void BasicBox::BuildRootSignature()
{
    CD3DX12_ROOT_PARAMETER slotRootParameter[1];

    CD3DX12_DESCRIPTOR_RANGE cbvTable = { };
    cbvTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_CBV, 1, 0);
    slotRootParameter[0].InitAsDescriptorTable(1, &cbvTable);

    CD3DX12_ROOT_SIGNATURE_DESC rootSignDesc(1,
                                             slotRootParameter,
                                             0,
                                             nullptr,
                                             D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

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

    ThrowIfFailed(m_d3dDevice->CreateRootSignature(0,
                                                   serializedRootSig->GetBufferPointer(),
                                                   serializedRootSig->GetBufferSize(),
                                                   IID_PPV_ARGS(&m_rootSignature)));
}

// ====================================================================================================================
void BasicBox::BuildDescriptorHeaps()
{
    D3D12_DESCRIPTOR_HEAP_DESC cbvHeapDesc = {};
    cbvHeapDesc.NumDescriptors = 1;
    cbvHeapDesc.Type           = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
    cbvHeapDesc.Flags          = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
    cbvHeapDesc.NodeMask       = 0;

    ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&cbvHeapDesc, IID_PPV_ARGS(&m_pCbvHeap)));
}

// ====================================================================================================================
void BasicBox::BuildConstantBuffers()
{
    m_objectCb = std::make_unique<UploadBuffer<ObjectConstants>>(m_d3dDevice.Get(), 1, true);
    
    UINT objCbBytes                         = BaseUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));
    D3D12_GPU_VIRTUAL_ADDRESS cbGpuVirtAddr = m_objectCb->Resource()->GetGPUVirtualAddress();

    int boxCbufIndex = 0;
    cbGpuVirtAddr    = boxCbufIndex * objCbBytes;

    D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = { };
    cbvDesc.BufferLocation                  = cbGpuVirtAddr;
    cbvDesc.SizeInBytes                     = BaseUtil::CalcConstantBufferByteSize(sizeof(ObjectConstants));

    m_d3dDevice->CreateConstantBufferView(&cbvDesc, m_pCbvHeap->GetCPUDescriptorHandleForHeapStart());
}

// ====================================================================================================================
int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   PSTR      pCmdLine,
                   int       nShowCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    BasicBox theApp(hInstance);
    int retCode = 0;

    if (theApp.Initialize() == true)
    {
        retCode = theApp.Run();
    }
    else
    {
        cout << "Error Initializing the app" << endl;
    }

    return retCode;
}