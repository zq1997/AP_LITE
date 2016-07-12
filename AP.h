#pragma once

#include <vector>
using namespace std;

#include "common.h"
#include <NetCon.h>

#include <wlanapi.h>
#pragma comment(lib,"wlanapi.lib")

#define MAX_NUMBER_OF_PEERS 16
#define AP_DEVICE_NAME L"Microsoft Hosted Network Virtual Adapter"

struct  Connection
{
    INetSharingConfiguration * pNSC = NULL;
    NETCON_PROPERTIES* pNP = NULL; 
};

class AP
{
private:
    int mStatus;
    HANDLE mClient;

    Connection* mpAPConnection = NULL;
    vector<Connection> *mpOtherConnections = NULL;

    void scanConnections(bool targetIsAPConn);
    void releaseConnections(void);
    void setSharing(unsigned int which);

    void setConfig(void);

public:
    const static int STATUS_ON = 1;
    const static int STATUS_OFF = 0;
    const static int STATUS_ERROR = -1;

    AP(void);
    ~AP(void);

    int getStatus(void);
    const vector<Connection> *getOtherConnection(void);
    int getPeerNumber(void);
    void switchStatus(unsigned int which);
};


