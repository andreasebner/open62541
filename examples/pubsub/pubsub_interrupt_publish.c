/* This work is licensed under a Creative Commons CCZero 1.0 Universal License.
 * See http://creativecommons.org/publicdomain/zero/1.0/ for more information. */

#include <signal.h>
#include <stdio.h>
#include <time.h>
#include "ua_log_stdout.h"
#include "ua_config_default.h"
#include "ua_network_pubsub_udp.h"
#include "ua_server.h"
#include "ua_server_internal.h"

/* Publish interval in milliseconds */
#define                      PUB_INTERVAL             0.25
/* Assign core affinity for threads */
#define                      CORE_TWO                 2
#define                      CORE_THREE               3
#define                      MAX_MEASUREMENTS         10000000
#define                      FAILURE_EXIT             -1
#define                      PUBLISHER_IP_ADDRESS     "192.168.0.179"
#define                      DATA_SET_WRITER_ID       62541

/* Variable for next cycle start time */
struct timespec              nextCycleStartTime;
/* When the timer was created */
struct timespec              pubStartTime;
/* Set server running as true */
UA_Boolean                   running                = UA_TRUE;
/* Interval in ns */
UA_Int64                     pubIntervalNs;
/* Variables corresponding to PubSub connection creation,
 * published data set and writer group */
UA_PubSubConnection*         connection;
UA_NodeId                    connectionIdent;
UA_NodeId                    publishedDataSetIdent;
UA_NodeId                    writerGroupIdent;
UA_NodeId                    counterNodePublisher;
UA_NodeId                    counterNodeSubscriber;
/* Variable for PubSub callback */
UA_ServerCallback            pubCallback;
/* Variables for counter data handling in address space */
UA_UInt64                    counterDataPublisher   = 0;
UA_UInt64                    counterDataSubscriber  = 0;
UA_Variant                   countPointerPublisher;
UA_Variant                   countPointerSubscriber;

/* For adding nodes in the server information model */
static void addServerNodes(UA_Server* server);

/* File to store the data and timestamps for different traffic */
FILE*                        fpPublisher;
char*                        filePublishedData      = "publisher_T5.csv";

/* Thread for publisher */
pthread_t                    tidPublisher;

/* Array to store published counter data */
UA_UInt64                    publishCounterValue[MAX_MEASUREMENTS];
size_t                       measurementsPublisher  = 0;

/* Array to store timestamp */
struct timespec              publishTimestamp[MAX_MEASUREMENTS];
struct timespec              dataModificationTime;

static void
addPubSubConfiguration(UA_Server* server) {
    /* Details about the connection configuration and handling are located
     * in the pubsub connection tutorial */
    UA_PubSubConnectionConfig connectionConfig;
    memset(&connectionConfig, 0, sizeof(connectionConfig));
    connectionConfig.name = UA_STRING
            ("UDP-UADP Connection 1");
    connectionConfig.transportProfileUri = UA_STRING
            ("http://opcfoundation.org/UA-Profile/Transport/pubsub-udp-uadp");
    connectionConfig.enabled = UA_TRUE;
    UA_NetworkAddressUrlDataType networkAddressUrl = {UA_STRING
                                                              (PUBLISHER_IP_ADDRESS),
                                                      UA_STRING("opc.udp://224.0.0.22:4840/")};
    UA_Variant_setScalar(&connectionConfig.address, &networkAddressUrl,
                         &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE]);
    connectionConfig.publisherId.numeric = UA_UInt32_random();
    UA_Server_addPubSubConnection(server, &connectionConfig, &connectionIdent);
    UA_PublishedDataSetConfig publishedDataSetConfig;
    memset(&publishedDataSetConfig, 0, sizeof(UA_PublishedDataSetConfig));
    publishedDataSetConfig.publishedDataSetType =
            UA_PUBSUB_DATASET_PUBLISHEDITEMS;
    publishedDataSetConfig.name = UA_STRING("Demo PDS");
    /* Create new PublishedDataSet based on the PublishedDataSetConfig. */
    UA_Server_addPublishedDataSet(server, &publishedDataSetConfig,
                                  &publishedDataSetIdent);
    UA_NodeId dataSetFieldIdentCounter;
    UA_DataSetFieldConfig counterValue;
    memset(&counterValue, 0, sizeof(UA_DataSetFieldConfig));
    counterValue.dataSetFieldType =
            UA_PUBSUB_DATASETFIELD_VARIABLE;
    counterValue.field.variable.fieldNameAlias = UA_STRING
            ("Counter Variable 1");
    counterValue.field.variable.promotedField = UA_FALSE;
    counterValue.field.variable.publishParameters.publishedVariable =
            counterNodePublisher;
    counterValue.field.variable.publishParameters.attributeId =
            UA_ATTRIBUTEID_VALUE;
    UA_Server_addDataSetField(server, publishedDataSetIdent, &counterValue,
                              &dataSetFieldIdentCounter);
    UA_WriterGroupConfig writerGroupConfig;
    memset(&writerGroupConfig, 0, sizeof(UA_WriterGroupConfig));
    writerGroupConfig.name = UA_STRING("Demo WriterGroup");
    writerGroupConfig.publishingInterval = PUB_INTERVAL;
    writerGroupConfig.enabled = UA_FALSE;
    writerGroupConfig.encodingMimeType = UA_PUBSUB_ENCODING_UADP;
    UA_Server_addWriterGroup(server, connectionIdent, &writerGroupConfig,
                             &writerGroupIdent);
    UA_NodeId dataSetWriterIdent;
    UA_DataSetWriterConfig dataSetWriterConfig;
    memset(&dataSetWriterConfig, 0, sizeof(UA_DataSetWriterConfig));
    dataSetWriterConfig.name = UA_STRING("Demo DataSetWriter");
    dataSetWriterConfig.dataSetWriterId = DATA_SET_WRITER_ID;
    dataSetWriterConfig.keyFrameCount = 10;
    UA_Server_addDataSetWriter(server, writerGroupIdent, publishedDataSetIdent,
                               &dataSetWriterConfig, &dataSetWriterIdent);
}

/**
 * **Published data handling**
 *
 * The published data is updated in the array using this function

static void
updateMeasurementsPublisher(struct timespec start_time,
                            UA_UInt64 counterValue)
{
    publishTimestamp[measurementsPublisher]        = start_time;
    publishCounterValue[measurementsPublisher]     = counterValue;
    measurementsPublisher++;
}
 */

/**
 * **Creation of nodes**
 *
 * The addServerNodes function is used to create the publisher and subscriber
 * nodes.
 */
static void addServerNodes(UA_Server* server)
{
    UA_NodeId             rttUseCaseID;
    UA_NodeId             newNodeId;
    UA_VariableAttributes publisherAttr;
    UA_VariableAttributes subscriberAttr;
    UA_UInt64             publishValue   = 0;
    UA_UInt64             subscribeValue = 0;
    UA_ObjectAttributes   rttUseCasettr  = UA_ObjectAttributes_default;
    rttUseCasettr.displayName            = UA_LOCALIZEDTEXT("en-US",
                                                            "RTT Use case");
    UA_Server_addObjectNode(server, UA_NODEID_NULL,
                            UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                            UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                            UA_QUALIFIEDNAME(1, "RTT Use case"), UA_NODEID_NUMERIC(0, UA_NS0ID_BASEOBJECTTYPE),
                            rttUseCasettr, NULL, &rttUseCaseID);

    publisherAttr                        = UA_VariableAttributes_default;
    UA_Variant_setScalar(&publisherAttr.value, &publishValue,
                         &UA_TYPES[UA_TYPES_UINT64]);
    publisherAttr.displayName            = UA_LOCALIZEDTEXT("en-US",
                                                            "Publisher Counter"
    );
    newNodeId                            = UA_NODEID_STRING(1,
                                                            "PublisherCounter");
    UA_Server_addVariableNode(server, newNodeId, rttUseCaseID,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                              UA_QUALIFIEDNAME(1, "Publisher Counter"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), publisherAttr, NULL,
                              &counterNodePublisher);
    subscriberAttr                       = UA_VariableAttributes_default;
    UA_Variant_setScalar(&subscriberAttr.value, &subscribeValue,
                         &UA_TYPES[UA_TYPES_UINT64]);
    subscriberAttr.displayName           = UA_LOCALIZEDTEXT("en-US",
                                                            "Subscriber Counter"
    );
    newNodeId                            = UA_NODEID_STRING(1,
                                                            "SubscriberCounter"
    );
    UA_Server_addVariableNode(server, newNodeId, rttUseCaseID,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                              UA_QUALIFIEDNAME(1, "Subscriber Counter"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), subscriberAttr, NULL,
                              &counterNodeSubscriber);
}

/* Stop signal */
static void stopHandler(int sign)
{
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "received ctrl-c");
    running = UA_FALSE;
}

int main(void)
{
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);
    UA_Server*       server;
    UA_StatusCode    retval              = UA_STATUSCODE_GOOD;
    UA_ServerConfig* config              =
            UA_ServerConfig_new_minimal(4840, NULL);
    fpPublisher                          = fopen(filePublishedData, "a");
    config->pubsubTransportLayers        = (UA_PubSubTransportLayer *) UA_malloc(sizeof(UA_PubSubTransportLayer));
    if (!config->pubsubTransportLayers) {
        UA_ServerConfig_delete(config);
        return FAILURE_EXIT;
    }
    config->pubsubTransportLayers[0]     = UA_PubSubTransportLayerUDPMP();
    config->pubsubTransportLayersSize++;

    server = UA_Server_new(config);
    addServerNodes(server);
    addPubSubConfiguration(server);
    /* Run the server */
    retval                               |= UA_Server_run(server, &running);
    /* Write the published data in the publisher_T1.csv file */
    size_t pubLoopVariable               = 0;
    for (pubLoopVariable = 0; pubLoopVariable < measurementsPublisher;
         pubLoopVariable++)
    {
        fprintf(fpPublisher, "%lu,%ld.%09ld\n",
                publishCounterValue[pubLoopVariable],
                publishTimestamp[pubLoopVariable].tv_sec,
                publishTimestamp[pubLoopVariable].tv_nsec);
    }
    /* Delete the server created */
    UA_Server_delete(server);
    /* Delete the server configuration */
    UA_ServerConfig_delete(config);
    /* Close the publisher file pointer */
    fclose(fpPublisher);
    return (int)retval;
}
