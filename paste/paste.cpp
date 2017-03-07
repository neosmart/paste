#include <Windows.h>

enum class ExitReason : int
{
	Success,
	ClipboardError,
	NoTextualData,
	SystemError
};

template <typename T>
int strlen(const T *src)
{
	int count = 0;
	for (const T *c = src; c != nullptr && *c != static_cast<T>(0); ++c)
	{
		++count;
	}

	return count;
}

void WriteError(const char *error)
{
	HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
	if (hErr == INVALID_HANDLE_VALUE || hErr == nullptr)
	{
		ExitProcess((UINT)ExitReason::SystemError);
	}
	DWORD charsWritten = -1;
	WriteConsoleA(hErr, error, strlen(error), &charsWritten, 0);
	CloseHandle(hErr);
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
		WriteError("Failed to open system clipboard!\n");
		ExitProcess((UINT)ExitReason::ClipboardError);
	}

	if (!ClipboardContainsFormat(CF_UNICODETEXT))
	{
		CloseClipboard();
		WriteError("Clipboard contains non-text data!\n");
		ExitProcess((UINT)ExitReason::NoTextualData);
	}

	HANDLE hData = GetClipboardData(CF_UNICODETEXT);
	if (hData == INVALID_HANDLE_VALUE || hData == nullptr)
	{
		CloseClipboard();
		WriteError("Unable to get clipboard data!\n");
		ExitProcess((UINT)ExitReason::ClipboardError);
	}
	const wchar_t *text = (const wchar_t *) GlobalLock(hData);
	if (text == nullptr)
	{
		CloseHandle(hData);
		CloseClipboard();
		WriteError("Unable to get clipboard data!\n");
		ExitProcess((UINT)ExitReason::ClipboardError);
	}

	DWORD charsWritten = -1;
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	WriteConsoleW(hOut, text, strlen(text), &charsWritten, 0);
	CloseHandle(hOut);

	GlobalUnlock(hData);
	CloseHandle(hData);
	CloseClipboard();

	ExitProcess((UINT)ExitReason::Success);
}
