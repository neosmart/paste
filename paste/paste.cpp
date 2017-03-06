#include <Windows.h>

int wstrlen(const wchar_t *src)
{
	int count = 0;
	for (const wchar_t *c = src; c != nullptr && *c != '\0'; ++c)
	{
		++count;
	}

	return count;
}

int main(int argc, const char *argv[])
{
	if (!OpenClipboard(nullptr))
	{
		ExitProcess(-1);
	}

	HANDLE hData = GetClipboardData(CF_UNICODETEXT);
	if (hData == INVALID_HANDLE_VALUE || hData == nullptr)
	{
		return -1;
	}
	const wchar_t *text = (const wchar_t *) GlobalLock(hData);
	if (text == nullptr)
	{
		CloseClipboard();
		ExitProcess(-1);
	}

	DWORD charsWritten = -1;
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	WriteConsoleW(hOut, text, wstrlen(text), &charsWritten, 0);
	CloseHandle(hOut);

	GlobalUnlock(hData);
	CloseClipboard();

	ExitProcess(0);
}
