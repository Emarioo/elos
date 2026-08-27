# DOOM
Done. See doomgeneric fork. Tweaked SDL linux port to ELOS.


# Plants VS Zombies

There are reverse engineering projects but they use SDL, windows, linux APIs which we'll need
to port over to ELOS.

Therefore we might want to make a Win32 compatibility layer for the original PvZ executable.
Could prove useful when porting other games.

The ELOS project is not about porting a bunch of games or maintaining Win32 or other operating system ABIs because
they are annoying. BUT we also want games to play in ELOS so... here we go.


The PvZ executable is/has:
- 32-bit x86 PE/COFF
- text, rdata, data sections
- Import tables
- No debug information

The DLLs we need to translate are:
- WSOCK32.dll
- KERNEL32.dll
- USER32.dll
- GDI32.dll
- ADVAPI32.dll
- SHELL32.dll
- ole32.dll
- OLEAUT32.dll
- WININET.dll
- WINMM.dll


Some stuff we need:
- 32-bit user mode
- COFF loading (take the oppurtunity to give loader some structure, make it a proper component, currently messy)
- More ELOS memory functions (as backend for win32 stuff)
- More ELOS file functions (as backend for win32 stuff, mainly thinking file mappings)


**Normally:** ELOS -> Syscall Interface -> ELF x64 Application (like Doom)

**Win32 Compat Layer:** ELOS -> Syscall Interface -> ELF x86 `win32_loader` -> PE/COFF Win32 (like PvZ)
`win32_loader` is a 32-bit user program that allocates executable memory and fixes import tables according to the PE/COFF file to load.



## Win32 functions to implement

Below are the functions we need to implement, yikes.

KERNEL32.dll

    DeleteFileA
    CreateFileA
    MoveFileExA
    SetThreadPriority
    Process32Next
    GetCommandLineW
    GetExitCodeProcess
    GlobalUnlock
    CreateMutexA
    OutputDebugStringA
    Sleep
    OpenProcess
    GetWindowsDirectoryA
    FreeLibrary
    EnumResourceNamesA
    SetFileAttributesA
    Process32First
    LeaveCriticalSection
    CreateFileMappingA
    CreateThread
    InitializeCriticalSection
    WaitForSingleObject
    DeleteCriticalSection
    EnterCriticalSection
    SetEvent
    GetModuleHandleA
    MapViewOfFile
    CreateEventA
    UnmapViewOfFile
    QueryPerformanceCounter
    QueryPerformanceFrequency
    GetCurrentProcessId
    VirtualFree
    VirtualAlloc
    CompareStringA
    InterlockedExchange
    SetLastError
    GetCurrentThread
    SetEnvironmentVariableA
    CloseHandle
    EnumSystemLocalesA
    GetLastError
    GetStringTypeA
    GetLocaleInfoA
    GetDateFormatA
    GetTimeFormatA
    CreatePipe
    GetFileAttributesA
    GetConsoleOutputCP
    WriteConsoleA
    SetCurrentDirectoryA
    GetCurrentDirectoryA
    GetEnvironmentStrings
    FreeEnvironmentStringsA
    HeapCreate
    HeapDestroy
    GetOEMCP
    GetACP
    HeapSize
    ReadFile
    FlushFileBuffers
    GetConsoleMode
    GetConsoleCP
    WriteFile
    TlsFree
    TlsSetValue
    TlsAlloc
    TlsGetValue
    SetFilePointer
    SetHandleCount
    LCMapStringA
    MoveFileA
    DuplicateHandle
    GetFileType
    SetStdHandle
    GetFullPathNameA
    GetDriveTypeA
    RaiseException
    RtlUnwind
    GetStartupInfoA
    GetCommandLineA
    ExitProcess
    HeapReAlloc
    ExitThread
    GetSystemTimeAsFileTime
    GetProcessHeap
    HeapAlloc
    HeapFree
    SetEndOfFile
    IsDebuggerPresent
    SetUnhandledExceptionFilter
    UnhandledExceptionFilter
    GetCurrentProcess
    LoadLibraryA
    ResumeThread
    GetTickCount
    CreateToolhelp32Snapshot
    GetModuleFileNameA
    GlobalLock
    TerminateProcess
    GetVolumeInformationA
    CreateProcessA
    GetUserDefaultLCID
    IsValidLocale
    CreateDirectoryA
    InterlockedDecrement
    InterlockedIncrement
    GetTimeZoneInformation
    CopyFileA
    OpenMutexA
    GetConsoleScreenBufferInfo
    GetConsoleCursorInfo
    GetStdHandle
    ReleaseMutex
    WaitForMultipleObjects
    OpenFileMappingA
    OpenEventA
    SetFileTime
    ReadConsoleInputA
    AllocConsole
    SetConsoleCursorPosition
    SetConsoleCtrlHandler
    SetConsoleCursorInfo
    GetCurrentThreadId
    SetConsoleTitleA
    PeekConsoleInputA
    GetVersionExA
    FindClose
    FindFirstFileA
    FindNextFileA
    GlobalAlloc
    VirtualProtect
    VirtualQuery
    GetThreadPriority
    MulDiv
    GetFileTime
    FreeConsole
    SetConsoleTextAttribute

USER32.dll

    FillRect
    CreateCursor
    DestroyCursor
    GetFocus
    GetQueueStatus
    WindowFromPoint
    DrawMenuBar
    SetClipboardData
    BeginPaint
    ReleaseDC
    UnregisterClassA
    GetSystemMenu
    DeleteMenu
    ScreenToClient
    GetWindowPlacement
    SetFocus
    PostThreadMessageA
    DestroyWindow
    DestroyCaret
    DispatchMessageA
    GetDesktopWindow
    SetWindowTextA
    GetCursor
    GetClientRect
    GetForegroundWindow
    SetTimer
    LoadImageA
    GetWindowThreadProcessId
    HideCaret
    IntersectRect
    RegisterClassA
    PostQuitMessage
    GetWindowTextLengthA
    SendMessageA
    GetMessageA
    GetCursorPos
    AppendMenuA
    TrackPopupMenu
    OpenClipboard
    CreateCaret
    MessageBoxA
    MoveWindow
    EnumDisplayMonitors
    GetWindowRect
    IsWindow
    IsIconic
    ShowCaret
    PostMessageA
    OpenIcon
    GetDC
    AdjustWindowRect
    EndPaint
    RegisterWindowMessageA
    TranslateMessage
    DefWindowProcA
    GetSystemMetrics
    IsWindowVisible
    BringWindowToTop
    CloseClipboard
    CreateWindowExA
    SetCaretPos
    GetWindowTextA
    SetForegroundWindow
    EnumWindows
    ShowWindow
    LoadCursorA
    ClientToScreen
    PeekMessageA
    CreatePopupMenu
    SetCursor
    GetParent
    ReleaseCapture
    SetCapture
    FlashWindowEx

GDI32.dll

    SelectClipRgn
    GdiFlush
    GetStockObject
    GetDeviceCaps
    CreateFontA
    GetTextMetricsA
    GetCharABCWidthsA
    GetObjectA
    CreateFontIndirectA
    SetBkMode
    IntersectClipRect
    DeleteObject
    DeleteDC
    SelectObject
    CreateCompatibleDC
    CreateDIBSection
    BitBlt
    SetDIBitsToDevice
    SetTextColor
    StretchBlt

ADVAPI32.dll

    RegCloseKey
    RegCreateKeyExA
    RegQueryValueExA
    RegOpenKeyExA
    RegSetValueExA

SHELL32.dll

    ShellExecuteA

ole32.dll

    CoUninitialize
    CoInitialize
    CoInitializeSecurity
    CoCreateInstance

WININET.dll

    InternetOpenA
    HttpOpenRequestA
    HttpQueryInfoA
    InternetReadFile
    InternetConnectA
    HttpSendRequestA
    InternetCloseHandle

WINMM.dll

    timeEndPeriod
    PlaySoundA
    timeBeginPeriod
    timeGetTime

WSOCK32.dll

    Ordinal   115
    Ordinal   116

OLEAUT32.dll

    Ordinal     6
    Ordinal   150
