#include "AP.h"

AP::AP(void)
{
    this->status = STATUS_OFF;
    DWORD version = 0;
    DWORD result = 0;

    result = WlanOpenHandle(WLAN_API_VERSION, NULL, &version, &client);
    if (ERROR_SUCCESS != result) {
        status = STATUS_ERROR;
        return;
    }

    BOOL allow = true;
    WLAN_HOSTED_NETWORK_REASON failedReason;
    result = WlanHostedNetworkSetProperty(client,
        wlan_hosted_network_opcode_enable,
        sizeof(BOOL),
        &allow,
        &failedReason,
        NULL);
    if (ERROR_SUCCESS != result) {
        status = STATUS_ERROR;
        return;
    }

    setSSID();
    setKEY();
    allowHostedNetWork();
    initConnections();
}

AP::~AP(void)
{
    if (status = STATUS_ON) {
        stopHostedNetWork();
        status = status == STATUS_ERROR ? STATUS_ERROR : STATUS_OFF;
    }

    releaseConnections();
    disallowHostedNetWork();

    BOOL allow = false;
    WLAN_HOSTED_NETWORK_REASON failedReason;
    WlanHostedNetworkSetProperty(client,
        wlan_hosted_network_opcode_enable,
        sizeof(BOOL),
        &allow,
        &failedReason,
        NULL);

    WlanCloseHandle(client, NULL);

    delete pOtherConnections;
}

void AP::initConnections(void)
{
    this->pOtherConnections = new vector<Connection>;
    if (status == STATUS_ERROR) {
        return;
    }

    CoInitialize(NULL);
    CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_PKT,
        RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);

    INetSharingManager * pNSM = NULL;
    HRESULT hr = ::CoCreateInstance(__uuidof(NetSharingManager), NULL, CLSCTX_ALL,
        __uuidof(INetSharingManager), (void**)&pNSM);
    if (hr != S_OK || !pNSM) {
        status = STATUS_ERROR;
    } else {

        INetSharingEveryConnectionCollection * pNSECC = NULL;
        hr = pNSM->get_EnumEveryConnection(&pNSECC);
        if (hr != S_OK || !pNSECC) {
            status = STATUS_ERROR;
        } else {

            IUnknown * pUnk = NULL;
            hr = pNSECC->get__NewEnum(&pUnk);
            if (hr != S_OK || !pUnk) {
                status = STATUS_ERROR;
            } else {

                IEnumVARIANT *pEV = NULL;
                hr = pUnk->QueryInterface(__uuidof(IEnumVARIANT), (void**)&pEV);
                if (hr != S_OK || !pEV) {
                    status = STATUS_ERROR;
                } else {

                    VARIANT v;
                    VariantInit(&v);
                    while (S_OK == pEV->Next(1, &v, NULL)) {
                        if (V_VT(&v) == VT_UNKNOWN) {
                            INetConnection *pNC = NULL;
                            V_UNKNOWN(&v)->QueryInterface(__uuidof(INetConnection), (void**)&pNC);
                            if (!pNC) {
                                status = STATUS_ERROR;
                            } else {

                                INetSharingConfiguration * pNSC = NULL;
                                hr = pNSM->get_INetSharingConfigurationForINetConnection(pNC, &pNSC);
                                if (hr != S_OK || !pNSC) {
                                    status = STATUS_ERROR;
                                } else {

                                    NETCON_PROPERTIES* pNP = NULL;
                                    hr = pNC->GetProperties(&pNP);
                                    if (hr != S_OK || !pNP) {
                                        status = STATUS_ERROR;
                                    } else if (pNP->Status == NCS_CONNECTED) {

                                        Connection conn;
                                        conn.pNC = pNC;
                                        conn.pNSC = pNSC;
                                        conn.pNP = pNP;
                                        if (StrCmpW(pNP->pszwDeviceName, AP_DEVICE_NAME)) {
                                            pOtherConnections->push_back(conn);
                                        } else {
                                            pAPConnection = new Connection;
                                            *pAPConnection = conn;
                                        }
                                    }
                                }
                            }
                        }
                    }
                    VariantClear(&v);
                    pEV->Release();
                }
                pUnk->Release();
            }
            pNSECC->Release();
        }
        pNSM->Release();
    }

    if (pAPConnection == NULL) {
        status = STATUS_ERROR;
    }
}

void AP::releaseConnections(void)
{
    vector<Connection>::iterator it = pOtherConnections->begin();
    for (; it != pOtherConnections->end(); ++it) {
        it->pNSC->Release();
        it->pNC->Release();
    }
    pOtherConnections->clear();
    delete pOtherConnections;
    pOtherConnections = NULL;

    if (pAPConnection != NULL) {
        pAPConnection->pNSC->Release();
        pAPConnection->pNC->Release();
        delete pAPConnection;
        pAPConnection = NULL;
    }

    CoUninitialize();
}

void AP::setSharing(int which)
{
    if (status == STATUS_ERROR) {
        return;
    }
    Connection publicSharing, privateSharing;

    pOtherConnections->push_back(*pAPConnection);
    vector<Connection>::iterator it = pOtherConnections->begin();
    for (; it != pOtherConnections->end(); ++it) {
        VARIANT_BOOL isEnable = FALSE;
        if (S_OK != it->pNSC->get_SharingEnabled(&isEnable)) {
            status = STATUS_ERROR;
            return;
        }
        if (isEnable) {
            SHARINGCONNECTIONTYPE type;
            if (S_OK != it->pNSC->get_SharingConnectionType(&type)) {
                status = STATUS_ERROR;
                return;
            } else {
                if (type == ICSSHARINGTYPE_PUBLIC) {
                    publicSharing = *it;
                } else {
                    privateSharing = *it;
                }
            }
        }
    }
    pOtherConnections->pop_back();

    if ((*pOtherConnections)[which].pNP->guidId != publicSharing.pNP->guidId) {
        if (S_OK != publicSharing.pNSC->DisableSharing()) {
            status = STATUS_ERROR;
            return;
        }
        if (S_OK != (*pOtherConnections)[which].pNSC->EnableSharing(ICSSHARINGTYPE_PUBLIC)) {
            status = STATUS_ERROR;
            return;
        }
    }

    if (pAPConnection->pNP->guidId != privateSharing.pNP->guidId) {
        if (S_OK != privateSharing.pNSC->DisableSharing()) {
            status = STATUS_ERROR;
            return;
        }
        if (S_OK != pAPConnection->pNSC->EnableSharing(ICSSHARINGTYPE_PRIVATE)) {
            status = STATUS_ERROR;
            return;
        }
    }
}

void AP::allowHostedNetWork(void)
{
    if (status != STATUS_ERROR) {
        PWLAN_HOSTED_NETWORK_REASON pReason = NULL;
        if (WlanHostedNetworkForceStart(client, pReason, NULL) != ERROR_SUCCESS) {
            status = STATUS_ERROR;
        }
    }
}

void AP::disallowHostedNetWork(void)
{
    if (status != STATUS_ERROR) {
        PWLAN_HOSTED_NETWORK_REASON pReason = NULL;
        if (WlanHostedNetworkForceStop(client, pReason, NULL) != ERROR_SUCCESS) {
            status = STATUS_ERROR;
        }
    }
}

void AP::startHostedNetWork(void)
{
    if (status != STATUS_ERROR) {
        PWLAN_HOSTED_NETWORK_REASON pReason = NULL;
        if (WlanHostedNetworkStartUsing(client, pReason, NULL) != ERROR_SUCCESS) {
            status = STATUS_ERROR;
        }
    }
}

void AP::stopHostedNetWork(void)
{
    if (status != STATUS_ERROR) {
        PWLAN_HOSTED_NETWORK_REASON pReason = NULL;
        if (WlanHostedNetworkStopUsing(client, pReason, NULL) != ERROR_SUCCESS) {
            status = STATUS_ERROR;
        }
    }
}

int AP::getStatus(void)
{
    return status;
}

const vector<Connection> *AP::getOtherConnection(void)
{
    return pOtherConnections;
}

int AP::getPeerNumber(void)
{
    if (status == STATUS_ERROR) {
        return 0;
    }

    PWLAN_HOSTED_NETWORK_STATUS pWlanHostedNetworkStatus = NULL;
    int result = WlanHostedNetworkQueryStatus(client, &pWlanHostedNetworkStatus, NULL);
    if (result != ERROR_SUCCESS ||
        pWlanHostedNetworkStatus->HostedNetworkState == wlan_hosted_network_unavailable) {
        return 0;
    }
    return pWlanHostedNetworkStatus->dwNumberOfPeers;
}

void AP::setSSID(void)
{
    if (status != STATUS_ERROR) {
        USES_CONVERSION;
        PCHAR name = W2A(gConfigData.ssid);

        DWORD result = 0;
        WLAN_HOSTED_NETWORK_REASON failedReason;
        WLAN_HOSTED_NETWORK_CONNECTION_SETTINGS settings;
        memset(&settings, 0, sizeof(WLAN_HOSTED_NETWORK_CONNECTION_SETTINGS));

        memcpy(settings.hostedNetworkSSID.ucSSID, name, strlen(name));
        settings.hostedNetworkSSID.uSSIDLength = strlen((char*)settings.hostedNetworkSSID.ucSSID);
        settings.dwMaxNumberOfPeers = MAX_NUMBER_OF_PEERS;

        result = WlanHostedNetworkSetProperty(client,
            wlan_hosted_network_opcode_connection_settings,
            sizeof(WLAN_HOSTED_NETWORK_CONNECTION_SETTINGS),
            &settings,
            &failedReason,
            NULL);
        if (ERROR_SUCCESS != result) {
            status = STATUS_ERROR;
        }
    }
}

void AP::setKEY(void)
{
    if (status != STATUS_ERROR) {
        USES_CONVERSION;
        PCHAR key = W2A(gConfigData.key);

        WLAN_HOSTED_NETWORK_REASON failedReason;
        DWORD result = 0;
        result = WlanHostedNetworkSetSecondaryKey(client,
            strlen(key) + 1,
            (PUCHAR)key,
            TRUE,
            FALSE,
            &failedReason,
            NULL);
        if (ERROR_SUCCESS != result) {
            status = STATUS_ERROR;
        }
    }
}

void AP::switchStatus(int which)
{
    if (status == STATUS_OFF) {
        setSharing(which);
        startHostedNetWork();
        status = status == STATUS_ERROR ? STATUS_ERROR : STATUS_ON;
    } else if (status == STATUS_ON) {
        stopHostedNetWork();
        status = status == STATUS_ERROR ? STATUS_ERROR : STATUS_OFF;
    }
}
