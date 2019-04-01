/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2017-2019 Fraunhofer IOSB (Author: Andreas Ebner)
 * Copyright (c) 2018-2019 Kalycito Infotech Private Limited
 */

#include <time.h>
#include <inttypes.h>
#include "ua_pubsub_ns0.h"
#include "ua_pubsub_manager.h"
#include "ua_log_stdout.h"

#ifdef UA_ENABLE_PUBSUB
/* conditional compilation */
#ifdef UA_ENABLE_PUBSUB_CUSTOM_PUBLISH_INTERRUPT

#define                      MAX_MEASUREMENTS         10000000
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
UA_Int64                pubIntervalNs;

/* Array to store published counter data */
extern UA_UInt32                    measurementCycles;
extern size_t                       publisherMeasurementsCounter;
extern UA_Int64                     currentPublishCycleTime[MAX_MEASUREMENTS];
extern UA_UInt64                    publishCounterValue[MAX_MEASUREMENTS];
extern struct timespec              calculatedCycleStartTime[MAX_MEASUREMENTS];
extern struct timespec              realCycleStartTime[MAX_MEASUREMENTS];
extern struct timespec              publishFinishedTimestamp[MAX_MEASUREMENTS];
extern FILE*                        fpPublisher;
char*                               filePublishedData;

/**
 * **Nanosecond field handling**
 *
 * The nanoSecondFieldConversion function is used to adjust the nanosecond field value.
 */
static void nanoSecondFieldConversion(struct timespec *timeSpecValue)
{
    /* Check if ns field is greater than '1 ns less than 1sec' */
    while (timeSpecValue->tv_nsec > (SECONDS_AS_NANO_SECONDS - 1))
    {
        /* Move to next second and remove it from ns field */
        timeSpecValue->tv_sec                     += 1;
        timeSpecValue->tv_nsec                    -= SECONDS_AS_NANO_SECONDS;
    }
}

/* Singal handler */
static void handler(int sig, siginfo_t* si, void* uc)
{
    if (si->si_value.sival_ptr != &pubEventTimer)
    {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "stray signal");
        return;
    } else {
        if(publisherMeasurementsCounter < measurementCycles){
            //save current configured publish interval
            currentPublishCycleTime[publisherMeasurementsCounter] = pubIntervalNs;
            //save the calculated starting time
            if(publisherMeasurementsCounter != 0) {
                calculatedCycleStartTime[publisherMeasurementsCounter].tv_nsec = 
                        calculatedCycleStartTime[publisherMeasurementsCounter - 1].tv_nsec + pubIntervalNs;
                calculatedCycleStartTime[publisherMeasurementsCounter].tv_sec = (long int)
                        calculatedCycleStartTime[publisherMeasurementsCounter - 1].tv_sec;
                nanoSecondFieldConversion(&calculatedCycleStartTime[publisherMeasurementsCounter]);
            }
            //save publish start time
            clock_gettime(CLOCKID, &realCycleStartTime[publisherMeasurementsCounter]);

            /*if(publisherMeasurementsCounter != 0) {
                calculatedCycleStartTime[publisherMeasurementsCounter].tv_nsec =
                        realCycleStartTime[publisherMeasurementsCounter-1].tv_nsec + pubIntervalNs;
                calculatedCycleStartTime[publisherMeasurementsCounter].tv_sec =
                        realCycleStartTime[publisherMeasurementsCounter-1].tv_sec;
                nanoSecondFieldConversion(&calculatedCycleStartTime[publisherMeasurementsCounter]);
            }*/
        }
        pubCallback(pubServer, pubData);
        if(publisherMeasurementsCounter < measurementCycles) {
            //save publish finished time
            clock_gettime(CLOCKID, &publishFinishedTimestamp[publisherMeasurementsCounter]);
            //value is currently set to the counter variable
            publishCounterValue[publisherMeasurementsCounter] = publisherMeasurementsCounter;
            publisherMeasurementsCounter++;
        } else if (publisherMeasurementsCounter == measurementCycles){
            fpPublisher                          = fopen(filePublishedData, "w+");
            for (size_t pubLoopVariable = 0; pubLoopVariable < publisherMeasurementsCounter; pubLoopVariable++)
            {
                fprintf(fpPublisher, "%" PRIu64",%" PRIu64 ",%ld.%09ld,%ld.%09ld,%ld.%09ld\n",
                        publishCounterValue[pubLoopVariable],
                        currentPublishCycleTime[pubLoopVariable],
                        calculatedCycleStartTime[pubLoopVariable].tv_sec,
                        calculatedCycleStartTime[pubLoopVariable].tv_nsec,
                        realCycleStartTime[pubLoopVariable].tv_sec,
                        realCycleStartTime[pubLoopVariable].tv_nsec,
                        publishFinishedTimestamp[pubLoopVariable].tv_sec,
                        publishFinishedTimestamp[pubLoopVariable].tv_nsec);
            }
            publisherMeasurementsCounter++;
            fclose(fpPublisher);
        }
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
    pubIntervalNs                       = (UA_Int64) (interval_ms * MILLI_AS_NANO_SECONDS);
    publisherMeasurementsCounter = 0;
    UA_Variant cycles;
    UA_StatusCode ret = UA_Server_readValue(server, UA_NODEID_STRING(1, "cycles"), &cycles);
    if(ret == UA_STATUSCODE_GOOD){
        measurementCycles = *((UA_UInt32 *) cycles.data);
    }

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
    clock_gettime(CLOCKID, &calculatedCycleStartTime[publisherMeasurementsCounter]);
    calculatedCycleStartTime[publisherMeasurementsCounter].tv_nsec =
            (long int) (calculatedCycleStartTime[publisherMeasurementsCounter].tv_nsec + pubIntervalNs);
    nanoSecondFieldConversion(&calculatedCycleStartTime[publisherMeasurementsCounter]);

    if (resultTimerCreate != 0)
    {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                       "Failed to arm the system timer");
        timer_delete(pubEventTimer);
        return UA_STATUSCODE_BADINTERNALERROR;
    }
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
    publisherMeasurementsCounter = 0;
    UA_Variant cycles;
    UA_StatusCode ret = UA_Server_readValue(server, UA_NODEID_STRING(1, "cycles"), &cycles);
    if(ret == UA_STATUSCODE_GOOD){
        measurementCycles = *((UA_UInt32 *) cycles.data);
    }

    pubIntervalNs                       = (UA_Int64) (interval_ms * MILLI_AS_NANO_SECONDS);
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
