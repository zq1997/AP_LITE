#include "AP.h"

AP::AP(void)
{
    DWORD version = 0;
    int result = WlanOpenHandle(WLAN_API_VERSION, NULL, &version, &mClient);
    if (ERROR_SUCCESS != result) {
        mStatus = STATUS_ERROR;
        return;
    }

    BOOL allow = true;
    result = WlanHostedNetworkSetProperty(mClient,
        wlan_hosted_network_opcode_enable,
        sizeof(BOOL),
        &allow,
        NULL,
        NULL);
    if (ERROR_SUCCESS != result) {
        mStatus = STATUS_ERROR;
        return;
    }

    PWLAN_HOSTED_NETWORK_STATUS pWlanHostedNetworkStatus = NULL;
    result = WlanHostedNetworkQueryStatus(mClient, &pWlanHostedNetworkStatus, NULL);
    if (result == ERROR_SUCCESS ||
        pWlanHostedNetworkStatus->HostedNetworkState != wlan_hosted_network_unavailable) {
        this->mStatus = pWlanHostedNetworkStatus->HostedNetworkState == wlan_hosted_network_active ?
            STATUS_ON : STATUS_OFF;
    } else {
        this->mStatus = STATUS_ERROR;
        return;
    }

    setConfig();

    CoInitialize(NULL);
    CoInitializeSecurity(NULL, -1, NULL, NULL, RPC_C_AUTHN_LEVEL_PKT,
        RPC_C_IMP_LEVEL_IMPERSONATE, NULL, EOAC_NONE, NULL);
    this->mpOtherConnections = new vector<Connection>;
    scanConnections(false);
}

AP::~AP(void)
{
    PWLAN_HOSTED_NETWORK_STATUS pWlanHostedNetworkStatus = NULL;
    int result = WlanHostedNetworkQueryStatus(mClient, &pWlanHostedNetworkStatus, NULL);
    if (result == ERROR_SUCCESS ||
        pWlanHostedNetworkStatus->HostedNetworkState == wlan_hosted_network_active) {
        WlanHostedNetworkForceStop(mClient, NULL, NULL);
    }

    releaseConnections();
    CoUninitialize();

    BOOL allow = false;
    WlanHostedNetworkSetProperty(mClient,
        wlan_hosted_network_opcode_enable,
        sizeof(BOOL),
        &allow,
        NULL,
        NULL);

    WlanCloseHandle(mClient, NULL);
}

void AP::scanConnections(bool targetIsAPConn)
{
    if (mStatus == STATUS_ERROR) {
        return;
    }

    INetSharingManager * pNSM = NULL;
    HRESULT hr = ::CoCreateInstance(__uuidof(NetSharingManager), NULL, CLSCTX_ALL,
        __uuidof(INetSharingManager), (void**)&pNSM);
    if (hr != S_OK || !pNSM) {
        mStatus = STATUS_ERROR;
    } else {

        INetSharingEveryConnectionCollection * pNSECC = NULL;
        hr = pNSM->get_EnumEveryConnection(&pNSECC);
        if (hr != S_OK || !pNSECC) {
            mStatus = STATUS_ERROR;
        } else {

            IUnknown * pUnk = NULL;
            hr = pNSECC->get__NewEnum(&pUnk);
            if (hr != S_OK || !pUnk) {
                mStatus = STATUS_ERROR;
            } else {

                IEnumVARIANT *pEV = NULL;
                hr = pUnk->QueryInterface(__uuidof(IEnumVARIANT), (void**)&pEV);
                if (hr != S_OK || !pEV) {
                    mStatus = STATUS_ERROR;
                } else {

                    VARIANT v;
                    VariantInit(&v);
                    while (S_OK == pEV->Next(1, &v, NULL)) {
                        if (V_VT(&v) == VT_UNKNOWN) {
                            INetConnection *pNC = NULL;
                            V_UNKNOWN(&v)->QueryInterface(__uuidof(INetConnection), (void**)&pNC);
                            if (!pNC) {
                                mStatus = STATUS_ERROR;
                            } else {

                                INetSharingConfiguration * pNSC = NULL;
                                hr = pNSM->get_INetSharingConfigurationForINetConnection(pNC, &pNSC);
                                if (hr != S_OK || !pNSC) {
                                    mStatus = STATUS_ERROR;
                                } else {

                                    NETCON_PROPERTIES* pNP = NULL;
                                    hr = pNC->GetProperties(&pNP);
                                    if (hr != S_OK || !pNP) {
                                        mStatus = STATUS_ERROR;
                                    } else {

                                        Connection conn;
                                        conn.pNSC = pNSC;
                                        conn.pNP = pNP;
                                        if (StrCmpW(pNP->pszwDeviceName, AP_DEVICE_NAME)) {
                                            if (!targetIsAPConn) {
                                                mpOtherConnections->push_back(conn);
                                            }
                                        } else {
                                            if (targetIsAPConn) {
                                                mpAPConnection = new Connection(conn);
                                            }
                                        }
                                        pNC->Release();
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

}

void AP::releaseConnections(void)
{
    vector<Connection>::iterator it = mpOtherConnections->begin();
    for (; it != mpOtherConnections->end(); ++it) {
        it->pNSC->Release();
    }
    mpOtherConnections->clear();
    delete mpOtherConnections;
    mpOtherConnections = NULL;

    if (mpAPConnection != NULL) {
        mpAPConnection->pNSC->Release();
        delete mpAPConnection;
        mpAPConnection = NULL;
    }
}

void AP::setSharing(unsigned int which)
{
    if (mStatus == STATUS_ERROR) {
        return;
    }
    Connection publicSharing, privateSharing;

    mpOtherConnections->push_back(*mpAPConnection);
    vector<Connection>::iterator it = mpOtherConnections->begin();
    for (; it != mpOtherConnections->end(); ++it) {
        VARIANT_BOOL isEnable = FALSE;

        if (S_OK != it->pNSC->get_SharingEnabled(&isEnable)) {
            mStatus = STATUS_ERROR;
            return;
        }
        if (isEnable) {
            SHARINGCONNECTIONTYPE type;
            if (S_OK != it->pNSC->get_SharingConnectionType(&type)) {
                mStatus = STATUS_ERROR;
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
    mpOtherConnections->pop_back();

    if (publicSharing.pNP == NULL ||
        (*mpOtherConnections)[which].pNP->guidId != publicSharing.pNP->guidId) {
        if (publicSharing.pNP != NULL && S_OK != publicSharing.pNSC->DisableSharing()) {
            mStatus = STATUS_ERROR;
            return;
        }
        if (S_OK != (*mpOtherConnections)[which].pNSC->EnableSharing(ICSSHARINGTYPE_PUBLIC)) {
            mStatus = STATUS_ERROR;
            return;
        }
    }

    if (privateSharing.pNP == NULL ||
        mpAPConnection->pNP->guidId != privateSharing.pNP->guidId) {
        if (privateSharing.pNP != NULL && S_OK != privateSharing.pNSC->DisableSharing()) {
            mStatus = STATUS_ERROR;
            return;
        }
        if (S_OK != mpAPConnection->pNSC->EnableSharing(ICSSHARINGTYPE_PRIVATE)) {
            mStatus = STATUS_ERROR;
            return;
        }
    }
}

int AP::getStatus(void)
{
    return mStatus;
}

const vector<Connection> *AP::getOtherConnections(void)
{
    return mpOtherConnections;
}

int AP::getPeerNumber(void)
{
    if (mStatus == STATUS_ERROR) {
        return 0;
    }

    PWLAN_HOSTED_NETWORK_STATUS pWlanHostedNetworkStatus = NULL;
    int result = WlanHostedNetworkQueryStatus(mClient, &pWlanHostedNetworkStatus, NULL);
    if (result != ERROR_SUCCESS ||
        pWlanHostedNetworkStatus->HostedNetworkState == wlan_hosted_network_unavailable) {
        return 0;
    }
    return pWlanHostedNetworkStatus->dwNumberOfPeers;
}

void AP::setConfig(void)
{
    if (mStatus == STATUS_ERROR) {
        return;
    }

    USES_CONVERSION;
    PCHAR ssid = W2A(gConfigData.ssid);
    PCHAR key = W2A(gConfigData.key);

    WLAN_HOSTED_NETWORK_CONNECTION_SETTINGS settings;
    memset(&settings, 0, sizeof(WLAN_HOSTED_NETWORK_CONNECTION_SETTINGS));

    memcpy(settings.hostedNetworkSSID.ucSSID, ssid, strlen(ssid));
    settings.hostedNetworkSSID.uSSIDLength = strlen((char*)settings.hostedNetworkSSID.ucSSID);
    settings.dwMaxNumberOfPeers = MAX_NUMBER_OF_PEERS;

    DWORD result = ERROR_SUCCESS;
    result = WlanHostedNetworkSetProperty(mClient,
        wlan_hosted_network_opcode_connection_settings,
        sizeof(WLAN_HOSTED_NETWORK_CONNECTION_SETTINGS),
        &settings,
        NULL,
        NULL);
    if (ERROR_SUCCESS != result) {
        mStatus = STATUS_ERROR;
        return;
    }

    result = WlanHostedNetworkSetSecondaryKey(mClient,
        strlen(key) + 1,
        (PUCHAR)key,
        TRUE,
        FALSE,
        NULL,
        NULL);
    if (ERROR_SUCCESS != result) {
        mStatus = STATUS_ERROR;
        return;
    }
}

void AP::switchStatus(unsigned int which)
{
    if (mStatus == STATUS_OFF) {
        mStatus = WlanHostedNetworkForceStart(mClient, NULL, NULL) == ERROR_SUCCESS ?
            STATUS_ON : STATUS_ERROR;
        if (mStatus == STATUS_ON && which >= 0 && which < mpOtherConnections->size()) {
            if (mpAPConnection == NULL) {
                scanConnections(true);
                if (mpAPConnection == NULL) {
                    mStatus = STATUS_ERROR;
                }
            }
            setSharing(which);
        }
    } else if (mStatus == STATUS_ON) {
        mStatus = WlanHostedNetworkForceStop(mClient, NULL, NULL) == ERROR_SUCCESS ?
            STATUS_OFF : STATUS_ERROR;
    }
}