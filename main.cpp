#include<Windows.h>

//Windowsプログラムのエントリーポイント
int WINAPI WinMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
	//出力ウィンドウに文字列を出力
	OutputDebugStringA("Hello, DirectX\n");

	return 0;
}
