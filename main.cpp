#include<Windows.h>
#include<cstdint>
#include<string>
#include<format>
#include <cassert>
#include <d3d12.h>
#include <dxgi1_6.h>
#include<dxcapi.h>
#include <vector>
#include <dxgidebug.h>
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxcompiler.lib")
#pragma comment(lib, "dxguid.lib")

#include"externals/imgui/imgui.h"
#include"externals/imgui/imgui_impl_win32.h"
#include"externals/imgui/imgui_impl_dx12.h"
extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);

#include"Vector4.h"
#include"Vector2.h"
#include"Matrix4x4.h"
#include"Matrix4x4Function.h"
#include"Vector3Function.h"

#include"externals/DirectXTex/DirectXTex.h"


//クライアント領域のサイズ
const int32_t kClientWidth = 1280;
const int32_t kClientHeight = 720;

struct VertexData
{
	Vector4 position;
	Vector2 texcoord;
};

struct BloomThreshold
{
	float threshold;
	Vector2 texelSize;
};

//-----------------------------------------FUNCTION-----------------------------------------//
void Log(const std::string& messege)
{
	OutputDebugStringA(messege.c_str());
}

std::wstring ConvertString(const std::string& str) {
	if (str.empty()) {
		return std::wstring();
	}

	auto sizeNeeded = MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), NULL, 0);
	if (sizeNeeded == 0) {
		return std::wstring();
	}
	std::wstring result(sizeNeeded, 0);
	MultiByteToWideChar(CP_UTF8, 0, reinterpret_cast<const char*>(&str[0]), static_cast<int>(str.size()), &result[0], sizeNeeded);
	return result;
}

std::string ConvertString(const std::wstring& str) {
	if (str.empty()) {
		return std::string();
	}

	auto sizeNeeded = WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), NULL, 0, NULL, NULL);
	if (sizeNeeded == 0) {
		return std::string();
	}
	std::string result(sizeNeeded, 0);
	WideCharToMultiByte(CP_UTF8, 0, str.data(), static_cast<int>(str.size()), result.data(), sizeNeeded, NULL, NULL);
	return result;
}

//CompileShader
IDxcBlob* CompileShader(
	const std::wstring& filePath,
	const wchar_t* profile,
	IDxcUtils* dxcUtils,
	IDxcCompiler3* dxcCompiler,
	IDxcIncludeHandler* includeHandler)
{
	//hlslファイルを読み込む
	Log(ConvertString(std::format(L"Begin CompileShader, path:{}, prefile:{}\n", filePath, profile)));
	IDxcBlobEncoding* shaderSource = nullptr;
	HRESULT hr = dxcUtils->LoadFile(filePath.c_str(), nullptr, &shaderSource);
	assert(SUCCEEDED(hr));

	DxcBuffer shaderSourceBuffer;
	shaderSourceBuffer.Ptr = shaderSource->GetBufferPointer();
	shaderSourceBuffer.Size = shaderSource->GetBufferSize();
	shaderSourceBuffer.Encoding = DXC_CP_UTF8;

	//コンパイルオプション
	LPCWSTR arguments[] = {
		filePath.c_str(),
		L"-E", L"main",
		L"-T", profile,
		L"-Zi", //デバッグ情報を出力
		L"-Qembed_debug", //デバッグ情報を埋め込む
		L"-Od", //最適化なし
		L"-Zpr", //行数情報を出力
	};

	//コンパイル
	IDxcResult* shaderResult = nullptr;
	hr = dxcCompiler->Compile(
		&shaderSourceBuffer,
		arguments, _countof(arguments),
		includeHandler,
		IID_PPV_ARGS(&shaderResult));
	assert(SUCCEEDED(hr));

	//エラーメッセージを取得
	IDxcBlobUtf8* shaderError = nullptr;
	shaderResult->GetOutput(DXC_OUT_ERRORS, IID_PPV_ARGS(&shaderError), nullptr);
	if (shaderError != nullptr && shaderError->GetStringLength() != 0)
	{
		Log(shaderError->GetStringPointer());
		assert(false);
	}

	//コンパイル結果を取得
	IDxcBlob* shaderBlob = nullptr;
	hr = shaderResult->GetOutput(DXC_OUT_OBJECT, IID_PPV_ARGS(&shaderBlob), nullptr);
	assert(SUCCEEDED(hr));
	Log(ConvertString(std::format(L"Compile Succeeded, path:{}, prefile:{}\n", filePath, profile)));
	shaderSource->Release();
	shaderResult->Release();
	return shaderBlob;

}

ID3D12Resource* CreateBufferResource(ID3D12Device* device, size_t sizeInBytes)
{
	// バッファの設定
	D3D12_RESOURCE_DESC bufferDesc{};
	bufferDesc.Dimension = D3D12_RESOURCE_DIMENSION_BUFFER;
	bufferDesc.Width = sizeInBytes;
	bufferDesc.Height = 1;
	bufferDesc.DepthOrArraySize = 1;
	bufferDesc.MipLevels = 1;
	bufferDesc.SampleDesc.Count = 1;
	bufferDesc.Layout = D3D12_TEXTURE_LAYOUT_ROW_MAJOR;

	// ヒープの設定
	D3D12_HEAP_PROPERTIES heapProperties{};
	heapProperties.Type = D3D12_HEAP_TYPE_UPLOAD;

	// リソースの生成
	ID3D12Resource* bufferResource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties,
		D3D12_HEAP_FLAG_NONE,
		&bufferDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		nullptr,
		IID_PPV_ARGS(&bufferResource));
	assert(SUCCEEDED(hr));
	return bufferResource;
}

ID3D12DescriptorHeap* CreateDescriptorHeap(ID3D12Device* device, D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT numDescriptors, bool shaderVisible)
{
	// ヒープの設定
	D3D12_DESCRIPTOR_HEAP_DESC heapDesc{};
	heapDesc.NumDescriptors = numDescriptors;
	heapDesc.Type = heapType;
	heapDesc.Flags = shaderVisible ? D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE : D3D12_DESCRIPTOR_HEAP_FLAG_NONE;

	// ヒープの生成
	ID3D12DescriptorHeap* descriptorHeap = nullptr;
	HRESULT hr = device->CreateDescriptorHeap(&heapDesc, IID_PPV_ARGS(&descriptorHeap));
	assert(SUCCEEDED(hr));
	return descriptorHeap;
}

DirectX::ScratchImage LoadTexture(const std::string& filePath)
{
	// テクスチャの読み込み
	DirectX::ScratchImage image;
	std::wstring filePathW = ConvertString(filePath);
	HRESULT hr = DirectX::LoadFromWICFile(filePathW.c_str(), DirectX::WIC_FLAGS_FORCE_SRGB, nullptr, image);
	assert(SUCCEEDED(hr));

	// mipmapを生成
	DirectX::ScratchImage mipImages{};
	hr = DirectX::GenerateMipMaps(image.GetImages(), image.GetImageCount(), image.GetMetadata(), DirectX::TEX_FILTER_SRGB, 0, mipImages);
	assert(SUCCEEDED(hr));

	return mipImages;

}

ID3D12Resource* CreateTextureResource(ID3D12Device* device, const DirectX::TexMetadata& metaData)
{
	// テクスチャの設定
	D3D12_RESOURCE_DESC resourceDesc{};
	resourceDesc.Width = UINT(metaData.width); // テクスチャの幅
	resourceDesc.Height = UINT(metaData.height); // テクスチャの高さ
	resourceDesc.DepthOrArraySize = UINT16(metaData.arraySize); // 配列サイズ
	resourceDesc.MipLevels = UINT16(metaData.mipLevels); // ミップマップレベル
	resourceDesc.Format = metaData.format; // フォーマット
	resourceDesc.SampleDesc.Count = 1; // サンプル数
	resourceDesc.Dimension = D3D12_RESOURCE_DIMENSION(metaData.dimension); // テクスチャの次元

	// ヒープの設定
	D3D12_HEAP_PROPERTIES heapProperties{}; 
	heapProperties.Type = D3D12_HEAP_TYPE_CUSTOM; // 細かい設定を行う
	heapProperties.CPUPageProperty = D3D12_CPU_PAGE_PROPERTY_WRITE_BACK; // WriteBackポリシーでCPUアクセス可能
	heapProperties.MemoryPoolPreference = D3D12_MEMORY_POOL_L0; // メモリプールの設定。プロセッサの近くに設定

	// Resourceの設定
	ID3D12Resource* textureResource = nullptr;
	HRESULT hr = device->CreateCommittedResource(
		&heapProperties, // ヒープの設定
		D3D12_HEAP_FLAG_NONE, // Heapの特殊な設定。特になし
		&resourceDesc, // リソースの設定
		D3D12_RESOURCE_STATE_GENERIC_READ, // リソースの初期状態. textureは基本読むだけなのでGENERIC_READ
		nullptr, // クリア値の設定。今回は使わないのでnullptr
		IID_PPV_ARGS(&textureResource)); // 生成したリソースのpointerへのpointerを取得
	assert(SUCCEEDED(hr));

	return textureResource;

}

void UploadTextureData(ID3D12Resource* textureResource, const DirectX::ScratchImage& mipImages)
{
	// MataDataの取得
	const DirectX::TexMetadata& metaData = mipImages.GetMetadata();
	// 全MipMapについて
	for (size_t mipLevel = 0; mipLevel < metaData.mipLevels; ++mipLevel)
	{
		const DirectX::Image* image = mipImages.GetImage(mipLevel, 0, 0);

		// textureに転送
		HRESULT hr = textureResource->WriteToSubresource(
			UINT(mipLevel), // 書き込むサブリソースのインデックス
			nullptr, // 全領域へコピペ
			image->pixels, // 元データのアドレス
			UINT(image->rowPitch), // 1行のバイト数
			UINT(image->slicePitch)); // 1枚のバイト数

		assert(SUCCEEDED(hr));
	}
}

//-----------------------------------------FUNCTION-----------------------------------------//


//------------------------------------------WINDOW------------------------------------------//
//ウィンドウプロシージャ
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
	if (ImGui_ImplWin32_WndProcHandler(hWnd, msg, wparam, lparam))
	{
		return true;
	}
	//メッセージによって処理を分岐
	switch (msg)
	{
		//ウィンドウが破棄されたとき
	case WM_DESTROY:
		//メッセージループを終了
		PostQuitMessage(0);
		break;
	}
	return DefWindowProc(hWnd, msg, wparam, lparam);
}



//Windowsプログラムのエントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	// COMの初期化
	CoInitializeEx(nullptr, COINIT_MULTITHREADED);

	WNDCLASS wc{};
	//ウィンドウプロシージャ
	wc.lpfnWndProc = WndProc;
	//クラス名
	wc.lpszClassName = L"CG2WindowClass";
	//インスタンスハンドル
	wc.hInstance = GetModuleHandle(nullptr);
	//カーソル
	wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
	//ウィンドウクラスを登録
	RegisterClass(&wc);

	//ウィンドウサイズを表す構造体にクライアント領域のサイズを入れる
	RECT wrc = { 0, 0, kClientWidth, kClientHeight };

	//ウィンドウの生成
	HWND hWnd = CreateWindow(
		wc.lpszClassName,	    //クラス名
		L"CG2",	                //タイトルバーの文字列
		WS_OVERLAPPEDWINDOW,	//ウィンドウスタイル
		CW_USEDEFAULT,		    //表示X座標
		CW_USEDEFAULT,		    //表示Y座標
		wrc.right - wrc.left,	//ウィンドウ幅
		wrc.bottom - wrc.top,	//ウィンドウ高さ
		nullptr,		        //親ウィンドウハンドル
		nullptr,		        //メニューハンドル
		wc.hInstance,		    //インスタンスハンドル
		nullptr);		        //追加パラメータ

	//ウィンドウを表示
	ShowWindow(hWnd, SW_SHOW);
	//-----------------------------------------WINDOW-----------------------------------------//

	//-----------------------------------------DebugLayer-----------------------------------------//
#ifdef _DEBUG
	ID3D12Debug1* debugController = nullptr;
	if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
	{
		//デバッグレイヤーを有効にする
		debugController->EnableDebugLayer();
		debugController->SetEnableGPUBasedValidation(TRUE);
	}
#endif
	//-----------------------------------------DebugLayer-----------------------------------------//


	//-----------------------------------------DirectX-----------------------------------------//
	//DXGIファクトリの生成
	IDXGIFactory7* dxgiFactory = nullptr;

	HRESULT hr = CreateDXGIFactory1(IID_PPV_ARGS(&dxgiFactory));
	assert(SUCCEEDED(hr));

	//アダプターの列挙
	IDXGIAdapter4* useAdapter = nullptr;
	for (UINT adapterIndex = 0; dxgiFactory->EnumAdapterByGpuPreference(adapterIndex, DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE, IID_PPV_ARGS(&useAdapter)) != DXGI_ERROR_NOT_FOUND; ++adapterIndex)
	{
		//アダプターの情報を取得
		DXGI_ADAPTER_DESC3 desc;
		hr = useAdapter->GetDesc3(&desc);
		assert(SUCCEEDED(hr));

		if (!(desc.Flags & DXGI_ADAPTER_FLAG3_SOFTWARE))
		{
			Log(std::format("Use Adapter:{}\n", ConvertString(desc.Description)));
			break;
		}
		useAdapter = nullptr;
	}
	assert(useAdapter != nullptr);

	// デバイスの生成
	ID3D12Device* device = nullptr;
	// 機能レベルとログ出力用文字列
	D3D_FEATURE_LEVEL featureLevels[] = {
		D3D_FEATURE_LEVEL_12_2,
		D3D_FEATURE_LEVEL_12_1,
		D3D_FEATURE_LEVEL_12_0
	};
	const char* featureLevelNames[] = {
		"12.2",
		"12.1",
		"12.0"
	};

	// 高い順に生成できるか試していく
	for (size_t i = 0; i < _countof(featureLevels); ++i) {
		hr = D3D12CreateDevice(useAdapter, featureLevels[i], IID_PPV_ARGS(&device));
		if (SUCCEEDED(hr)) {
			Log(std::format("Feature Level: {}\n", featureLevelNames[i]));
			break;
		}
	}
	assert(device != nullptr);
	Log("D3D12Device Created\n");

#ifdef _DEBUG
	ID3D12InfoQueue* infoQueue = nullptr;
	if (SUCCEEDED(device->QueryInterface(IID_PPV_ARGS(&infoQueue))))
	{
		// デバッグレイヤーの設定
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_CORRUPTION, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_ERROR, TRUE);
		infoQueue->SetBreakOnSeverity(D3D12_MESSAGE_SEVERITY_WARNING, TRUE);

		D3D12_MESSAGE_ID denyIds[] = {
			D3D12_MESSAGE_ID_RESOURCE_BARRIER_MISMATCHING_COMMAND_LIST_TYPE,
		};

		D3D12_MESSAGE_SEVERITY severities[] = {
			D3D12_MESSAGE_SEVERITY_INFO
		};

		D3D12_INFO_QUEUE_FILTER filter = {};
		filter.DenyList.NumIDs = _countof(denyIds);
		filter.DenyList.pIDList = denyIds;
		filter.DenyList.NumSeverities = _countof(severities);
		filter.DenyList.pSeverityList = severities;

		infoQueue->PushStorageFilter(&filter);

		infoQueue->Release();
	}
#endif


	// CommandQueueの生成
	ID3D12CommandQueue* commandQueue = nullptr;
	D3D12_COMMAND_QUEUE_DESC queueDesc{};
	hr = device->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&commandQueue));
	assert(SUCCEEDED(hr));

	// CommandAllocatorの生成
	ID3D12CommandAllocator* commandAllocator = nullptr;
	hr = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&commandAllocator));
	assert(SUCCEEDED(hr));

	// CommandListの生成
	ID3D12GraphicsCommandList* commandList = nullptr;
	hr = device->CreateCommandList(0, D3D12_COMMAND_LIST_TYPE_DIRECT, commandAllocator, nullptr, IID_PPV_ARGS(&commandList));
	assert(SUCCEEDED(hr));

	// SwapChainの生成
	IDXGISwapChain4* swapChain = nullptr;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc{};
	swapChainDesc.Width = kClientWidth;   // 画面の幅
	swapChainDesc.Height = kClientHeight; // 画面の高さ
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // 色の形式(バックバッファのフォーマット)
	swapChainDesc.SampleDesc.Count = 1; // マルチサンプルしない
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT; // バックバッファとして使用
	swapChainDesc.BufferCount = 2; // バッファ数
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD; // フリップ後破棄
	//コマンドキュー、ウィンドウハンドル、設定を渡して生成する
	hr = dxgiFactory->CreateSwapChainForHwnd(commandQueue, hWnd, &swapChainDesc, nullptr, nullptr, reinterpret_cast<IDXGISwapChain1**>(&swapChain));
	assert(SUCCEEDED(hr));

	// DescriptorHeapの生成
	ID3D12DescriptorHeap* rtvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 2, false);
	ID3D12DescriptorHeap* srvDescriptorHeap = CreateDescriptorHeap(device, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 128, true);
	//D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
	//rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビュー用
	//rtvHeapDesc.NumDescriptors = 2; // バッファ数分
	//hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap));
	//assert(SUCCEEDED(hr));

	// SwapChainからResourceを取得
	ID3D12Resource* swapChainResources[2] = { nullptr };
	hr = swapChain->GetBuffer(0, IID_PPV_ARGS(&swapChainResources[0]));
	assert(SUCCEEDED(hr));
	hr = swapChain->GetBuffer(1, IID_PPV_ARGS(&swapChainResources[1]));
	assert(SUCCEEDED(hr));

	// レンダーターゲットビューの生成
	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc{};
	rtvDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB; // 出力結果をSRGBに変換して書き込む
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D; // 2Dテクスチャとして書き込む
	// DescriptorHeapの先頭を取得
	D3D12_CPU_DESCRIPTOR_HANDLE rtvStartHandle = rtvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	// RTVを二つ作るのでDescriptorSizeを二つ用意
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle[2];
	rtvHandle[0] = rtvStartHandle;
	device->CreateRenderTargetView(swapChainResources[0], &rtvDesc, rtvHandle[0]);
	rtvHandle[1].ptr = rtvHandle[0].ptr + device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
	device->CreateRenderTargetView(swapChainResources[1], &rtvDesc, rtvHandle[1]);


	//FenceとEventの生成
	ID3D12Fence* fence = nullptr;
	uint64_t fenceValue = 0;
	hr = device->CreateFence(fenceValue, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));
	assert(SUCCEEDED(hr));

	HANDLE fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);
	assert(fenceEvent != nullptr);


	//-----------------------------------------DXC初期化-----------------------------------------//
	IDxcUtils* dxcUtils = nullptr;
	IDxcCompiler3* dxcCompiler = nullptr;
	hr = DxcCreateInstance(CLSID_DxcUtils, IID_PPV_ARGS(&dxcUtils));
	assert(SUCCEEDED(hr));
	hr = DxcCreateInstance(CLSID_DxcCompiler, IID_PPV_ARGS(&dxcCompiler));
	assert(SUCCEEDED(hr));

	IDxcIncludeHandler* includeHandler = nullptr;
	hr = dxcUtils->CreateDefaultIncludeHandler(&includeHandler);
	assert(SUCCEEDED(hr));
	//-----------------------------------------DXC初期化-----------------------------------------//

	//-----------------------------------------/PSO-----------------------------------------///
	// rootSignatureの生成
	D3D12_ROOT_SIGNATURE_DESC descriptionRootSignature{};
	descriptionRootSignature.Flags = D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;

	// Samplerの設定
	D3D12_STATIC_SAMPLER_DESC samplerDesc[1]{};
	samplerDesc[0].Filter = D3D12_FILTER_MIN_MAG_MIP_LINEAR; // テクスチャの補間方法
	samplerDesc[0].AddressU = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
	samplerDesc[0].AddressV = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
	samplerDesc[0].AddressW = D3D12_TEXTURE_ADDRESS_MODE_WRAP; // テクスチャの繰り返し方法
	samplerDesc[0].ComparisonFunc = D3D12_COMPARISON_FUNC_NEVER; // 比較しない
	samplerDesc[0].MaxLOD = D3D12_FLOAT32_MAX; // ミップマップの最大LOD
	samplerDesc[0].ShaderRegister = 0; // レジスタ番号
	samplerDesc[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	descriptionRootSignature.pStaticSamplers = samplerDesc;
	descriptionRootSignature.NumStaticSamplers = _countof(samplerDesc);

	// DescriptorRangeの設定。
	D3D12_DESCRIPTOR_RANGE descriptorRange[1] = {};
	descriptorRange[0].BaseShaderRegister = 0; // レジスタ番号
	descriptorRange[0].NumDescriptors = 1; // ディスクリプタ数
	descriptorRange[0].RangeType = D3D12_DESCRIPTOR_RANGE_TYPE_SRV; // SRVを使う
	descriptorRange[0].OffsetInDescriptorsFromTableStart = D3D12_DESCRIPTOR_RANGE_OFFSET_APPEND; // Offsetを自動計算

	// RootParameterの設定。複数設定できるので配列
	D3D12_ROOT_PARAMETER rootParameters[4] = {};

	rootParameters[0].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
	rootParameters[0].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParameters[0].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド

	rootParameters[1].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
	rootParameters[1].ShaderVisibility = D3D12_SHADER_VISIBILITY_VERTEX; // 頂点シェーダーで使う
	rootParameters[1].Descriptor.ShaderRegister = 0; // レジスタ番号とバインド 

	rootParameters[2].ParameterType = D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE; // ディスクリプタテーブルを使う
	rootParameters[2].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParameters[2].DescriptorTable.pDescriptorRanges = descriptorRange; // ディスクリプタレンジを設定
	rootParameters[2].DescriptorTable.NumDescriptorRanges = _countof(descriptorRange); // レンジの数

	rootParameters[3].ParameterType = D3D12_ROOT_PARAMETER_TYPE_CBV; // 定数バッファビューを使う
	rootParameters[3].ShaderVisibility = D3D12_SHADER_VISIBILITY_PIXEL; // ピクセルシェーダーで使う
	rootParameters[3].Descriptor.ShaderRegister = 1; // レジスタ番号とバインド

	descriptionRootSignature.pParameters = rootParameters;
	descriptionRootSignature.NumParameters = _countof(rootParameters);

	ID3DBlob* signatureBlob = nullptr;
	ID3DBlob* errorBlob = nullptr;
	hr = D3D12SerializeRootSignature(&descriptionRootSignature, D3D_ROOT_SIGNATURE_VERSION_1, &signatureBlob, &errorBlob);
	if (FAILED(hr))
	{
		Log(reinterpret_cast<char*>(errorBlob->GetBufferPointer()));
		assert(false);
	}
	ID3D12RootSignature* rootSignature = nullptr;
	hr = device->CreateRootSignature(0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature));
	signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature);
	assert(SUCCEEDED(hr));


	// InputLayout
	D3D12_INPUT_ELEMENT_DESC inputElementDescs[2] = {};
	inputElementDescs[0].SemanticName = "POSITION";
	inputElementDescs[0].SemanticIndex = 0;
	inputElementDescs[0].Format = DXGI_FORMAT_R32G32B32A32_FLOAT;
	inputElementDescs[0].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	inputElementDescs[1].SemanticName = "TEXCOORD";
	inputElementDescs[1].SemanticIndex = 0;
	inputElementDescs[1].Format = DXGI_FORMAT_R32G32_FLOAT;
	inputElementDescs[1].AlignedByteOffset = D3D12_APPEND_ALIGNED_ELEMENT;
	D3D12_INPUT_LAYOUT_DESC inputLayoutDesc{};
	inputLayoutDesc.pInputElementDescs = inputElementDescs;
	inputLayoutDesc.NumElements = _countof(inputElementDescs);

	// BlendState
	D3D12_BLEND_DESC blendDesc{};
	blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	// RasterizerState
	D3D12_RASTERIZER_DESC rasterizerDesc{};
	// 三角形の中を塗りつぶす
	rasterizerDesc.FillMode = D3D12_FILL_MODE_SOLID;
	// 裏面を表示しない
	rasterizerDesc.CullMode = D3D12_CULL_MODE_BACK;

	// shaderのコンパイル
	IDxcBlob* vertexShaderBlob = CompileShader(L"Object3d.VS.hlsl", L"vs_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert(vertexShaderBlob != nullptr);

	IDxcBlob* pixelShaderBlob = CompileShader(L"Object3d.PS.hlsl", L"ps_6_0", dxcUtils, dxcCompiler, includeHandler);
	assert(pixelShaderBlob != nullptr);

	// PSOの生成
	D3D12_GRAPHICS_PIPELINE_STATE_DESC graphicsPipelineStateDesc{};
	graphicsPipelineStateDesc.pRootSignature = rootSignature;
	graphicsPipelineStateDesc.InputLayout = inputLayoutDesc;
	graphicsPipelineStateDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
	graphicsPipelineStateDesc.BlendState = blendDesc;
	graphicsPipelineStateDesc.RasterizerState = rasterizerDesc;
	// 書き込むRTVの情報
	graphicsPipelineStateDesc.NumRenderTargets = 1;
	graphicsPipelineStateDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM_SRGB;
	// 利用するトポロジ（形状）のタイプ。三角形
	graphicsPipelineStateDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	// どのように画面に色を打ち込むかの設定
	graphicsPipelineStateDesc.SampleDesc.Count = 1;
	graphicsPipelineStateDesc.SampleMask = D3D12_DEFAULT_SAMPLE_MASK;
	// 実際に生成
	ID3D12PipelineState* graphicsPipelineState = nullptr;
	hr = device->CreateGraphicsPipelineState(&graphicsPipelineStateDesc, IID_PPV_ARGS(&graphicsPipelineState));
	assert(SUCCEEDED(hr));

	//-----------------------------------------PSO-----------------------------------------///



	//----------------------------------------Resource---------------------------------------//

	// 頂点バッファのリソースを作る。頂点三つ分のサイズ------------------------------------//
	ID3D12Resource* vertexResource = CreateBufferResource(device, sizeof(VertexData) * 3);

	//VertexBufferView
	// 頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferView{};
	// リソースの先頭アドレスから使う
	vertexBufferView.BufferLocation = vertexResource->GetGPUVirtualAddress();
	// 使用するリソースのサイズは頂点三つ分のサイズ
	vertexBufferView.SizeInBytes = sizeof(VertexData) * 3;
	// 一つの頂点のサイズ
	vertexBufferView.StrideInBytes = sizeof(VertexData);

	// 頂点リソースにデータを書き込む
	VertexData* vertexData = nullptr;
	// アドレスを取得
	vertexResource->Map(0, nullptr, reinterpret_cast<void**>(&vertexData));
	// 左下
	vertexData[0].position = { -0.5f, -0.5f, 0.0f, 1.0f };
	vertexData[0].texcoord = { 0.0f, 1.0f };
	// 上
	vertexData[1].position = { 0.0f, 0.5f, 0.0f, 1.0f };
	vertexData[1].texcoord = { 0.5f, 0.0f };
	// 右下
	vertexData[2].position = { 0.5f, -0.5f, 0.0f, 1.0f };
	vertexData[2].texcoord = { 1.0f, 1.0f };


	// マテリアル用のリソースを作る。今回はcolor1つ分のサイズ--------------------------------------//
	ID3D12Resource* materialResource = CreateBufferResource(device, sizeof(Vector4));
	// マテリアルにデータを書き込む
	Vector4* materialData = nullptr;
	// アドレスを取得
	materialResource->Map(0, nullptr, reinterpret_cast<void**>(&materialData));
	// 色
	materialData[0] = { 1.0f, 1.0f, 1.0f, 1.0f };


	// WVP用のCBufferリソースを作る。Matrix4x4分のサイズ----------------------------------------------//
	ID3D12Resource* wvpResource = CreateBufferResource(device, sizeof(Matrix4x4));
	// WVPにデータを書き込む
	Matrix4x4* TrasformationMatrixData = nullptr;
	// WVP行列を作成
	Transform transform{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	Transform cameraTransform{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 5.0f} };
	Matrix4x4 worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
	Matrix4x4 cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
	Matrix4x4 viewMatrix = Inverse(cameraMatrix);
	Matrix4x4 projectionMatrix = MakePerspectiveMatrix(0.45f, static_cast<float>(kClientWidth) / static_cast<float>(kClientHeight), 0.1f, 100.0f);
	Matrix4x4 wvpMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
	// アドレスを取得
	wvpResource->Map(0, nullptr, reinterpret_cast<void**>(&TrasformationMatrixData));
	// 単位行列を書き込んでいく
	*TrasformationMatrixData = wvpMatrix;


	// Textureを読んで転送する---------------------------------------------------------------//
	DirectX::ScratchImage mipImages = LoadTexture("resources/blloomTest1.png");
	const DirectX::TexMetadata& metaData = mipImages.GetMetadata();
	ID3D12Resource* textureResource = CreateTextureResource(device, metaData);
	UploadTextureData(textureResource, mipImages);

	// Texture用のBloomThresholdCBufferリソースを作成
	ID3D12Resource* BloomThresholdResource = CreateBufferResource(device, sizeof(BloomThreshold));
	// BloomThresholdにデータを書き込む
	BloomThreshold* bloomThresholdData = nullptr;
	// アドレスを取得
	BloomThresholdResource->Map(0, nullptr, reinterpret_cast<void**>(&bloomThresholdData));
	// データを書き込む
	bloomThresholdData->threshold = 1.0f;
	bloomThresholdData->texelSize.x = 1.0f;
	bloomThresholdData->texelSize.y = 1.0f;

	// Texture用のSRVを作成
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc{};
	srvDesc.Format = metaData.format;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D; // 2Dテクスチャ
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Texture2D.MipLevels = UINT(metaData.mipLevels);

	// SRVを作成するDescriptorの位置を決める
	D3D12_CPU_DESCRIPTOR_HANDLE textureSrvHandleCPU = srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	D3D12_GPU_DESCRIPTOR_HANDLE textureSrvHandleGPU = srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	// 先頭はImGuiが使ってるのでその次を使う
	textureSrvHandleCPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	textureSrvHandleGPU.ptr += device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
	// SRVを作成
	device->CreateShaderResourceView(textureResource, &srvDesc, textureSrvHandleCPU);


	// Sprite用の頂点リソースを作成---------------------------------------------------------------//
	ID3D12Resource* vertexResourceSprite = CreateBufferResource(device, sizeof(VertexData) * 6);

	// 頂点バッファビューを作成する
	D3D12_VERTEX_BUFFER_VIEW vertexBufferViewSprite{};
	vertexBufferViewSprite.BufferLocation = vertexResourceSprite->GetGPUVirtualAddress(); // リソースの先頭アドレスから使う
	vertexBufferViewSprite.SizeInBytes = sizeof(VertexData) * 6; // 使用するリソースのサイズは頂点6つ分のサイズ
	vertexBufferViewSprite.StrideInBytes = sizeof(VertexData); // 一つの頂点のサイズ

	// 頂点リソースにデータを書き込む
	VertexData* vertexDataSprite = nullptr;
	// Map
	vertexResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&vertexDataSprite));

	// 一つ目の三角形
	vertexDataSprite[0].position = { 0.0f, 640.0f, 0.0f, 1.0f }; // 左下
	vertexDataSprite[0].texcoord = { 0.0f, 1.0f };
	vertexDataSprite[1].position = { 0.0f, 0.0f, 0.0f, 1.0f }; // 左上
	vertexDataSprite[1].texcoord = { 0.0f, 0.0f };
	vertexDataSprite[2].position = { 420.0f, 640.0f, 0.0f, 1.0f }; // 右下
	vertexDataSprite[2].texcoord = { 1.0f, 1.0f };

	// 二つ目の三角形
	vertexDataSprite[3].position = { 0.0f, 0.0f, 0.0f, 1.0f }; // 左上
	vertexDataSprite[3].texcoord = { 0.0f, 0.0f };
	vertexDataSprite[4].position = { 420.0f, 0.0f, 0.0f, 1.0f }; // 右上
	vertexDataSprite[4].texcoord = { 1.0f, 0.0f };
	vertexDataSprite[5].position = { 420.0f, 640.0f, 0.0f, 1.0f }; // 右下
	vertexDataSprite[5].texcoord = { 1.0f, 1.0f };

	// Sprite用のTrasformationMatrixCBufferリソースを作成---------------------------------------------------------------//
	ID3D12Resource* TrasformationMatrixResourceSprite = CreateBufferResource(device, sizeof(Matrix4x4));
	// Wデータを書き込む
	Matrix4x4* TrasformationMatrixDataSprite = nullptr;
	// Map
	TrasformationMatrixResourceSprite->Map(0, nullptr, reinterpret_cast<void**>(&TrasformationMatrixDataSprite));
	// 単位行列を書き込んでいく
	*TrasformationMatrixDataSprite = MakeIdentityMatrix4x4();

	Transform transformSprite{ {1.0f, 1.0f, 1.0f}, {0.0f, 0.0f, 0.0f}, {0.0f, 0.0f, 0.0f} };
	transformSprite.translate.x = 400.0f;
	transformSprite.translate.y = 40.0f;

	// Sprite用のwvpMatrix
	Matrix4x4 worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
	Matrix4x4 viewMatrixSprite = MakeIdentityMatrix4x4();
	Matrix4x4 projectionMatrixSprite = MakeOrthoMatrix(0.0f, 0.0f, float(kClientWidth), float(kClientHeight), 0.0f, 100.0f);
	Matrix4x4 wvpMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));

	*TrasformationMatrixDataSprite = wvpMatrixSprite;


	//--------------------------Resource--------------------------//

	// viewPortの設定
	D3D12_VIEWPORT viewPort{};
	viewPort.Width = kClientWidth;
	viewPort.Height = kClientHeight;
	viewPort.TopLeftX = 0;
	viewPort.TopLeftY = 0;
	viewPort.MinDepth = 0.0f;
	viewPort.MaxDepth = 1.0f;

	// scissorRectの設定
	D3D12_RECT scissorRect{};
	scissorRect.left = 0;
	scissorRect.right = kClientWidth;
	scissorRect.top = 0;
	scissorRect.bottom = kClientHeight;

	//-----------------------------------------DirectX-----------------------------------------//


	//-----------------------------------------Imgui-----------------------------------------//

	//ImGuiの初期化
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGui::StyleColorsDark();
	ImGui_ImplWin32_Init(hWnd);
	ImGui_ImplDX12_Init(
		device,
		swapChainDesc.BufferCount,
		rtvDesc.Format,
		srvDescriptorHeap,
		srvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(),
		srvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	//-----------------------------------------Imgui-----------------------------------------//



	//---------------------------------------------------GAMELOOP-----------------------------------------------------//
	MSG msg{};

	while (msg.message != WM_QUIT)
	{
		//メッセージがある場合は処理
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);

		} else {
			//ここに更新処理を書く

			//-------------imguiの初期化-------------//
			ImGui_ImplDX12_NewFrame();
			ImGui_ImplWin32_NewFrame();
			ImGui::NewFrame();
			//-------------imguiの初期化-------------//

			//-----------画面の色を変える-----------//
			// バックバッファのインデックスを取得
			UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();

			// ---PRESENTからRTV用のStateにするTransitionBarrierを張る--- //
			D3D12_RESOURCE_BARRIER barrier{};
			barrier.Type = D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;
			barrier.Flags = D3D12_RESOURCE_BARRIER_FLAG_NONE;
			// バリアを張る対象のリソース。現在のバックバッファに対して行う
			barrier.Transition.pResource = swapChainResources[backBufferIndex];
			// 遷移前（現在）のresourceState
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_PRESENT;
			// 遷移後のresourceState
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_RENDER_TARGET;
			// バリアを張る
			commandList->ResourceBarrier(1, &barrier);
			// ---PRESENTからRTV用のStateにするTransitionBarrierを張る--- //

			// 描画先のRTVを設定する
			commandList->OMSetRenderTargets(1, &rtvHandle[backBufferIndex], false, nullptr);

			// クリアカラー
			float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };

			// 画面をクリア
			commandList->ClearRenderTargetView(rtvHandle[backBufferIndex], clearColor, 0, nullptr);

			// imgui描画用のDescriptorHeapを設定。ClearRenderTargetViewの後、実際の描画する前に設定する
			ID3D12DescriptorHeap* heaps[] = { srvDescriptorHeap };
			commandList->SetDescriptorHeaps(_countof(heaps), heaps);

			//-------------------ImGui-------------------//
			ImGui::Begin("SpriteCameraTransform");
			ImGui::SliderFloat3("SpriteScale", &transformSprite.scale.x, 0.0f, 2.0f);
			ImGui::SliderFloat3("SpriteTranslate", &transformSprite.translate.x, 0.0f, 1280.0f);
			ImGui::SliderFloat3("CameraScale", &cameraTransform.scale.x, 0.0f, 2.0f);
			ImGui::SliderFloat3("CameraRotate", &cameraTransform.rotate.x, 0.0f, 6.28f);
			ImGui::SliderFloat3("CameraTranslate", &cameraTransform.translate.x, 0.0f, 30.0f);
			ImGui::End();

			ImGui::Begin("BloomParameter");
			ImGui::SliderFloat("BloomThreshold", &bloomThresholdData->threshold, 0.0f, 1.0f);
			ImGui::SliderFloat("TexelSizeX", &bloomThresholdData->texelSize.x, 0.0f, 100.0f);
			ImGui::SliderFloat("TexelSizeY", &bloomThresholdData->texelSize.y, 0.0f, 100.0f);
			ImGui::End();

			//-------------------ImGui-------------------//

			// ImGuiの内部コマンドを生成。描画処理の前に行う
			ImGui::Render();

			//-----------三角形の描画-----------//
			// 描画先のRTVを設定する
			commandList->OMSetRenderTargets(1, &rtvHandle[backBufferIndex], false, nullptr);
			// バックバッファをクリア
			commandList->ClearRenderTargetView(rtvHandle[backBufferIndex], clearColor, 0, nullptr);

			commandList->RSSetViewports(1, &viewPort);
			commandList->RSSetScissorRects(1, &scissorRect);
			commandList->SetGraphicsRootSignature(rootSignature);
			commandList->SetPipelineState(graphicsPipelineState);
			commandList->IASetVertexBuffers(0, 1, &vertexBufferView);
			commandList->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);

			// マテリアルの設定。色を変える
			commandList->SetGraphicsRootConstantBufferView(0, materialResource->GetGPUVirtualAddress()); // マテリアルCBufferの場所を設定

			// WVPのcBufferの設定
			commandList->SetGraphicsRootConstantBufferView(1, wvpResource->GetGPUVirtualAddress()); // WVPのCBufferの場所を設定

			// 三角形の座標変換
			transform.rotate.y += 0.03f;
			worldMatrix = MakeAffineMatrix(transform.scale, transform.rotate, transform.translate);
			cameraMatrix = MakeAffineMatrix(cameraTransform.scale, cameraTransform.rotate, cameraTransform.translate);
			viewMatrix = Inverse(cameraMatrix);
			projectionMatrix = MakePerspectiveMatrix(0.45f, static_cast<float>(kClientWidth) / static_cast<float>(kClientHeight), 0.1f, 100.0f);
			wvpMatrix = Multiply(worldMatrix, Multiply(viewMatrix, projectionMatrix));
			*TrasformationMatrixData = wvpMatrix;

			// Textureの設定
			commandList->SetGraphicsRootDescriptorTable(2, textureSrvHandleGPU);

			// BloomThresholdの設定
			commandList->SetGraphicsRootConstantBufferView(3, BloomThresholdResource->GetGPUVirtualAddress());

			// 描画
			//commandList->DrawInstanced(3, 1, 0, 0);
			//-----------三角形の描画-----------//



			//-----------Spriteの描画-----------//
			// spriteの座標変換
			worldMatrixSprite = MakeAffineMatrix(transformSprite.scale, transformSprite.rotate, transformSprite.translate);
			wvpMatrixSprite = Multiply(worldMatrixSprite, Multiply(viewMatrixSprite, projectionMatrixSprite));

			*TrasformationMatrixDataSprite = wvpMatrixSprite;

			commandList->IASetVertexBuffers(0, 1, &vertexBufferViewSprite);
			commandList->SetGraphicsRootConstantBufferView(1, TrasformationMatrixResourceSprite->GetGPUVirtualAddress());
			commandList->DrawInstanced(6, 1, 0, 0);
			//-----------Spriteの描画-----------//


			// commandListにimguiの描画コマンドを積む。描画処理の後、RTVからPRESENT Stateに戻す前に行う
			ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), commandList);


			// ---RTVからPRESENTStateに戻すTransitionBarrierを張る--- //
			barrier.Transition.StateBefore = D3D12_RESOURCE_STATE_RENDER_TARGET;
			barrier.Transition.StateAfter = D3D12_RESOURCE_STATE_PRESENT;
			commandList->ResourceBarrier(1, &barrier);
			// ---RTVからPRESENTStateに戻すTransitionBarrierを張る--- //

			// コマンドリストを閉じる
			hr = commandList->Close();
			assert(SUCCEEDED(hr));

			// GPUにコマンドリストの実行を行わせる
			ID3D12CommandList* commandLists[] = { commandList };
			commandQueue->ExecuteCommandLists(1, commandLists);

			// GPUとOSに画面の交換を行うよう通知する
			swapChain->Present(1, 0);

			//Fenceを使ってGPUの処理が終わるのを待つ
			fenceValue++;
			commandQueue->Signal(fence, fenceValue);

			if (fence->GetCompletedValue() < fenceValue)
			{
				fence->SetEventOnCompletion(fenceValue, fenceEvent);
				WaitForSingleObject(fenceEvent, INFINITE);
			}

			// 次のフレーム用のコマンドリストを準備
			hr = commandAllocator->Reset();
			assert(SUCCEEDED(hr));
			hr = commandList->Reset(commandAllocator, nullptr);
			assert(SUCCEEDED(hr));
			//-----------画面の色を変える-----------//

			//ここに描画処理を書く



		}
	}

	//-------------------------------------------------GAMELOOP-----------------------------------------------------/
	
	CoUninitialize();

	// 解放
	ImGui_ImplDX12_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	// リソースの解放
	wvpResource->Release();
	materialResource->Release();
	vertexResource->Release();
	textureResource->Release();
	BloomThresholdResource->Release();
	TrasformationMatrixResourceSprite->Release();
	vertexResourceSprite->Release();
	TrasformationMatrixResourceSprite->Release();
	dxcUtils->Release();
	dxcCompiler->Release();
	includeHandler->Release();
	materialResource->Release();
	vertexResource->Release();
	graphicsPipelineState->Release();
	signatureBlob->Release();
	if (errorBlob)
	{
		errorBlob->Release();
	}
	rootSignature->Release();
	pixelShaderBlob->Release();
	vertexShaderBlob->Release();

	CloseHandle(fenceEvent);
	fence->Release();
	rtvDescriptorHeap->Release();
	swapChainResources[0]->Release();
	swapChainResources[1]->Release();
	swapChain->Release();
	commandList->Release();
	commandAllocator->Release();
	commandQueue->Release();
	device->Release();
	useAdapter->Release();
	dxgiFactory->Release();
#ifdef _DEBUG
	debugController->Release();
#endif 
	CloseWindow(hWnd);


	// リソースリークをチェック
	IDXGIDebug1* debug = nullptr;
	if (SUCCEEDED(DXGIGetDebugInterface1(0, IID_PPV_ARGS(&debug))))
	{
		debug->ReportLiveObjects(DXGI_DEBUG_ALL, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_APP, DXGI_DEBUG_RLO_ALL);
		debug->ReportLiveObjects(DXGI_DEBUG_D3D12, DXGI_DEBUG_RLO_ALL);
		debug->Release();
	}

	return 0;
}
