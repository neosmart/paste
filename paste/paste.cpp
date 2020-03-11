#include <Windows.h>

// The maximum number of chars to buffer before issuing a write
#define MAX_BUF 1024

HANDLE _processHeap;

enum class ExitReason : int
{
	Success,
	ClipboardError,
	NoTextualData,
	SystemError,
	CtrlC,
};

enum class LineEnding : int
{
	AsIs,
	CrLf,
	Lf
};

template<typename T>
T* _malloc(DWORD count)
{
	DWORD bytes = sizeof(T) * count;
	return (T*)HeapAlloc(_processHeap, HEAP_ZERO_MEMORY, bytes);
}

template<typename T>
void _free(T *obj)
{
	HeapFree(_processHeap, 0, obj);
}

int WINAPI CtrlHandler(DWORD ctrlType) {
	switch (ctrlType) {
	case CTRL_C_EVENT:
		ExitProcess((UINT)ExitReason::CtrlC);
	default:
		return false;
	}
	return true;
}

void Write(const wchar_t *text, DWORD outputHandle = STD_OUTPUT_HANDLE, DWORD chars = -1)
{
	chars = chars != -1 ? chars : lstrlen(text);

	HANDLE hOut = GetStdHandle(outputHandle);
	if (hOut == INVALID_HANDLE_VALUE || hOut == nullptr)
	{
		ExitProcess((UINT)-1);
	}
	DWORD consoleMode;
	bool isConsole = GetConsoleMode(hOut, &consoleMode) != 0;

	DWORD result = 0;
	DWORD charsWritten = -1;
	if (isConsole)
	{
		result = WriteConsoleW(hOut, text, chars, &charsWritten, nullptr);
	}
	else
	{
		// WSL fakes the console and requires UTF-8 output
		DWORD utf8ByteCount = WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, chars + 1, nullptr, 0, nullptr, nullptr); // include null
		auto utf8Bytes = _malloc<char>(utf8ByteCount);
		WideCharToMultiByte(CP_UTF8, WC_ERR_INVALID_CHARS, text, -1, utf8Bytes, utf8ByteCount, nullptr, nullptr);
		result = WriteFile(hOut, utf8Bytes, utf8ByteCount - 1 /* remove null */, &charsWritten, nullptr);
		if (charsWritten != utf8ByteCount - 1)
		{
			ExitProcess(GetLastError());
		}
		_free(utf8Bytes);
	}

	if (result == 0)
	{
		ExitProcess((UINT)GetLastError());
	}
}

void Write(const wchar_t text, DWORD outputHandle = STD_OUTPUT_HANDLE)
{
	return Write(&text, outputHandle, 1);
}

void WriteError(const wchar_t *text)
{
	return Write(text, STD_ERROR_HANDLE);
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
	WCHAR buf[MAX_BUF];
	auto i = 0;
	for (auto *ptr = text; ptr && *ptr; ++ptr)
	{
		if (lineEnding == LineEnding::Lf && *ptr == L'\r') {
			continue;
		}
		else if (lineEnding == LineEnding::CrLf
			&& (i == 0 || buf[i-1] != '\r') && *ptr == '\n') {
			buf[i++] = '\r';
		}
		buf[i++] = *ptr;
		if (i == MAX_BUF) {
			Write(buf, STD_OUTPUT_HANDLE, i);
			i = 0;
		}
	}
	Write(buf, STD_OUTPUT_HANDLE, i);
}

int wmain(void)
{
	_processHeap = GetProcessHeap();
	SetConsoleCtrlHandler(CtrlHandler, true);

	int argc;
	LPWSTR *argv = CommandLineToArgvW(GetCommandLine(), &argc);
	LineEnding lineEnding = LineEnding::AsIs;

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
