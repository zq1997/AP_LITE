#include "common.h" 

static INT_PTR CALLBACK DlgProc(HWND dlg, UINT msg, WPARAM wParam, LPARAM lParam);
static void DialogInit(void);
static void Quit(void);
static void Submit(void);

static ConfigData gConfigData;
static HWND gDialog;

LPWSTR gOriginFilename;

int WINAPI Config_WinMain(HINSTANCE instance, LPWSTR _originFilename)
{
    gOriginFilename = _originFilename;
    HWND gDialog = CreateDialog(instance,
        MAKEINTRESOURCE(IDD_CONFIG), GetDesktopWindow(), (DLGPROC)DlgProc);
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
    switch (msg) {
    case WM_INITDIALOG:
        DialogInit();
        break;
    case WM_SYSCOMMAND:
        if (wParam == SC_CLOSE) {
            Quit();
        }
        break;
    case WM_COMMAND:
        if (LOWORD(wParam) == IDC_SUBMIT) {
            Submit();
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

    SendMessage(gDialog, WM_SETICON, ICON_BIG, (LPARAM)LoadIcon(GetModuleHandleW(NULL),
        MAKEINTRESOURCE(IDI_ICON)));
    str.LoadStringW(IDS_CONFIG_DIALOG_TITLE);
    SetWindowText(gDialog, str);

    str.LoadStringW(IDS_SSID_TITLE);
    SetWindowText(GetDlgItem(gDialog, IDC_NEW_SSID_TITLE), str);
    str.LoadStringW(IDS_KEY_TITLE);
    SetWindowText(GetDlgItem(gDialog, IDC_NEW_KEY_TITLE), str);

    HMODULE module = GetModuleHandle(NULL);
    HRSRC resource = FindResource(module, MAKEINTRESOURCE(IDR_CONFIG_FILE), RT_RCDATA);
    HGLOBAL global = LoadResource(module, resource);
    int size = SizeofResource(module, resource);
    if (size == sizeof(gConfigData)) {
        memcpy(&gConfigData, LockResource(global), size);
    }
    SetWindowText(GetDlgItem(gDialog, IDC_NEW_SSID), gConfigData.ssid);
    SetWindowText(GetDlgItem(gDialog, IDC_NEW_KEY), gConfigData.key);

    CheckDlgButton(gDialog, IDC_ASK_BEFORE_QUIT,
        gConfigData.askBeforeQuit ? BST_CHECKED : BST_UNCHECKED);
    str.LoadStringW(IDS_ASK_BEFORE_QUIT);
    SetWindowText(GetDlgItem(gDialog, IDC_ASK_BEFORE_QUIT), str);

    str.LoadStringW(IDS_SUBMIT);
    SetWindowText(GetDlgItem(gDialog, IDC_SUBMIT), str);
}


static void Quit(void)
{
    WCHAR selfFilename[PATH_SIZE];
    WCHAR cmd[CMD_SIZE];
    GetModuleFileName(NULL, selfFilename, PATH_SIZE);
    StrCpy(cmd, gOriginFilename);
    StrCat(cmd, L" " CLEAN_OPTION " ");
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

static void Submit(void)
{
    GetWindowText(GetDlgItem(gDialog, IDC_NEW_SSID), gConfigData.ssid, SSID_SIZE);
    GetWindowText(GetDlgItem(gDialog, IDC_NEW_KEY), gConfigData.key, KEY_SIZE);
    gConfigData.askBeforeQuit = IsDlgButtonChecked(gDialog, IDC_ASK_BEFORE_QUIT) == BST_CHECKED;

    HANDLE updateRes = BeginUpdateResource(gOriginFilename, FALSE);
    CStringW str;
    str.LoadStringW(IDS_OUTPUT_FAIL);

    if (updateRes == NULL) {
        SetWindowText(GetDlgItem(gDialog, IDC_OUTPUT), str);
        return;
    }

    if (!UpdateResource(updateRes, RT_RCDATA, MAKEINTRESOURCE(IDR_CONFIG_FILE),
        MAKELANGID(LANG_NEUTRAL, SUBLANG_NEUTRAL), &gConfigData, sizeof gConfigData)) {
        SetWindowText(GetDlgItem(gDialog, IDC_OUTPUT), str);
        return;
    }

    if (!EndUpdateResource(updateRes, FALSE)) {
        SetWindowText(GetDlgItem(gDialog, IDC_OUTPUT), str);
        return;
    }

    str.LoadStringW(IDS_OUTPUT_SUCCESS);
    SetWindowText(GetDlgItem(gDialog, IDC_OUTPUT), str);
}