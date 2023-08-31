#include <Windows.h>
#include <tchar.h>
#include<d3d12.h>
#include<dxgi1_6.h>
#include<DirectXMath.h>
#include <vector>

#include<d3dcompiler.h>
#include<DirectXTex.h>
#include<d3dx12.h>

#ifdef _DEBUG
#include <iostream>
#endif // _DEBUG
#include <string>


#pragma comment(lib,"DirectXTex.lib")
#pragma comment(lib,"d3d12.lib")
#pragma comment(lib,"dxgi.lib")
#pragma comment(lib,"d3dcompiler.lib")

using namespace std;
using namespace DirectX;


struct Vertex
{
	XMFLOAT3 pos;//XYZ座標
	XMFLOAT2 uv;//UV座標
};


////ノイズテクスチャの作成
//struct TexRGBA 
//{
//	unsigned char R, G, B, A;
//};

//@brief コンソール画面にフォーマット付き文字列を表示
//@param format フォーマット(%dや%f等の)
//@param 可変長引数
//@remarks デバッグ用
void DebugOutputFormatString(const char* format, ...)
{
#ifdef _DEBUG
	va_list valist;
	va_start(valist, format);
	printf(format, valist);
	va_end(valist);
#endif // _DEBUG
}


LRESULT WindowProcedure(HWND hwnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	//ウィンドウが破棄されたら呼ばれる
	if (msg == WM_DESTROY)
	{
		PostQuitMessage(0); //アプリの終了を伝える
		return 0;
	}
	return DefWindowProc(hwnd, msg, wparam, lparam);	//既定の処理を行う
}

const unsigned int window_width = 1280;
const unsigned int window_height = 720;

ID3D12Device* _dev = nullptr;
IDXGIFactory6* _dxgiFactory = nullptr;
IDXGISwapChain4* _swapchain = nullptr;
ID3D12CommandAllocator* _cmdAllocator = nullptr;
ID3D12GraphicsCommandList* _cmdList = nullptr;
ID3D12CommandQueue* _cmdQueue = nullptr;


void EnableDebugLayer() 
{
	ID3D12Debug* debugLayer = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugLayer)))) 
	{
		debugLayer->EnableDebugLayer();		//デバッグレイヤーを有効化
		debugLayer->Release();				//有効化したらインターフェイスを開放する
	}
}

#ifdef _DEBUG
int main()
{
#else
int WINAPI WinmMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#endif // _DEBUG
	DebugOutputFormatString("Show window test.");
	
	WNDCLASSEX w = {};
	w.cbSize = sizeof(WNDCLASSEX);
	w.lpfnWndProc = (WNDPROC)WindowProcedure;	//コールバック関数の指定
	w.lpszClassName = _T("DX12Sample");			//アプリケーションハンドル名
	w.hInstance = GetModuleHandle(nullptr);		//ハンドルの取得

	RegisterClassEx(&w);						//アプリケーションクラス

	RECT wrc = { 0, 0, window_width, window_height };	//ウィンドウサイズ

	//関数を使ってウィンドウサイズを補正する
	AdjustWindowRect(&wrc, WS_OVERLAPPEDWINDOW, false);

	//ウィンドウオブジェクトの生成
	HWND hwnd = CreateWindow(
		w.lpszClassName,		//クラス名指定
		_T("DX12テスト"),		//タイトルバーの文字
		WS_OVERLAPPEDWINDOW,	//タイトルバーと境界線のあるウィンドウ
		CW_USEDEFAULT,			//表示X座標はOSに任せる
		CW_USEDEFAULT,			//表示Y座標はOSに任せる
		wrc.right - wrc.left,	//ウィンドウ幅
		wrc.bottom - wrc.top,	//ウィンドウ高
		nullptr,				//親ウィンドウハンドル
		nullptr,				//メニューハンドル
		w.hInstance,			//呼び出しアプリケーションハンドル
		nullptr);				//追加パラメーター


#ifdef _DEBUG
	//デバッグレイヤーをオンに
	EnableDebugLayer();
#endif

	//DirectX12まわり初期化


	HRESULT result = S_OK;
	if (FAILED(CreateDXGIFactory2(DXGI_CREATE_FACTORY_DEBUG, IID_PPV_ARGS(&_dxgiFactory)))) 
	{
		if (FAILED(CreateDXGIFactory2(0, IID_PPV_ARGS(&_dxgiFactory)))) 
		{
			return -1;
		}
	}

	//アダプター列挙用
	std::vector <IDXGIAdapter*> adapters;
	//特定の名前を持つアダプターオブジェクトが入る
	IDXGIAdapter* tmpAdapter = nullptr;
	for (int i = 0; _dxgiFactory->EnumAdapters(i, &tmpAdapter) != DXGI_ERROR_NOT_FOUND; ++i)
	{
		adapters.push_back(tmpAdapter);
	}

	for (auto adpt : adapters) 
	{
		DXGI_ADAPTER_DESC adesc = {};
		adpt->GetDesc(&adesc);
		std::wstring strDesc = adesc.Description;
		if (strDesc.find(L"NVIDIA") != std::string::npos) 
		{
			tmpAdapter = adpt;
			break;
		}
	}

	//フィーチャレベル列挙
	D3D_FEATURE_LEVEL levels[] = {
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0,
		D3D_FEATURE_LEVEL_11_1,
		D3D_FEATURE_LEVEL_11_0,
	};

	//Direct3Dデバイスの初期化
	D3D_FEATURE_LEVEL featureLevel;
	for (auto l : levels) {
		if (D3D12CreateDevice(tmpAdapter, l, IID_PPV_ARGS(&_dev)) == S_OK) 
		{
			featureLevel = l;
			break;
		}
	}

	result = _dev->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&_cmdAllocator));
	result = _dev->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, _cmdAllocator, nullptr, IID_PPV_ARGS(&_cmdList));

	//_cmdList->Close();
	D3D12_COMMAND_QUEUE_DESC cmdQueueDesc = {};
	cmdQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;//タイムアウトなし
	cmdQueueDesc.NodeMask = 0;
	cmdQueueDesc.Priority = D3D12_COMMAND_QUEUE_PRIORITY_NORMAL;//プライオリティ特に指定なし
	cmdQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;//コマンドリストと合わせる
	result = _dev->CreateCommandQueue(&cmdQueueDesc, IID_PPV_ARGS(&_cmdQueue));//コマンドキュー生成


	//スワップチェーン生成
	DXGI_SWAP_CHAIN_DESC1 swapchainDesc = {};
	swapchainDesc.Width = window_width;
	swapchainDesc.Height = window_height;
	swapchainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapchainDesc.Stereo = false;
	swapchainDesc.SampleDesc.Count = 1;
	swapchainDesc.SampleDesc.Quality = 0;
	swapchainDesc.BufferUsage = DXGI_USAGE_BACK_BUFFER;
	swapchainDesc.BufferCount = 2;
	//バックバッファーは伸び縮み可
	swapchainDesc.Scaling = DXGI_SCALING_STRETCH;
	//フィリップ後に破棄
	swapchainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	//指定なし
	swapchainDesc.AlphaMode = DXGI_ALPHA_MODE_UNSPECIFIED;
	//ウィンドウ<=>フルスクリーン切り替え可
	swapchainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;


	result = _dxgiFactory->CreateSwapChainForHwnd(
		_cmdQueue,
		hwnd,
		&swapchainDesc,
		nullptr,
		nullptr,
		(IDXGISwapChain1**)&_swapchain);
	//


	//ディスクリプタヒープ
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc = {};
	heapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;//レンダーターゲットビューなので当然RTV
	heapDesc.NodeMask = 0;
	heapDesc.NumDescriptors = 2;//表裏の２つ
	heapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;//特に指定なし
	ID3D12DescriptorHeap* rtvHeaps = nullptr;
	result = _dev->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&rtvHeaps));
	//

	//ディスクリプタとスワップチェーン紐づけ
	DXGI_SWAP_CHAIN_DESC swcDesc = {};
	result = _swapchain->GetDesc(&swcDesc);
	std::vector<ID3D12Resource*> _backBuffers(swcDesc.BufferCount);


	//SRGBレンダーターゲットビュー設定
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};

	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;	//ガンマ補正あり
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;

	D3D12_CPU_DESCRIPTOR_HANDLE handle = rtvHeaps->GetCPUDescriptorHandleForHeapStart();	//ディスクリプタ先頭のアドレスを得る
	for (size_t i = 0; i < swcDesc.BufferCount; ++i) 
	{
		result = _swapchain->GetBuffer(static_cast<UINT>(i), IID_PPV_ARGS(&_backBuffers[i]));

		//レンダーターゲットビューを生成
		_dev->CreateRenderTargetView(_backBuffers[i], &rtvDesc, handle);

		//ポインターをずらす
		handle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	}
	//


	//フェンス作成
	ID3D12Fence* _fence = nullptr;
	UINT64 _fenceVal = 0;
	result = _dev->CreateFence(_fenceVal, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&_fence));
	//


	//ウィンドウ表示
	ShowWindow(hwnd, SW_SHOW);

	//PMDヘッダ構造体
	struct PMDHeader
	{
		float version;
		char model_name[20];
		char comment[256];
	};

	char signature[3] = {};	//シグネチャ
	PMDHeader pmdheader = {};
	FILE* fp;
	auto err = fopen_s(&fp, "Model/初音ミク.pmd", "rb");
	if (fp == nullptr)
	{

		return -1;
	}
	fread(signature, sizeof(signature), 1, fp);
	fread(&pmdheader, sizeof(pmdheader), 1, fp);

	unsigned int vertNum;//頂点数
	fread(&vertNum, sizeof(vertNum), 1, fp);
	constexpr size_t pmdvertex_size = 38;//頂点1つあたりのサイズ

	std::vector<unsigned char> vertices(vertNum * pmdvertex_size);//バッファ確保
	fread(vertices.data(), vertices.size(), 1, fp);

	auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(vertices.size());

	ID3D12Resource* vertBuff = nullptr;
	result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&vertBuff));
	
	//頂点情報のコピー
	unsigned char* vertMap = nullptr;

	result = vertBuff->Map(0, nullptr, (void**)&vertMap);
	std::copy(std::begin(vertices), std::end(vertices), vertMap);
	vertBuff->Unmap(0, nullptr);

	D3D12_VERTEX_BUFFER_VIEW vbView = {};
	vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();//バッファの仮想アドレス
	vbView.SizeInBytes = vertices.size();		//全バイト数
	vbView.StrideInBytes = pmdvertex_size;		//１頂点のバイト数

	fclose(fp);


	//Vertex vertices[] =
	//{
	//	{{   -1.0f, -1.0f, 0.0f}, {0.0f, 1.0f}},	//左下
	//	{{   -1.0f,  1.0f, 0.0f}, {0.0f, 0.0f}},	//左上
	//	{{	  1.0f, -1.0f, 0.0f}, {1.0f, 1.0f}},	//右下
	//	{{    1.0f,  1.0f, 0.0f}, {1.0f, 0.0f}},	//右上
	//};

	//頂点バッファーの生成
	//D3D12_HEAP_PROPERTIES heapprop = {};
	//heapprop.Type = D3D12_HEAP_TYPE_UPLOAD;
	//heapprop.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_UNKNOWN;
	//heapprop.MemoryPoolPreference = D3D12_MEMORY_POOL_UNKNOWN;


	//D3D12_RESOURCE_DESC resdesc = {};	
	//resdesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	//resdesc.Width = sizeof(vertices);
	//resdesc.Height = 1;
	//resdesc.DepthOrArraySize = 1;
	//resdesc.MipLevels = 1;
	//resdesc.Format = DXGI_FORMAT_UNKNOWN;
	//resdesc.SampleDesc.Count = 1;
	//resdesc.Flags = D3D12_RESOURCE_FLAG_NONE;
	//resdesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;


	////UPLOAD(確保は可能)
	//ID3D12Resource* vertBuff = nullptr;
	//result = _dev->CreateCommittedResource(
	//	&heapprop,
	//	D3D12_HEAP_FLAG_NONE,
	//	&resdesc,
	//	D3D12_RESOURCE_STATE_GENERIC_READ,
	//	nullptr,
	//	IID_PPV_ARGS(&vertBuff));
	//
	//D3DX12.hで省略可↓


	//ID3D12Resource* vertBuff = nullptr;

	//auto heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	//auto resDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(vertices));

	//result = _dev->CreateCommittedResource(
	//	&heapProp,
	//	D3D12_HEAP_FLAG_NONE,
	//	&resDesc,
	//	D3D12_RESOURCE_STATE_GENERIC_READ,
	//	nullptr,
	//	IID_PPV_ARGS(&vertBuff)
	//);		↑モデル用に移動

	////頂点情報のコピー
	//Vertex* vertMap = nullptr;
	//result = vertBuff->Map(0, nullptr, (void**)&vertMap);

	//std::copy(std::begin(vertices), std::end(vertices), vertMap);

	//vertBuff->Unmap(0, nullptr);
	////		↑モデル用に移動

	//D3D12_VERTEX_BUFFER_VIEW vbView = {};
	//vbView.BufferLocation = vertBuff->GetGPUVirtualAddress();//バッファの仮想アドレス
	//vbView.SizeInBytes = sizeof(vertices);//全バイト数
	//vbView.StrideInBytes = sizeof(vertices[0]);//1頂点あたりのバイト数
	//			↑モデル用に移動

	unsigned short indices[] = { 0,1,2, 2,1,3 };

	ID3D12Resource* idxBuff = nullptr;
	//設定は、バッファのサイズ以外頂点バッファの設定を使いまわして
	//OKだと思います。
	heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	resDesc = CD3DX12_RESOURCE_DESC::Buffer(sizeof(indices));

	result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&idxBuff));

	//作ったバッファにインデックスデータをコピー
	unsigned short* mappedIdx = nullptr;
	idxBuff->Map(0, nullptr, (void**)&mappedIdx);
	std::copy(std::begin(indices), std::end(indices), mappedIdx);
	idxBuff->Unmap(0, nullptr);

	//インデックスバッファビューを作成
	D3D12_INDEX_BUFFER_VIEW ibView = {};
	ibView.BufferLocation = idxBuff->GetGPUVirtualAddress();
	ibView.Format = DXGI_FORMAT_R16_UINT;
	ibView.SizeInBytes = sizeof(indices);


	//hlslファイル読込
	ID3DBlob* _vsBlob = nullptr;
	ID3DBlob* _psBlob = nullptr;

	ID3DBlob* errorBlob = nullptr;
	result = D3DCompileFromFile(L"BasicVertexShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicVS",
		"vs_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&_vsBlob,
		&errorBlob);

	//エラーチェック
	if (FAILED(result)) 
	{
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) 
		{
			::OutputDebugStringA("ファイルが見当たりません");
		}
		else 
		{
			std::string errstr;
			errstr.resize(errorBlob->GetBufferSize());
			std::copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errstr.begin());
			errstr += "\n";
			OutputDebugStringA(errstr.c_str());
		}
		exit(1);
	}


	result = D3DCompileFromFile(L"BasicPixelShader.hlsl",
		nullptr,
		D3D_COMPILE_STANDARD_FILE_INCLUDE,
		"BasicPS",
		"ps_5_0",
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION,
		0,
		&_psBlob,
		&errorBlob);
	//エラーチェック
	if (FAILED(result)) 
	{
		if (result == HRESULT_FROM_WIN32(ERROR_FILE_NOT_FOUND)) 
		{
			::OutputDebugStringA("ファイルが見当たりません");
		}
		else 
		{
			std::string errstr;
			errstr.resize(errorBlob->GetBufferSize());
			std::copy_n((char*)errorBlob->GetBufferPointer(), errorBlob->GetBufferSize(), errstr.begin());
			errstr += "\n";
			OutputDebugStringA(errstr.c_str());
		}
		exit(1);
	}
	//


	D3D12_INPUT_ELEMENT_DESC inputLayout[] = {
	{
		"POSITION",
		0,
		DXGI_FORMAT_R32G32B32_FLOAT,
		0,
		D3D12_APPEND_ALIGNED_ELEMENT,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		0
	},
	{
		"NORMAL",
		0,
		DXGI_FORMAT_R32G32B32_FLOAT,
		0,
		D3D12_APPEND_ALIGNED_ELEMENT,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		0
	},
	{
		"TEXCOORD",
		0,
		DXGI_FORMAT_R32G32_FLOAT,
		0,
		D3D12_APPEND_ALIGNED_ELEMENT,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		0
	},
	{
		"BONE_NO",
		0,
		DXGI_FORMAT_R16G16_UINT,
		0,
		D3D12_APPEND_ALIGNED_ELEMENT,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		0
	},
	{
		"WEIGHT",
		0,
		DXGI_FORMAT_R8_UINT,
		0,
		D3D12_APPEND_ALIGNED_ELEMENT,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		0
	},
	{
		"EDGE_FLG",
		0,
		DXGI_FORMAT_R8_UINT,
		0,
		D3D12_APPEND_ALIGNED_ELEMENT,
		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
		0
	},
	};

	////頂点レイアウト
	//D3D12_INPUT_ELEMENT_DESC inputLayout[] =
	//{
	//	//座標情報
	//	{ 
	//		"POSITION",
	//		0,
	//		DXGI_FORMAT_R32G32B32_FLOAT,
	//		0,
	//		D3D12_APPEND_ALIGNED_ELEMENT,
	//		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
	//		0
	//	},

	//	//uv情報
	//	{
	//		"TEXCOORD",
	//		0,
	//		DXGI_FORMAT_R32G32_FLOAT,
	//		0,
	//		D3D12_APPEND_ALIGNED_ELEMENT,
	//		D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,
	//		0
	//	}
	//};		↑モデル用に移動




	D3D12_GRAPHICS_PIPELINE_STATE_DESC gpipeline = {};

	gpipeline.pRootSignature = nullptr;		//あとで

	//頂点シェーダーとピクセルシェーダーのセット
	gpipeline.VS.pShaderBytecode = _vsBlob->GetBufferPointer();
	gpipeline.VS.BytecodeLength = _vsBlob->GetBufferSize();
	gpipeline.PS.pShaderBytecode = _psBlob->GetBufferPointer();
	gpipeline.PS.BytecodeLength = _psBlob->GetBufferSize();

	//デフォルトのサンプルマスクを表す定数
	gpipeline.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;

	gpipeline.BlendState.AlphaToCoverageEnable = false;
	gpipeline.BlendState.IndependentBlendEnable = false;

	///
	D3D12_RENDER_TARGET_BLEND_DESC renderTargetBlendDesc = {};

	//ひとまず加算や乗算やαブレンディングは使用しない
	renderTargetBlendDesc.BlendEnable = false;
	//ひとまず論理演算は使用しない
	renderTargetBlendDesc.LogicOpEnable = false;
	renderTargetBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	//レンダターゲットにセット
	gpipeline.BlendState.RenderTarget[0] = renderTargetBlendDesc;
	///

	//まだアンチエイリアスは使わない為false
	gpipeline.RasterizerState.MultisampleEnable = false;

	gpipeline.RasterizerState.CullMode = D3D12_CULL_MODE_NONE;	//カリングなし
	gpipeline.RasterizerState.FillMode = D3D12_FILL_MODE_SOLID;	//中身を塗りつぶす
	gpipeline.RasterizerState.DepthClipEnable = true;			//深度方向のクリッピングは有効に


	gpipeline.InputLayout.pInputElementDescs = inputLayout;//レイアウト先頭アドレス
	gpipeline.InputLayout.NumElements = _countof(inputLayout);//レイアウト配列数

	//トライアングルリスト
	gpipeline.IBStripCutValue = D3D12_INDEX_BUFFER_STRIP_CUT_VALUE_DISABLED;//ストリップ時のカットなし
	gpipeline.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;//三角形で構成

	//レンダターゲット設定
	gpipeline.NumRenderTargets = 1;//今は１つのみ
	gpipeline.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;//0～1に正規化されたRGBA

	//アンチエイリアシングのサメのサンプル数設定
	gpipeline.SampleDesc.Count = 1;//サンプリングは1ピクセルにつき１
	gpipeline.SampleDesc.Quality = 0;//クオリティは最低

	//ルートシグネチャ
	ID3D12RootSignature* rootsignature = nullptr;

	//ディスクリプタレンジ作成	テクスチャと定数用の2つ
	D3D12_DESCRIPTOR_RANGE descTblRange[2] = {};		//テクスチャと定数の２つ

	//テクスチャ用レジスター0番
	descTblRange[0].NumDescriptors = 1;		//テクスチャ１つ
	descTblRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV;	//種別はテクスチャ
	descTblRange[0].BaseShaderRegister = 0;	//0番スロットから
	descTblRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//定数用レジスター0番
	descTblRange[1].NumDescriptors = 1;		//テクスチャ１つ
	descTblRange[1].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_CBV;	//種別は定数
	descTblRange[1].BaseShaderRegister = 0;	//0番スロットから
	descTblRange[1].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND;

	//ルートパラメーター作成
	D3D12_ROOT_PARAMETER rootparam = {};
	rootparam.ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;
	rootparam.DescriptorTable.pDescriptorRanges = &descTblRange[0];//デスクリプタレンジのアドレス
	rootparam.DescriptorTable.NumDescriptorRanges = 2;//デスクリプタレンジ数
	rootparam.ShaderVisibility = D3D12_SHADER_VISIBILITY_ALL;//全てのシェーダから見える


	//サンプラーの設定
	D3D12_STATIC_SAMPLER_DESC samplerDesc = {};

	samplerDesc.AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP;		//横方向の繰り返し
	samplerDesc.AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP;		//縦方向の繰り返し
	samplerDesc.AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP;		//奥方向の繰り返し
	samplerDesc.BorderColor = D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;	//ボーダーは黒
	samplerDesc.Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR;		//線形補間
	samplerDesc.MaxLOD = D3D12_FLOAT32_MAX;						//ミップマップ最大値
	samplerDesc.MinLOD = 0.0f;									//ミップマップ最小値
	samplerDesc.ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL;	//ピクセルシェーダーから見える
	samplerDesc.ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER;	//リサンプリングしない


	//ルートシグネチャ作成
	D3D12_ROOT_SIGNATURE_DESC rootSignatureDesc = {};

	rootSignatureDesc.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
	rootSignatureDesc.pParameters = &rootparam;
	rootSignatureDesc.NumParameters = 1;

	rootSignatureDesc.pStaticSamplers = &samplerDesc;
	rootSignatureDesc.NumStaticSamplers = 1;


	ID3DBlob* rootSigBlob = nullptr;
	result = D3D12SerializeRootSignature(
		&rootSignatureDesc,
		D3D_ROOT_SIGNATURE_VERSION_1_0,
		&rootSigBlob,
		&errorBlob);

	result = _dev->CreateRootSignature(
		0,
		rootSigBlob->GetBufferPointer(),
		rootSigBlob->GetBufferSize(),
		IID_PPV_ARGS(&rootsignature));

	rootSigBlob->Release();

	//パイプラインに設定
	gpipeline.pRootSignature = rootsignature;

	//グラフィックスパイプラインステートオブジェクトの生成
	ID3D12PipelineState* _pipelinestate = nullptr;

	result = _dev->CreateGraphicsPipelineState(&gpipeline, IID_PPV_ARGS(&_pipelinestate));


	//ビューポート
	D3D12_VIEWPORT viewport = {};
	viewport.Width = window_width;//出力先の幅(ピクセル数)
	viewport.Height = window_height;//出力先の高さ(ピクセル数)
	viewport.TopLeftX = 0;//出力先の左上座標X
	viewport.TopLeftY = 0;//出力先の左上座標Y
	viewport.MaxDepth = 1.0f;//深度最大値
	viewport.MinDepth = 0.0f;//深度最小値

	//シザー矩形
	D3D12_RECT scissorrect = {};
	scissorrect.top = 0;//切り抜き上座標
	scissorrect.left = 0;//切り抜き左座標
	scissorrect.right = scissorrect.left + window_width;//切り抜き右座標
	scissorrect.bottom = scissorrect.top + window_height;//切り抜き下座標


	//テクスチャのロード
	TexMetadata metadata = {};
	ScratchImage scratchImg = {};

	result = LoadFromWICFile(
		L"img/textest.png",
		WIC_FLAGS_NONE,
		&metadata,
		scratchImg);

	auto img = scratchImg.GetImage(0, 0, 0);	//生データ抽出


	//ノイズテクスチャ
	//vector<TexRGBA> texturedata(256 * 256);
	//for (auto& rgba : texturedata) 
	//{
	//	rgba.R = rand() % 256;
	//	rgba.G = rand() % 256;
	//	rgba.B = rand() % 256;
	//	rgba.A = 255;//アルファは1.0という事にします。
	//}

	//WriteToSubresourceで転送する用のヒープ設定
	D3D12_HEAP_PROPERTIES texHeapProp = {};

	texHeapProp.Type = D3D12_HEAP_TYPE_CUSTOM;		//DEFALLTでもUPLOADでもない
	//ライトバック
	texHeapProp.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK;
	//転送はL0,つまりCPU側から行う
	texHeapProp.MemoryPoolPreference = D3D12_MEMORY_POOL_L0;
	//単一アダプターのため0
	texHeapProp.CreationNodeMask = 0;
	texHeapProp.VisibleNodeMask = 0;


	resDesc = {};

	resDesc.Format = metadata.format;	//RGBAフォーマット		※srvDescのformatも同じもの
	resDesc.Width = metadata.width;		//幅
	resDesc.Height = metadata.height;	//高さ
	resDesc.DepthOrArraySize = metadata.arraySize;
	resDesc.SampleDesc.Count = 1;	//アンチエイリアシングしない
	resDesc.SampleDesc.Quality = 0;	//クオリティは最低
	resDesc.MipLevels = metadata.mipLevels;
	resDesc.Dimension = static_cast<D3D12_RESOURCE_DIMENSION>(metadata.dimension);	//2Dテクスチャ用
	resDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;	//レイアウトは決定しない
	resDesc.Flags = D3D12_RESOURCE_FLAG_NONE;		//特にフラグ無し


	//リソース生成
	ID3D12Resource* texbuff = nullptr;

	result = _dev->CreateCommittedResource(
		&texHeapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,		//テクスチャ用指定
		nullptr,
		IID_PPV_ARGS(&texbuff));

	//テクスチャバッファーの転送
	result = texbuff->WriteToSubresource(
		0,
		nullptr,			//全領域へコピー
		img->pixels,		//元データアドレス
		img->rowPitch,		//1ラインサイズ
		img->slicePitch		//1枚サイズ
	);

	auto worldMat = XMMatrixRotationY(XM_PIDIV4);
	XMFLOAT3 eye(0, 10, -15);
	XMFLOAT3 target(0, 10, 0);
	XMFLOAT3 up(0, 1, 0);

	auto viewMat = XMMatrixLookAtLH(
		XMLoadFloat3(&eye),
		XMLoadFloat3(&target),
		XMLoadFloat3(&up));

	auto projMat = XMMatrixPerspectiveFovLH(
		XM_PIDIV2,		//画角90°
		static_cast<float>(window_width) / static_cast<float>(window_height),		//アスペクト比
		1.0f,		//視錐台の近い面
		100.0f);		//		　遠い面

	//XMMATRIX matrix = XMMatrixIdentity();

	//matrix.r[0].m128_f32[0] = 2.0f / window_width;
	//matrix.r[1].m128_f32[1] = -2.0f / window_height;

	//matrix.r[3].m128_f32[0] =  -1.0f;
	//matrix.r[3].m128_f32[1] =  1.0f;

	//	1/640,		 0,		 0,		-1,
	//	    0,	1/-360,		 0,		 1,
	//	    0,		 0,		 1,		 0,
	//	    0,		 0,		 0,		 1,


	ID3D12Resource* constBuff = nullptr;
	heapProp = CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD);
	resDesc = CD3DX12_RESOURCE_DESC::Buffer((sizeof(XMMATRIX) + 0xff) & ~0xff);
	
	result = _dev->CreateCommittedResource(
		&heapProp,
		D3D12_HEAP_FLAG_NONE,
		&resDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&constBuff)
	);

	XMMATRIX* mapMatrix;//マップ先を示すポインタ
	result = constBuff->Map(0, nullptr, (void**)&mapMatrix);//マップ
	*mapMatrix = worldMat * viewMat * projMat;

	//シェーダーリソースビュー用のディスクリプタヒープ作成
	ID3D12DescriptorHeap* basicDescHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC descHeapDesc = {};

	descHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;//シェーダから見えるように
	descHeapDesc.NodeMask = 0;//マスクは0
	descHeapDesc.NumDescriptors = 2;//SRVとCBV
	descHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;//シェーダリソースビュー(および定数、UAVも)
	result = _dev->CreateDescriptorHeap(
		&descHeapDesc,
		IID_PPV_ARGS(&basicDescHeap));//生成


	//シェーダーリソースビュー作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};

	srvDesc.Format = metadata.format;	//rgba(0.0f～1.0fに正規化)
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;	//2Dテクスチャ
	srvDesc.Texture2D.MipLevels = 1;		//ミップマップは使用しないので１


	auto basicHeapHandle = basicDescHeap->GetCPUDescriptorHandleForHeapStart();

	//シェーダリソースビューの作成
	_dev->CreateShaderResourceView(
		texbuff, //ビューと関連付けるバッファ
		&srvDesc, //先ほど設定したテクスチャ設定情報
		basicHeapHandle
	);
	basicHeapHandle.ptr += _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


	D3D12_CONSTANT_BUFFER_VIEW_DESC cbvDesc = {};

	cbvDesc.BufferLocation = constBuff->GetGPUVirtualAddress();
	cbvDesc.SizeInBytes = static_cast<UINT>(constBuff->GetDesc().Width);
	//定数バッファビューの作成
	_dev->CreateConstantBufferView(&cbvDesc, basicHeapHandle);


	MSG msg = {};
	unsigned int frame = 0;
	float angle = 0.0f;
	while (true)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}

		//アプリケーションが終わるときにmessageがWM_QUITになる
		if (msg.message == WM_QUIT)
		{
			break;
		}


		angle += 0.1f;
		worldMat = XMMatrixRotationY(angle);
		*mapMatrix = worldMat * viewMat * projMat;

		//DirectX処理
		//バックバッファのインデックスを取得
		auto bbIdx = _swapchain->GetCurrentBackBufferIndex();

		//リソースバリア
		//D3D12_RESOURCE_BARRIER BarrierDesc = {};
		//BarrierDesc.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
		//BarrierDesc.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
		//BarrierDesc.Transition.pResource = _backBuffers[bbIdx];
		//BarrierDesc.Transition.Subresource = D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;
		//BarrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
		//BarrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;

		//d3dx12.h でリソースバリアを以下に省略可↓

		auto barrierDesc = CD3DX12_RESOURCE_BARRIER::Transition(
			_backBuffers[bbIdx],
			D3D12_RESOURCE_STATE_PRESENT,
			D3D12_RESOURCE_STATE_RENDER_TARGET);

		_cmdList->ResourceBarrier(1, &barrierDesc);		//バリア実行

		//パイプラインステート実行
		_cmdList->SetPipelineState(_pipelinestate);

		//レンダーターゲットを指定
		auto rtvH = rtvHeaps->GetCPUDescriptorHandleForHeapStart();
		rtvH.ptr += static_cast<ULONG_PTR>(bbIdx * _dev->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV));
		_cmdList->OMSetRenderTargets(1, &rtvH, false, nullptr);

		//画面クリア
		//float r, g, b;
		//r = (float)(0xff & frame >> 16) / 255.0f;
		//g = (float)(0xff & frame >> 8) / 255.0f;
		//b = (float)(0xff & frame >> 0) / 255.0f;
		//float clearColor[] = { r,g,b,1.0f };//グラデーション

		float clearColor[] = { 1.0f,1.0f,1.0f,1.0f };//白色

		_cmdList->ClearRenderTargetView(rtvH, clearColor, 0, nullptr);
		++frame;
		_cmdList->RSSetViewports(1, &viewport);
		_cmdList->RSSetScissorRects(1, &scissorrect);

		//ルートシグネチャ実行
		_cmdList->SetGraphicsRootSignature(rootsignature);

		//プリミティブ設定
		_cmdList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_POINTLIST);
		//頂点設定
		_cmdList->IASetVertexBuffers(0, 1, &vbView);
		_cmdList->IASetIndexBuffer(&ibView);


		_cmdList->SetGraphicsRootSignature(rootsignature);
		_cmdList->SetDescriptorHeaps(1, &basicDescHeap);

		_cmdList->SetGraphicsRootDescriptorTable(
			0,		//ルートパラメーターインデックス
			basicDescHeap->GetGPUDescriptorHandleForHeapStart());		//ヒープアドレス

		//描画命令
		_cmdList->DrawInstanced(vertNum, 1, 0, 0);
		_cmdList->DrawIndexedInstanced(6, 1, 0, 0, 0);

		//barrierDesc.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
		//barrierDesc.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
		//d3dx12で↓
		barrierDesc = CD3DX12_RESOURCE_BARRIER::Transition(
			_backBuffers[bbIdx],
			D3D12_RESOURCE_STATE_RENDER_TARGET,
			D3D12_RESOURCE_STATE_PRESENT);

		_cmdList->ResourceBarrier(1, &barrierDesc);

		//命令のクローズ
		_cmdList->Close();

		//コマンドリストの実行
		{
			ID3D12CommandList* cmdlists[] = { _cmdList };
			_cmdQueue->ExecuteCommandLists(1, cmdlists);

			////待ち
			_cmdQueue->Signal(_fence, ++_fenceVal);

			if (_fence->GetCompletedValue() != _fenceVal) 
			{
				//イベントハンドルの取得
				auto event = CreateEvent(nullptr, false, false, nullptr);
				_fence->SetEventOnCompletion(_fenceVal, event);
				//イベントが発生するまで待ち続ける
				WaitForSingleObject(event, INFINITE);
				//イベントハンドルを閉じる
				CloseHandle(event);
			}

			_cmdAllocator->Reset();//キューをクリア
			_cmdList->Reset(_cmdAllocator, nullptr);//再びコマンドリストをためる準備
		}


		//画面のフリップ
		_swapchain->Present(1, 0);
	}
	//クラス登録解除
	UnregisterClass(w.lpszClassName, w.hInstance);

	return 0;

}

