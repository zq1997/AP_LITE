#pragma once

#include <vector>
using namespace std;

#include "common.h"
#include <wlanapi.h>
#include <NetCon.h>
#pragma comment(lib,"wlanapi.lib")

#define MAX_NUMBER_OF_PEERS 16
#define AP_DEVICE_NAME L"Microsoft Hosted Network Virtual Adapter"

struct  Connection
{
    INetConnection * pNC = NULL;
    INetSharingConfiguration * pNSC = NULL;
    NETCON_PROPERTIES* pNP = NULL; 
};

class AP
{
private:
    int status;
    HANDLE client;

    Connection* pAPConnection = NULL;
    vector<Connection> *pOtherConnections = NULL;

    void initConnections(void);
    void releaseConnections(void);
    void setSharing(int which);

    void allowHostedNetWork(void);
    void disallowHostedNetWork(void);
    void startHostedNetWork(void);
    void stopHostedNetWork(void);
    void setSSID(void);
    void setKEY(void);

public:
    const static int STATUS_ON = 1;
    const static int STATUS_OFF = 0;
    const static int STATUS_ERROR = -1;

    AP(void);
    ~AP(void);

    int getStatus(void);
    const vector<Connection> *getOtherConnection(void);
    int getPeerNumber(void);
    void switchStatus(int which);
};


