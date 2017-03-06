#include <Windows.h>
#include <stdio.h>

int main()
{
	if (!OpenClipboard(nullptr))
	{
		return -1;
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
		return -1;
	}

	printf("%S", text);
	GlobalUnlock(hData);
	CloseClipboard();

    return 0;
}
