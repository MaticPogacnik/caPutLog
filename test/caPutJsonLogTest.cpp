/*************************************************************************\
* Copyright (c) 2010 Brookhaven Science Associates, as Operator of
*     Brookhaven National Laboratory.
* Copyright (c) 2010 UChicago Argonne LLC, as Operator of Argonne
*     National Laboratory.
* EPICS BASE is distributed subject to a Software License Agreement found
* in file LICENSE that is included with this distribution.
\*************************************************************************/
/*
 *      Author: Michael Davidsaver
 *      Date:   2010-09-24
 */

// Standard library includes
#include <string>
#include <cstring>
// #include <stdio.h>
// #include <stdlib.h>
// #include <string.h>

#include<chrono>
#include<thread>


// #include "epicsAssert.h"
// #include "epicsThread.h"
// #include "epicsEvent.h"
// #include "dbDefs.h"
// #include "epicsUnitTest.h"
// #include "iocLog.h"
// #include "logClient.h"
// #include "osiSock.h"
#include "fdmgr.h"

// Epics base includes
#include <osiSock.h>
#include <envDefs.h>
#include <errlog.h>
#include <dbAccessDefs.h>
#include <dbUnitTest.h>
#include <testMain.h>

#include <dbEvent.h>
#include <dbServer.h>
#include <asDbLib.h>
#include <iocInit.h>
#include <cadef.h>

// This module includes
#include "caPutJsonLogTask.h"


static dbEventCtx testEvtCtx;

#define CA_SERVER_PORT "65535"
const char *server_port = CA_SERVER_PORT;

extern "C" {
    void dbTestIoc_registerRecordDeviceDriver(struct dbBase *);
}


/*******************************************************************************
* Logger server implementation
*******************************************************************************/
static void *pfdctx;
static SOCKET sock;
static SOCKET insock;
static std::string incLogMsg;

static void readFromClient(void *pParam)
{
    char recvbuf[1024];
    int recvLength;
    std::string

    memset(recvbuf, 0, 1024);
    recvLength = recv(insock, recvbuf, 1024, 0);
    if (recvLength > 0) {
        incLogMsg.append(recvbuf);

        if (incLogMsg.find('\n') != std::string::npos) {
            //TODO: we have whole msg, check if it matches
        }
    }
}


static void acceptNewClient ( void *pParam )
{
    osiSocklen_t addrSize;
    struct sockaddr_in addr;
    int status;

    addrSize = sizeof ( addr );
    insock = epicsSocketAccept ( sock, (struct sockaddr *)&addr, &addrSize );
    testOk(insock != INVALID_SOCKET && addrSize >= sizeof (addr),
        "Accepted new client");

    status = fdmgr_add_callback(pfdctx, insock, fdi_read,
        readFromClient,  NULL);

    testOk(status >= 0, "Client read configured");
}



void logServer(void) {

    int status;
    struct sockaddr_in serverAddr;
    struct sockaddr_in actualServerAddr;
    osiSocklen_t actualServerAddrSize;

    testDiag("Starting log server");

    // Create socket
    sock = epicsSocketCreate(AF_INET, SOCK_STREAM, 0);
    if (sock == INVALID_SOCKET) {
        testAbort("epicsSocketCreate failed.");
    }

    // We listen on a an available port.
    memset((void *)&serverAddr, 0, sizeof serverAddr);
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = htons(0);
    status = bind (sock, (struct sockaddr *)&serverAddr,
                   sizeof (serverAddr) );
    if (status < 0) {
        testAbort("bind failed; all ports in use?");
    }

    status = listen(sock, 10);
    if (status < 0) {
        testAbort("listen failed!");
    }

    // Determine the port that the OS chose
    actualServerAddrSize = sizeof actualServerAddr;
    memset((void *)&actualServerAddr, 0, sizeof serverAddr);
    status = getsockname(sock, (struct sockaddr *) &actualServerAddr,
         &actualServerAddrSize);
    if (status < 0) {
        testAbort("Can't find port number!");
    }

    pfdctx = (void *) fdmgr_init();
    if (status < 0) {
        testAbort("fdmgr_init failed!");
    }

    status = fdmgr_add_callback(pfdctx, sock, fdi_read,
        acceptNewClient, &serverAddr);

    if (status < 0) {
        testAbort("fdmgr_add_callback failed!");
    }



}



/*******************************************************************************
* CA helper functions
*******************************************************************************/
int writeVal(const char *pv, short type, int size, void * pValue) {

    chid pchid;
    int status;

    SEVCHK(ca_create_channel(pv, NULL, NULL, 0, &pchid), "Create channel failed");
    SEVCHK(ca_pend_io ( 1.0 ), "ca_pend_io");
    SEVCHK(ca_array_put(type, size, pchid, pValue), "caput error");
    SEVCHK(ca_pend_io ( 1.0 ), "ca_pend_io");
    SEVCHK(ca_clear_channel(pchid), "ca_clear_channel");
}

/*******************************************************************************
* Tests
*******************************************************************************/
void runTests(void *arg) {

    epicsThreadSleep(10);

    //Create CA context for the thread
    ca_context_create(ca_enable_preemptive_callback);

    //Run
    double v = 5677;
    writeVal("ao", DBR_DOUBLE, 1, &v);
    epicsThreadSleep(2);
    v = 11;
    writeVal("ao", DBR_DOUBLE, 1, &v);
    epicsThreadSleep(2);
    v = 88;
    writeVal("ao", DBR_DOUBLE, 1, &v);
    epicsThreadSleep(2);
    v = 99;
    writeVal("ao", DBR_DOUBLE, 1, &v);


    //Destroy test thread CA context
    ca_context_destroy();
}


/*******************************************************************************
* IOC helper functions
*******************************************************************************/
void startIoc() {
    eltc(0);
    if(iocBuild() || iocRun())
        testAbort("Failed to start up test database");
    if(!(testEvtCtx=db_init_events()))
        testAbort("Failed to initialize test dbEvent context");
    if(DB_EVENT_OK!=db_start_events(testEvtCtx, "CAS-test", NULL, NULL, epicsThreadPriorityCAServerLow))
        testAbort("Failed to start test dbEvent context");
    eltc(1);
}

void stopIoc(){
    testIocShutdownOk();
    //TODO dbStopSeQrvers();
}

/*******************************************************************************
* Main function
*******************************************************************************/
MAIN(caPutJsonLogTests)
{

    testPlan(0);

    // Prepare test IOC
    // epicsEnvSet("EPICS_CA_SERVER_PORT", server_port);
    testDiag("Starting test IOC");
    testdbPrepare();
    testdbReadDatabase("dbTestIoc.dbd", NULL, NULL);
    dbTestIoc_registerRecordDeviceDriver(pdbbase);
    testdbReadDatabase("caPutJsonLogTest.db", NULL, NULL);
    asSetFilename("asg.cfg");
    startIoc();

    CaPutJsonLogTask *logger =  CaPutJsonLogTask::getInstance();
    if (logger == nullptr) testAbort("Failed to initialize logger.");
    logger->initialize("192.168.56.1:9001", caPutJsonLogAll);

    epicsThreadSleep(1);
    testDiag("Test IOC ready");


    testDiag("Starting test thread");
    epicsThreadOpts opts = EPICS_THREAD_OPTS_INIT;
    opts.stackSize = epicsThreadGetStackSize(epicsThreadStackMedium);
    opts.priority = epicsThreadPriorityMedium;
    opts.joinable = 1;

    epicsThreadId threadId;
    const char * threadName = "caPutJsonLogTest";
    threadId = epicsThreadCreateOpt(threadName,
                                    (EPICSTHREADFUNC)
                                    runTests,
                                    NULL,
                                    &opts);

    epicsThreadMustJoin(threadId);
    testDiag("Test thread finished");

    // Keep ioc running for now
    epicsThreadSleep(100);

    testDiag("Done");
    return testDone();
}
