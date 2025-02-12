#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>
#include <windows.h>
#include <shellapi.h>

// 将 Unix 时间戳转换为 LDAP 时间戳
int64_t UnixToLDAP(int64_t unixTimestamp) {
    const int64_t EPOCH_DIFFERENCE = 11644473600LL;
    return (unixTimestamp + EPOCH_DIFFERENCE) * 10000000LL;
}

int main(int argc, char *argv[]) {
    // (权限提升代码 - 与之前相同，使用 ShellExecute) ...
      // 检查是否已经以管理员权限运行
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
        // 如果没有管理员权限，则重新启动自身并请求管理员权限
        if (argc == 1) { // 确保只在第一次运行时重新启动
            wchar_t szPath[MAX_PATH];
            if (GetModuleFileNameW(NULL, szPath, ARRAYSIZE(szPath))) { // 获取当前可执行文件的路径
                // 使用 ShellExecute 以管理员权限启动自身
                SHELLEXECUTEINFOW sei = { sizeof(sei) };
                sei.lpVerb = L"runas";          // 请求管理员权限
                sei.lpFile = szPath;             // 可执行文件路径
                sei.lpParameters = L"requestAdmin"; // 传递一个参数，防止无限循环
                sei.hwnd = NULL;
                sei.nShow = SW_NORMAL;

                if (!ShellExecuteExW(&sei)) {
                    DWORD dwError = GetLastError();
                    if (dwError == ERROR_CANCELLED) {
                        // 用户取消了 UAC 提示
                        fprintf(stderr, "用户拒绝了管理员权限请求。\n");
                    } else {
                        fprintf(stderr, "ShellExecuteEx 失败: 错误码 %ld\n", dwError);
                    }
                    return 1;
                }
                return 0; // 成功启动了新进程，退出当前进程
            }
        }
        return 0;  // 如果已经传递了参数，说明是重新启动后的进程，直接返回
    }

    // 获取当前系统时间
    time_t now = time(NULL);
    unsigned int unixTimestamp = (unsigned int)now;

    // 转换为 LDAP 时间戳 (64 位整数)
    int64_t ldapTimestamp = UnixToLDAP((int64_t)unixTimestamp);

    // 输出结果 (十六进制)
    printf("Unix Timestamp (InstallDate): 0x%X\n", unixTimestamp);
    printf("LDAP Timestamp (InstallTime): 0x%" PRIX64 "\n", ldapTimestamp);

    // 打开注册表项
    HKEY hKey;
    LSTATUS result = RegOpenKeyEx(
        HKEY_LOCAL_MACHINE,
        "SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",
        0,
        KEY_SET_VALUE,
        &hKey
    );

    if (result != ERROR_SUCCESS) {
        fprintf(stderr, "无法打开注册表项: 错误码 %ld\n", result);
        return 1;
    }

    // 写入 InstallDate (DWORD)
    result = RegSetValueEx(
        hKey,
        "InstallDate",
        0,
        REG_DWORD,
        (const BYTE*)&unixTimestamp,
        sizeof(unixTimestamp)
    );

    if (result != ERROR_SUCCESS) {
        fprintf(stderr, "无法写入 InstallDate: 错误码 %ld\n", result);
        RegCloseKey(hKey);
        return 1;
    }

    // 写入 InstallTime (REG_QWORD)
    result = RegSetValueEx(
        hKey,
        "InstallTime",
        0,
        REG_QWORD, // 正确的类型: REG_QWORD
        (const BYTE*)&ldapTimestamp, // 直接传递 int64_t 变量的地址
        sizeof(ldapTimestamp) //  int64_t 的大小
    );

    if (result != ERROR_SUCCESS) {
        fprintf(stderr, "无法写入 InstallTime: 错误码 %ld\n", result);
        RegCloseKey(hKey);
        return 1;
    }

    // 关闭注册表项
    RegCloseKey(hKey);

    printf("注册表已更新。\n");

    return 0;
}