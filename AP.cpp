#include "common.h"  

static INT_PTR CALLBACK DlgProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);
static void DialogInit(void);
static void Switch(void);
static void Config(void);
static void RightClickIcon(void);
static void ShowAbout(void);

static HWND gDialog;
static HINSTANCE gInstance;
static NOTIFYICONDATA gNid;

static bool gApOn = false;
static int gUserCount = 0;
static ConfigData gConfigData;

int WINAPI AP_WinMain(HINSTANCE instance)
{
    gInstance = instance;
    HWND gDialog = CreateDialog(gInstance,
        MAKEINTRESOURCE(IDD_AP), GetDesktopWindow(), (DLGPROC)DlgProc);
    if (!gDialog) {
        return 0;
    }
    ShowWindow(gDialog, SW_SHOW);
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0)) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }
    return 0;
}

static INT_PTR CALLBACK DlgProc(HWND dialog, UINT msg, WPARAM wParam, LPARAM lParam)
{
    gDialog = dialog;
    CStringW str;

    switch (msg) {
    case WM_INITDIALOG:
        DialogInit();
        break;

    case WM_SYSCOMMAND:
        if (wParam == SC_CLOSE) {
            if (gApOn && gConfigData.turnOffOnQuit) {
                Switch();
            }
            PostQuitMessage(0);
        }
        break;

    case WM_SHOWTASK:
        if (LOWORD(wParam) == IDI_ICON && LOWORD(lParam) == WM_RBUTTONDOWN) {
            RightClickIcon();
        }
        break;

    case WM_COMMAND:
        switch (LOWORD(wParam)) {
        case IDC_SWITCH:
        case ID_MENU_SWITCH:
            Switch();
            break;
        case IDC_CONFIG:
            Config();
            break;
        case IDC_MINIMIZE:
            Shell_NotifyIcon(NIM_ADD, &gNid);
            ShowWindow(dialog, SW_HIDE);
            break;
        case ID_MENU_ABOUT:
            str.LoadStringW(IDS_ABOUT_CONTENT);
            MessageBox(NULL, str, L"我", MB_OK);
            break;
        case ID_MENU_SHOW:
            ShowWindow(dialog, SW_SHOW);
            Shell_NotifyIcon(NIM_DELETE, &gNid);
            break;
        case ID_MENU_QUIT:
            if (gApOn && gConfigData.turnOffOnQuit) {
                Switch();
            }
            PostQuitMessage(0);
            break;
        default:
            break;
        }
        break;

    default:
        break;
    }
    return 0;
}


static void DialogInit(void)
{
    CStringW str;
    str.LoadStringW(IDS_AP_DIALOG_TITLT);

    gNid.cbSize = sizeof(NOTIFYICONDATA);
    gNid.hWnd = gDialog;
    gNid.uID = IDI_ICON;
    gNid.uFlags = NIF_ICON | NIF_MESSAGE | NIF_TIP;
    gNid.uCallbackMessage = WM_SHOWTASK;
    gNid.hIcon = LoadIcon(gInstance, MAKEINTRESOURCE(IDI_ICON));
    StrCpy(gNid.szTip, str);

    SendMessage(gDialog, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(GetModuleHandleW(NULL),
        MAKEINTRESOURCE(IDI_ICON)));
    SetWindowText(gDialog, str);

    str.LoadStringW(IDS_SSID_TITLE);
    SetWindowText(GetDlgItem(gDialog, IDC_SSID_TITLE), str);
    str.LoadStringW(IDS_PASSWORD_TITLE);
    SetWindowText(GetDlgItem(gDialog, IDC_PASSWORD_TITLE), str);

    HMODULE module = GetModuleHandle(NULL);
    HRSRC resource = FindResource(module, MAKEINTRESOURCE(IDR_CONFIG_FILE), L"BIN");
    HGLOBAL global = LoadResource(module, resource);
    int size = SizeofResource(module, resource);
    if (size == sizeof(gConfigData)) {
        memcpy(&gConfigData, LockResource(global), size);
    }
    SetWindowText(GetDlgItem(gDialog, IDC_SSID), gConfigData.ssid);
    SetWindowText(GetDlgItem(gDialog, IDC_PASSWORD), gConfigData.password);

    str.LoadStringW(gApOn ? IDS_SWITCH_OFF : IDS_SWITCH_ON);
    SetWindowText(GetDlgItem(gDialog, IDC_SWITCH), str);
    str.LoadStringW(IDS_CONFIG);
    SetWindowText(GetDlgItem(gDialog, IDC_CONFIG), str);
    str.LoadStringW(IDS_MINIMIZE);
    SetWindowText(GetDlgItem(gDialog, IDC_MINIMIZE), str);

    str.LoadStringW(IDS_INFO_PREFIX);
    str.AppendFormat(L"%d", gUserCount);
    SetWindowText(GetDlgItem(gDialog, IDC_INFO), str);
}

static void Switch(void)
{

}

static void Config(void)
{
    WCHAR selfFilename[PATH_SIZE];
    WCHAR tempPath[PATH_SIZE];
    WCHAR cmd[CMD_SIZE];
    GetModuleFileName(NULL, selfFilename, PATH_SIZE);
    GetTempPath(PATH_SIZE, tempPath);
    GetTempFileName(tempPath, L"AP_LITE", 0, cmd);
    PathRenameExtension(cmd, L".exe");

    if (CopyFile(selfFilename, cmd, false)) {
        StrCat(cmd, L" " CONFIG_OPTION " ");
        StrCat(cmd, selfFilename);

        STARTUPINFO si;
        ZeroMemory(&si, sizeof(si));
        si.cb = sizeof(si);
        PROCESS_INFORMATION pi;
        ZeroMemory(&pi, sizeof(pi));

        if (!CreateProcess(NULL, cmd,
            NULL, NULL, FALSE, 0, NULL, NULL, &si, &pi)) {
            return;
        }

        CloseHandle(pi.hProcess);
        CloseHandle(pi.hThread);
        PostQuitMessage(0);
    }
}

static void RightClickIcon(void)
{
    UINT uFlag = MF_BYPOSITION | MF_STRING;
    POINT clickPoint;
    GetCursorPos(&clickPoint);
    HMENU menu = CreatePopupMenu();
    CStringW str;

    str.LoadStringW(IDS_ABOUT);
    InsertMenu(menu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_MENU_ABOUT, str);

    str.LoadStringW(gApOn ? IDS_SWITCH_OFF : IDS_SWITCH_ON);
    InsertMenu(menu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_MENU_SWITCH, str);

    str.LoadStringW(IDS_SHOW);
    InsertMenu(menu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_MENU_SHOW, str);  

    str.LoadStringW(IDS_QUIT);
    InsertMenu(menu, 0xFFFFFFFF, MF_BYPOSITION | MF_STRING, ID_MENU_QUIT, str);

    SetForegroundWindow(gDialog);
    TrackPopupMenu(menu, TPM_LEFTALIGN | TPM_LEFTBUTTON | TPM_BOTTOMALIGN, 
        clickPoint.x, clickPoint.y, 0, gDialog, NULL);
}

static void ShowAbout(void)
{

}