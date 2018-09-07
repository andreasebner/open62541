/* This Source Code Form is subject to the terms of the Mozilla Public
 * License, v. 2.0. If a copy of the MPL was not distributed with this
 * file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Copyright (c) 2017-2018 Fraunhofer IOSB (Author: Andreas Ebner)
 */

#include "src_generated/ua_types_generated_encoding_binary.h"
#include "ua_plugin_pubsub.h"
#include "ua_server_pubsub.h"
#include "ua_types.h"
#include "ua_pubsub_ns0.h"
#include "ua_pubsub.h"

#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL /* conditional compilation */

typedef struct{
    UA_NodeId parentNodeId;
    UA_UInt32 parentCalssifier;
    UA_UInt32 elementClassiefier;
} UA_NodePropertyContext;

static UA_StatusCode
addPubSubObjectNode(UA_Server *server, char* name, UA_UInt32 objectid,
                    UA_UInt32 parentid, UA_UInt32 referenceid, UA_UInt32 type_id) {
    UA_ObjectAttributes object_attr = UA_ObjectAttributes_default;
    object_attr.displayName = UA_LOCALIZEDTEXT("", name);
    return UA_Server_addObjectNode(server, UA_NODEID_NUMERIC(0, objectid),
                                   UA_NODEID_NUMERIC(0, parentid),
                                   UA_NODEID_NUMERIC(0, referenceid),
                                   UA_QUALIFIEDNAME(0, name),
                                   UA_NODEID_NUMERIC(0, type_id),
                                   object_attr, NULL, NULL);
}

static UA_StatusCode
writePubSubNs0VariableArray(UA_Server *server, UA_UInt32 id, void *v,
                            size_t length, const UA_DataType *type) {
    UA_Variant var;
    UA_Variant_init(&var);
    UA_Variant_setArray(&var, v, length, type);
    return UA_Server_writeValue(server, UA_NODEID_NUMERIC(0, id), var);
}

static UA_NodeId
findSingleChildNode(UA_Server *server, UA_QualifiedName targetName,
                    UA_NodeId referenceTypeId, UA_NodeId startingNode){
    UA_NodeId resultNodeId;
    UA_RelativePathElement rpe;
    UA_RelativePathElement_init(&rpe);
    rpe.referenceTypeId = referenceTypeId;
    rpe.isInverse = false;
    rpe.includeSubtypes = false;
    rpe.targetName = targetName;
    UA_BrowsePath bp;
    UA_BrowsePath_init(&bp);
    bp.startingNode = startingNode;
    bp.relativePath.elementsSize = 1;
    bp.relativePath.elements = &rpe;
    UA_BrowsePathResult bpr =
            UA_Server_translateBrowsePathToNodeIds(server, &bp);
    if(bpr.statusCode != UA_STATUSCODE_GOOD ||
       bpr.targetsSize < 1)
        return UA_NODEID_NULL;
    if(UA_NodeId_copy(&bpr.targets[0].targetId.nodeId, &resultNodeId) != UA_STATUSCODE_GOOD){
        UA_BrowsePathResult_deleteMembers(&bpr);
        return UA_NODEID_NULL;
    }
    UA_BrowsePathResult_deleteMembers(&bpr);
    return resultNodeId;
}

static void
onRead(UA_Server *server, const UA_NodeId *sessionId, void *sessionContext,
       const UA_NodeId *nodeid, void *nodeContext,
       const UA_NumericRange *range, const UA_DataValue *data) {
    UA_Variant value;
    UA_Variant_init(&value);
    UA_NodeId myNodeId;
	UA_WriterGroup *writerGroup = NULL;
    switch(((UA_NodePropertyContext *) nodeContext)->parentCalssifier){
        case UA_NS0ID_PUBSUBCONNECTIONTYPE:
            break;
        case UA_NS0ID_WRITERGROUPTYPE:
            myNodeId = ((UA_NodePropertyContext *) nodeContext)->parentNodeId;
            writerGroup = UA_WriterGroup_findWGbyId(server, myNodeId);
            if(!writerGroup)
                return;
            switch(((UA_NodePropertyContext *) nodeContext)->elementClassiefier){
                case UA_NS0ID_WRITERGROUPTYPE_WRITERGROUPID:
                    UA_Variant_setScalar(&value, &writerGroup->config.writerGroupId, &UA_TYPES[UA_TYPES_UINT16]);
                    break;
                case UA_NS0ID_WRITERGROUPTYPE_PUBLISHINGINTERVAL:
                    UA_Variant_setScalar(&value, &writerGroup->config.publishingInterval, &UA_TYPES[UA_TYPES_DURATION]);
                    break;
                case UA_NS0ID_WRITERGROUPTYPE_KEEPALIVETIME:
                    UA_Variant_setScalar(&value, &writerGroup->config.keepAliveTime, &UA_TYPES[UA_TYPES_DURATION]);
                    break;
                case UA_NS0ID_WRITERGROUPTYPE_PRIORITY:
                    UA_Variant_setScalar(&value, &writerGroup->config.priority, &UA_TYPES[UA_TYPES_BYTE]);
                    break;
                default:
                    UA_LOG_WARNING(server->config.logger, UA_LOGCATEGORY_SERVER, "Read error! Unknown property.");
            }
            break;
        default:
            UA_LOG_WARNING(server->config.logger, UA_LOGCATEGORY_SERVER, "Read error! Unknown parent element.");
    }
    UA_Server_writeValue(server, *nodeid, value);
}

static void
onWrite(UA_Server *server, const UA_NodeId *sessionId, void *sessionContext,
        const UA_NodeId *nodeId, void *nodeContext,
        const UA_NumericRange *range, const UA_DataValue *data){
    UA_Variant value;
    UA_NodeId myNodeId;
	UA_WriterGroup *writerGroup = NULL;
    switch(((UA_NodePropertyContext *) nodeContext)->parentCalssifier){
        case UA_NS0ID_PUBSUBCONNECTIONTYPE:
            break;
        case UA_NS0ID_WRITERGROUPTYPE:
            myNodeId = ((UA_NodePropertyContext *) nodeContext)->parentNodeId;
            writerGroup = UA_WriterGroup_findWGbyId(server, myNodeId);
            UA_WriterGroupConfig writerGroupConfig;
            memset(&writerGroupConfig, 0, sizeof(writerGroupConfig));
            if(!writerGroup)
                return;
            switch(((UA_NodePropertyContext *) nodeContext)->elementClassiefier){
                case UA_NS0ID_WRITERGROUPTYPE_PUBLISHINGINTERVAL:
                    UA_Server_getWriterGroupConfig(server, writerGroup->identifier, &writerGroupConfig);
                    writerGroupConfig.publishingInterval = *((UA_Duration *) data->value.data);
                    UA_Server_updateWriterGroupConfig(server, writerGroup->identifier, &writerGroupConfig);
                    UA_Variant_setScalar(&value, data->value.data, &UA_TYPES[UA_TYPES_DURATION]);
                    UA_WriterGroupConfig_deleteMembers(&writerGroupConfig);
                    break;
                default:
                    UA_LOG_WARNING(server->config.logger, UA_LOGCATEGORY_SERVER, "Write error! Unknown property element.");
            }
            break;
        default:
            UA_LOG_WARNING(server->config.logger, UA_LOGCATEGORY_SERVER, "Read error! Unknown parent element.");
    }
}

static UA_StatusCode
addVariableValueSource(UA_Server *server, UA_ValueCallback valueCallback,
                       UA_NodeId node, UA_NodePropertyContext *context){
    UA_Server_setNodeContext(server, node, context);
    return UA_Server_setVariableNode_valueCallback(server, node, valueCallback);
}

/*************************************************/
/*            PubSubConnection                   */
/*************************************************/
UA_StatusCode
addPubSubConnectionRepresentation(UA_Server *server, UA_PubSubConnection *connection){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    if(connection->config->name.length > 512)
        return UA_STATUSCODE_BADOUTOFMEMORY;
    UA_STACKARRAY(char, connectionName, sizeof(char) * connection->config->name.length +1);
    memcpy(connectionName, connection->config->name.data, connection->config->name.length);
    connectionName[connection->config->name.length] = '\0';
    //This code block must use a lock
    UA_Nodestore_remove(server, &connection->identifier);
    UA_NodeId pubSubConnectionNodeId;
    UA_ObjectAttributes attr = UA_ObjectAttributes_default;
    attr.displayName = UA_LOCALIZEDTEXT("de-DE", connectionName);
    retVal |= UA_Server_addNode_begin(server, UA_NODECLASS_OBJECT, UA_NODEID_NUMERIC(0, connection->identifier.identifier.numeric),
    UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE), UA_NODEID_NUMERIC(0, UA_NS0ID_HASPUBSUBCONNECTION),
    UA_QUALIFIEDNAME(0, connectionName), UA_NODEID_NUMERIC(0, UA_NS0ID_PUBSUBCONNECTIONTYPE), (const UA_NodeAttributes*)&attr, &UA_TYPES[UA_TYPES_OBJECTATTRIBUTES], NULL, &pubSubConnectionNodeId);
    addPubSubObjectNode(server, "Address", connection->identifier.identifier.numeric+1, pubSubConnectionNodeId.identifier.numeric,  UA_NS0ID_HASCOMPONENT, UA_NS0ID_NETWORKADDRESSURLTYPE);
    UA_Server_addNode_finish(server, pubSubConnectionNodeId);

    //End lock zone
    UA_NodeId addressNode, urlNode, interfaceNode;
    addressNode = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "Address"),
                                      UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                      UA_NODEID_NUMERIC(0, connection->identifier.identifier.numeric));
    urlNode = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "Url"),
                                  UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT), addressNode);
    interfaceNode = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "NetworkInterface"),
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT), addressNode);

    UA_NetworkAddressUrlDataType *networkAddressUrlDataType = ((UA_NetworkAddressUrlDataType *) connection->config->address.data);
    UA_Variant value;
    UA_Variant_init(&value);
    UA_Variant_setScalar(&value, &networkAddressUrlDataType->url, &UA_TYPES[UA_TYPES_STRING]);
    UA_Server_writeValue(server, urlNode, value);
    UA_Variant_setScalar(&value, &networkAddressUrlDataType->networkInterface, &UA_TYPES[UA_TYPES_STRING]);
    UA_Server_writeValue(server, interfaceNode, value);
#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
    retVal |= UA_Server_addReference(server, connection->identifier,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_PUBSUBCONNECTIONTYPE_ADDWRITERGROUP), true);
    retVal |= UA_Server_addReference(server, connection->identifier,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_PUBSUBCONNECTIONTYPE_ADDREADERGROUP), true);
    retVal |= UA_Server_addReference(server, connection->identifier,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_PUBSUBCONNECTIONTYPE_REMOVEGROUP), true);
#endif
    return retVal;
}

static UA_StatusCode
addPubSubConnectionAction(UA_Server *server,
                    const UA_NodeId *sessionId, void *sessionHandle,
                    const UA_NodeId *methodId, void *methodContext,
                    const UA_NodeId *objectId, void *objectContext,
                    size_t inputSize, const UA_Variant *input,
                    size_t outputSize, UA_Variant *output){

    UA_PubSubConnectionDataType pubSubConnectionDataType = *((UA_PubSubConnectionDataType *) input[0].data);
    UA_NetworkAddressUrlDataType networkAddressUrlDataType;
    memset(&networkAddressUrlDataType, 0, sizeof(networkAddressUrlDataType));
    UA_ExtensionObject eo = pubSubConnectionDataType.address;
    if(eo.encoding == UA_EXTENSIONOBJECT_ENCODED_BYTESTRING){
        size_t offset = 0;
        UA_NetworkAddressUrlDataType_decodeBinary(&eo.content.encoded.body, &offset, &networkAddressUrlDataType);
        if(networkAddressUrlDataType.url.length > 512)
            return UA_STATUSCODE_BADOUTOFMEMORY;
        UA_STACKARRAY(char, buffer, sizeof(char) * networkAddressUrlDataType.url.length +1);
        memcpy(buffer, networkAddressUrlDataType.url.data, networkAddressUrlDataType.url.length);
        buffer[networkAddressUrlDataType.url.length] = '\0';
        printf("%s\n", buffer);
    }

    UA_PubSubConnectionConfig connectionConfig;
    memset(&connectionConfig, 0, sizeof(UA_PubSubConnectionConfig));
    connectionConfig.transportProfileUri = UA_STRING("http://opcfoundation.org/UA-Profile/Transport/pubsub-udp-uadp");
    connectionConfig.name = pubSubConnectionDataType.name;
    UA_Variant_setScalar(&connectionConfig.address, &networkAddressUrlDataType,
                         &UA_TYPES[UA_TYPES_NETWORKADDRESSURLDATATYPE]);
    //call API function and create the connection
    UA_NodeId connectionId;
    if(UA_Server_addPubSubConnection(server, &connectionConfig, &connectionId) != UA_STATUSCODE_GOOD){
        //error handling
    };
    for(size_t i = 0; i < pubSubConnectionDataType.writerGroupsSize; i++){
        //UA_PubSubConnection_addWriterGroup(server, UA_NODEID_NULL, NULL, NULL);
    };
    for(size_t i = 0; i < pubSubConnectionDataType.readerGroupsSize; i++){
        //UA_PubSubConnection_addReaderGroup(server, NULL, NULL, NULL);
    };
    UA_NetworkAddressUrlDataType_deleteMembers(&networkAddressUrlDataType);
    return UA_STATUSCODE_GOOD;
}

UA_StatusCode
removePubSubConnectionRepresentation(UA_Server *server, UA_PubSubConnection *connection){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
    retVal |= UA_Server_deleteReference(server, connection->identifier, UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT), true,
                                           UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_PUBSUBCONNECTIONTYPE_ADDWRITERGROUP),
                                           false);
    retVal |= UA_Server_deleteReference(server, connection->identifier, UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT), true,
                                           UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_PUBSUBCONNECTIONTYPE_ADDREADERGROUP),
                                           false);
    retVal |= UA_Server_deleteReference(server, connection->identifier, UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT), true,
                                           UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_PUBSUBCONNECTIONTYPE_REMOVEGROUP),
                                           false);
#endif
    retVal |= UA_Server_deleteNode(server, connection->identifier, true);
    return retVal;
}

static UA_StatusCode
removeConnectionAction(UA_Server *server,
                       const UA_NodeId *sessionId, void *sessionHandle,
                       const UA_NodeId *methodId, void *methodContext,
                       const UA_NodeId *objectId, void *objectContext,
                       size_t inputSize, const UA_Variant *input,
                       size_t outputSize, UA_Variant *output){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_NodeId nodeToRemove = *((UA_NodeId *) input[0].data);
    retVal |= UA_Server_removePubSubConnection(server, nodeToRemove);
    return retVal;
}

/*************************************************/
/*                PublishedDataSet               */
/*************************************************/
UA_StatusCode
addPublishedDataItemsRepresentation(UA_Server *server, UA_PublishedDataSet *publishedDataSet){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    if(publishedDataSet->config.name.length > 512)
        return UA_STATUSCODE_BADOUTOFMEMORY;
    UA_STACKARRAY(char, pdsName, sizeof(char) * publishedDataSet->config.name.length +1);
    memcpy(pdsName, publishedDataSet->config.name.data, publishedDataSet->config.name.length);
    pdsName[publishedDataSet->config.name.length] = '\0';
    //This code block must use a lock
    UA_Nodestore_remove(server, &publishedDataSet->identifier);
    retVal |= addPubSubObjectNode(server, pdsName, publishedDataSet->identifier.identifier.numeric, UA_NS0ID_PUBLISHSUBSCRIBE_PUBLISHEDDATASETS,
                            UA_NS0ID_HASPROPERTY, UA_NS0ID_PUBLISHEDDATAITEMSTYPE);
    //End lock zone
    UA_NodeId configurationVersionNode;
    configurationVersionNode = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "ConfigurationVersion"),
                                                   UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                                   UA_NODEID_NUMERIC(0, publishedDataSet->identifier.identifier.numeric));
    UA_Variant value;
    UA_Variant_init(&value);
    UA_Variant_setScalar(&value, &publishedDataSet->dataSetMetaData.configurationVersion, &UA_TYPES[UA_TYPES_CONFIGURATIONVERSIONDATATYPE]);
    UA_Server_writeValue(server, configurationVersionNode, value);
#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
    retVal |= UA_Server_addReference(server, publishedDataSet->identifier,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_PUBLISHEDDATAITEMSTYPE_ADDVARIABLES), true);
    retVal |= UA_Server_addReference(server, publishedDataSet->identifier,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_PUBLISHEDDATAITEMSTYPE_REMOVEVARIABLES), true);
#endif
    return retVal;
}

static UA_StatusCode
addPublishedDataItemsAction(UA_Server *server,
                            const UA_NodeId *sessionId, void *sessionHandle,
                            const UA_NodeId *methodId, void *methodContext,
                            const UA_NodeId *objectId, void *objectContext,
                            size_t inputSize, const UA_Variant *input,
                            size_t outputSize, UA_Variant *output){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_String name = *((UA_String *) input[0].data);
/*    UA_String * fieldNameAliases = (UA_String *) input[1].data;
    UA_DataSetFieldFlags * fieldFlags = (UA_DataSetFieldFlags *) input[2].data;
    UA_PublishedVariableDataType *variablesToAdd = (UA_PublishedVariableDataType *) input[3].data;*/
    //TODO how to get the array size?

    //implementation hints:
    // 1. create the pds
    // 2. call for the paramters the addVariableAction --> same params
    UA_PublishedDataSetConfig publishedDataSetConfig;
    for(size_t i = 0; i < 42; i ++) {
        memset(&publishedDataSetConfig, 0, sizeof(UA_PublishedDataSetConfig));
        publishedDataSetConfig.name = name,
                retVal |= UA_Server_addPublishedDataSet(server, &publishedDataSetConfig, NULL).addResult;
    }
    return retVal;
}

UA_StatusCode
removePublishedDataSetRepresentation(UA_Server *server, UA_PublishedDataSet *publishedDataSet){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    retVal |= UA_Server_deleteNode(server, publishedDataSet->identifier, false);
    return retVal;
}

static UA_StatusCode
removePublishedDataSetAction(UA_Server *server,
                             const UA_NodeId *sessionId, void *sessionHandle,
                             const UA_NodeId *methodId, void *methodContext,
                             const UA_NodeId *objectId, void *objectContext,
                             size_t inputSize, const UA_Variant *input,
                             size_t outputSize, UA_Variant *output){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_NodeId nodeToRemove = *((UA_NodeId *) input[0].data);
    retVal |= UA_Server_removePublishedDataSet(server, nodeToRemove);
    return retVal;
}

static UA_StatusCode
addDataSetFolderAction(UA_Server *server,
                       const UA_NodeId *sessionId, void *sessionHandle,
                       const UA_NodeId *methodId, void *methodContext,
                       const UA_NodeId *objectId, void *objectContext,
                       size_t inputSize, const UA_Variant *input,
                       size_t outputSize, UA_Variant *output){
    /* defined in R 1.04 9.1.4.5.7 */
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_String newFolderName = *((UA_String *) input[0].data);
    UA_NodeId generatedId;
    UA_ObjectAttributes objectAttributes = UA_ObjectAttributes_default;
    UA_LocalizedText name;
    name.text = newFolderName;
    name.locale = UA_STRING("en-US");
    objectAttributes.displayName = name;
    retVal |= UA_Server_addObjectNode(server, UA_NODEID_NULL, *objectId, UA_NODEID_NUMERIC(0,UA_NS0ID_ORGANIZES),
                                      UA_QUALIFIEDNAME(0, "DataSetFolder"), UA_NODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE),
                                      objectAttributes, NULL, &generatedId);
    UA_Variant_setScalarCopy(output, &generatedId, &UA_TYPES[UA_TYPES_NODEID]);
#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
    retVal |= UA_Server_addReference(server, generatedId,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_ADDPUBLISHEDDATAITEMS), true);
    retVal |= UA_Server_addReference(server, generatedId,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_REMOVEPUBLISHEDDATASET), true);
    retVal |= UA_Server_addReference(server, generatedId,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_ADDDATASETFOLDER), true);
    retVal |= UA_Server_addReference(server, generatedId,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_REMOVEDATASETFOLDER), true);
#endif
    return retVal;
}

static UA_StatusCode
removeDataSetFolderAction(UA_Server *server,
                          const UA_NodeId *sessionId, void *sessionHandle,
                          const UA_NodeId *methodId, void *methodContext,
                          const UA_NodeId *objectId, void *objectContext,
                          size_t inputSize, const UA_Variant *input,
                          size_t outputSize, UA_Variant *output){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_NodeId nodeToRemove = *((UA_NodeId *) input[0].data);
#ifdef UA_ENABLE_PUBSUB_METHODS
    retVal |= UA_Server_deleteReference(server, nodeToRemove, UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT), true,
                                        UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_ADDPUBLISHEDDATAITEMS),
                                        false);
    retVal |= UA_Server_deleteReference(server, nodeToRemove, UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT), true,
                                        UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_REMOVEPUBLISHEDDATASET),
                                        false);
    retVal |= UA_Server_deleteReference(server, nodeToRemove, UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT), true,
                                        UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_ADDDATASETFOLDER),
                                        false);
    retVal |= UA_Server_deleteReference(server, nodeToRemove, UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT), true,
                                        UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_REMOVEDATASETFOLDER),
                                        false);
#endif
    retVal |= UA_Server_deleteNode(server, nodeToRemove, false);

    return retVal;
}

UA_StatusCode
addVariablesRepresentation(UA_Server *server, UA_DataSetField *dataSetField){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
/*    UA_NodeId dataSetMetaDataNode;
    dataSetMetaDataNode = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "DataSetMetaData"),
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                                        dataSetField->publishedDataSet);*/
    //todo change the field content
    return retVal;
}

static UA_StatusCode
addVariablesAction(UA_Server *server,
                   const UA_NodeId *sessionId, void *sessionHandle,
                   const UA_NodeId *methodId, void *methodContext,
                   const UA_NodeId *objectId, void *objectContext,
                   size_t inputSize, const UA_Variant *input,
                   size_t outputSize, UA_Variant *output){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_ConfigurationVersionDataType configurationVersion = *((UA_ConfigurationVersionDataType *)input[0].data);
    UA_String * fieldNameAliases = (UA_String *) input[1].data;
    UA_Boolean * promotedFields = (UA_Boolean *) input[2].data;
    UA_PublishedVariableDataType *variablesToAdd = (UA_PublishedVariableDataType *) input[3].data;
    //TODO how to get the array size?
    UA_DataSetFieldConfig fieldConfig;
    for(size_t i = 0; i < 42; i ++){
        memset(&fieldConfig, 0, sizeof(UA_DataSetFieldConfig));
        fieldConfig.field.variable.fieldNameAlias = fieldNameAliases[i];
        fieldConfig.field.variable.publishParameters = variablesToAdd[i];
        fieldConfig.field.variable.configurationVersion = configurationVersion;
        if(promotedFields[i]){
            fieldConfig.field.variable.promotedField = UA_TRUE;
        }
        retVal |= UA_Server_addDataSetField(server, *objectId, &fieldConfig, NULL).result;
    }
    return retVal;
}

UA_StatusCode
removeVariablesRepresentation(UA_Server *server, UA_DataSetField *dataSetField){
    UA_StatusCode retVal = UA_STATUSCODE_BADNOTIMPLEMENTED;
/*    UA_NodeId dataSetMetaDataNode;
    dataSetMetaDataNode = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "DataSetMetaData"),
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                                        dataSetField->publishedDataSet);*/
    //todo change the field content
    return retVal;
}

static UA_StatusCode
removeVariablesAction(UA_Server *server,
                      const UA_NodeId *sessionId, void *sessionHandle,
                      const UA_NodeId *methodId, void *methodContext,
                      const UA_NodeId *objectId, void *objectContext,
                      size_t inputSize, const UA_Variant *input,
                      size_t outputSize, UA_Variant *output){

    UA_StatusCode retVal = UA_STATUSCODE_BADNOTIMPLEMENTED;
    //UA_ConfigurationVersionDataType configurationVersion = *((UA_ConfigurationVersionDataType *)input[0].data);
    UA_UInt32 * variablesToRemove = (UA_UInt32 *) input[1].data;

    //TODO how to get the array size?
    for(size_t i = 0; i < 42; i ++){
        //search the corresponding field inside the PDS
        UA_PublishedDataSet *publishedDataSet = UA_PublishedDataSet_findPDSbyId(server, *objectId);
        UA_DataSetField *tmpField;
        size_t counter = 0;
        LIST_FOREACH(tmpField, &publishedDataSet->fields, listEntry){
            if(counter == variablesToRemove[i]){
                retVal |= UA_Server_removeDataSetField(server, tmpField->identifier).result;
            }
        }
    }
    return retVal;
}

/**********************************************/
/*               WriterGroup                  */
/**********************************************/
UA_StatusCode
addWriterGroupRepresentation(UA_Server *server, UA_WriterGroup *writerGroup){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    if(writerGroup->config.name.length > 512)
        return UA_STATUSCODE_BADOUTOFMEMORY;
    UA_STACKARRAY(char, wgName, sizeof(char) * writerGroup->config.name.length + 1);
    memcpy(wgName, writerGroup->config.name.data, writerGroup->config.name.length);
    wgName[writerGroup->config.name.length] = '\0';
    //This code block must use a lock
    UA_Nodestore_remove(server, &writerGroup->identifier);
    retVal |= addPubSubObjectNode(server, wgName, writerGroup->identifier.identifier.numeric, writerGroup->linkedConnection.identifier.numeric,
                            UA_NS0ID_HASCOMPONENT, UA_NS0ID_WRITERGROUPTYPE);
    //End lock zone
    UA_NodeId keepAliveNode, publishingIntervalNode, priorityNode, writerGroupIdNode;
    keepAliveNode = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "KeepAliveTime"),
                                        UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                                        UA_NODEID_NUMERIC(0, writerGroup->identifier.identifier.numeric));
    publishingIntervalNode = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "PublishingInterval"),
                                                 UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                                                 UA_NODEID_NUMERIC(0, writerGroup->identifier.identifier.numeric));
    UA_NodePropertyContext * publishingIntervalContext = (UA_NodePropertyContext *) UA_malloc(sizeof(UA_NodePropertyContext));
    publishingIntervalContext->parentNodeId = writerGroup->identifier;
    publishingIntervalContext->parentCalssifier = UA_NS0ID_WRITERGROUPTYPE;
    publishingIntervalContext->elementClassiefier = UA_NS0ID_WRITERGROUPTYPE_PUBLISHINGINTERVAL;
    UA_ValueCallback valueCallback;
    valueCallback.onRead = onRead;
    valueCallback.onWrite = onWrite;
    retVal |= addVariableValueSource(server, valueCallback, publishingIntervalNode, publishingIntervalContext);
    UA_Server_writeAccessLevel(server, publishingIntervalNode, (UA_ACCESSLEVELMASK_READ ^ UA_ACCESSLEVELMASK_WRITE));

    priorityNode = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "Priority"),
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                                       UA_NODEID_NUMERIC(0, writerGroup->identifier.identifier.numeric));
    writerGroupIdNode = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "WriterGroupId"),
                                            UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY),
                                            UA_NODEID_NUMERIC(0, writerGroup->identifier.identifier.numeric));
    UA_Variant value;
    UA_Variant_init(&value);
    UA_Variant_setScalar(&value, &writerGroup->config.publishingInterval, &UA_TYPES[UA_TYPES_DURATION]);
    UA_Server_writeValue(server, publishingIntervalNode, value);
    UA_Variant_setScalar(&value, &writerGroup->config.keepAliveTime, &UA_TYPES[UA_TYPES_DURATION]);
    UA_Server_writeValue(server, keepAliveNode, value);
    UA_Variant_setScalar(&value, &writerGroup->config.priority, &UA_TYPES[UA_TYPES_BYTE]);
    UA_Server_writeValue(server, priorityNode, value);
    UA_Variant_setScalar(&value, &writerGroup->config.writerGroupId, &UA_TYPES[UA_TYPES_UINT16]);
    UA_Server_writeValue(server, writerGroupIdNode, value);
    return retVal;
}

static UA_StatusCode
addWriterGroupAction(UA_Server *server,
                     const UA_NodeId *sessionId, void *sessionHandle,
                     const UA_NodeId *methodId, void *methodContext,
                     const UA_NodeId *objectId, void *objectContext,
                     size_t inputSize, const UA_Variant *input,
                     size_t outputSize, UA_Variant *output) {
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    //UA_WriterGroupDataType writerGroupDataType = *((UA_WriterGroupDataType *) input[0].data);
    UA_NodeId generatedId;
    UA_WriterGroupConfig writerGroupConfig;
    memset(&writerGroupConfig, 0, sizeof(UA_WriterGroupConfig));
    //todo set config
    retVal |= UA_Server_addWriterGroup(server, *objectId, &writerGroupConfig, &generatedId);
    retVal |= UA_Variant_setScalarCopy(output, &generatedId, &UA_TYPES[UA_TYPES_NODEID]);
    return retVal;
}

UA_StatusCode
removeWriterGroupRepresentation(UA_Server *server, UA_WriterGroup *writerGroup) {
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    retVal |= UA_Server_deleteNode(server, writerGroup->identifier, false);
    return retVal;
}

/**********************************************/
/*               ReaderGroup                  */
/**********************************************/
UA_StatusCode
addReaderGroupRepresentation(UA_Server *server){
    //TODO implement reader part
    return UA_STATUSCODE_BADNOTIMPLEMENTED;
}

static UA_StatusCode
addReaderGroupAction(UA_Server *server,
                     const UA_NodeId *sessionId, void *sessionHandle,
                     const UA_NodeId *methodId, void *methodContext,
                     const UA_NodeId *objectId, void *objectContext,
                     size_t inputSize, const UA_Variant *input,
                     size_t outputSize, UA_Variant *output){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    //TODO implement reader part
    return retVal;
}

/*********************************************************/
/*               Generic Group handling                  */
/*********************************************************/
static UA_StatusCode
removeGroupAction(UA_Server *server,
                  const UA_NodeId *sessionId, void *sessionHandle,
                  const UA_NodeId *methodId, void *methodContext,
                  const UA_NodeId *objectId, void *objectContext,
                  size_t inputSize, const UA_Variant *input,
                  size_t outputSize, UA_Variant *output){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_NodeId nodeToDelete = *((UA_NodeId *) input[0].data);
    UA_Server_removeWriterGroup(server, nodeToDelete);
    return retVal;
}

/**********************************************/
/*               DataSetWriter                */
/**********************************************/
UA_StatusCode
addDataSetWriterRepresentation(UA_Server *server, UA_DataSetWriter *dataSetWriter){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    if(dataSetWriter->config.name.length > 512)
        return UA_STATUSCODE_BADOUTOFMEMORY;
    UA_STACKARRAY(char, dswName, sizeof(char) * dataSetWriter->config.name.length + 1);
    memcpy(dswName, dataSetWriter->config.name.data, dataSetWriter->config.name.length);
    dswName[dataSetWriter->config.name.length] = '\0';
    //This code block must use a lock
    UA_Nodestore_remove(server, &dataSetWriter->identifier);
    retVal |= addPubSubObjectNode(server, dswName, dataSetWriter->identifier.identifier.numeric, dataSetWriter->linkedWriterGroup.identifier.numeric,
                            UA_NS0ID_HASDATASETWRITER, UA_NS0ID_DATASETWRITERTYPE);
    //End lock zone
    retVal |= UA_Server_addReference(server, dataSetWriter->connectedDataSet,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_DATASETTOWRITER),
                                     UA_EXPANDEDNODEID_NUMERIC(0, dataSetWriter->identifier.identifier.numeric), true);
    retVal |= UA_Server_addReference(server, dataSetWriter->connectedDataSet,
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_DATASETTOWRITER),
                                     UA_EXPANDEDNODEID_NUMERIC(0, dataSetWriter->identifier.identifier.numeric), false);
    return retVal;
}

static UA_StatusCode
addDataSetWriterAction(UA_Server *server,
                       const UA_NodeId *sessionId, void *sessionHandle,
                       const UA_NodeId *methodId, void *methodContext,
                       const UA_NodeId *objectId, void *objectContext,
                       size_t inputSize, const UA_Variant *input,
                       size_t outputSize, UA_Variant *output){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_DataSetWriterDataType dataSetWriter = (*(UA_DataSetWriterDataType *) input[0].data);
    UA_DataSetWriterConfig writerConfig;
    UA_NodeId generatedId, *connectedPDS = NULL;
    //TODO change the dataSetWriter config
    //search the connected PDS by name
    for(size_t i = 0; i < server->pubSubManager.publishedDataSetsSize; i++){
        //TODO fix address access to structure element
        if(UA_String_equal(&server->pubSubManager.publishedDataSets[i].config.name, &dataSetWriter.name)){
            //TODO fix address access to structure element
            connectedPDS = &server->pubSubManager.publishedDataSets[i].identifier;
        }
    }
    if(!connectedPDS){
        return UA_STATUSCODE_BADINTERNALERROR;
    }
    writerConfig.name = dataSetWriter.name;
    memset(&writerConfig, 0, sizeof(UA_DataSetWriterConfig));
    retVal |= UA_Server_addDataSetWriter(server, *objectId, *connectedPDS, &writerConfig, &generatedId);

    UA_Variant_setScalar(output, &generatedId, &UA_TYPES[UA_TYPES_NODEID]);
    return retVal;
}

UA_StatusCode
removeDataSetWriterRepresentation(UA_Server *server, UA_DataSetWriter *dataSetWriter) {
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    retVal |= UA_Server_deleteNode(server, dataSetWriter->identifier, false);
    return retVal;
}

static UA_StatusCode
removeDataSetWriterAction(UA_Server *server,
                          const UA_NodeId *sessionId, void *sessionHandle,
                          const UA_NodeId *methodId, void *methodContext,
                          const UA_NodeId *objectId, void *objectContext,
                          size_t inputSize, const UA_Variant *input,
                          size_t outputSize, UA_Variant *output){
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_NodeId dataSetWriter = *((UA_NodeId *) input[0].data);
    retVal |= UA_Server_removeWriterGroup(server, dataSetWriter);
    return retVal;
}

/**********************************************/
/*               DataSetReader                */
/**********************************************/
UA_StatusCode
addDataSetReaderRepresentation(UA_Server *server){
    //TODO implement reader part
    return UA_STATUSCODE_BADNOTIMPLEMENTED;
}

static UA_StatusCode
addDataSetReaderAction(UA_Server *server,
                       const UA_NodeId *sessionId, void *sessionHandle,
                       const UA_NodeId *methodId, void *methodContext,
                       const UA_NodeId *objectId, void *objectContext,
                       size_t inputSize, const UA_Variant *input,
                       size_t outputSize, UA_Variant *output){
    UA_StatusCode retVal = UA_STATUSCODE_BADNOTIMPLEMENTED;
    //TODO implement reader part
    return retVal;
}

UA_StatusCode
removeDataSetReaderRepresentation(UA_Server *server){
    //TODO implement reader part
    return UA_STATUSCODE_BADNOTIMPLEMENTED;
}

static UA_StatusCode
removeDataSetReaderAction(UA_Server *server,
                          const UA_NodeId *sessionId, void *sessionHandle,
                          const UA_NodeId *methodId, void *methodContext,
                          const UA_NodeId *objectId, void *objectContext,
                          size_t inputSize, const UA_Variant *input,
                          size_t outputSize, UA_Variant *output){
    UA_StatusCode retVal = UA_STATUSCODE_BADNOTIMPLEMENTED;
    //TODO implement reader part
    return retVal;
}

/**********************************************/
/*                Destructors                 */
/**********************************************/

static void
connectionTypeDestructor(UA_Server *server,
                         const UA_NodeId *sessionId, void *sessionContext,
                         const UA_NodeId *typeId, void *typeContext,
                         const UA_NodeId *nodeId, void **nodeContext) {
    UA_LOG_INFO(server->config.logger, UA_LOGCATEGORY_USERLAND, "Connection destructor called!");
}

static void
writerGroupTypeDestructor(UA_Server *server,
                          const UA_NodeId *sessionId, void *sessionContext,
                          const UA_NodeId *typeId, void *typeContext,
                          const UA_NodeId *nodeId, void **nodeContext) {
    UA_LOG_INFO(server->config.logger, UA_LOGCATEGORY_USERLAND, "WriterGroup destructor called!");
    UA_NodeId intervalNode;
    intervalNode = findSingleChildNode(server, UA_QUALIFIEDNAME(0, "PublishingInterval"),
                                       UA_NODEID_NUMERIC(0, UA_NS0ID_HASPROPERTY), *nodeId);
    UA_NodePropertyContext *internalConnectionContext;
    UA_Server_getNodeContext(server, intervalNode, (void **) &internalConnectionContext);
    if(!UA_NodeId_equal(&UA_NODEID_NULL , &intervalNode)){
        UA_free(internalConnectionContext);
    }
}

static void
readerGroupTypeDestructor(UA_Server *server,
                          const UA_NodeId *sessionId, void *sessionContext,
                          const UA_NodeId *typeId, void *typeContext,
                          const UA_NodeId *nodeId, void **nodeContext) {
    UA_LOG_INFO(server->config.logger, UA_LOGCATEGORY_USERLAND, "ReaderGroup destructor called!");
}

static void
dataSetWriterTypeDestructor(UA_Server *server,
                            const UA_NodeId *sessionId, void *sessionContext,
                            const UA_NodeId *typeId, void *typeContext,
                            const UA_NodeId *nodeId, void **nodeContext) {
    UA_LOG_INFO(server->config.logger, UA_LOGCATEGORY_USERLAND, "DataSetWriter destructor called!");
}

static void
dataSetReaderTypeDestructor(UA_Server *server,
                            const UA_NodeId *sessionId, void *sessionContext,
                            const UA_NodeId *typeId, void *typeContext,
                            const UA_NodeId *nodeId, void **nodeContext) {
    UA_LOG_INFO(server->config.logger, UA_LOGCATEGORY_USERLAND, "DataSetReader destructor called!");
}

UA_StatusCode
UA_Server_initPubSubNS0(UA_Server *server) {
    UA_StatusCode retVal = UA_STATUSCODE_GOOD;
    UA_String profileArray[server->config.pubsubTransportLayersSize];
    for(size_t i = 0; i < server->config.pubsubTransportLayersSize; i++){
        profileArray[i] = server->config.pubsubTransportLayers->transportProfileUri;
    }

    retVal |= writePubSubNs0VariableArray(server, UA_NS0ID_PUBLISHSUBSCRIBE_SUPPORTEDTRANSPORTPROFILES,
                                    profileArray,
                                    1, &UA_TYPES[UA_TYPES_STRING]);

    retVal |= UA_Server_setMethodNode_callback(server,
                                               UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE_ADDCONNECTION), addPubSubConnectionAction);
    retVal |= UA_Server_setMethodNode_callback(server,
                                               UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE_REMOVECONNECTION), removeConnectionAction);
    retVal |= UA_Server_setMethodNode_callback(server,
                                               UA_NODEID_NUMERIC(0, UA_NS0ID_PUBSUBCONNECTIONTYPE_ADDWRITERGROUP), addWriterGroupAction);
    retVal |= UA_Server_setMethodNode_callback(server,
                                               UA_NODEID_NUMERIC(0, UA_NS0ID_PUBSUBCONNECTIONTYPE_ADDREADERGROUP), addReaderGroupAction);
    retVal |= UA_Server_setMethodNode_callback(server,
                                               UA_NODEID_NUMERIC(0, UA_NS0ID_PUBSUBCONNECTIONTYPE_REMOVEGROUP), removeGroupAction);
    retVal |= UA_Server_setMethodNode_callback(server,
                                               UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHEDDATAITEMSTYPE_ADDVARIABLES), addVariablesAction);
    retVal |= UA_Server_setMethodNode_callback(server,
                                               UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHEDDATAITEMSTYPE_REMOVEVARIABLES), removeVariablesAction);
    retVal |= UA_Server_setMethodNode_callback(server,
                                               UA_NODEID_NUMERIC(0, UA_NS0ID_WRITERGROUPTYPE_ADDDATASETWRITER), addDataSetWriterAction);
    retVal |= UA_Server_setMethodNode_callback(server,
                                               UA_NODEID_NUMERIC(0, UA_NS0ID_WRITERGROUPTYPE_REMOVEDATASETWRITER), removeDataSetWriterAction);
    retVal |= UA_Server_setMethodNode_callback(server,
                                               UA_NODEID_NUMERIC(0, UA_NS0ID_READERGROUPTYPE_ADDDATASETREADER), addDataSetReaderAction);
    retVal |= UA_Server_setMethodNode_callback(server,
                                               UA_NODEID_NUMERIC(0, UA_NS0ID_READERGROUPTYPE_REMOVEDATASETREADER), removeDataSetReaderAction);
#ifdef UA_ENABLE_PUBSUB_INFORMATIONMODEL_METHODS
    retVal |= UA_Server_addReference(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE_PUBLISHEDDATASETS),
                                         UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                         UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_ADDPUBLISHEDDATAITEMS), true);
    retVal |= UA_Server_addReference(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE_PUBLISHEDDATASETS),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_REMOVEPUBLISHEDDATASET), true);
    retVal |= UA_Server_addReference(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE_PUBLISHEDDATASETS),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_ADDDATASETFOLDER), true);
    retVal |= UA_Server_addReference(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBLISHSUBSCRIBE_PUBLISHEDDATASETS),
                                     UA_NODEID_NUMERIC(0, UA_NS0ID_HASCOMPONENT),
                                     UA_EXPANDEDNODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_REMOVEDATASETFOLDER), true);
    retVal |= UA_Server_setMethodNode_callback(server,
                                               UA_NODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_ADDDATASETFOLDER), addDataSetFolderAction);
    retVal |= UA_Server_setMethodNode_callback(server,
                                               UA_NODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_REMOVEDATASETFOLDER), removeDataSetFolderAction);
    retVal |= UA_Server_setMethodNode_callback(server,
                                               UA_NODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_ADDPUBLISHEDDATAITEMS), addPublishedDataItemsAction);
    retVal |= UA_Server_setMethodNode_callback(server,
                                               UA_NODEID_NUMERIC(0, UA_NS0ID_DATASETFOLDERTYPE_REMOVEPUBLISHEDDATASET), removePublishedDataSetAction);
#endif
    UA_NodeTypeLifecycle liveCycle;
    liveCycle.constructor = NULL;
    liveCycle.destructor = connectionTypeDestructor;
    UA_Server_setNodeTypeLifecycle(server, UA_NODEID_NUMERIC(0, UA_NS0ID_PUBSUBCONNECTIONTYPE), liveCycle);
    liveCycle.destructor = writerGroupTypeDestructor;
    UA_Server_setNodeTypeLifecycle(server, UA_NODEID_NUMERIC(0, UA_NS0ID_WRITERGROUPTYPE), liveCycle);
    liveCycle.destructor = readerGroupTypeDestructor;
    UA_Server_setNodeTypeLifecycle(server, UA_NODEID_NUMERIC(0, UA_NS0ID_READERGROUPTYPE), liveCycle);
    liveCycle.destructor = dataSetWriterTypeDestructor;
    UA_Server_setNodeTypeLifecycle(server, UA_NODEID_NUMERIC(0, UA_NS0ID_DATASETWRITERDATATYPE), liveCycle);
    liveCycle.destructor = dataSetReaderTypeDestructor;
    UA_Server_setNodeTypeLifecycle(server, UA_NODEID_NUMERIC(0, UA_NS0ID_DATASETREADERDATATYPE), liveCycle);

    return retVal;
}

#endif /* UA_ENABLE_PUBSUB_INFORMATIONMODEL */
