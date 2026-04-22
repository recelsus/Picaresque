cmake_minimum_required(VERSION 3.20)

project(PicaresqueServer LANGUAGES CXX)

enable_testing()

option(PICARESQUE_ENABLE_DROGON_SERVER "Build the Drogon HTTP server skeleton" ON)

find_package(OpenSSL REQUIRED)
find_path(JSONCPP_INCLUDE_DIRS json/json.h)
find_library(JSONCPP_LIBRARIES NAMES jsoncpp)

if(NOT JSONCPP_INCLUDE_DIRS OR NOT JSONCPP_LIBRARIES)
  message(FATAL_ERROR "jsoncpp library not found")
endif()

if(NOT TARGET Jsoncpp_lib)
  add_library(Jsoncpp_lib INTERFACE IMPORTED)
  set_target_properties(Jsoncpp_lib PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${JSONCPP_INCLUDE_DIRS}"
    INTERFACE_LINK_LIBRARIES "${JSONCPP_LIBRARIES}"
  )
endif()

add_library(picaresque_permission
  src/permission/access.cpp
  src/permission/authorization.cpp
  src/permission/errors.cpp
  src/permission/resource.cpp
  src/permission/resolver.cpp
  src/permission/validation.cpp
)

target_include_directories(picaresque_permission
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(picaresque_permission PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

add_executable(permission_tests
  tests/permission_tests.cpp
)

target_link_libraries(permission_tests
  PRIVATE
    picaresque_permission
)

add_test(NAME permission_tests COMMAND permission_tests)

add_executable(resource_permission_tests
  tests/resource_permission_tests.cpp
)

target_link_libraries(resource_permission_tests
  PRIVATE
    picaresque_permission
)

add_test(NAME resource_permission_tests COMMAND resource_permission_tests)

add_library(picaresque_access
  src/access/in_memory_repository.cpp
  src/access/ip_matcher.cpp
  src/access/policy.cpp
)

target_include_directories(picaresque_access
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(picaresque_access PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

add_executable(access_tests
  tests/access_tests.cpp
)

target_link_libraries(access_tests
  PRIVATE
    picaresque_access
)

add_test(NAME access_tests COMMAND access_tests)

add_library(picaresque_auth_hash
  src/auth/hash.cpp
)

target_include_directories(picaresque_auth_hash
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(picaresque_auth_hash PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

target_link_libraries(picaresque_auth_hash
  PUBLIC
    OpenSSL::Crypto
)

add_library(picaresque_user
  src/group/group_management_service.cpp
  src/user/in_memory_user_group_repository.cpp
  src/user/user_management_service.cpp
)

target_include_directories(picaresque_user
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(picaresque_user PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

target_link_libraries(picaresque_user
  PUBLIC
    picaresque_auth_hash
    picaresque_permission
)

add_library(picaresque_auth
  src/auth/auth_service.cpp
)

target_include_directories(picaresque_auth
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(picaresque_auth PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

target_link_libraries(picaresque_auth
  PUBLIC
    picaresque_auth_hash
    picaresque_user
)

add_executable(group_management_tests
  tests/group_management_tests.cpp
)

target_link_libraries(group_management_tests
  PRIVATE
    picaresque_permission
    picaresque_user
)

add_test(NAME group_management_tests COMMAND group_management_tests)

add_executable(auth_tests
  tests/auth_tests.cpp
)

target_link_libraries(auth_tests
  PRIVATE
    picaresque_auth
    picaresque_user
)

add_test(NAME auth_tests COMMAND auth_tests)

add_library(picaresque_article
  src/article/article_service.cpp
  src/article/in_memory_article_repository.cpp
)

target_include_directories(picaresque_article
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(picaresque_article PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

target_link_libraries(picaresque_article
  PUBLIC
    picaresque_permission
    picaresque_user
)

add_executable(article_tests
  tests/article_tests.cpp
)

target_link_libraries(article_tests
  PRIVATE
    picaresque_article
    picaresque_permission
    picaresque_user
)

add_test(NAME article_tests COMMAND article_tests)

add_library(picaresque_table
  src/table/in_memory_table_repository.cpp
  src/table/table_service.cpp
)

target_include_directories(picaresque_table
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}/include
)

set_target_properties(picaresque_table PROPERTIES
  CXX_STANDARD 20
  CXX_STANDARD_REQUIRED ON
  CXX_EXTENSIONS OFF
)

target_link_libraries(picaresque_table
  PUBLIC
    Jsoncpp_lib
    picaresque_permission
    picaresque_user
)

add_executable(table_tests
  tests/table_tests.cpp
)

target_link_libraries(table_tests
  PRIVATE
    picaresque_permission
    picaresque_table
    picaresque_user
)

add_test(NAME table_tests COMMAND table_tests)

if(PICARESQUE_ENABLE_DROGON_SERVER)
  find_package(Drogon QUIET)
  find_path(MYSQL_INCLUDE_DIR mysql/mysql.h)
  find_library(MYSQLCLIENT_LIBRARY NAMES mysqlclient mariadb)

  if(Drogon_FOUND AND MYSQL_INCLUDE_DIR AND MYSQLCLIENT_LIBRARY)
    add_executable(migration_runner_tests
      tests/migration_runner_tests.cpp
      src/migration/migration_runner.cpp
    )

    target_include_directories(migration_runner_tests
      PRIVATE
        ${MYSQL_INCLUDE_DIR}
    )

    set_target_properties(migration_runner_tests PROPERTIES
      CXX_STANDARD 20
      CXX_STANDARD_REQUIRED ON
      CXX_EXTENSIONS OFF
    )

    target_link_libraries(migration_runner_tests
      PRIVATE
        Drogon::Drogon
        ${MYSQLCLIENT_LIBRARY}
    )

    add_test(NAME migration_runner_tests COMMAND migration_runner_tests)

    add_executable(request_context_tests
      tests/request_context_tests.cpp
      src/access/mysql_access_repository.cpp
      src/http/json_serialization.cpp
      src/http/request_context.cpp
      src/user/mysql_user_group_repository.cpp
    )

    target_include_directories(request_context_tests
      PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${MYSQL_INCLUDE_DIR}
    )

    set_target_properties(request_context_tests PROPERTIES
      CXX_STANDARD 20
      CXX_STANDARD_REQUIRED ON
      CXX_EXTENSIONS OFF
    )

    target_link_libraries(request_context_tests
      PRIVATE
        Drogon::Drogon
        ${MYSQLCLIENT_LIBRARY}
        OpenSSL::Crypto
        picaresque_access
        picaresque_auth
        picaresque_permission
        picaresque_table
        picaresque_user
    )

    add_test(NAME request_context_tests COMMAND request_context_tests)

    add_executable(picaresque_migrate
      src/migration/main.cpp
      src/migration/migration_runner.cpp
    )

    target_include_directories(picaresque_migrate
      PRIVATE
        ${MYSQL_INCLUDE_DIR}
    )

    set_target_properties(picaresque_migrate PROPERTIES
      CXX_STANDARD 20
      CXX_STANDARD_REQUIRED ON
      CXX_EXTENSIONS OFF
    )

    target_link_libraries(picaresque_migrate
      PRIVATE
        Drogon::Drogon
        ${MYSQLCLIENT_LIBRARY}
    )

    add_executable(picaresque_server
      src/access/mysql_access_repository.cpp
      src/article/mysql_article_repository.cpp
      src/http/article_controller.cpp
      src/http/auth_controller.cpp
      src/http/group_management_controller.cpp
      src/http/json_serialization.cpp
      src/http/request_context.cpp
      src/http/table_controller.cpp
      src/http/user_management_controller.cpp
      src/main.cpp
      src/table/mysql_table_repository.cpp
      src/user/mysql_user_group_repository.cpp
    )

    target_include_directories(picaresque_server
      PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
        ${MYSQL_INCLUDE_DIR}
    )

    set_target_properties(picaresque_server PROPERTIES
      CXX_STANDARD 20
      CXX_STANDARD_REQUIRED ON
      CXX_EXTENSIONS OFF
    )
    target_link_libraries(picaresque_server
      PRIVATE
        Drogon::Drogon
        ${MYSQLCLIENT_LIBRARY}
        OpenSSL::Crypto
        picaresque_access
        picaresque_article
        picaresque_auth
        picaresque_permission
        picaresque_table
        picaresque_user
    )
  else()
    message(STATUS "Drogon or MySQL client library not found. Server and migration targets will not be built.")
  endif()
endif()
