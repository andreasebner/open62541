/* This work is licensed under a Creative Commons CCZero 1.0 Universal License.
 * See http://creativecommons.org/publicdomain/zero/1.0/ for more information. */

#include <open62541/server.h>
#include <open62541/plugin/log.h>
#include <open62541/plugin/log_stdout.h>
#include <open62541/server_config_default.h>
#include <open62541/plugin/pubsub_udp.h>

UA_Boolean running = true;
UA_NodeId publishedDataSetIdent, dataSetFieldIdent, writerGroupIdent, connectionIdentifier;

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

static UA_NodeId
addVariable(UA_Server *server) {
    UA_NodeId outNodeId;
    /* Define the attribute of the myInteger variable node */
    UA_VariableAttributes attr = UA_VariableAttributes_default;
    UA_Int32 myInteger = 42;
    UA_Variant_setScalar(&attr.value, &myInteger, &UA_TYPES[UA_TYPES_INT32]);
    attr.description = UA_LOCALIZEDTEXT("en-US","Normal to RT Node");
    attr.displayName = UA_LOCALIZEDTEXT("en-US","Normal to RT Node");
    attr.dataType = UA_TYPES[UA_TYPES_INT32].typeId;
    attr.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

    /* Add the variable node to the information model */
    UA_QualifiedName myIntegerName = UA_QUALIFIEDNAME(1, "Normal to RT Node");
    UA_NodeId parentNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER);
    UA_NodeId parentReferenceNodeId = UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES);
    UA_Server_addVariableNode(server, UA_NODEID_NULL, parentNodeId,
                              parentReferenceNodeId, myIntegerName,
                              UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE), attr, NULL, &outNodeId);
    return outNodeId;
}

int main(void){
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);
    UA_Server *server = UA_Server_new();
    UA_ServerConfig *config = UA_Server_getConfig(server);
    UA_ServerConfig_setDefault(config);
    config->pubsubTransportLayers = (UA_PubSubTransportLayer *) UA_malloc(sizeof(UA_PubSubTransportLayer));
    if(!config->pubsubTransportLayers) {
        UA_Server_delete(server);
        return -1;
    }
    config->pubsubTransportLayers[0] = UA_PubSubTransportLayerUDPMP();
    config->pubsubTransportLayersSize++;

    addMinimalPubSubConfiguration(server);

    /*add rt configuration*/
    UA_WriterGroupConfig writerGroupConfig;
    memset(&writerGroupConfig, 0, sizeof(UA_WriterGroupConfig));
    writerGroupConfig.name = UA_STRING("Demo WriterGroup");
    writerGroupConfig.publishingInterval = 1000;
    writerGroupConfig.enabled = UA_FALSE;
    writerGroupConfig.writerGroupId = 100;
    writerGroupConfig.encodingMimeType = UA_PUBSUB_ENCODING_UADP;
    writerGroupConfig.messageSettings.encoding             = UA_EXTENSIONOBJECT_DECODED;
    writerGroupConfig.messageSettings.content.decoded.type = &UA_TYPES[UA_TYPES_UADPWRITERGROUPMESSAGEDATATYPE];
    UA_UadpWriterGroupMessageDataType writerGroupMessage;
    UA_UadpWriterGroupMessageDataType_init(&writerGroupMessage);
    writerGroupMessage.networkMessageContentMask = (UA_UadpNetworkMessageContentMask) ((UA_UadpNetworkMessageContentMask) UA_UADPNETWORKMESSAGECONTENTMASK_PUBLISHERID |
                                                                                       (UA_UadpNetworkMessageContentMask) UA_UADPNETWORKMESSAGECONTENTMASK_GROUPHEADER |
                                                                                       (UA_UadpNetworkMessageContentMask) UA_UADPNETWORKMESSAGECONTENTMASK_WRITERGROUPID |
                                                                                       (UA_UadpNetworkMessageContentMask) UA_UADPNETWORKMESSAGECONTENTMASK_PAYLOADHEADER);
    writerGroupConfig.messageSettings.content.decoded.data = &writerGroupMessage;
    writerGroupConfig.rtLevel = UA_PUBSUB_RT_FIXED_SIZE;
    UA_Server_addWriterGroup(server, connectionIdentifier, &writerGroupConfig, &writerGroupIdent);
    /* Add one DataSetWriter */
    UA_NodeId dataSetWriterIdent;
    UA_DataSetWriterConfig dataSetWriterConfig;
    memset(&dataSetWriterConfig, 0, sizeof(UA_DataSetWriterConfig));
    dataSetWriterConfig.name = UA_STRING("Demo DataSetWriter");
    dataSetWriterConfig.dataSetWriterId = 62541;
    dataSetWriterConfig.keyFrameCount = 10;
    UA_Server_addDataSetWriter(server, writerGroupIdent, publishedDataSetIdent, &dataSetWriterConfig, &dataSetWriterIdent);

    /*change existing var to an RT variable */
    UA_NodeId addedNodId = addVariable(server);
    UA_Int32 *integerRTValue = UA_Int32_new();
    UA_Variant variantRT;
    UA_Variant_init(&variantRT);
    UA_Variant_setScalar(&variantRT, integerRTValue, &UA_TYPES[UA_TYPES_INT32]);
    UA_Server_swapExistingVariableNodeToRT(server, addedNodId, &variantRT);

    /* setup DataSetField config */
    UA_DataSetFieldConfig dsfConfig;
    memset(&dsfConfig, 0, sizeof(UA_DataSetFieldConfig));
    dsfConfig.field.variable.staticValueSourceEnabled = UA_TRUE;
    /* use the node id here as an alternative and then the node context*/
    /* alternative structure {UA_Boolean rtNode; UA_Variant value} */
    dsfConfig.field.variable.staticValueSource.value = variantRT;
    UA_Server_addDataSetField(server, publishedDataSetIdent, &dsfConfig, &dataSetFieldIdent);

    /* add new RT variable to the server */
    UA_VariableAttributes variableAttributes = UA_VariableAttributes_default;
    variableAttributes.dataType = UA_TYPES[UA_TYPES_INT32].typeId;
    variableAttributes.displayName = UA_LOCALIZEDTEXT("en-EN", "Int32 PubSub test node");
    variableAttributes.accessLevel = UA_ACCESSLEVELMASK_READ | UA_ACCESSLEVELMASK_WRITE;

    UA_Int32 *integerRTValue2 = UA_Int32_new();
    *integerRTValue2 = 1000;
    UA_Variant variantRT2;
    UA_Variant_init(&variantRT2);
    UA_Variant_setScalar(&variantRT2, integerRTValue2, &UA_TYPES[UA_TYPES_INT32]);
    variableAttributes.value = variantRT2;
    variableAttributes.displayName = UA_LOCALIZEDTEXT("en-EN", "Direct RT Node");
    UA_Server_addPubSubRTvariableNode(server, UA_NODEID_NULL,
                                      UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                                      UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                                      UA_QUALIFIEDNAME(1, "Direct RT Node"),
                                      UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                                      variableAttributes, &variantRT2, NULL, NULL);

    /* setup second DataSetField config */
    UA_DataSetFieldConfig dsfConfig2;
    memset(&dsfConfig2, 0, sizeof(UA_DataSetFieldConfig));
    dsfConfig2.field.variable.staticValueSourceEnabled = UA_TRUE;
    /* use the node id here as an alternative and then the node context*/
    /* alternative structure {UA_Boolean rtNode; UA_Variant value} */
    dsfConfig2.field.variable.staticValueSource.value = variantRT2;
    UA_Server_addDataSetField(server, publishedDataSetIdent, &dsfConfig2, NULL);

    /* Freeze the PubSub configuration (and start implicitly the publish callback) */
    UA_Server_freezeWriterGroupConfiguration(server, writerGroupIdent);
    UA_Server_setWriterGroupOperational(server, writerGroupIdent);

    UA_StatusCode retval = UA_Server_run(server, &running);
    UA_Server_delete(server);
    return retval == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;
}
