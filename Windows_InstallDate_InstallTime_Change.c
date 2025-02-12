#ifndef UNICODE
#define UNICODE
#endif
#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <windows.h>
#include <shellapi.h>

// Compile with: gcc -static -mwindows -fexec-charset=UTF-8 -finput-charset=UTF-8 Windows_InstallDate_InstallTime_Change.c -o Windows_InstallDate_InstallTime_Change.exe -lshell32 -luser32

// Manually declare the MessageBoxTimeoutW function
typedef UINT (WINAPI *MSGBOXTIMEOUTW)(HWND, LPCWSTR, LPCWSTR, UINT, WORD, DWORD);

// Convert Unix timestamp to LDAP timestamp
int64_t UnixToLDAP(int64_t unixTimestamp) {
    const int64_t EPOCH_DIFFERENCE = 11644473600LL;
    return (unixTimestamp + EPOCH_DIFFERENCE) * 10000000LL;
}

int main(int argc, wchar_t *argv[]) {
    // Set console output to UTF-8 (although we won't be using the console)
    SetConsoleOutputCP(CP_UTF8);

    // (UAC elevation code - same as before, using ShellExecute) ...
    // Check if the program is already running with administrator privileges
    BOOL isElevated = FALSE;
    HANDLE hToken = NULL;
    if (OpenProcessToken(GetCurrentProcess(), TOKEN_QUERY, &hToken)) {
        TOKEN_ELEVATION elevation;
        DWORD cbSize = sizeof(TOKEN_ELEVATION);
        if (GetTokenInformation(hToken, TokenElevation, &elevation, sizeof(elevation), &cbSize)) {
            isElevated = elevation.TokenIsElevated;
        }
        CloseHandle(hToken);
    }

    if (!isElevated) {
        // If not running as administrator, relaunch the program and request administrator privileges
        if (argc == 1) { // Ensure it only restarts the first time
            wchar_t szPath[MAX_PATH];
            if (GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath))) { // Get the path of the current executable
                // Use ShellExecute to start the program with administrator privileges
                SHELLEXECUTEINFOW sei = { sizeof(sei) };
                sei.lpVerb = L"runas";          // Request administrator privileges
                sei.lpFile = szPath;             // Executable path
                sei.lpParameters = L"requestAdmin"; // Pass a parameter to prevent an infinite loop
                sei.hwnd = NULL;
                sei.nShow = SW_NORMAL;

                if (!ShellExecuteExW(&sei)) {
                    DWORD dwError = GetLastError();
                    if (dwError == ERROR_CANCELLED) {
                        // User canceled the UAC prompt
                        fwprintf(stderr, L"User declined the request for administrator privileges.\n");
                    } else {
                        fwprintf(stderr, L"ShellExecuteEx failed: Error code %ld\n", dwError);
                    }
                    return 1;
                }
                return 0; // Successfully launched the new process, exit the current process
            }
        }
        return 0;  // If the parameter has been passed, it indicates that it is the restarted process, return directly
    }

    // Get current system time
    time_t now = time(NULL);
    unsigned int unixTimestamp = (unsigned int)now;

    // Convert to LDAP timestamp (64-bit integer)
    int64_t ldapTimestamp = UnixToLDAP((int64_t)unixTimestamp);

    // Output the results (in hexadecimal) -  (These lines won't actually output to the console in a GUI app)
    wprintf(L"Unix Timestamp (InstallDate): 0x%X\n", unixTimestamp);
    wprintf(L"LDAP Timestamp (InstallTime): 0x%" PRIX64 "\n", ldapTimestamp);

    // Open the registry key
    HKEY hKey;
    LSTATUS result = RegOpenKeyExW(
        HKEY_LOCAL_MACHINE,
        L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        0,
        KEY_SET_VALUE,
        &hKey
    );

    if (result != ERROR_SUCCESS) {
        fwprintf(stderr, L"Failed to open registry key: Error code %ld\n", result);
        return 1;
    }

    // Write InstallDate (DWORD)
    result = RegSetValueExW(
        hKey,
        L"InstallDate",
        0,
        REG_DWORD,
        (const BYTE*)&unixTimestamp,
        sizeof(unixTimestamp)
    );

    if (result != ERROR_SUCCESS) {
        fwprintf(stderr, L"Failed to write InstallDate: Error code %ld\n", result);
        RegCloseKey(hKey);
        return 1;
    }

    // Write InstallTime (REG_QWORD)
    result = RegSetValueExW(
        hKey,
        L"InstallTime",
        0,
        REG_QWORD,
        (const BYTE*)&ldapTimestamp,
        sizeof(ldapTimestamp)
    );

    if (result != ERROR_SUCCESS) {
        fwprintf(stderr, L"Failed to write InstallTime: Error code %ld\n", result);
        RegCloseKey(hKey);
        return 1;
    }

    // Close the registry key
    RegCloseKey(hKey);

    wprintf(L"Registry updated.\n"); // This won't be seen in the GUI app

    // Get and format the current date (using wide characters, and combine all text)
    wchar_t dateStr[256];
    struct tm *timeinfo;
    timeinfo = localtime(&now);
    wcsftime(dateStr, sizeof(dateStr) / sizeof(wchar_t), L"Your operating system installation date has been changed to: %Y-%m-%d\nThis program was written by CXT, technical support: cxthhhhh.com", timeinfo);

    // Display a message box (using MessageBoxTimeoutW, auto-close after 2 seconds)
    HMODULE hUser32 = GetModuleHandleW(L"user32.dll"); // Get the handle of user32.dll
    MSGBOXTIMEOUTW pMessageBoxTimeoutW = (MSGBOXTIMEOUTW)GetProcAddress(hUser32, "MessageBoxTimeoutW");

    if (pMessageBoxTimeoutW != NULL) {
        pMessageBoxTimeoutW(NULL, dateStr, L"Notice", MB_OK | MB_ICONINFORMATION, 0, 2000);
    } else {
        fwprintf(stderr, L"Failed to find the MessageBoxTimeoutW function.\n");
        // As a fallback, you can use MessageBoxW (which won't auto-close)
        MessageBoxW(NULL, dateStr, L"Notice", MB_OK | MB_ICONINFORMATION);
    }

    return 0;
}