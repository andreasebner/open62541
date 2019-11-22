/* This work is licensed under a Creative Commons CCZero 1.0 Universal License.
 * See http://creativecommons.org/publicdomain/zero/1.0/ for more information. */

#include <open62541/plugin/log_stdout.h>
#include <open62541/plugin/pubsub_udp.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>

#include <signal.h>
#include <stdlib.h>
#include <time.h>
#include <open62541/server_pubsub.h>
#include "bufmalloc.h"

/* possible options: PUBSUB_CONFIG_FASTPATH_NONE, PUBSUB_CONFIG_FASTPATH_FIXED_OFFSETS, PUBSUB_CONFIG_FASTPATH_STATIC_VALUES */
#define PUBSUB_CONFIG_FASTPATH_FIXED_OFFSETS
#define PUBSUB_CONFIG_PUBLISH_CYCLE_MS 100
#define PUBSUB_CONFIG_PUBLISH_CYCLES 100
#define PUBSUB_CONFIG_FIELD_COUNT 10

#define MAX_MEASUREMENTS         10000
#define MILLI_AS_NANO_SECONDS    (1000 * 1000)
#define SECONDS_AS_NANO_SECONDS  (1000 * 1000 * 1000)
#define CLOCKID                  CLOCK_MONOTONIC
#define SIG                      SIGUSR1
#define PUB_INTERVAL             0.25 /* Publish interval in milliseconds */
#define DATA_SET_WRITER_ID       62541
#define MEASUREMENT_OUTPUT       "publisher_measurement.csv"

/* Arrays to store measurement data */
UA_Int32 currentPublishCycleTime_RT_none[MAX_MEASUREMENTS+1];
struct timespec calculatedCycleStartTime_RT_none[MAX_MEASUREMENTS+1];
struct timespec cycleStartDelay_RT_none[MAX_MEASUREMENTS+1];
struct timespec cycleDuration_RT_none[MAX_MEASUREMENTS+1];

UA_Int32 currentPublishCycleTime_RT_static_source[MAX_MEASUREMENTS+1];
struct timespec calculatedCycleStartTime_RT_static_source[MAX_MEASUREMENTS+1];
struct timespec cycleStartDelay_RT_static_source[MAX_MEASUREMENTS+1];
struct timespec cycleDuration_RT_static_source[MAX_MEASUREMENTS+1];

UA_Int32 currentPublishCycleTime_RT_fixed_size[MAX_MEASUREMENTS+1];
struct timespec calculatedCycleStartTime_RT_fixed_size[MAX_MEASUREMENTS+1];
struct timespec cycleStartDelay_RT_fixed_size[MAX_MEASUREMENTS+1];
struct timespec cycleDuration_RT_fixed_size[MAX_MEASUREMENTS+1];
size_t publisherMeasurementsCounter  = 0;
enum rtMode{RT_NONE, RT_DIRECT_VALUE_ACCESS, RT_FIXED_OFFSETS} currentMeasurementMode;

/**
 * The PubSub RT level example points out the configuration of different PubSub RT-levels. These levels will be
 * used together with an RT capable OS for deterministic message generation. The main target is to reduce the time
 * spread and effort during the publish cycle. Most of the RT levels are based on pre-generated and buffered
 * DataSetMesseges and NetworkMessages. Since changes in the PubSub-configuration will invalidate the buffered
 * frames, the PubSub configuration must be frozen after the configuration phase.
 *
 * This example can be configured to compare and measure the different PubSub options by the following define options:
 * PUBSUB_CONFIG_FASTPATH_NONE -> The published fields are added to the information model and published in the default non-RT mode
 * PUBSUB_CONFIG_FASTPATH_STATIC_VALUES -> The published fields are not visible in the information model. The publish cycle is improved
 * by prevent the value lookup within the information model.
 * PUBSUB_CONFIG_FASTPATH_FIXED_OFFSETS -> The published fields are not visible in the information model. After the PubSub-configuration
 * freeze, the NetworkMessages and DataSetMessages will be calculated and buffered. During the publish cycle these buffers will only be updated.
 */

UA_NodeId publishedDataSetIdent, dataSetFieldIdent, writerGroupIdent, connectionIdentifier;
UA_UInt32 *valueStore[PUBSUB_CONFIG_FIELD_COUNT];

UA_Boolean running = true;
static void stopHandler(int sign) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "received ctrl-c");
    running = false;
}

/* The following PubSub configuration does not differ from the 'normal' configuration */
static void
addMinimalPubSubConfiguration(UA_Server * server){
    /* Add one PubSubConnection */
    UA_PubSubConnectionConfig connectionConfig;
    memset(&connectionConfig, 0, sizeof(connectionConfig));
    connectionConfig.name = UA_STRING("UDP-UADP Connection 1");
    connectionConfig.transportProfileUri = UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-udp-uadp");
    connectionConfig.enabled = UA_TRUE;
    UA_NetworkAddressUrlDataType networkAddressUrl = {UA_STRING_NULL , UA_STRING("opc.udp://224.0.0.22:4840/")};
    UA_Variant_setScalar(&connectionConfig.address, &networkAddressUrl, &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE]);
    connectionConfig.publisherId.numeric = UA_UInt32_random();
    UA_Server_addPubSubConnection(server, &connectionConfig, &connectionIdentifier);
    /* Add one PublishedDataSet */
    UA_PublishedDataSetConfig publishedDataSetConfig;
    memset(&publishedDataSetConfig, 0, sizeof(UA_PublishedDataSetConfig));
    publishedDataSetConfig.publishedDataSetType = UA_PUBSUB_DATASET_PUBLISHEDITEMS;
    publishedDataSetConfig.name = UA_STRING("Demo PDS");
    /* Add one DataSetField to the PDS */
    UA_Server_addPublishedDataSet(server, &publishedDataSetConfig, &publishedDataSetIdent);
}

/* fields for the realtime publishing */
UA_ServerCallback pubCallback = NULL; /* Sentinel if a timer is active */
timer_t pubEventTimer;
struct sigevent pubEvent;
struct sigaction signalAction;
UA_Server *measurementServer;
void *pubData;
UA_Int64 pubIntervalNs;
UA_Boolean measurementRunning = UA_FALSE;

static void
timespec_diff(struct timespec *start, struct timespec *stop,
              struct timespec *result) {
    if((stop->tv_nsec - start->tv_nsec) < 0) {
        result->tv_sec = stop->tv_sec - start->tv_sec - 1;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec + 1000000000;
    } else {
        result->tv_sec = stop->tv_sec - start->tv_sec;
        result->tv_nsec = stop->tv_nsec - start->tv_nsec;
    }
}

/* Used to adjust the nanosecond > 1s field value */
static void
nanoSecondFieldConversion(struct timespec *timeSpecValue) {
    while(timeSpecValue->tv_nsec > (SECONDS_AS_NANO_SECONDS - 1)) {
        timeSpecValue->tv_sec += 1;
        timeSpecValue->tv_nsec -= SECONDS_AS_NANO_SECONDS;
    }
}

/* Signal handler */
static void
publishInterrupt(int sig, siginfo_t* si, void* uc) {
    if(si->si_value.sival_ptr != &pubEventTimer) {
        UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND, "stray signal");
        return;
    }

    /* Execute the publish callback in the interrupt */
    struct timespec begin, end;
    clock_gettime(CLOCKID, &begin);
    useMembufAlloc();
    pubCallback(measurementServer, pubData);
    useNormalAlloc();
    clock_gettime(CLOCKID, &end);

    if(publisherMeasurementsCounter >= MAX_MEASUREMENTS)
        return;

    switch(currentMeasurementMode){
        case RT_NONE:
            /* Save current configured publish interval */
            currentPublishCycleTime_RT_none[publisherMeasurementsCounter] = (UA_Int32) pubIntervalNs;
            /* Save the difference to the calculated time */
            timespec_diff(&calculatedCycleStartTime_RT_none[publisherMeasurementsCounter],
                          &begin, &cycleStartDelay_RT_none[publisherMeasurementsCounter]);
            /* Save the duration of the publish callback */
            timespec_diff(&begin, &end, &cycleDuration_RT_none[publisherMeasurementsCounter]);
            publisherMeasurementsCounter++;
            /* Save the calculated starting time for the next cycle */
            calculatedCycleStartTime_RT_none[publisherMeasurementsCounter].tv_nsec =
                    calculatedCycleStartTime_RT_none[publisherMeasurementsCounter - 1].tv_nsec + pubIntervalNs;
            calculatedCycleStartTime_RT_none[publisherMeasurementsCounter].tv_sec =
                    calculatedCycleStartTime_RT_none[publisherMeasurementsCounter - 1].tv_sec;
            nanoSecondFieldConversion(&calculatedCycleStartTime_RT_none[publisherMeasurementsCounter]);
            break;
        case RT_DIRECT_VALUE_ACCESS:
            /* Save current configured publish interval */
            currentPublishCycleTime_RT_static_source[publisherMeasurementsCounter] = (UA_Int32) pubIntervalNs;
            /* Save the difference to the calculated time */
            timespec_diff(&calculatedCycleStartTime_RT_static_source[publisherMeasurementsCounter],
                          &begin, &cycleStartDelay_RT_static_source[publisherMeasurementsCounter]);
            /* Save the duration of the publish callback */
            timespec_diff(&begin, &end, &cycleDuration_RT_static_source[publisherMeasurementsCounter]);
            publisherMeasurementsCounter++;
            /* Save the calculated starting time for the next cycle */
            calculatedCycleStartTime_RT_static_source[publisherMeasurementsCounter].tv_nsec =
                    calculatedCycleStartTime_RT_static_source[publisherMeasurementsCounter - 1].tv_nsec + pubIntervalNs;
            calculatedCycleStartTime_RT_static_source[publisherMeasurementsCounter].tv_sec =
                    calculatedCycleStartTime_RT_static_source[publisherMeasurementsCounter - 1].tv_sec;
            nanoSecondFieldConversion(&calculatedCycleStartTime_RT_static_source[publisherMeasurementsCounter]);
            break;
        case RT_FIXED_OFFSETS:
            /* Save current configured publish interval */
            currentPublishCycleTime_RT_fixed_size[publisherMeasurementsCounter] = (UA_Int32) pubIntervalNs;
            /* Save the difference to the calculated time */
            timespec_diff(&calculatedCycleStartTime_RT_fixed_size[publisherMeasurementsCounter],
                          &begin, &cycleStartDelay_RT_fixed_size[publisherMeasurementsCounter]);
            /* Save the duration of the publish callback */
            timespec_diff(&begin, &end, &cycleDuration_RT_fixed_size[publisherMeasurementsCounter]);
            publisherMeasurementsCounter++;
            /* Save the calculated starting time for the next cycle */
            calculatedCycleStartTime_RT_fixed_size[publisherMeasurementsCounter].tv_nsec =
                    calculatedCycleStartTime_RT_fixed_size[publisherMeasurementsCounter - 1].tv_nsec + pubIntervalNs;
            calculatedCycleStartTime_RT_fixed_size[publisherMeasurementsCounter].tv_sec =
                    calculatedCycleStartTime_RT_fixed_size[publisherMeasurementsCounter - 1].tv_sec;
            nanoSecondFieldConversion(&calculatedCycleStartTime_RT_fixed_size[publisherMeasurementsCounter]);
            break;
        default:
            return;
    }

    /* Write the pubsub measurement data */
    if(publisherMeasurementsCounter == MAX_MEASUREMENTS) {
       measurementRunning = UA_FALSE;
    }
}

/* The following three methods are originally defined in
 * /src/pubsub/ua_pubsub_manager.c. We provide a custom implementation here to
 * use system interrupts instead if time-triggered callbacks in the OPC UA
 * server control flow. */
UA_StatusCode
UA_PubSubManager_addRepeatedCallback(UA_Server *server,
                                     UA_ServerCallback callback,
                                     void *data, UA_Double interval_ms,
                                     UA_UInt64 *callbackId) {
    if(pubCallback) {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                       "At most one publisher can be registered for interrupt callbacks");
        return UA_STATUSCODE_BADINTERNALERROR;
    }

    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Adding a publisher with a cycle time of %lf milliseconds", interval_ms);

    /* Set global values for the publish callback */
    int resultTimerCreate = 0;
    pubIntervalNs = (UA_Int64) (interval_ms * MILLI_AS_NANO_SECONDS);

    /* Handle the signal */
    memset(&signalAction, 0, sizeof(signalAction));
    signalAction.sa_flags = SA_SIGINFO;
    signalAction.sa_sigaction = publishInterrupt;
    sigemptyset(&signalAction.sa_mask);
    sigaction(SIG, &signalAction, NULL);

    /* Create the timer */
    memset(&pubEventTimer, 0, sizeof(pubEventTimer));
    pubEvent.sigev_notify = SIGEV_SIGNAL;
    pubEvent.sigev_signo = SIG;
    pubEvent.sigev_value.sival_ptr = &pubEventTimer;
    resultTimerCreate = timer_create(CLOCKID, &pubEvent, &pubEventTimer);
    if(resultTimerCreate != 0) {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                       "Failed to create a system event with code %i", resultTimerCreate);
        return UA_STATUSCODE_BADINTERNALERROR;
    }

    /* Arm the timer */
    struct itimerspec timerspec;
    timerspec.it_interval.tv_sec = (long int) (pubIntervalNs / (SECONDS_AS_NANO_SECONDS));
    timerspec.it_interval.tv_nsec = (long int) (pubIntervalNs % SECONDS_AS_NANO_SECONDS);
    timerspec.it_value.tv_sec = (long int) (pubIntervalNs / (SECONDS_AS_NANO_SECONDS));
    timerspec.it_value.tv_nsec = (long int) (pubIntervalNs % SECONDS_AS_NANO_SECONDS);
    resultTimerCreate = timer_settime(pubEventTimer, 0, &timerspec, NULL);
    if(resultTimerCreate != 0) {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                       "Failed to arm the system timer with code %i", resultTimerCreate);
        timer_delete(pubEventTimer);
        return UA_STATUSCODE_BADINTERNALERROR;
    }

    /* Start taking measurements */
    publisherMeasurementsCounter = 0;

    switch(currentMeasurementMode){
        case RT_NONE:
            clock_gettime(CLOCKID, &calculatedCycleStartTime_RT_none[0]);
            calculatedCycleStartTime_RT_none[0].tv_nsec += pubIntervalNs;
            nanoSecondFieldConversion(&calculatedCycleStartTime_RT_none[0]);
            break;
        case RT_DIRECT_VALUE_ACCESS:
            clock_gettime(CLOCKID, &calculatedCycleStartTime_RT_static_source[0]);
            calculatedCycleStartTime_RT_static_source[0].tv_nsec += pubIntervalNs;
            nanoSecondFieldConversion(&calculatedCycleStartTime_RT_static_source[0]);
            break;
        case RT_FIXED_OFFSETS:
            clock_gettime(CLOCKID, &calculatedCycleStartTime_RT_fixed_size[0]);
            calculatedCycleStartTime_RT_fixed_size[0].tv_nsec += pubIntervalNs;
            nanoSecondFieldConversion(&calculatedCycleStartTime_RT_fixed_size[0]);
            break;
        default:
            return UA_STATUSCODE_BADCONFIGURATIONERROR;
    }

    /* Set the callback -- used as a sentinel to detect an operational publisher */
    measurementServer = server;
    pubCallback = callback;
    pubData = data;

    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
UA_PubSubManager_changeRepeatedCallbackInterval(UA_Server *server,
                                                UA_UInt64 callbackId,
                                                UA_Double interval_ms) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Switching the publisher cycle to %lf milliseconds", interval_ms);

    struct itimerspec timerspec;
    int resultTimerCreate = 0;
    pubIntervalNs = (UA_Int64) (interval_ms * MILLI_AS_NANO_SECONDS);
    timerspec.it_interval.tv_sec = (long int) (pubIntervalNs / SECONDS_AS_NANO_SECONDS);
    timerspec.it_interval.tv_nsec = (long int) (pubIntervalNs % SECONDS_AS_NANO_SECONDS);
    timerspec.it_value.tv_sec = (long int) (pubIntervalNs / (SECONDS_AS_NANO_SECONDS));
    timerspec.it_value.tv_nsec = (long int) (pubIntervalNs % SECONDS_AS_NANO_SECONDS);
    resultTimerCreate = timer_settime(pubEventTimer, 0, &timerspec, NULL);
    if(resultTimerCreate != 0) {
        UA_LOG_WARNING(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                       "Failed to arm the system timer");
        timer_delete(pubEventTimer);
        return UA_STATUSCODE_BADINTERNALERROR;
    }

    switch(currentMeasurementMode){
        case RT_NONE:
            clock_gettime(CLOCKID, &calculatedCycleStartTime_RT_none[publisherMeasurementsCounter]);
            calculatedCycleStartTime_RT_none[publisherMeasurementsCounter].tv_nsec += pubIntervalNs;
            nanoSecondFieldConversion(&calculatedCycleStartTime_RT_none[publisherMeasurementsCounter]);
            break;
        case RT_DIRECT_VALUE_ACCESS:
            clock_gettime(CLOCKID, &calculatedCycleStartTime_RT_static_source[publisherMeasurementsCounter]);
            calculatedCycleStartTime_RT_static_source[publisherMeasurementsCounter].tv_nsec += pubIntervalNs;
            nanoSecondFieldConversion(&calculatedCycleStartTime_RT_static_source[publisherMeasurementsCounter]);
            break;
        case RT_FIXED_OFFSETS:
            clock_gettime(CLOCKID, &calculatedCycleStartTime_RT_fixed_size[publisherMeasurementsCounter]);
            calculatedCycleStartTime_RT_fixed_size[publisherMeasurementsCounter].tv_nsec += pubIntervalNs;
            nanoSecondFieldConversion(&calculatedCycleStartTime_RT_fixed_size[publisherMeasurementsCounter]);
            break;
        default:
            return UA_STATUSCODE_BADCONFIGURATIONERROR;
    }
    return UA_STATUSCODE_GOOD;
}

void
UA_PubSubManager_removeRepeatedPubSubCallback(UA_Server *server, UA_UInt64 callbackId) {
    if(!pubCallback)
        return;
    timer_delete(pubEventTimer);
    pubCallback = NULL; /* So that a new callback can be registered */
}

static void
startMeasurementCycle(UA_Server *server, UA_UInt16 fields, UA_Duration publishCycle, UA_UInt32 cycles){
    UA_Server_setWriterGroupDisabled(server, writerGroupIdent);
    UA_Server_unfreezeWriterGroupConfiguration(server, writerGroupIdent);
    UA_WriterGroupConfig writerGroupConfig;
    memset(&writerGroupConfig, 0, sizeof(UA_WriterGroupConfig));
    UA_Server_getWriterGroupConfig(server, writerGroupIdent, &writerGroupConfig);
    writerGroupConfig.publishingInterval = PUBSUB_CONFIG_PUBLISH_CYCLE_MS;
    UA_Server_updateWriterGroupConfig(server, writerGroupIdent, &writerGroupConfig);

    //run the measurement with none RT optimizations
    UA_DataSetFieldConfig dsfConfig;
    memset(&dsfConfig, 0, sizeof(UA_DataSetFieldConfig));
    UA_NodeId collectionNodeID;
    UA_NodeId_init(&collectionNodeID);
    UA_ObjectAttributes objectAttributes = UA_ObjectAttributes_default;
    objectAttributes.displayName = UA_LOCALIZEDTEXT("en-EN", "PublishNodeCollection");
    UA_Server_addObjectNode(server, UA_NODEID_NULL, UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
            UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT), UA_QUALIFIEDNAME(0, "NodeCollection"),
            UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE), objectAttributes, NULL, &collectionNodeID);
    for(size_t i = 0; i < fields; i++){
        UA_VariableAttributes attributes = UA_VariableAttributes_default;
        UA_Variant value;
        UA_Variant_init(&value);
        UA_UInt32 intValue = (UA_UInt32) i * 1000;
        UA_Variant_setScalar(&value, &intValue, &UA_TYPES[UA_TYPES_UINT32]);
        attributes.value = value;
        if(UA_Server_addVariableNode(server, UA_NODEID_NUMERIC(1,  1000 + (UA_UInt32) i),
                                     collectionNodeID, UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                     UA_QUALIFIEDNAME(1, "variable"), UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                                     attributes, NULL, NULL) != UA_STATUSCODE_GOOD){
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "Failed to add Publish-Node to the server. Node number: %zu", i);
            continue;
        }
        dsfConfig.field.variable.publishParameters.publishedVariable = UA_NODEID_NUMERIC(1, 1000 + (UA_UInt32) i);
        dsfConfig.field.variable.publishParameters.attributeId = UA_ATTRIBUTEID_VALUE;
        UA_Server_addDataSetField(server, publishedDataSetIdent, &dsfConfig, &dataSetFieldIdent);
    }

    /* Freeze the PubSub configuration (and start implicitly the publish callback) */
    UA_Server_freezeWriterGroupConfiguration(server, writerGroupIdent);
    UA_Server_setWriterGroupOperational(server, writerGroupIdent);
    while(!measurementRunning)
        usleep(1000);
    UA_Server_setWriterGroupDisabled(server, writerGroupIdent);
    UA_Server_unfreezeWriterGroupConfiguration(server, writerGroupIdent);
    //cleanup
    UA_Server_deleteNode(server, collectionNodeID, UA_TRUE);

#ifdef EXTENDED
    //run the measurement with RT direct value access
    writerGroupConfig.rtLevel = UA_PUBSUB_RT_DIRECT_VALUE_ACCESS;
    UA_Server_updateWriterGroupConfig(server, writerGroupIdent, &writerGroupConfig);
    //TODO RT level update maybe not possible
    /* Add one DataSetField with static value source to PDS */
    for(size_t i = 0; i < fields; i++){
        memset(&dsfConfig, 0, sizeof(UA_DataSetFieldConfig));
        /* Create Variant and configure as DataSetField source */
        UA_UInt32 *intValue = UA_UInt32_new();
        *intValue = (UA_UInt32) i * 1000;
        valueStore[i] = intValue;
        UA_Variant variant;
        memset(&variant, 0, sizeof(UA_Variant));
        UA_Variant_setScalar(&variant, intValue, &UA_TYPES[UA_TYPES_UINT32]);
        UA_DataValue staticValueSource;
        memset(&staticValueSource, 0, sizeof(staticValueSource));
        staticValueSource.value = variant;
        dsfConfig.field.variable.staticValueSourceEnabled = UA_TRUE;
        dsfConfig.field.variable.staticValueSource.value = variant;
        UA_Server_addDataSetField(server, publishedDataSetIdent, &dsfConfig, &dataSetFieldIdent);
    }
    UA_Server_freezeWriterGroupConfiguration(server, writerGroupIdent);
    UA_Server_setWriterGroupOperational(server, writerGroupIdent);
    while(!measurementRunning)
        usleep(1000);
    UA_Server_setWriterGroupDisabled(server, writerGroupIdent);
    UA_Server_unfreezeWriterGroupConfiguration(server, writerGroupIdent);

    //run the measurement with fixed offsets
    writerGroupConfig.rtLevel = UA_PUBSUB_RT_FIXED_SIZE;
    writerGroupConfig.rtLevel = UA_PUBSUB_RT_DIRECT_VALUE_ACCESS;
    UA_Server_updateWriterGroupConfig(server, writerGroupIdent, &writerGroupConfig);
    //TODO RT level update maybe not possible */
    /* Add one DataSetField with static value source to PDS */
    for(size_t i = 0; i < fields; i++){
        memset(&dsfConfig, 0, sizeof(UA_DataSetFieldConfig));
        /* Create Variant and configure as DataSetField source */
        UA_UInt32 *intValue = UA_UInt32_new();
        *intValue = (UA_UInt32) i * 1000;
        valueStore[i] = intValue;
        UA_Variant variant;
        memset(&variant, 0, sizeof(UA_Variant));
        UA_Variant_setScalar(&variant, intValue, &UA_TYPES[UA_TYPES_UINT32]);
        UA_DataValue staticValueSource;
        memset(&staticValueSource, 0, sizeof(staticValueSource));
        staticValueSource.value = variant;
        dsfConfig.field.variable.staticValueSourceEnabled = UA_TRUE;
        dsfConfig.field.variable.staticValueSource.value = variant;
        UA_Server_addDataSetField(server, publishedDataSetIdent, &dsfConfig, &dataSetFieldIdent);
    }
    UA_Server_freezeWriterGroupConfiguration(server, writerGroupIdent);
    UA_Server_setWriterGroupOperational(server, writerGroupIdent);
    while(!measurementRunning)
        usleep(1000);
    UA_Server_setWriterGroupDisabled(server, writerGroupIdent);
    UA_Server_unfreezeWriterGroupConfiguration(server, writerGroupIdent);
#endif
    //TODO Write Down CSV File into local folder
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_USERLAND,
                "Logging the measurements to %s", MEASUREMENT_OUTPUT);

    FILE *fpPublisher = fopen(MEASUREMENT_OUTPUT, "w");
    for(size_t i = 0; i < publisherMeasurementsCounter; i++) {
        fprintf(fpPublisher, "%u, %u, %ld.%09ld, %ld.%09ld, %ld.%09ld, "
                             "%u, %ld.%09ld, %ld.%09ld, %ld.%09ld, "
                             "%u, %ld.%09ld, %ld.%09ld, %ld.%09ld\n",
                i,
                currentPublishCycleTime_RT_none[i],
                calculatedCycleStartTime_RT_none[i].tv_sec,
                calculatedCycleStartTime_RT_none[i].tv_nsec,
                cycleStartDelay_RT_none[i].tv_sec,
                cycleStartDelay_RT_none[i].tv_nsec,
                cycleDuration_RT_none[i].tv_sec,
                cycleDuration_RT_none[i].tv_nsec,
                currentPublishCycleTime_RT_static_source[i],
                calculatedCycleStartTime_RT_static_source[i].tv_sec,
                calculatedCycleStartTime_RT_static_source[i].tv_nsec,
                cycleStartDelay_RT_static_source[i].tv_sec,
                cycleStartDelay_RT_static_source[i].tv_nsec,
                cycleDuration_RT_static_source[i].tv_sec,
                cycleDuration_RT_static_source[i].tv_nsec,
                currentPublishCycleTime_RT_fixed_size[i],
                calculatedCycleStartTime_RT_fixed_size[i].tv_sec,
                calculatedCycleStartTime_RT_fixed_size[i].tv_nsec,
                cycleStartDelay_RT_fixed_size[i].tv_sec,
                cycleStartDelay_RT_fixed_size[i].tv_nsec,
                cycleDuration_RT_fixed_size[i].tv_sec,
                cycleDuration_RT_fixed_size[i].tv_nsec);
    }
    fclose(fpPublisher);

}

/*
static void
valueUpdateCallback(UA_Server *server, void *data) {
#if defined PUBSUB_CONFIG_FASTPATH_FIXED_OFFSETS || defined PUBSUB_CONFIG_FASTPATH_STATIC_VALUES
    for (int i = 0; i < PUBSUB_CONFIG_FIELD_COUNT; ++i)
        *valueStore[i] = *valueStore[i] + 1;
    if(*valueStore[0] > PUBSUB_CONFIG_PUBLISH_CYCLES)
        running = false;
#endif
#if defined PUBSUB_CONFIG_FASTPATH_NONE
    for(size_t i = 0; i < PUBSUB_CONFIG_FIELD_COUNT; i++){
        UA_Variant value;
        UA_Variant_init(&value);
        if(UA_Server_readValue(server, UA_NODEID_NUMERIC(1, 1000 + (UA_UInt32) i), &value) != UA_STATUSCODE_GOOD) {
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "Failed to read publish value. Node number: %zu", i);
            continue;
        }
        UA_UInt32 *intValue = (UA_UInt32 *) value.data;
        *intValue = *intValue + 1;
        if(UA_Server_writeValue(server, UA_NODEID_NUMERIC(1, 1000 + (UA_UInt32) i), value) != UA_STATUSCODE_GOOD){
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "Failed to update publish value. Node number: %zu", i);
            continue;
        }
        if(i == 0 && *intValue  > PUBSUB_CONFIG_PUBLISH_CYCLES)
            running = false;
        UA_Variant_deleteMembers(&value);
    }
#endif
}
*/


static UA_StatusCode
measurementMethodCallback(UA_Server *server,
                         const UA_NodeId *sessionId, void *sessionHandle,
                         const UA_NodeId *methodId, void *methodContext,
                         const UA_NodeId *objectId, void *objectContext,
                         size_t inputSize, const UA_Variant *input,
                         size_t outputSize, UA_Variant *output) {
    UA_UInt32 *publishCycles = (UA_UInt32 *)input->data;
    if(*publishCycles > 0)
        startMeasurementCycle(server, 10, 10, *publishCycles);

    UA_StatusCode statusCode = UA_STATUSCODE_GOOD;
    UA_Variant_setScalarCopy(output, &statusCode, &UA_TYPES[UA_TYPES_STATUSCODE]);
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "PubSub measurement was called.");
    return UA_STATUSCODE_GOOD;
}

int main(void) {
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    measurementServer = UA_Server_new();
    UA_ServerConfig *config = UA_Server_getConfig(measurementServer);
    UA_ServerConfig_setDefault(config);

    config->pubsubTransportLayers = (UA_PubSubTransportLayer *) UA_malloc(sizeof(UA_PubSubTransportLayer));
    if(!config->pubsubTransportLayers) {
        UA_Server_delete(measurementServer);
        return -1;
    }
    config->pubsubTransportLayers[0] = UA_PubSubTransportLayerUDPMP();
    config->pubsubTransportLayersSize++;

    /*Add standard PubSub configuration (no difference to the std. configuration)*/
    addMinimalPubSubConfiguration(measurementServer);

    UA_WriterGroupConfig writerGroupConfig;
    memset(&writerGroupConfig, 0, sizeof(UA_WriterGroupConfig));
    writerGroupConfig.name = UA_STRING("Demo WriterGroup");
    writerGroupConfig.publishingInterval = PUBSUB_CONFIG_PUBLISH_CYCLE_MS;
    writerGroupConfig.enabled = UA_FALSE;
    writerGroupConfig.writerGroupId = 100;
    writerGroupConfig.encodingMimeType = UA_PUBSUB_ENCODING_UADP;
    writerGroupConfig.messageSettings.encoding             = UA_EXTENSIONOBJECT_DECODED;
    writerGroupConfig.messageSettings.content.decoded.type = &UA_TYPES[UA_TYPES_UADPWRITERGROUPMESSAGEDATATYPE];
    UA_UadpWriterGroupMessageDataType writerGroupMessage;
    UA_UadpWriterGroupMessageDataType_init(&writerGroupMessage);
    /* Change message settings of writerGroup to send PublisherId,
     * WriterGroupId in GroupHeader and DataSetWriterId in PayloadHeader
     * of NetworkMessage */
    writerGroupMessage.networkMessageContentMask = (UA_UadpNetworkMessageContentMask) ((UA_UadpNetworkMessageContentMask) UA_UADPNETWORKMESSAGECONTENTMASK_PUBLISHERID |
                                                    (UA_UadpNetworkMessageContentMask) UA_UADPNETWORKMESSAGECONTENTMASK_GROUPHEADER |
                                                    (UA_UadpNetworkMessageContentMask) UA_UADPNETWORKMESSAGECONTENTMASK_WRITERGROUPID |
                                                    (UA_UadpNetworkMessageContentMask) UA_UADPNETWORKMESSAGECONTENTMASK_PAYLOADHEADER);
    writerGroupConfig.messageSettings.content.decoded.data = &writerGroupMessage;

    /*
#ifdef PUBSUB_CONFIG_FASTPATH_FIXED_OFFSETS
    writerGroupConfig.rtLevel = UA_PUBSUB_RT_FIXED_SIZE;
#elif defined PUBSUB_CONFIG_FASTPATH_STATIC_VALUES
    writerGroupConfig.rtLevel = UA_PUBSUB_RT_DIRECT_VALUE_ACCESS;
#endif
    UA_Server_addWriterGroup(server, connectionIdentifier, &writerGroupConfig, &writerGroupIdent);
    // Add one DataSetWriter
    UA_NodeId dataSetWriterIdent;
    UA_DataSetWriterConfig dataSetWriterConfig;
    memset(&dataSetWriterConfig, 0, sizeof(UA_DataSetWriterConfig));
    dataSetWriterConfig.name = UA_STRING("Demo DataSetWriter");
    dataSetWriterConfig.dataSetWriterId = 62541;
    dataSetWriterConfig.keyFrameCount = 10;
    UA_Server_addDataSetWriter(server, writerGroupIdent, publishedDataSetIdent, &dataSetWriterConfig, &dataSetWriterIdent);

#if defined PUBSUB_CONFIG_FASTPATH_FIXED_OFFSETS || defined PUBSUB_CONFIG_FASTPATH_STATIC_VALUES
    // Add one DataSetField with static value source to PDS //
    UA_DataSetFieldConfig dsfConfig;
    for(size_t i = 0; i < PUBSUB_CONFIG_FIELD_COUNT; i++){
        memset(&dsfConfig, 0, sizeof(UA_DataSetFieldConfig));
        // Create Variant and configure as DataSetField source //
        UA_UInt32 *intValue = UA_UInt32_new();
        *intValue = (UA_UInt32) i * 1000;
        valueStore[i] = intValue;
        UA_Variant variant;
        memset(&variant, 0, sizeof(UA_Variant));
        UA_Variant_setScalar(&variant, intValue, &UA_TYPES[UA_TYPES_UINT32]);
        UA_DataValue staticValueSource;
        memset(&staticValueSource, 0, sizeof(staticValueSource));
        staticValueSource.value = variant;
        dsfConfig.field.variable.staticValueSourceEnabled = UA_TRUE;
        dsfConfig.field.variable.staticValueSource.value = variant;
        UA_Server_addDataSetField(server, publishedDataSetIdent, &dsfConfig, &dataSetFieldIdent);
    }
#endif
#if defined PUBSUB_CONFIG_FASTPATH_NONE
    UA_DataSetFieldConfig dsfConfig;
    memset(&dsfConfig, 0, sizeof(UA_DataSetFieldConfig));
    for(size_t i = 0; i < PUBSUB_CONFIG_FIELD_COUNT; i++){
        UA_VariableAttributes attributes = UA_VariableAttributes_default;
        UA_Variant value;
        UA_Variant_init(&value);
        UA_UInt32 intValue = (UA_UInt32) i * 1000;
        UA_Variant_setScalar(&value, &intValue, &UA_TYPES[UA_TYPES_UINT32]);
        attributes.value = value;
        if(UA_Server_addVariableNode(server, UA_NODEID_NUMERIC(1,  1000 + (UA_UInt32) i),
                UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER), UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                UA_QUALIFIEDNAME(1, "variable"), UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                 attributes, NULL, NULL) != UA_STATUSCODE_GOOD){
            UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "Failed to add Publish-Node to the server. Node number: %zu", i);
            continue;
        }
        dsfConfig.field.variable.publishParameters.publishedVariable = UA_NODEID_NUMERIC(1, 1000 + (UA_UInt32) i);
        dsfConfig.field.variable.publishParameters.attributeId = UA_ATTRIBUTEID_VALUE;
        UA_Server_addDataSetField(server, publishedDataSetIdent, &dsfConfig, &dataSetFieldIdent);
    }
#endif
    // The PubSub configuration is currently editable and the publish callback is not running //
    writerGroupConfig.publishingInterval = PUBSUB_CONFIG_PUBLISH_CYCLE_MS;
    UA_Server_updateWriterGroupConfig(server, writerGroupIdent, &writerGroupConfig);

     */

    UA_Argument inputArgument;
    UA_Argument_init(&inputArgument);
    inputArgument.description = UA_LOCALIZEDTEXT("en-US", "PublishCycles");
    inputArgument.name = UA_STRING("PublishCycles");
    inputArgument.dataType = UA_TYPES[UA_TYPES_UINT32].typeId;
    inputArgument.valueRank = UA_VALUERANK_SCALAR;

    UA_Argument outputArgument;
    UA_Argument_init(&outputArgument);
    outputArgument.description = UA_LOCALIZEDTEXT("en-US", "Result");
    outputArgument.name = UA_STRING("Result");
    outputArgument.dataType = UA_TYPES[UA_TYPES_STATUSCODE].typeId;
    outputArgument.valueRank = UA_VALUERANK_SCALAR;

    UA_MethodAttributes helloAttr = UA_MethodAttributes_default;
    helloAttr.description = UA_LOCALIZEDTEXT("en-US","StartPubSubMeasurement");
    helloAttr.displayName = UA_LOCALIZEDTEXT("en-US","StartPubSubMeasurement");
    helloAttr.executable = true;
    helloAttr.userExecutable = true;
    UA_Server_addMethodNode(measurementServer, UA_NODEID_NUMERIC(1,62541),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASORDEREDCOMPONENT),
                            UA_QUALIFIEDNAME(1, "StartPubSubMeasurement"),
                            helloAttr, &measurementMethodCallback,
                            1, &inputArgument, 1, &outputArgument,
                            NULL, NULL);

    /* Freeze the PubSub configuration (and start implicitly the publish callback) */
    UA_Server_freezeWriterGroupConfiguration(measurementServer, writerGroupIdent);
    UA_Server_setWriterGroupOperational(measurementServer, writerGroupIdent);

    /*
    UA_UInt64 callbackId;
    UA_Server_addRepeatedCallback(server, valueUpdateCallback, NULL, PUBSUB_CONFIG_PUBLISH_CYCLE_MS, &callbackId);
     */

    startMeasurementCycle(measurementServer, 10, 10, 1000);
    UA_StatusCode retval = UA_STATUSCODE_GOOD;
    retval |= UA_Server_run(measurementServer, &running);

    UA_Server_delete(measurementServer);
    return retval == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;
}
