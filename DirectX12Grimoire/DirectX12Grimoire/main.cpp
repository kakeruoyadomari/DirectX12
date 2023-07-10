#include <Windows.h>
#ifdef _DEBUG
#include <iostream>
#endif // _DEBUG

using namespace std;

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

#ifdef _DEBUG
int main()
{
#else
int WINAPI WinmMain(HINSTANCE, HINSTANCE, LPSTR, int)
{
#endif // _DEBUG
	DebugOutputFormatString("Show window test.");
	getchar();
	return 0;

}
