#include <stdio.h>
#include <time.h>
#include <stdint.h>
#include <inttypes.h>

// 将 Unix 时间戳转换为 LDAP 时间戳
int64_t UnixToLDAP(int64_t unixTimestamp) {
    const int64_t EPOCH_DIFFERENCE = 11644473600LL;
    return (unixTimestamp + EPOCH_DIFFERENCE) * 10000000LL;
}

int main() {
    // 获取当前系统时间
    time_t now = time(NULL);
    unsigned int unixTimestamp = (unsigned int)now;

    // 转换为 LDAP 时间戳 (64 位整数)
    int64_t ldapTimestamp = UnixToLDAP((int64_t)unixTimestamp);

    // 输出结果 (十六进制)
    printf("Unix Timestamp (InstallDate): 0x%X\n", unixTimestamp);
    printf("LDAP Timestamp (InstallTime): 0x%" PRIX64 "\n", ldapTimestamp);

    // 创建并写入 .reg 文件
    FILE *regFile = fopen("UpdateInstallTime.reg", "w");
    if (regFile != NULL) {
        fprintf(regFile, "Windows Registry Editor Version 5.00\n\n");
        fprintf(regFile, "; 本程序由CXT编写，技术支持cxthhhhh.com\n"); // 添加注释
        fprintf(regFile, "; 此 .reg 文件用于更新 Windows 安装时间\n");
        fprintf(regFile, "[HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion]\n");

        // 写入 InstallDate 的十六进制 dword 表示
        fprintf(regFile, "; InstallDate (Unix 时间戳, 十六进制)\n");
        fprintf(regFile, "\"InstallDate\"=dword:%x\n", unixTimestamp);

        // 写入 InstallTime 的 hex(b) 表示 (小端序)
        fprintf(regFile, "; InstallTime (LDAP 时间戳, 十六进制字节序列, 小端序)\n");
        fprintf(regFile, "\"InstallTime\"=hex(b):");
        for (int i = 0; i < 8; i++) {
            uint8_t byte = (ldapTimestamp >> (i * 8)) & 0xFF;
            fprintf(regFile, "%02x", byte);
            if (i < 7) {
                fprintf(regFile, ",");
            }
        }
        fprintf(regFile, "\n");

        fclose(regFile);
        printf("已生成注册表文件 UpdateInstallTime.reg\n");
    } else {
        fprintf(stderr, "无法创建注册表文件!\n");
        return 1;
    }

    return 0;
}