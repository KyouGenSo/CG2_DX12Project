#include<Windows.h>
#include<cstdint>
#include<string>
#include<format>
#include <cassert>
#include <d3d12.h>
#include <dxgi1_6.h>
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3d12.lib")

//クライアント領域のサイズ
const int32_t kClientWidth = 1280;
const int32_t kClientHeight = 720;

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

//ウィンドウプロシージャ
LRESULT CALLBACK WndProc(HWND hWnd, UINT msg, WPARAM wparam, LPARAM lparam)
{
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
//-----------------------------------------FUNCTION-----------------------------------------//


//Windowsプログラムのエントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	//-----------------------------------------WINDOW-----------------------------------------//
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
	ID3D12DescriptorHeap* rtvDescriptorHeap = nullptr;
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc{};
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV; // レンダーターゲットビュー用
	rtvHeapDesc.NumDescriptors = 2; // バッファ数分
	hr = device->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&rtvDescriptorHeap));
	assert(SUCCEEDED(hr));

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

	// バックバッファのインデックスを取得
	UINT backBufferIndex = swapChain->GetCurrentBackBufferIndex();
	// 描画先のRTVを設定する
	commandList->OMSetRenderTargets(1, &rtvHandle[backBufferIndex], false, nullptr);
	// クリアカラー
	float clearColor[] = { 0.1f, 0.25f, 0.5f, 1.0f };
	// バックバッファをクリア
	commandList->ClearRenderTargetView(rtvHandle[backBufferIndex], clearColor, 0, nullptr);
	// すべてのコマンドを積んだらcommandListをcloseして内容を確定させる
	hr = commandList->Close();
	assert(SUCCEEDED(hr));

	// commandListを実行
	ID3D12CommandList* commandLists[] = { commandList };
	commandQueue->ExecuteCommandLists(1, commandLists);
	// GPUとOSに画面の交換を行うよう通知する
	swapChain->Present(1, 0);
	// 次のフレーム用のコマンドリストを準備
	hr = commandAllocator->Reset();
	assert(SUCCEEDED(hr));
	hr = commandList->Reset(commandAllocator, nullptr);
	assert(SUCCEEDED(hr));



	//-----------------------------------------DirectX-----------------------------------------//


	//-----------------------------------------GAMELOOP-----------------------------------------//
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



			//ここに描画処理を書く



		}
	}

	//-----------------------------------------GAMELOOP-----------------------------------------//


	return 0;
}
