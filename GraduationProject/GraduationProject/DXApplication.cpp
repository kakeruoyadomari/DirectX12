#include "DXApplication.h"

DXApplication::DXApplication(unsigned int width, unsigned int height, std::wstring title)
	: title_(title)
	, windowWidth_(width)
	, windowHeight_(height)
	, viewport_(0.0f, 0.0f, static_cast<float>(windowWidth_), static_cast<float>(windowHeight_))
	, scissorrect_(0, 0, static_cast<LONG>(windowWidth_), static_cast<LONG>(windowHeight_))
	, vertexBufferView_({})
	, indexBufferView_({})
	, fenceValue_(0)
	, fenceEvent_(nullptr)
{
}

// 初期化処理
void DXApplication::OnInit(HWND hwnd)
{
	LoadPipeline(hwnd);
	LoadAssets();
}

void DXApplication::LoadPipeline(HWND hwnd)
{
	UINT dxgiFactoryFlags = 0;

#ifdef _DEBUG
	{
		// デバッグレイヤーを有効にする
		ComPtr<ID3D12Debug> debugLayer;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer)))) {
			debugLayer->EnableDebugLayer();
			dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
		}
	}
#endif

	// DXGIFactoryの初期化
	ComPtr<IDXGIFactory6> dxgiFactory;
	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory)));

	// デバイスの初期化
	CreateD3D12Device(dxgiFactory.Get(), device_.ReleaseAndGetAddressOf());

	// コマンド関連の初期化
	{
		// コマンドキュー
		D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};
		commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE; // タイムアウト無し
		commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT; // コマンドリストと合わせる
		ThrowIfFailed(device_->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(commandQueue_.ReleaseAndGetAddressOf())));
		// コマンドアロケータ
		ThrowIfFailed(device_->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(commandAllocator_.ReleaseAndGetAddressOf())));
		// コマンドリスト
		ThrowIfFailed(device_->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator_.Get(), nullptr, IID_PPV_ARGS(commandList_.ReleaseAndGetAddressOf())));
		ThrowIfFailed(commandList_->Close());
	}

	// スワップチェーンの初期化
	{
		DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
		swapchainDesc.BufferCount = kFrameCount;
		swapchainDesc.Width = windowWidth_;
		swapchainDesc.Height = windowHeight_;
		swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		swapchainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
		swapchainDesc.SampleDesc.Count = 1;
		ThrowIfFailed(dxgiFactory->CreateSwapChainForHwnd(
			commandQueue_.Get(),
			hwnd,
			&swapchainDesc,
			nullptr,
			nullptr,
			(IDXGISwapChain1**)swapchain_.ReleaseAndGetAddressOf()));
	}

	// ディスクリプタヒープの初期化
	{
		D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
		rtvHeapDesc.NumDescriptors = kFrameCount;            //表裏の２つ
		rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;   //レンダーターゲットビュー
		rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE; //指定なし
		ThrowIfFailed(device_->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(rtvHeaps_.ReleaseAndGetAddressOf())));
	}

	// スワップチェーンと関連付けてレンダーターゲットビューを生成
	{
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeaps_->GetCPUDescriptorHandleForHeapStart());
		for (UINT i = 0; i < kFrameCount; ++i)
		{
			ThrowIfFailed(swapchain_->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(renderTargets_[i].ReleaseAndGetAddressOf())));
			device_->CreateRenderTargetView(renderTargets_[i].Get(), nullptr, rtvHandle);
			rtvHandle.Offset(1, device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
		}
	}

	// フェンスの生成
	{
		ThrowIfFailed(device_->CreateFence(fenceValue_, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(fence_.ReleaseAndGetAddressOf())));
		fenceEvent_ = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	}
}

void DXApplication::LoadAssets()
{
	// ルートシグネチャの生成
	{
		D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};
		rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
		ComPtr<ID3DBlob> rootSignatureBlob = nullptr;
		ComPtr<ID3DBlob> errorBlob = nullptr;
		ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1, &rootSignatureBlob, &errorBlob));
		ThrowIfFailed(device_->CreateRootSignature(0, rootSignatureBlob->GetBufferPointer(), rootSignatureBlob->GetBufferSize(), IID_PPV_ARGS(rootsignature_.ReleaseAndGetAddressOf())));
	}

	// パイプラインステートの生成
	{
		// シェーダーオブジェクトの生成
#if defined(_DEBUG)
		UINT compileFlags = D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION;
#else
		UINT compileFlags = 0;
#endif
		ComPtr<ID3DBlob> vsBlob;
		ComPtr<ID3DBlob> psBlob;
		D3DCompileFromFile(L"BasicVertexShader.hlsl", nullptr, nullptr, "BasicVS", "vs_5_0", compileFlags, 0, &vsBlob, nullptr);
		D3DCompileFromFile(L"BasicPixelShader.hlsl", nullptr, nullptr, "BasicPS", "ps_5_0", compileFlags, 0, &psBlob, nullptr);

		// 頂点レイアウトの生成
		D3D12_INPUT_ELEMENT_DESC inputElementDescs[] =
		{
			{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, D3D12_APPEND_ALIGNED_ELEMENT, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		};

		// パイプラインステートオブジェクト(PSO)を生成
		D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
		psoDesc.pRootSignature = rootsignature_.Get();
		psoDesc.InputLayout = { inputElementDescs, _countof(inputElementDescs) }; // 入力レイアウトの設定
		psoDesc.VS = CD3DX12_SHADER_BYTECODE(vsBlob.Get());                       // 頂点シェーダ
		psoDesc.PS = CD3DX12_SHADER_BYTECODE(psBlob.Get());                       // ピクセルシェーダ
		psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);         // ラスタライザーステート
		psoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);                   // ブレンドステート
		psoDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;                           // サンプルマスクの設定
		psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;   // トポロジタイプ
		psoDesc.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;    // ストリップ時のカット設定
		psoDesc.NumRenderTargets = 1;                                             // レンダーターゲット数
		psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;                       // レンダーターゲットフォーマット
		psoDesc.SampleDesc.Count = 1;                                             // マルチサンプリングの設定
		ThrowIfFailed(device_->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(pipelinestate_.ReleaseAndGetAddressOf())));
	}

	// 頂点バッファビューの生成
	{
		// 頂点定義
		DirectX::XMFLOAT3 vertices[] = {
			{-0.4f,-0.7f, 0.0f} , //左下
			{-0.4f, 0.7f, 0.0f} , //左上
			{ 0.4f,-0.7f, 0.0f} , //右下
			{ 0.4f, 0.7f, 0.0f} , //右上
		};
		const UINT vertexBufferSize = sizeof(vertices);
		auto vertexHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto vertexResDesc = CD3DX12_RESOURCE_DESC::Buffer(vertexBufferSize);
		// 頂点バッファーの生成
		ThrowIfFailed(device_->CreateCommittedResource(
			&vertexHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&vertexResDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(vertexBuffer_.ReleaseAndGetAddressOf())));
		// 頂点情報のコピー
		DirectX::XMFLOAT3* vertexMap = nullptr;
		ThrowIfFailed(vertexBuffer_->Map(0, nullptr, (void**)&vertexMap));
		std::copy(std::begin(vertices), std::end(vertices), vertexMap);
		vertexBuffer_->Unmap(0, nullptr);
		// 頂点バッファービューの生成
		vertexBufferView_.BufferLocation = vertexBuffer_->GetGPUVirtualAddress();
		vertexBufferView_.SizeInBytes = vertexBufferSize;
		vertexBufferView_.StrideInBytes = sizeof(vertices[0]);
	}


	// インデックスバッファビューの生成
	{
		// インデックス定義
		unsigned short indices[] = {
			0, 1, 2,
			2, 1, 3
		};
		const UINT indexBufferSize = sizeof(indices);
		auto indexHeapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
		auto indexResDesc = CD3DX12_RESOURCE_DESC::Buffer(indexBufferSize);
		// インデックスバッファの生成
		ThrowIfFailed(device_->CreateCommittedResource(
			&indexHeapProp,
			D3D12_HEAP_FLAG_NONE,
			&indexResDesc,
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(indexBuffer_.ReleaseAndGetAddressOf())));
		// インデックス情報のコピー
		unsigned short* indexMap = nullptr;
		indexBuffer_->Map(0, nullptr, (void**)&indexMap);
		std::copy(std::begin(indices), std::end(indices), indexMap);
		indexBuffer_->Unmap(0, nullptr);
		// インデックスバッファビューの生成
		indexBufferView_.BufferLocation = indexBuffer_->GetGPUVirtualAddress();
		indexBufferView_.SizeInBytes = indexBufferSize;
		indexBufferView_.Format = DXGI_FORMAT_R16_UINT;
	}
}

// 更新処理
void DXApplication::OnUpdate()
{
}

// 描画処理
void DXApplication::OnRender()
{
	// コマンドリストのリセット
	{
		ThrowIfFailed(commandAllocator_->Reset());
		ThrowIfFailed(commandList_->Reset(commandAllocator_.Get(), pipelinestate_.Get()));
	}

	// コマンドリストの生成
	{
		// バックバッファのインデックスを取得
		auto frameIndex = swapchain_->GetCurrentBackBufferIndex();

		// リソースバリアの設定 (PRESENT -> RENDER_TARGET)
		auto startResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets_[frameIndex].Get(), D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
		commandList_->ResourceBarrier(1, &startResourceBarrier);

		// パイプラインステートと必要なオブジェクトを設定
		commandList_->SetPipelineState(pipelinestate_.Get());         // パイプラインステート
		commandList_->SetGraphicsRootSignature(rootsignature_.Get()); // ルートシグネチャ
		commandList_->RSSetViewports(1, &viewport_);                  // ビューポート
		commandList_->RSSetScissorRects(1, &scissorrect_);            // シザー短形

		// レンダーターゲットの設定
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHandle(rtvHeaps_->GetCPUDescriptorHandleForHeapStart(), frameIndex, device_->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
		commandList_->OMSetRenderTargets(1, &rtvHandle, true, nullptr);

		// 画面クリア
		float clearColor[] = { 1.0f, 1.0f, 0.0f, 1.0f };  // 黄色
		commandList_->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);

		// 描画処理の設定
		commandList_->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST); // プリミティブトポロジの設定 (三角ポリゴン)
		commandList_->IASetVertexBuffers(0, 1, &vertexBufferView_);                // 頂点バッファ
		commandList_->IASetIndexBuffer(&indexBufferView_);                         // インデックスバッファ
		commandList_->DrawIndexedInstanced(6, 1, 0, 0, 0);                         // 描画

		// リソースバリアの設定 (RENDER_TARGET -> PRESENT)
		auto endResourceBarrier = CD3DX12_RESOURCE_BARRIER::Transition(renderTargets_[frameIndex].Get(), D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
		commandList_->ResourceBarrier(1, &endResourceBarrier);

		// 命令のクローズ
		commandList_->Close();
	}

	// コマンドリストの実行
	{
		ID3D12CommandList* commandLists[] = { commandList_.Get() };
		commandQueue_->ExecuteCommandLists(1, commandLists);
		// 画面のスワップ
		ThrowIfFailed(swapchain_->Present(1, 0));
	}

	// GPU処理の終了を待機
	{
		ThrowIfFailed(commandQueue_->Signal(fence_.Get(), ++fenceValue_));
		if (fence_->GetCompletedValue() < fenceValue_) {
			ThrowIfFailed(fence_->SetEventOnCompletion(fenceValue_, fenceEvent_));
			WaitForSingleObject(fenceEvent_, INFINITE);
		}
	}
}

// 終了処理
void DXApplication::OnDestroy()
{
	CloseHandle(fenceEvent_);
}

// D3D12Deviceの生成
void DXApplication::CreateD3D12Device(IDXGIFactory6* dxgiFactory, ID3D12Device** d3d12device)
{
	ID3D12Device* tmpDevice = nullptr;

	// グラフィックスボードの選択
	std::vector <IDXGIAdapter*> adapters;
	IDXGIAdapter* tmpAdapter = nullptr;
	for (int i = 0; SUCCEEDED(dxgiFactory->EnumAdapters(i, &tmpAdapter)); ++i)
	{
		adapters.push_back(tmpAdapter);
	}
	for (auto adapter : adapters)
	{
		DXGI_ADAPTER_DESC adapterDesc;
		adapter->GetDesc(&adapterDesc);
		// AMDを含むアダプターオブジェクトを探して格納（見つからなければnullptrでデフォルト）
		// 製品版の場合は、オプション画面から選択させて設定する必要がある
		std::wstring strAdapter = adapterDesc.Description;
		if (strAdapter.find(L"AMD") != std::string::npos)
		{
			tmpAdapter = adapter;
			break;
		}
	}

	// Direct3Dデバイスの初期化
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};
	for (auto level : levels) {
		// 生成可能なバージョンが見つかったらループを打ち切り
		if (SUCCEEDED(D3D12CreateDevice(tmpAdapter, level, IID_PPV_ARGS(&tmpDevice)))) {
			break;
		}
	}
	*d3d12device = tmpDevice;
}

void DXApplication::ThrowIfFailed(HRESULT hr)
{
	if (FAILED(hr))
	{
		// hrのエラー内容をthrowする
		char s_str[64] = {};
		sprintf_s(s_str, "HRESULT of 0x%08X", static_cast<UINT>(hr));
		std::string errMessage = std::string(s_str);
		throw std::runtime_error(errMessage);
	}
}
