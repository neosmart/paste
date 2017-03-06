#include <Windows.h>

enum class ExitReason : int
{
	Success,
	ClipboardError,
	NoTextualData
};

int wstrlen(const wchar_t *src)
{
	int count = 0;
	for (const wchar_t *c = src; c != nullptr && *c != '\0'; ++c)
	{
		++count;
	}

	return count;
}

bool ClipboardContainsFormat(UINT format)
{
	bool firstTime = true;
	for (UINT f = 0; firstTime || f != 0; f = EnumClipboardFormats(f))
	{
		firstTime = false;
		if (f == format)
		{
			return true;
		}
	}
	return false;
}

int main(int argc, const char *argv[])
{
	if (!OpenClipboard(nullptr))
	{
		ExitProcess((UINT)ExitReason::ClipboardError);
	}

	if (!ClipboardContainsFormat(CF_UNICODETEXT))
	{
		ExitProcess((UINT)ExitReason::NoTextualData);
	}

	HANDLE hData = GetClipboardData(CF_UNICODETEXT);
	if (hData == INVALID_HANDLE_VALUE || hData == nullptr)
	{
		CloseClipboard();
		ExitProcess((UINT)ExitReason::ClipboardError);
	}
	const wchar_t *text = (const wchar_t *) GlobalLock(hData);
	if (text == nullptr)
	{
		CloseClipboard();
		ExitProcess((UINT)ExitReason::ClipboardError);
	}

	DWORD charsWritten = -1;
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	WriteConsoleW(hOut, text, wstrlen(text), &charsWritten, 0);
	CloseHandle(hOut);

	GlobalUnlock(hData);
	CloseClipboard();

	ExitProcess((UINT)ExitReason::Success);
}
