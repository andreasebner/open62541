/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2017-2019 Fraunhofer IOSB (Author: Andreas Ebner)
 * Copyright (c) 2018-2019 Kalycito Infotech Private Limited
 */

#include <time.h>
#include "ua_pubsub_ns0.h"
#include "ua_pubsub_manager.h"
#include "ua_log_stdout.h"

#ifdef UA_ENABLE_PUBSUB
/* conditional compilation */
#ifdef UA_ENABLE_PUBSUB_CUSTOM_PUBLISH_INTERRUPT

#define                      MICRO_AS_NANO_SECONDS    1000
#define                      MILLI_AS_NANO_SECONDS    (1000 * 1000)
#define                      SECONDS_AS_NANO_SECONDS  (1000 * 1000 * 1000)
#define                      CLOCKID                  CLOCK_REALTIME
#define                      SIG                      SIGUSR1

/* For one publish callback only... */
UA_Server*              pubServer;
void*                   pubData;
struct sigevent         pubEvent;
timer_t                 pubEventTimer;
struct sigaction        signalAction;
UA_ServerCallback       pubCallback;

//extern struct timespec  dataModificationTime;

/* Singal handler */
static void handler(int sig, siginfo_t* si, void* uc)
{
    if (si->si_value.sival_ptr != &pubEventTimer)
    {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "stray signal");
        return;
    } else {
        //TODO make the write IRQ save
        //UA_NodeId currentNodeIdPublisher;
        //currentNodeIdPublisher = UA_NODEID_STRING(1, "PublisherCounter");
//        clock_gettime(CLOCKID, &dataModificationTime);
        //UA_Variant_setScalar(&countPointerPublisher, &counterDataSubscriber,
        //                     &UA_TYPES[UA_TYPES_UINT64]);
        //UA_Server_writeValue(pubServer, currentNodeIdPublisher,
        //                     countPointerPublisher);
        pubCallback(pubServer, pubData);
        //if (counterDataSubscriber > COUNTER_ZERO)
        //{
        //updateMeasurementsPublisher(dataModificationTime, counterDataSubscriber);
        //}
    }
}

/* Add a callback for cyclic repetition */
UA_StatusCode
UA_PubSubManager_addRepeatedCallback(UA_Server *server,
                                     UA_ServerCallback callback,
                                     void *data, UA_Double interval_ms,
                                     UA_UInt64 *callbackId)
{
    memset(&signalAction, 0, sizeof(signalAction));
    memset(&pubEventTimer, 0, sizeof(pubEventTimer));
    /* Arm the timer */
    struct itimerspec timerspec;
    int               resultTimerCreate = 0;
    pubServer                           = server;
    pubCallback                         = callback;
    pubData                             = data;
    UA_Int64 pubIntervalNs              = (UA_Int64) (interval_ms * MILLI_AS_NANO_SECONDS);
    //pubIntervalNs                     = interval * MILLI_SECONDS;

    /* Handle the signal */
    signalAction.sa_flags               = SA_SIGINFO;
    signalAction.sa_sigaction           = handler;
    sigemptyset(&signalAction.sa_mask);
    sigaction(SIG, &signalAction, NULL);

    /* Create the event */
    pubEvent.sigev_notify               = SIGEV_SIGNAL;
    pubEvent.sigev_signo                = SIG;
    pubEvent.sigev_value.sival_ptr      = &pubEventTimer;
    resultTimerCreate                   = timer_create(CLOCKID, &pubEvent,
                                                       &pubEventTimer);
    if (resultTimerCreate != 0)
    {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                       "Failed to create a system event");
        return UA_STATUSCODE_BADINTERNALERROR;
    }

    timerspec.it_interval.tv_sec        = (long int) (pubIntervalNs / (SECONDS_AS_NANO_SECONDS));
    timerspec.it_interval.tv_nsec       = (long int) (pubIntervalNs % SECONDS_AS_NANO_SECONDS);
    timerspec.it_value.tv_sec           = (long int) (pubIntervalNs / (SECONDS_AS_NANO_SECONDS));
    timerspec.it_value.tv_nsec          = (long int) (pubIntervalNs % SECONDS_AS_NANO_SECONDS);
    resultTimerCreate                   = timer_settime(pubEventTimer, 0, &timerspec, NULL);
    if (resultTimerCreate != 0)
    {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                       "Failed to arm the system timer");
        timer_delete(pubEventTimer);
        return UA_STATUSCODE_BADINTERNALERROR;
    }

    //clock_gettime(CLOCKID, &pubStartTime);
    return UA_STATUSCODE_GOOD;
}

/* Modify the interval of the callback for cyclic repetition */
UA_StatusCode
UA_PubSubManager_changeRepeatedCallbackInterval(UA_Server *server,
                                                UA_UInt64 callbackId,
                                                UA_Double interval_ms)
{
    struct itimerspec timerspec;
    int               resultTimerCreate = 0;
    UA_Int64 pubIntervalNs              = (UA_Int64) (interval_ms * MILLI_AS_NANO_SECONDS);
    timerspec.it_interval.tv_sec        = (long int) (pubIntervalNs % SECONDS_AS_NANO_SECONDS);
    timerspec.it_interval.tv_nsec       = (long int) (pubIntervalNs % SECONDS_AS_NANO_SECONDS);
    timerspec.it_value.tv_sec           = (long int) (pubIntervalNs / (SECONDS_AS_NANO_SECONDS));
    timerspec.it_value.tv_nsec          = (long int) (pubIntervalNs % SECONDS_AS_NANO_SECONDS);
    resultTimerCreate                   = timer_settime(pubEventTimer, 0, &timerspec, NULL);
    if (resultTimerCreate != 0)
    {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                       "Failed to arm the system timer");
        timer_delete(pubEventTimer);
        return UA_STATUSCODE_BADINTERNALERROR;
    }

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Changed the publish callback to interval %lf", interval_ms);
    return UA_STATUSCODE_GOOD;
}

/* Remove the callback added for cyclic repetition */
void
UA_PubSubManager_removeRepeatedPubSubCallback(UA_Server *server, UA_UInt64 callbackId)
{
    timer_delete(pubEventTimer);
}

#endif
#endif /* UA_ENABLE_PUBSUB */
