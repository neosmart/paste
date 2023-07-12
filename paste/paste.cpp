#include <Windows.h>

// The maximum number of chars to buffer before issuing a write
#define MAX_BUF 1024

HANDLE _processHeap;

struct Utf8Buffer {
    char *Buffer;
    DWORD Length;
} _utf8Buffer;

enum class ExitReason : int {
    Success,
    ClipboardError,
    NoTextualData,
    SystemError,
    CtrlC,
};

enum class LineEnding : int {
    AsIs,
    CrLf,
    Lf
};

template <typename T>
T *_malloc(DWORD count)
{
    DWORD bytes = sizeof(T) * count;
    return (T *) HeapAlloc(_processHeap, HEAP_ZERO_MEMORY, bytes);
}

template <typename T>
T *_realloc(T *ptr, DWORD count)
{
    DWORD bytes = sizeof(T) * count;
    return (T *) HeapReAlloc(_processHeap, HEAP_ZERO_MEMORY, ptr, bytes);
}

template <typename T>
void _free(T *ptr)
{
    HeapFree(_processHeap, 0, ptr);
}

int WINAPI CtrlHandler(DWORD ctrlType)
{
    switch (ctrlType) {
        case CTRL_C_EVENT:
        case CTRL_BREAK_EVENT:
        case CTRL_CLOSE_EVENT:
        case CTRL_LOGOFF_EVENT:
        case CTRL_SHUTDOWN_EVENT:
            ExitProcess((UINT) ExitReason::CtrlC);
        default:
            return false;
    }
    return true;
}

void Write(const wchar_t *text, DWORD outputHandle = STD_OUTPUT_HANDLE, DWORD chars = -1)
{
    chars = chars != -1 ? chars : lstrlen(text);

    HANDLE hOut = GetStdHandle(outputHandle);
    if (hOut == INVALID_HANDLE_VALUE || hOut == nullptr) {
        ExitProcess((UINT) -1);
    }
    DWORD consoleMode;
    bool isConsole = GetConsoleMode(hOut, &consoleMode) != 0;

    DWORD result = 0;
    DWORD charsWritten = -1;
    if (isConsole) {
        result = WriteConsoleW(hOut, text, chars, &charsWritten, nullptr);
    }
    else {
        // WSL fakes the console and requires UTF-8 output. Assume !isConsole to mean WSL mode.

        // WideCharToMultiByte does not support chunking in case of fixed output buffer size. It
        // assumes you can dynamically allocate memory and expects you to call it with a fixed
        // output buffer size of 0 and a nullptr to get back the number of bytes required to fit the
        // conversion, which you will then dynamically allocate. While it *does* stream its output
        // and abort only when there's insufficient room left in the provided buffer, it does not
        // report back how many source characters were consumed by the operation so it's impossible
        // to figure out what to pass in as the starting point for a subsequent continuation call to
        // WideCharToMultiByte(). Realistically, the only recourse would be to check beforehand if
        // the needed size exceeds the capacity of a fixed- length buffer, then test different
        // lengths of input to see what *would* fit in the buffer in each go. Or just use a
        // dynamically allocated buffer.
        DWORD utf8ByteCount = WideCharToMultiByte(
            CP_UTF8, WC_ERR_INVALID_CHARS, text, chars, nullptr, 0, nullptr, nullptr);
        if (_utf8Buffer.Buffer == nullptr) {
            _utf8Buffer.Buffer = _malloc<char>(utf8ByteCount);
            _utf8Buffer.Length = utf8ByteCount;
        }
        else if (_utf8Buffer.Length < utf8ByteCount) {
            _utf8Buffer.Buffer = _realloc<char>(_utf8Buffer.Buffer, utf8ByteCount);
            _utf8Buffer.Length = utf8ByteCount;
        }
        // "WideCharToMultiByte function operates most efficiently when both lpDefaultChar and
        // lpUsedDefaultChar are set to NULL."
        DWORD bytesConverted = WideCharToMultiByte(CP_UTF8,
                                                   WC_ERR_INVALID_CHARS,
                                                   text,
                                                   chars,
                                                   _utf8Buffer.Buffer,
                                                   _utf8Buffer.Length,
                                                   nullptr,
                                                   nullptr);
        result = WriteFile(hOut, _utf8Buffer.Buffer, bytesConverted, &charsWritten, nullptr);
        if (charsWritten != utf8ByteCount) {
            ExitProcess(GetLastError());
        }
    }

    if (result == 0) {
        ExitProcess((UINT) GetLastError());
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
    for (UINT f = 0; firstTime || f != 0; f = EnumClipboardFormats(f)) {
        firstTime = false;
        if (f == format) {
            return true;
        }
    }
    return false;
}

void print(const WCHAR *text, LineEnding lineEnding)
{
    WCHAR buf[MAX_BUF];
    auto i = 0;
    for (auto *ptr = text; ptr && *ptr; ++ptr) {
        if (lineEnding == LineEnding::Lf && *ptr == L'\r') {
            continue;
        }
        else if (lineEnding == LineEnding::CrLf && (i == 0 || buf[i - 1] != '\r') && *ptr == '\n') {
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
    _utf8Buffer = {};

    SetConsoleCtrlHandler(CtrlHandler, true);

    int argc;
    LPWSTR *argv = CommandLineToArgvW(GetCommandLine(), &argc);
    LineEnding lineEnding = LineEnding::AsIs;

    if (argc == 2) {
        if (lstrcmpi(argv[1], L"--lf") == 0) {
            lineEnding = LineEnding::Lf;
        }
        else if (lstrcmpi(argv[1], L"--crlf") == 0) {
            lineEnding = LineEnding::CrLf;
        }
        else if (lstrcmpi(argv[1], L"--help") == 0 || lstrcmpi(argv[1], L"-h") == 0
                 || lstrcmpi(argv[1], L"/?") == 0 || lstrcmpi(argv[1], L"/help") == 0
                 || lstrcmpi(argv[1], L"/h") == 0) {
            Write(L"paste.exe by NeoSmart Technologies\n");
            Write(L"https://github.com/neosmart/paste\n\n");
            Write(L"USAGE: paste.exe [--lf|--crlf|--help]\n");
            ExitProcess(0);
        }
    }

    if (!OpenClipboard(nullptr)) {
        WriteError(L"Failed to open system clipboard!\n");
        ExitProcess((UINT) ExitReason::ClipboardError);
    }

    if (!ClipboardContainsFormat(CF_UNICODETEXT)) {
        CloseClipboard();
        WriteError(L"Clipboard contains non-text data!\n");
        ExitProcess((UINT) ExitReason::NoTextualData);
    }

    HANDLE hData = GetClipboardData(CF_UNICODETEXT);
    if (hData == INVALID_HANDLE_VALUE || hData == nullptr) {
        CloseClipboard();
        WriteError(L"Unable to get clipboard data!\n");
        ExitProcess((UINT) ExitReason::ClipboardError);
    }

    const wchar_t *text = (const wchar_t *) GlobalLock(hData);
    if (text == nullptr) {
        CloseClipboard();
        WriteError(L"Unable to get clipboard data!\n");
        ExitProcess((UINT) ExitReason::ClipboardError);
    }

    print(text, lineEnding);

    GlobalUnlock(hData);
    CloseClipboard();

    ExitProcess((UINT) ExitReason::Success);
}
