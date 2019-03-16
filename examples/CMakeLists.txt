cmake_minimum_required(VERSION 3.0...3.12)
project(open62541-examples)
if(${CMAKE_VERSION} VERSION_LESS 3.12)
    cmake_policy(VERSION ${CMAKE_MAJOR_VERSION}.${CMAKE_MINOR_VERSION})
endif()
# This examples folder can also be built standalone.
# First install open62541 using `make install` then
# copy this folder to any other location and call CMake directly:
#
# cp -r open62541/examples $HOME/open62541_examples
# cd $HOME/open62541_examples
# mkdir build && cd build
# cmake -DUA_NAMESPACE_ZERO=FULL ..
# make -j

if(CMAKE_PROJECT_NAME STREQUAL PROJECT_NAME)
    # Examples are built standalone. Find installed open62541

    if(UA_NAMESPACE_ZERO STREQUAL "FULL")
        find_package(open62541 REQUIRED COMPONENTS FullNamespace)
    else()
        find_package(open62541 REQUIRED)
    endif()

    if(NOT UA_TOOLS_DIR)
        set(UA_TOOLS_DIR ${open62541_TOOLS_DIR})
    endif()

    function(assign_source_group)
        # define empty function. We don't need it in standalone
    endfunction(assign_source_group)

    include_directories(${PROJECT_BINARY_DIR}/src_generated)
endif()

# Required for common.h header file used in examples
include_directories(${CMAKE_CURRENT_LIST_DIR})

if(UA_ENABLE_AMALGAMATION)
    add_definitions(-DUA_ENABLE_AMALGAMATION)
endif()

#############################
# Compiled binaries folders #
#############################

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin/examples)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_DEBUG ${CMAKE_BINARY_DIR}/bin/examples)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELEASE ${CMAKE_BINARY_DIR}/bin/examples)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_RELWITHDEBINFO ${CMAKE_BINARY_DIR}/bin/examples)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY_MINSIZEREL ${CMAKE_BINARY_DIR}/bin/examples)

macro(add_example EXAMPLE_NAME EXAMPLE_SOURCE)
  add_executable(${EXAMPLE_NAME} ${STATIC_OBJECTS} ${EXAMPLE_SOURCE} ${ARGN} ${PROJECT_SOURCE_DIR}/common.h)
  if(UA_ENABLE_PUBSUB)
      if(UA_ENABLE_PUBSUB_CUSTOM_PUBLISH_INTERRUPT)
          target_link_libraries(${EXAMPLE_NAME} open62541 ${open62541_LIBRARIES} ua_pubsub_custom_manager)
      endif()
  endif()
  target_link_libraries(${EXAMPLE_NAME} open62541::open62541)
  assign_source_group(${EXAMPLE_SOURCE})
  set_target_properties(${EXAMPLE_NAME} PROPERTIES FOLDER "open62541/examples")
  set_target_properties(${EXAMPLE_NAME} PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
  if(UA_COMPILE_AS_CXX)
      set_source_files_properties(${EXAMPLE_SOURCE} PROPERTIES LANGUAGE CXX)
  endif()
endmacro()

#############
# Tutorials #
#############

add_example(tutorial_datatypes tutorial_datatypes.c)

add_example(tutorial_server_firststeps tutorial_server_firststeps.c)

add_example(tutorial_server_variable tutorial_server_variable.c)

add_example(tutorial_server_datasource tutorial_server_datasource.c)

if(UA_ENABLE_SUBSCRIPTIONS)
    add_example(tutorial_server_monitoreditems tutorial_server_monitoreditems.c)
endif()

add_example(tutorial_server_variabletype tutorial_server_variabletype.c)

add_example(tutorial_server_object tutorial_server_object.c)

if(UA_ENABLE_METHODCALLS)
    add_example(tutorial_server_method tutorial_server_method.c)
endif()

add_example(tutorial_client_firststeps tutorial_client_firststeps.c)

add_example(tutorial_client_events tutorial_client_events.c)

if(UA_ENABLE_SUBSCRIPTIONS_EVENTS)
    add_example(tutorial_server_events tutorial_server_events.c)
endif()

##################
# Example Server #
##################

add_example(server_ctt server_ctt.c)

install(PROGRAMS $<TARGET_FILE:server_ctt>
        DESTINATION bin
        RENAME ua_server_ctt.exe)

##################
# Example Client #
##################

add_example(client client.c)

if(UA_ENABLE_HISTORIZING)
    add_example(client_historical client_historical.c)
endif()

install(PROGRAMS $<TARGET_FILE:client>
        DESTINATION bin
        RENAME ua_client)

add_example(client_async client_async.c)

add_example(client_connect_loop client_connect_loop.c)

add_example(client_connectivitycheck_loop client_connectivitycheck_loop.c)

if(UA_ENABLE_SUBSCRIPTIONS)
    add_example(client_subscription_loop client_subscription_loop.c)
endif()

####################
# Feature Examples #
####################

add_example(server_mainloop server_mainloop.c)

add_example(server_instantiation server_instantiation.c)

add_example(server_repeated_job server_repeated_job.c)

add_example(server_inheritance server_inheritance.c)

if(UA_ENABLE_HISTORIZING)
    add_example(tutorial_server_historicaldata tutorial_server_historicaldata.c)
endif()

if(UA_ENABLE_ENCRYPTION)
    add_example(server_basic128rsa15 encryption/server_basic128rsa15.c)
    add_example(server_basic256sha256 encryption/server_basic256sha256.c)
    add_example(client_encryption encryption/client_encryption.c)
    # common.h
    target_include_directories(server_basic128rsa15 PRIVATE "${PROJECT_SOURCE_DIR}/examples")
    target_include_directories(server_basic256sha256 PRIVATE "${PROJECT_SOURCE_DIR}/examples")
    target_include_directories(client_encryption PRIVATE "${PROJECT_SOURCE_DIR}/examples")
endif()

add_example(custom_datatype_client custom_datatype/client_types_custom.c)
add_example(custom_datatype_server custom_datatype/server_types_custom.c)

if(UA_ENABLE_NODEMANAGEMENT)
    add_example(access_control_server access_control/server_access_control.c)
    add_example(access_control_client access_control/client_access_control.c)
endif()

if(UA_BUILD_SELFSIGNED_CERTIFICATE)
  find_package(OpenSSL REQUIRED)
  add_custom_command(OUTPUT server_cert.der
                     COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/tools/certs/create_self-signed.py ${CMAKE_CURRENT_BINARY_DIR}
                     DEPENDS ${CMAKE_SOURCE_DIR}/tools/certs/create_self-signed.py
                             ${CMAKE_SOURCE_DIR}/tools/certs/localhost.cnf)
  add_custom_target(selfsigned ALL DEPENDS server_cert.der)
  add_executable(server_certificate server_certificate.c ${STATIC_OBJECTS} server_cert.der)
  target_link_libraries(server_certificate open62541 ${open62541_LIBRARIES})
endif()

if(UA_ENABLE_DISCOVERY_MULTICAST)
    add_example(discovery_server_lds discovery/server_lds.c)
    add_example(discovery_server_register discovery/server_register.c)
    add_example(discovery_client_find_servers discovery/client_find_servers.c)
    add_example(discovery_server_multicast discovery/server_multicast.c)
endif()

add_subdirectory(nodeset)

####################
#  Example PubSub  #
####################

if(UA_ENABLE_PUBSUB)
    if(UA_ENABLE_PUBSUB_CUSTOM_PUBLISH_INTERRUPT)
        include_directories(${CMAKE_SOURCE_DIR}/include)
        include_directories(${CMAKE_SOURCE_DIR}/plugins)
        include_directories(${CMAKE_BINARY_DIR})
        include_directories(${CMAKE_BINARY_DIR}/src_generated)
        include_directories(${CMAKE_SOURCE_DIR}/arch)
        include_directories(${CMAKE_SOURCE_DIR}/arch/posix)
        include_directories(${CMAKE_SOURCE_DIR}/examples)
        include_directories(${CMAKE_SOURCE_DIR}/src/server/)
        include_directories(${CMAKE_SOURCE_DIR}/src)
        include_directories(${CMAKE_SOURCE_DIR}/src/pubsub)
        include_directories(${CMAKE_SOURCE_DIR}/deps)
        add_definitions(-D_GNU_SOURCE)
    endif()
endif()

if(UA_ENABLE_PUBSUB)
    if(UA_ENABLE_PUBSUB_CUSTOM_PUBLISH_INTERRUPT)
        add_library(ua_pubsub_custom_manager ${CMAKE_SOURCE_DIR}/plugins/pubsub/ua_pubsub_jobs_systemirq.c)
    endif()
endif()

if(UA_ENABLE_PUBSUB)
    add_example(tutorial_pubsub_connection pubsub/tutorial_pubsub_connection.c)
    add_example(tutorial_pubsub_publish pubsub/tutorial_pubsub_publish.c)

    if(UA_ENABLE_AMALGAMATION)
        message(WARNING "PubSub subscriber tutorial (preview) can not be used with AMALGAMATION. Skipping tutorial_pubsub_subscribe.")
    else(NOT UA_ENABLE_AMALGAMATION)
        add_example(tutorial_pubsub_subscribe pubsub/tutorial_pubsub_subscribe.c)
        add_example(pubsub_subscribe_standalone pubsub/pubsub_subscribe_standalone.c)
    endif()

    if(UA_ENABLE_PUBSUB_CUSTOM_PUBLISH_INTERRUPT)
        assign_source_group(${EXAMPLE_SOURCE})

        if(UA_ENABLE_PUBSUB_CUSTOM_PUBLISH_INTERRUPT_TSN)
            set(custom_pubsub_sources ${PROJECT_SOURCE_DIR}/plugins/ua_network_pubsub_udp_custom_interrupt.c)
            add_library(custom_pubsub ${custom_pubsub_sources})#this line is moved from the eof? correct if no tsn?
        endif()

        add_executable(custom_interrupt_publisher pubsub/pubsub_interrupt_publisher.c)
        add_executable(pubsub_interrupt_publish pubsub/pubsub_interrupt_publish.c ${CMAKE_SOURCE_DIR}/plugins/pubsub/ua_pubsub_jobs_systemirq.c)
        #target_link_libraries (custom_interrupt_publisher custom_pubsub)

        add_executable(custom_interrupt_loopback pubsub/pubsub_interrupt_loopback.c)
        #target_link_libraries (custom_interrupt_loopback custom_pubsub)

        #todo remove development cmake targets
        #add_executable(custom_interrupt_test pubsub/pubsub_interrupt_test.c)
        ##add_executable(custom_interrupt_test pubsub/pubsub_interrupt_test.c ${CMAKE_SOURCE_DIR}/src/pubsub/ua_pubsub_custom_manager.c)
        #add_executable(custom_interrupt_test2 pubsub/pubsub_interrupt_test2.c)
        add_executable(custom_interrupt_loopback_st pubsub/pubsub_interrupt_loopback_st.c)
        add_executable(custom_interrupt_publisher_st pubsub/pubsub_interrupt_publisher_st.c)

        set_target_properties(custom_interrupt_publisher PROPERTIES FOLDER "open62541/examples")
        set_target_properties(custom_interrupt_loopback PROPERTIES FOLDER "open62541/examples")
        set_target_properties(custom_interrupt_loopback_st PROPERTIES FOLDER "open62541/examples")
        set_target_properties(custom_interrupt_publisher_st PROPERTIES FOLDER "open62541/examples")
        set_target_properties(pubsub_interrupt_publish PROPERTIES FOLDER "open62541/examples")

        #todo remove development cmake targets
        #set_target_properties(custom_interrupt_test PROPERTIES FOLDER "open62541/examples")
        #set_target_properties(custom_interrupt_test2 PROPERTIES FOLDER "open62541/examples")

        set_target_properties(custom_interrupt_publisher PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
        set_target_properties(custom_interrupt_loopback PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
        set_target_properties(custom_interrupt_loopback_st PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
        set_target_properties(pubsub_interrupt_publish PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
        set_target_properties(custom_interrupt_publisher_st PROPERTIES VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

        target_link_libraries(custom_interrupt_publisher open62541 ${open62541_LIBRARIES} rt pthread)
        #target_link_libraries(custom_interrupt_publisher open62541 ${open62541_LIBRARIES} rt pthread mbedtls mbedx509 mbedcrypto) original line with crypto
        target_link_libraries(custom_interrupt_loopback open62541 ${open62541_LIBRARIES} rt pthread)
        #target_link_libraries(custom_interrupt_loopback open62541 ${open62541_LIBRARIES} rt pthread mbedtls mbedx509 mbedcrypto)
        target_link_libraries(custom_interrupt_loopback_st open62541 ${open62541_LIBRARIES} rt pthread)
        target_link_libraries(custom_interrupt_publisher_st open62541 ${open62541_LIBRARIES} rt pthread)
        target_link_libraries(pubsub_interrupt_publish open62541 ${open62541_LIBRARIES} rt pthread)

        #todo remove development cmake targets
        #target_link_libraries(custom_interrupt_test open62541 ${open62541_LIBRARIES} rt pthread)
        #target_link_libraries(custom_interrupt_test2 open62541 ${open62541_LIBRARIES} rt pthread)

    endif()

endif()
