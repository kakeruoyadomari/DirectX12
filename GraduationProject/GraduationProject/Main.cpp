#include "Win32Application.h"

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nCmdShow)
{
	DXApplication dxApp(1280, 720, L"DX Sample");
	Win32Application::Run(&dxApp, hInstance);
	return 0;
}