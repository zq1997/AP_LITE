#pragma once
#include <windows.h> 
#include <atlstr.h> 
#include "resource.h"

#define PATH_SIZE (MAX_PATH + 1)
#define CMD_SIZE (MAX_PATH * 2 + 32 + 1)
#define SSID_SIZE 64
#define KEY_SIZE 64
#define SSID_MIN_LEN 1
#define KEY_MIN_LEN 8

#define CONFIG_OPTION "-- config"
#define CLEAN_OPTION "-- clean"

#define WM_SHOWTASK (WM_USER +1)

struct ConfigData {
    WCHAR ssid[SSID_SIZE] = L"AP_LITE";
    WCHAR key[KEY_SIZE] = L"AsyncCode";
    bool askBeforeQuit = true;
};

extern ConfigData gConfigData;
