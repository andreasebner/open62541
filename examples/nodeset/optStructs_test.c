//
// Created by grigory on 22.01.20.
//

#include "optStructs_test.h"

#ifdef UA_ENABLE_AMALGAMATION
#include "open62541.h"
#else
#include <open62541/plugin/log_stdout.h>
#include <open62541/server.h>
#include <open62541/server_config_default.h>
#endif

#include "open62541/optstructs_generated_encoding_binary.h"
#include "open62541/optstructs_generated_handling.h"

#include <signal.h>
#include <stdlib.h>

UA_Boolean running = true;

static void stopHandler(int sign) {
    UA_LOG_INFO(UA_Log_Stdout, UA_LOGCATEGORY_SERVER, "received ctrl-c");
    running = false;
}


int main(int argc, char** argv) {
    signal(SIGINT, stopHandler);
    signal(SIGTERM, stopHandler);

    UA_Server *server = UA_Server_new();
    UA_ServerConfig_setDefault(UA_Server_getConfig(server));


    UA_Location* l = UA_Location_new();
    l->switchField = 0;
    l->local = UA_STRING("local");
    l->nMEA = UA_STRING("nmea");
    l->name = UA_STRING("name");

    UA_ByteString *lBuf = UA_ByteString_new();
    size_t lsize = UA_Location_calcSizeBinary(l);
    if(lsize == 0) printf("SwitchField too large\n");
    UA_ByteString_allocBuffer(lBuf, lsize);
    memset(lBuf->data, 0, lsize);

    UA_Byte *lBufPos = lBuf->data;
    const UA_Byte *lBufEnd = &lBuf->data[lBuf->length];

    UA_Location_encodeBinary(l, &lBufPos, lBufEnd);

    for(size_t i=0; i<lBuf->length; i++){
        printf("%02X", lBuf->data[i]);
    }
    printf("\n");

    UA_Location *ld = UA_Location_new();
    size_t lOffset = 0;
    UA_Location_decodeBinary(lBuf, &lOffset, ld);

    printf("switchField: %d\n", ld->switchField);
    printf("nMEA: %s\n", ld->nMEA.data);
    printf("local: %s\n", ld->local.data);
    printf("wGS84: %s\n", ld->wGS84.data);
    printf("name: %s\n", ld->name.data);

//----------------------------------------------------------------------

    UA_NotOptStruct* r = UA_NotOptStruct_new();

    r->antennaName = UA_STRING("hello");
    r->testDouble = 2.0;
    r->testFloat = 3.0;
    r->testInt = 4;

    UA_ByteString *buf = UA_ByteString_new();
    size_t msgSize = UA_NotOptStruct_calcSizeBinary(r);
    UA_ByteString_allocBuffer(buf, msgSize);
    memset(buf->data, 0, msgSize);

    UA_Byte *bufSettingsPos = buf->data;
    const UA_Byte *bufEnd = &buf->data[buf->length];

    UA_NotOptStruct_encodeBinary(r, &bufSettingsPos, bufEnd);

/*--------------------------------------------------------------------*/

    UA_OptStruct* o = UA_OptStruct_new();

    o->hasAntennaName = false;
    o->hasTestDouble = true;
    o->testInt = 3;
    o->testFloat = 4.0;
    o->testDouble = 5.0;


    UA_ByteString *bufO = UA_ByteString_new();
    size_t msgSizeO = UA_OptStruct_calcSizeBinary(o);
    UA_ByteString_allocBuffer(bufO, msgSizeO);
    memset(bufO->data, 0, msgSizeO);


    UA_Byte *bufOptStructPos = bufO->data;
    const UA_Byte *bufOptStructEnd = &bufO->data[bufO->length];

    UA_OptStruct_encodeBinary(o, &bufOptStructPos, bufOptStructEnd);

    for(size_t i=0; i<bufO->length; i++){
        printf("%02X", bufO->data[i]);
    }
    printf("\n");

    UA_OptStruct *od = UA_OptStruct_new();
    size_t offset = 0;
    UA_OptStruct_decodeBinary(bufO, &offset, od);

    printf("hasAntennaName: %d\n", od->hasAntennaName);
    printf("hasTestDouble: %d\n", od->hasTestDouble);
    printf("testInt: %d\n", od->testInt);
    printf("antennaName: %s\n", od->antennaName.data);
    printf("testFloat: %f\n", od->testFloat);
    printf("testDouble: %f\n", od->testDouble);

    UA_StatusCode retval;

    retval = UA_Server_run(server, &running);

    UA_Server_delete(server);
    return retval == UA_STATUSCODE_GOOD ? EXIT_SUCCESS : EXIT_FAILURE;
}
