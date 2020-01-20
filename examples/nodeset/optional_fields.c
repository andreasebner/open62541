/* This work is licensed under a Creative Commons CCZero 1.0 Universal License.
 * See http://creativecommons.org/publicdomain/zero/1.0/ for more information. */

#include <open62541/plugin/log_stdout.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>

#include "open62541/namespace0_generated.h"
#include "open62541/namespace_di_generated.h"
#include "open62541/namespace_autoid_generated.h"
#include "open62541/types_autoid_generated_encoding_binary.h"
#include "open62541/types_autoid_generated_handling.h"

#include "optional_type.h"

#include <signal.h>
#include <stdlib.h>

UA_Boolean running = true;

static void stopHandler(int sign) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "received ctrl-c");
    running = false;
}

/*
static void
add3PointDataType(UA_Server *server) {
    UA_VariableTypeAttributes dattr = UA_VariableTypeAttributes_default;
    dattr.description = UA_LOCALIZEDTEXT("en-US", "3D Point");
    dattr.displayName = UA_LOCALIZEDTEXT("en-US", "3D Point");
    dattr.dataType = PointType.typeId;
    dattr.valueRank = UA_VALUERANK_SCALAR;

    Point p;
    p.x = 0.0;
    p.y = 0.0;
    p.z = 0.0;
    UA_Variant_setScalar(&dattr.value, &p, &PointType);

    UA_Server_addVariableTypeNode(server, PointType.typeId,
                                  UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                                  UA_NODEID_NUMERIC(0, UA_NS0ID_HASSUBTYPE),
                                  UA_QUALIFIEDNAME(1, "3D.Point"),
                                  UA_NODEID_NUMERIC(0, UA_NS0ID_BASEDATAVARIABLETYPE),
                                  dattr, NULL, NULL);

}

static void
add3DPointVariable(UA_Server *server) {
    Point p;
    p.x = 3.0;
    p.y = 4.0;
    p.z = 5.0;
    UA_VariableAttributes vattr = UA_VariableAttributes_default;
    vattr.description = UA_LOCALIZEDTEXT("en-US", "3D Point");
    vattr.displayName = UA_LOCALIZEDTEXT("en-US", "3D Point");
    vattr.dataType = PointType.typeId;
    vattr.valueRank = UA_VALUERANK_SCALAR;
    UA_Variant_setScalar(&vattr.value, &p, &PointType);

    UA_Server_addVariableNode(server, UA_NODEID_STRING(1, "3D.Point"),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_OBJECTSFOLDER),
                              UA_NODEID_NUMERIC(0, UA_NS0ID_ORGANIZES),
                              UA_QUALIFIEDNAME(1, "3D.Point"),
                              PointType.typeId, vattr, NULL, NULL);
}
*/

int main(int argc, char** argv) {
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    UA_Server *server = UA_Server_new();
    UA_ServerConfig_setDefault(UA_Server_getConfig(server));

    /*add3PointDataType(server);
    add3DPointVariable(server);*/
    /*
    UA_RtlsLocationResult *r = UA_RtlsLocationResult_new();
    UA_RtlsLocationResult_init(r);
    UA_ByteString *buf = UA_ByteString_new();
    size_t msgSize = UA_RtlsLocationResult_calcSizeBinary(r);
    UA_ByteString_allocBuffer(buf, msgSize);

    UA_Byte *bufSettingsPos = buf->data;
    const UA_Byte *bufEnd = &buf->data[buf->length];

    UA_RtlsLocationResult_encodeBinary(r, &bufSettingsPos, bufEnd);
*/


    UA_StatusCode retval;/* = namespace0_generated(server);
    if(retval != UA_STATUSCODE_GOOD) {
        UA_LOG_ERROR(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "Adding NS0 failed. Please check previous error output.");
        UA_Server_delete(server);
        return (int)UA_STATUSCODE_BADUNEXPECTEDERROR;
    }*/

    retval = UA_Server_run(server, &running);

    UA_Server_delete(server);
    return retval == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;
}
