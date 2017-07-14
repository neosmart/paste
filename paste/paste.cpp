#include <Windows.h>

enum class ExitReason : int
{
	Success,
	ClipboardError,
	NoTextualData,
	SystemError
};

enum class LineEnding : int
{
	AsIs,
	CrLf,
	Lf
};

void Write(const wchar_t *text, DWORD length = -1)
{
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == INVALID_HANDLE_VALUE || hOut == nullptr)
	{
		ExitProcess((UINT)ExitReason::SystemError);
	}
	DWORD charsWritten = -1;
	WriteConsoleW(hOut, text, length != -1 ? length : lstrlen(text), &charsWritten, 0);
	CloseHandle(hOut);
}

void Write(const char *text, DWORD length = -1)
{
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hOut == INVALID_HANDLE_VALUE || hOut == nullptr)
	{
		ExitProcess((UINT)ExitReason::SystemError);
	}
	DWORD charsWritten = -1;
	WriteConsoleA(hOut, text, length != -1 ? length : lstrlenA(text), &charsWritten, 0);
	CloseHandle(hOut);
}

void Write(const wchar_t text)
{
	return Write(&text, 1);
}

void WriteError(const wchar_t *error)
{
	HANDLE hErr = GetStdHandle(STD_ERROR_HANDLE);
	if (hErr == INVALID_HANDLE_VALUE || hErr == nullptr)
	{
		ExitProcess((UINT)ExitReason::SystemError);
	}
	DWORD charsWritten = -1;
	WriteConsole(hErr, error, lstrlen(error), &charsWritten, 0);
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

void print(const WCHAR *text, LineEnding lineEnding)
{
	DWORD charsWritten = -1;
	HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);

	switch (lineEnding)
	{
	case LineEnding::AsIs:
		WriteConsoleW(hOut, text, lstrlen(text), &charsWritten, nullptr);
		break;
	case LineEnding::Lf:
		for (auto ptr = text; ptr && *ptr; ++ptr)
		{
			if (*ptr != '\r')
			{
				WriteConsoleW(hOut, ptr, 1, &charsWritten, nullptr);
			}
		}
		break;
	case LineEnding::CrLf:
		for (auto ptr = text; ptr && *ptr; ++ptr)
		{
			if (*ptr == '\n' && (ptr == text || *(ptr -1) != '\r'))
			{
				WriteConsoleW(hOut, L"\r", 1, &charsWritten, nullptr);
			}
			WriteConsoleW(hOut, ptr, 1, &charsWritten, nullptr);
		}
		break;
	}
	CloseHandle(hOut);
}

int wmain(int argc, const WCHAR *argv[], const WCHAR *envp[])
{
	LineEnding lineEnding = LineEnding::AsIs;
	Write(L"wstring");
	Write("string");

	if (argc == 2)
	{
		if (lstrcmpi(argv[1], L"--lf") == 0)
		{
			lineEnding = LineEnding::Lf;
		}
		else if (lstrcmpi(argv[1], L"--crlf") == 0)
		{
			lineEnding = LineEnding::CrLf;
		}
	}

	if (!OpenClipboard(nullptr))
	{
		WriteError(L"Failed to open system clipboard!\n");
		ExitProcess((UINT)ExitReason::ClipboardError);
	}

	if (!ClipboardContainsFormat(CF_UNICODETEXT))
	{
		CloseClipboard();
		WriteError(L"Clipboard contains non-text data!\n");
		ExitProcess((UINT)ExitReason::NoTextualData);
	}

	HANDLE hData = GetClipboardData(CF_UNICODETEXT);
	if (hData == INVALID_HANDLE_VALUE || hData == nullptr)
	{
		CloseClipboard();
		WriteError(L"Unable to get clipboard data!\n");
		ExitProcess((UINT)ExitReason::ClipboardError);
	}

	const wchar_t *text = (const wchar_t *) GlobalLock(hData);
	if (text == nullptr)
	{
		CloseClipboard();
		WriteError(L"Unable to get clipboard data!\n");
		ExitProcess((UINT)ExitReason::ClipboardError);
	}

	print(text, lineEnding);

	GlobalUnlock(hData);
	CloseClipboard();

	ExitProcess((UINT)ExitReason::Success);
}
