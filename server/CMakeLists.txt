cmake_minimum_required(VERSION 3.20)

project(PicaresqueServer LANGUAGES CXX)

enable_testing()

option(PICARESQUE_ENABLE_DROGON_SERVER "Build the Drogon HTTP server skeleton" ON)

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

add_library(picaresque_user
  src/group/group_management_service.cpp
  src/user/in_memory_state.cpp
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

target_link_libraries(picaresque_user PUBLIC picaresque_permission)

add_executable(group_management_tests
  tests/group_management_tests.cpp
)

target_link_libraries(group_management_tests
  PRIVATE
    picaresque_permission
    picaresque_user
)

add_test(NAME group_management_tests COMMAND group_management_tests)

if(PICARESQUE_ENABLE_DROGON_SERVER)
  find_package(Drogon QUIET)

  if(Drogon_FOUND)
    add_executable(picaresque_server
      src/http/group_management_controller.cpp
      src/http/json_serialization.cpp
      src/http/user_management_controller.cpp
      src/main.cpp
    )

    target_include_directories(picaresque_server
      PRIVATE
        ${CMAKE_CURRENT_SOURCE_DIR}/include
    )

    set_target_properties(picaresque_server PROPERTIES
      CXX_STANDARD 20
      CXX_STANDARD_REQUIRED ON
      CXX_EXTENSIONS OFF
    )
    target_link_libraries(picaresque_server
      PRIVATE
        Drogon::Drogon
        picaresque_permission
        picaresque_user
    )
  else()
    message(STATUS "Drogon not found. The HTTP server target will not be built.")
  endif()
endif()
