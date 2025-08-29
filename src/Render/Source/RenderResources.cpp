#include "pch.h"
#include "Header/RenderResources.h"

RenderResources::RenderResources(HWND hwnd, uint32 width, uint32 height)
{
	CreateDXGIFactory();
	CreateDXGIAdapters();
	CreateDevice(m_pAdapter);
	CreateCommandQueue(m_pDevice);
	CreateDescriptorHeap(m_pDevice);
	CreateSwapChain(m_pFactory, m_pCommandQueue, hwnd, width, height);
	m_rtvDescriptorSize = m_pDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	CreateRenderTargets(m_pDevice);
	CreateCommandAllocator(m_pDevice);

	CreatePipelineState(m_pDevice, L"../../res/Gameplay/shaders/shader.hlsl");
}

RenderResources::~RenderResources()
{
	WaitForGpu();

	if (m_pCommandList) { m_pCommandList->Release(); m_pCommandList = nullptr; }
	if (m_pPipelineState) { m_pPipelineState->Release(); m_pPipelineState = nullptr; }
	if (m_pRootSignature) { m_pRootSignature->Release(); m_pRootSignature = nullptr; }
	if (m_pCommandAllocator) { m_pCommandAllocator->Release(); m_pCommandAllocator = nullptr; }

	for (uint32 n = 0; n < FrameCount; n++)
	{
		if (m_pRenderTargets[n]) { m_pRenderTargets[n]->Release(); m_pRenderTargets[n] = nullptr; }
	}

	if (m_pRtvHeap) { m_pRtvHeap->Release(); m_pRtvHeap = nullptr; }
	if (m_pSwapChain) { m_pSwapChain->Release(); m_pSwapChain = nullptr; }
	if (m_pCommandQueue) { m_pCommandQueue->Release(); m_pCommandQueue = nullptr; }

	if (m_pFence) { m_pFence->Release(); m_pFence = nullptr; }
	if (m_fenceEvent)
	{
		CloseHandle(m_fenceEvent);
		m_fenceEvent = nullptr;
	}

	if (m_pDevice) { m_pDevice->Release(); m_pDevice = nullptr; }
	if (m_pAdapter) { m_pAdapter->Release(); m_pAdapter = nullptr; }
	if (m_pFactory) { m_pFactory->Release(); m_pFactory = nullptr; }

	IDXGIDebug1* dxgiDebug;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&dxgiDebug))))
	{
		dxgiDebug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		dxgiDebug->Release();
	}
}

void RenderResources::WaitForGpu()
{
	if (m_pCommandQueue && m_pFence)
	{
		UINT64 fenceValue = m_fenceValue + 1;
		if (SUCCEEDED(m_pCommandQueue->Signal(m_pFence, fenceValue)))
		{
			m_fenceValue = fenceValue;
			if (m_pFence->GetCompletedValue() < fenceValue)
			{
				if (m_fenceEvent)
				{
					m_pFence->SetEventOnCompletion(fenceValue, m_fenceEvent);
					WaitForSingleObject(m_fenceEvent, INFINITE);
				}
			}
		}
	}
}

IDXGIFactory2* RenderResources::GetDXGIFactory()
{
	return m_pFactory;
}

IDXGIAdapter* RenderResources::GetDXGIAdapters()
{
	return m_pAdapter;
}

void RenderResources::CreateDXGIFactory()
{
	if (SUCCEEDED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&m_pFactory))))
	{
		Utils::DebugLog("DXGI Factory has been created !");
		return;
	}
	Utils::DebugError("Error while creating DXGI Factory");
}

void RenderResources::CreateDXGIAdapters()
{
	IDXGIFactory6* pFactory6;

	if (SUCCEEDED(m_pFactory->QueryInterface(IID_PPV_ARGS(&pFactory6))))
	{
		if (FAILED(pFactory6->EnumAdapterByGpuPreference(0, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&m_pAdapter))))
		{
			Utils::DebugError("Error finding the adapter");
		}

		pFactory6->Release();
	}
	else
	{
		Utils::DebugError("Adapter not compatible with factory 6");
	}
}

void RenderResources::CreateDevice(IDXGIAdapter* pAdapter)
{
	if (SUCCEEDED(D3D12CreateDevice(pAdapter, D3D_FEATURE_LEVEL_11_1, IID_PPV_ARGS(&m_pDevice))))
	{
		Utils::DebugLog("Device has successfuly been created !");
		return;
	}
	Utils::DebugError("Error while creating the device !");
}

void RenderResources::CreateCommandQueue(ID3D12Device* pDevice)
{
	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
	desc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	desc.NodeMask = 0;

	if (SUCCEEDED(pDevice->CreateCommandQueue(&desc, IID_PPV_ARGS(&m_pCommandQueue))))
	{
		Utils::DebugLog("Command queue has been created !");
		return;
	}

	Utils::DebugError("Error while creating the command queue !");
}

void RenderResources::CreateSwapChain(IDXGIFactory2* pFactory, ID3D12CommandQueue* pCommandQueue, HWND hwnd, uint32 width, uint32 height)
{
	DXGI_SWAP_CHAIN_DESC1 desc = {};
	desc.Width = width;
	desc.Height = height;
	desc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	desc.Stereo = FALSE;
	desc.SampleDesc.Count = 1;
	desc.SampleDesc.Quality = 0;
	desc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	desc.BufferCount = FrameCount;
	desc.Scaling = DXGI_SCALING_NONE;
	desc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	desc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;
	desc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING | DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

	IDXGISwapChain1* pSwapChain1 = nullptr;

	if (SUCCEEDED(pFactory->CreateSwapChainForHwnd(pCommandQueue, hwnd, &desc, nullptr, nullptr, &pSwapChain1)))
	{
		if (SUCCEEDED(pSwapChain1->QueryInterface(__uuidof(IDXGISwapChain3), (void**)&m_pSwapChain)))
		{
			Utils::DebugLog("SwapChain has successfuly been created !");
			m_frameIndex = m_pSwapChain->GetCurrentBackBufferIndex();
			pSwapChain1->Release();
			return;
		}
		Utils::DebugError("Error while casting the swap chain from swap chain 1 to 3 !");
		pSwapChain1->Release();
		return;
	}
	Utils::DebugError("Error while creating the swap chain !");
}

void RenderResources::CreateDescriptorHeap(ID3D12Device* pDevice)
{
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	desc.NumDescriptors = FrameCount;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	if (SUCCEEDED(pDevice->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&m_pRtvHeap))))
	{
		Utils::DebugLog("RTV descriptor heap has been created !");
		return;
	}
	Utils::DebugError("Error while creating the RTV descriptor heap !");
}

void RenderResources::CreateRenderTargets(ID3D12Device* pDevice)
{
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = m_pRtvHeap->GetCPUDescriptorHandleForHeapStart();

	for (uint32 n = 0; n < FrameCount; n++)
	{
		if (SUCCEEDED(m_pSwapChain->GetBuffer(n, IID_PPV_ARGS(&m_pRenderTargets[n]))))
		{
			pDevice->CreateRenderTargetView(m_pRenderTargets[n], nullptr, rtvHandle);
			rtvHandle.ptr += m_rtvDescriptorSize;
		}
		else
		{
			Utils::DebugError("Error while creating render target number: ", n);
		}
	}

}

void RenderResources::CreateCommandAllocator(ID3D12Device* pDevice)
{
	if (SUCCEEDED(m_pDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_pCommandAllocator))))
	{
		Utils::DebugLog("Command Allocator has been created !");
		return;
	}
	Utils::DebugError("Error while creating the Command Allocator !");
}

void RenderResources::CreateRootSignature(ID3D12Device* pDevice)
{
	D3D12_ROOT_SIGNATURE_DESC desc = {};
	desc.NumParameters = 0;
	desc.pParameters = nullptr;
	desc.NumStaticSamplers = 0;
	desc.pStaticSamplers = nullptr;
	desc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	ID3DBlob* signature = nullptr;
	ID3DBlob* error = nullptr;

	if (SUCCEEDED(D3D12SerializeRootSignature(&desc, D3D_ROOT_SIGNATURE_VERSION_1, &signature, &error)))
	{
		if (SUCCEEDED(m_pDevice->CreateRootSignature(0, signature->GetBufferPointer(), signature->GetBufferSize(), IID_PPV_ARGS(&m_pRootSignature))))
		{
			Utils::DebugLog("Root Signature has been created !");
			return;
		}
		Utils::DebugError("Error while creating the Root Signature !");
	}
	Utils::DebugError("Error while creating the Serialize Root Signature: ", error);

	signature->Release();

	if (error)
		error->Release();

}


ID3DBlob* RenderResources::CompileShader(const std::wstring& path, const char* target)
{
	UINT flags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION | D3DCOMPILE_WARNINGS_ARE_ERRORS | D3DCOMPILE_ALL_RESOURCES_BOUND;

	const char* entryPoint = (!target || strcmp(target, "vs_5_0") != 0) ? "psmain" : "vsmain";

	ID3DBlob* compiledShader;
	ID3DBlob* errorBlob;

	HRESULT hr = D3DCompileFromFile(path.c_str(), 0, D3D_COMPILE_STANDARD_FILE_INCLUDE, entryPoint, target, flags, 0, &compiledShader, &errorBlob);

	if (FAILED(hr))
	{
		Utils::DebugError("Shader loading error was: ", hr, ", for shader: ", path);
		if (errorBlob)
		{
			Utils::DebugError("Shader compilation error: ", (const char*)errorBlob->GetBufferPointer(), ", for shader :", path);
		}
		return nullptr;
	}

	return compiledShader;
}

void RenderResources::CreatePipelineState(ID3D12Device* pDevice, const std::wstring& shaderPath)
{
	if (!pDevice)
	{
		Utils::DebugError(" PSO device was not initialized !");
		return;
	}

	ID3DBlob* vsBlob = CompileShader(shaderPath, "vs_5_0");
	ID3DBlob* psBlob = CompileShader(shaderPath, "ps_5_0");

	CreateRootSignature(pDevice);

	D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
	{
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "COLOR", 0, DXGI_FORMAT_R32G32B32A32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};

	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.InputLayout = { inputElementDescs , _countof(inputElementDescs)};
	psoDesc.pRootSignature = m_pRootSignature;
	psoDesc.VS = { vsBlob->GetBufferPointer(), vsBlob->GetBufferSize() };
	psoDesc.PS = { psBlob->GetBufferPointer(), psBlob->GetBufferSize() };


	D3D12_DEPTH_STENCIL_DESC depthStencilDesc = {};
	depthStencilDesc.DepthEnable = TRUE;
	depthStencilDesc.DepthWriteMask = D3D12_DEPTH_WRITE_MASK_ALL;
	depthStencilDesc.DepthFunc = D3D12_COMPARISON_FUNC_LESS;
	depthStencilDesc.StencilEnable = FALSE;

	psoDesc.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;
	psoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_BACK;
	psoDesc.RasterizerState.FrontCounterClockwise = FALSE;
	psoDesc.RasterizerState.DepthBias = D3D12_DEFAULT_DEPTH_BIAS;
	psoDesc.RasterizerState.SlopeScaledDepthBias = D3D12_DEFAULT_SLOPE_SCALED_DEPTH_BIAS;
	psoDesc.RasterizerState.DepthClipEnable = TRUE;
	psoDesc.RasterizerState.MultisampleEnable = FALSE;
	psoDesc.RasterizerState.AntialiasedLineEnable = FALSE;
	psoDesc.RasterizerState.ForcedSampleCount = 0;
	psoDesc.RasterizerState.ConservativeRaster = D3D12_CONSERVATIVE_RASTERIZATION_MODE_OFF;

	psoDesc.BlendState.AlphaToCoverageEnable = FALSE;
	psoDesc.BlendState.RenderTarget[0].BlendEnable = FALSE;
	psoDesc.BlendState.RenderTarget[0].SrcBlend = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[0].DestBlend = D3D12_BLEND_ZERO;
	psoDesc.BlendState.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_ONE;
	psoDesc.BlendState.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ZERO;
	psoDesc.BlendState.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
	psoDesc.BlendState.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	psoDesc.DepthStencilState = depthStencilDesc;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;

	psoDesc.SampleMask = UINT_MAX;
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.SampleDesc.Count = 1;

	if (FAILED(pDevice->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&m_pPipelineState))))
	{
		Utils::DebugError("Error failed to create pso !");
		return;
	}
}

void RenderResources::CreateCommandList(ID3D12Device* pDevice, ID3D12CommandAllocator* pcmdAllocator, ID3D12PipelineState* pPso)
{
	if (SUCCEEDED(pDevice->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, pcmdAllocator, pPso, IID_PPV_ARGS(&m_pCommandList))))
	{
		Utils::DebugLog("CommandList successfuly created !");
		m_pCommandList->Close();
		return;
	}
}