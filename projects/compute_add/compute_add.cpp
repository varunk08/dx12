#include <iostream>
#include <Windows.h>
#include <DirectXColors.h>
#include <vector>
#include <unordered_map>
#include <string>
#include <fstream>
#include "../common/BaseApp.h"

using namespace std;
using namespace DirectX;
using Microsoft::WRL::ComPtr;

constexpr unsigned NumDataElements = 32;

struct Data
{
	XMFLOAT3 v1;
};

class InitD3dApp : public BaseApp
{
public:
    InitD3dApp(HINSTANCE hInstance);
    ~InitD3dApp();

    virtual bool Initialize() override;
    void BuildInputs();
    void BuildComputePipeline();
    void BuildComputeShader();
    void ReadbackOutput();
    void DoComputeWork();
private:
    virtual void OnResize() override;
    virtual void Update(const BaseTimer& timer) override;
    virtual void Draw(const BaseTimer& timer) override;
    virtual void OnMouseDown(WPARAM btnState, int x, int y);
    virtual void OnMouseUp(WPARAM btnState, int x, int y);
    virtual void OnMouseMove(WPARAM btnState, int x, int y);

    ComPtr<ID3D12Resource> inputBufferA_ = nullptr;
	ComPtr<ID3D12Resource> inputUploadBufferA_ = nullptr;

    ComPtr<ID3D12Resource> inputBufferB_ = nullptr;
	ComPtr<ID3D12Resource> inputUploadBufferB_ = nullptr;

    ComPtr<ID3D12Resource> outputBuffer_ = nullptr;
	ComPtr<ID3D12Resource> readBackBuffer_ = nullptr;

    std::unordered_map<std::string,  ComPtr<ID3DBlob>> shaders_;
    std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> pipelines_;
    ComPtr<ID3D12RootSignature> rootSignature_ = nullptr;
    ComPtr<ID3D12DescriptorHeap> srvDescHeap_ = nullptr;
};

InitD3dApp::InitD3dApp(
    HINSTANCE hInstance)
    :
    BaseApp(hInstance)
{
}

InitD3dApp::~InitD3dApp()
{
}

bool InitD3dApp::Initialize()
{
    bool result = true;

    result = BaseApp::Initialize();

    if (result == true)
    {
        // Reset the command list to prep for initialization commands.
        ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), nullptr));

        BuildInputs();
        BuildComputeShader();
        BuildComputePipeline();

        // Execute the initialization commands.
        ThrowIfFailed(m_commandList->Close());
        ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
        m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

        // Wait until initialization is complete.
        FlushCommandQueue();

        DoComputeWork();

    }

    return result;
}

void InitD3dApp::DoComputeWork()
{
    	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(m_directCmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(m_commandList->Reset(m_directCmdListAlloc.Get(), pipelines_["vecAdd"].Get()));

	m_commandList->SetComputeRootSignature(rootSignature_.Get());

	m_commandList->SetComputeRootShaderResourceView(0, inputBufferA_->GetGPUVirtualAddress());
	m_commandList->SetComputeRootShaderResourceView(1, inputBufferB_->GetGPUVirtualAddress());
	m_commandList->SetComputeRootUnorderedAccessView(2, outputBuffer_->GetGPUVirtualAddress());

    // dispatch 1 thread group
	m_commandList->Dispatch(1, 1, 1);

	// Schedule to copy the data to the default buffer to the readback buffer.
	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(outputBuffer_.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE));

	m_commandList->CopyResource(readBackBuffer_.Get(), outputBuffer_.Get());

	m_commandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(outputBuffer_.Get(),
		D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON));

	// Done recording commands.
	ThrowIfFailed(m_commandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { m_commandList.Get() };
	m_commandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait for the work to finish.
	FlushCommandQueue();

	// Map the data so we can read it on CPU.
	Data* mappedData = nullptr;
	ThrowIfFailed(readBackBuffer_->Map(0, nullptr, reinterpret_cast<void**>(&mappedData)));

	std::ofstream fout("results.txt");

	for(int i = 0; i < NumDataElements; ++i)
	{
		fout << "(" << mappedData[i].v1.x << ", " << mappedData[i].v1.y << ", " << mappedData[i].v1.z << ")" << std::endl;
	}

	readBackBuffer_->Unmap(0, nullptr);
}

void InitD3dApp::BuildInputs()
{
    	// Generate some data.
	std::vector<Data> dataA(NumDataElements);
	std::vector<Data> dataB(NumDataElements);
	for(int i = 0; i < NumDataElements; ++i)
	{
        float data = static_cast<float>(i);
		dataA[i].v1 = XMFLOAT3(data, data, data);
		dataB[i].v1 = XMFLOAT3(data, data, data);
	}

	UINT64 byteSize = dataA.size()*sizeof(Data);

	// Create some buffers to be used as SRVs.
	inputBufferA_ = BaseUtil::CreateDefaultBuffer(
		m_d3dDevice.Get(),
		m_commandList.Get(),
		dataA.data(),
		byteSize,
		inputUploadBufferA_);

	inputBufferB_ = BaseUtil::CreateDefaultBuffer(
		m_d3dDevice.Get(),
		m_commandList.Get(),
		dataB.data(),
		byteSize,
		inputUploadBufferB_);

	// Create the buffer that will be a UAV.
	ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize, D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS),
		D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
		nullptr,
		IID_PPV_ARGS(&outputBuffer_)));

	ThrowIfFailed(m_d3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_READBACK),
		D3D12_HEAP_FLAG_NONE,
		&CD3DX12_RESOURCE_DESC::Buffer(byteSize),
		D3D12_RESOURCE_STATE_COPY_DEST,
		nullptr,
		IID_PPV_ARGS(&readBackBuffer_)));
}

void InitD3dApp::OnResize()
{
    BaseApp::OnResize();
}

void InitD3dApp::Update(const BaseTimer & timer)
{
    if ((m_currentFence != 0) && (m_fence->GetCompletedValue() < m_currentFence)) {
        HANDLE eventHandle = CreateEventEx(nullptr, false, false, EVENT_ALL_ACCESS);
        ThrowIfFailed(m_fence->SetEventOnCompletion(m_currentFence, eventHandle));
        WaitForSingleObject(eventHandle, INFINITE);
        CloseHandle(eventHandle);
    }
}

void InitD3dApp::Draw(const BaseTimer & timer)
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

void InitD3dApp::OnMouseDown(WPARAM btnState, int x, int y)
{
    std::wstring time = std::to_wstring(m_timer.TotalTimeInSecs());
    time += std::wstring(L" seconds since start of app.");
    MessageBoxW(0, time.c_str(), L"Hello", MB_OK);
}

void InitD3dApp::OnMouseUp(WPARAM btnState, int x, int y)
{
}

void InitD3dApp::OnMouseMove(WPARAM btnState, int x, int y)
{
}

void InitD3dApp::BuildComputePipeline()
{
    // Root parameter can be a table, root descriptor or root constants.
    CD3DX12_ROOT_PARAMETER slotRootParameter[3];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsShaderResourceView(0);
    slotRootParameter[1].InitAsShaderResourceView(1);
    slotRootParameter[2].InitAsUnorderedAccessView(0);

    // A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(3, slotRootParameter,
		0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_NONE);

    // create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
    ComPtr<ID3DBlob> serializedRootSig = nullptr;
    ComPtr<ID3DBlob> errorBlob = nullptr;
    HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
        serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

    if(errorBlob != nullptr)
    {
        ::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
    }
    ThrowIfFailed(hr);

    ThrowIfFailed(m_d3dDevice->CreateRootSignature(
		0,
        serializedRootSig->GetBufferPointer(),
        serializedRootSig->GetBufferSize(),
        IID_PPV_ARGS(rootSignature_.GetAddressOf())));

	D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
	computePsoDesc.pRootSignature = rootSignature_.Get();
	computePsoDesc.CS =	{ reinterpret_cast<BYTE*>(shaders_["vecAddCS"]->GetBufferPointer()), shaders_["vecAddCS"]->GetBufferSize()	};
	computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(m_d3dDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&pipelines_["vecAdd"])));
}

void InitD3dApp::BuildComputeShader()
{
    shaders_["vecAddCS"] = BaseUtil::CompileShader(L"shaders\\computeAdd.hlsl", nullptr, "CS", "cs_5_0");
}

void InitD3dApp::ReadbackOutput()
{
}

int WINAPI WinMain(HINSTANCE hInstance,
                   HINSTANCE hPrevInstance,
                   PSTR      pCmdLine,
                   int       nShowCmd)
{
    // Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
    _CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

    InitD3dApp theApp(hInstance);
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