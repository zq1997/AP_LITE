#include "common.h"

int WINAPI AP_WinMain(HINSTANCE instance);
int WINAPI Config_WinMain(HINSTANCE instance, LPWSTR originFilename);

int WINAPI WinMain(HINSTANCE instance, HINSTANCE prevInstance, LPSTR cmd, int nShow)
{
    USES_CONVERSION;
    LPSTR arg;
    LPWSTR filename;
    if ((arg = strstr(cmd, CONFIG_OPTION)) != NULL) {
        arg += strlen(CONFIG_OPTION);
        filename = A2W(arg);
        PathRemoveBlanks(filename);
        Config_WinMain(instance, filename);
    } else if ((arg = strstr(cmd, CLEAN_OPTION)) != NULL) {
        arg += strlen(CLEAN_OPTION);
        filename = A2W(arg);
        PathRemoveBlanks(filename);
        DeleteFile(filename);
        PathRenameExtension(filename, L".tmp");
        DeleteFile(filename);
        AP_WinMain(instance);
    } else {
        AP_WinMain(instance);
    }
}