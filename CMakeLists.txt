cmake_minimum_required(VERSION 3.20)
project(wcm_core VERSION 1.1.0 LANGUAGES C)

set(CMAKE_C_STANDARD 17)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

option(WCM_BUILD_TESTS "Build WCM Core verification tests" ON)
option(WCM_BUILD_REFERENCE "Build the deterministic reference control loop" ON)
option(WCM_BUILD_INTEGRATION_EXAMPLE "Build the public integration skeleton" ON)
option(WCM_BUILD_PORT_TEMPLATE_CHECK "Compile-check the target port template" ON)
option(WCM_BUILD_POSIX_PORT "Build the POSIX integration port" ${UNIX})
option(WCM_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" ON)
option(WCM_ENABLE_SANITIZERS "Enable AddressSanitizer and UndefinedBehaviorSanitizer" OFF)
option(WCM_ENABLE_TSAN "Enable ThreadSanitizer" OFF)
option(WCM_ENABLE_HARDENING "Enable additional compiler hardening for supported hosted toolchains" OFF)
set(WCM_BUILD_TAG "release" CACHE STRING "Integrator-visible build tag")
set(WCM_PROFILE "STANDARD" CACHE STRING "WCM profile: TINY, STANDARD, or EXTENDED")
set_property(CACHE WCM_PROFILE PROPERTY STRINGS TINY STANDARD EXTENDED)

if(WCM_PROFILE STREQUAL "TINY")
  set(WCM_PROFILE_DEFINE WCM_PROFILE_TINY)
elseif(WCM_PROFILE STREQUAL "EXTENDED")
  set(WCM_PROFILE_DEFINE WCM_PROFILE_EXTENDED)
else()
  set(WCM_PROFILE_DEFINE WCM_PROFILE_STANDARD)
endif()

function(wcm_enable_warnings target)
  if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic -Wconversion -Wshadow)
    if(WCM_WARNINGS_AS_ERRORS)
      target_compile_options(${target} PRIVATE -Werror)
    endif()
  endif()
endfunction()

add_library(wcm_core STATIC
  src/world.c
  src/witness.c
  src/snapshot.c
  src/intent.c
  src/concordance.c
  src/admission.c
  src/dependency.c
  src/rebind.c
  src/capability.c
  src/ingress.c
  src/metrics.c
  src/diagnostics.c
  src/anchor.c
  src/runtime.c
  src/event.c
  src/health.c
  src/persistence.c
  src/build.c
)
add_library(WCM::Core ALIAS wcm_core)
set_target_properties(wcm_core PROPERTIES EXPORT_NAME Core)
target_include_directories(wcm_core PUBLIC
  $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
  $<INSTALL_INTERFACE:include>)
target_compile_definitions(wcm_core PUBLIC WCM_PROFILE=${WCM_PROFILE_DEFINE})
target_compile_definitions(wcm_core PRIVATE WCM_BUILD_TAG="${WCM_BUILD_TAG}")
wcm_enable_warnings(wcm_core)
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang")
  target_compile_options(wcm_core PRIVATE -fno-common)
  if(WCM_ENABLE_HARDENING AND UNIX)
    target_compile_options(wcm_core PRIVATE -fstack-protector-strong)
  endif()
endif()

if(WCM_ENABLE_SANITIZERS AND WCM_ENABLE_TSAN)
  message(FATAL_ERROR "WCM_ENABLE_SANITIZERS and WCM_ENABLE_TSAN are mutually exclusive")
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND WCM_ENABLE_SANITIZERS)
  target_compile_options(wcm_core PUBLIC -fsanitize=address,undefined -fno-omit-frame-pointer)
  target_link_options(wcm_core PUBLIC -fsanitize=address,undefined)
endif()
if(CMAKE_C_COMPILER_ID MATCHES "GNU|Clang" AND WCM_ENABLE_TSAN)
  target_compile_options(wcm_core PUBLIC -fsanitize=thread -fno-omit-frame-pointer)
  target_link_options(wcm_core PUBLIC -fsanitize=thread)
endif()

if(WCM_BUILD_PORT_TEMPLATE_CHECK)
  add_library(wcm_port_template_check OBJECT ports/template/wcm_target_port.c)
  target_link_libraries(wcm_port_template_check PRIVATE WCM::Core)
  wcm_enable_warnings(wcm_port_template_check)
endif()

if(WCM_BUILD_POSIX_PORT)
  find_package(Threads REQUIRED)
  add_library(wcm_port_posix STATIC ports/posix/wcm_posix.c)
  add_library(WCM::PortPOSIX ALIAS wcm_port_posix)
  set_target_properties(wcm_port_posix PROPERTIES EXPORT_NAME PortPOSIX)
  target_include_directories(wcm_port_posix PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/ports/posix/include>
    $<INSTALL_INTERFACE:include>)
  target_link_libraries(wcm_port_posix PUBLIC WCM::Core Threads::Threads)
  wcm_enable_warnings(wcm_port_posix)
endif()

include(GNUInstallDirs)
include(CMakePackageConfigHelpers)
set(WCM_CONFIG_HAS_POSIX_PORT ${WCM_BUILD_POSIX_PORT})
configure_package_config_file(
  cmake/WCMCoreConfig.cmake.in
  ${CMAKE_CURRENT_BINARY_DIR}/WCMCoreConfig.cmake
  INSTALL_DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/WCMCore)
configure_file(cmake/wcm-core.pc.in ${CMAKE_CURRENT_BINARY_DIR}/wcm-core.pc @ONLY)

write_basic_package_version_file(
  ${CMAKE_CURRENT_BINARY_DIR}/WCMCoreConfigVersion.cmake
  VERSION ${PROJECT_VERSION}
  COMPATIBILITY SameMajorVersion)

install(TARGETS wcm_core EXPORT WCMCoreTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
if(WCM_BUILD_POSIX_PORT)
  install(TARGETS wcm_port_posix EXPORT WCMCoreTargets ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
  install(DIRECTORY ports/posix/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
endif()
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(DIRECTORY ports/baremetal/include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT WCMCoreTargets NAMESPACE WCM:: DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/WCMCore)
install(FILES ${CMAKE_CURRENT_BINARY_DIR}/wcm-core.pc DESTINATION ${CMAKE_INSTALL_LIBDIR}/pkgconfig)
install(FILES
  ${CMAKE_CURRENT_BINARY_DIR}/WCMCoreConfig.cmake
  ${CMAKE_CURRENT_BINARY_DIR}/WCMCoreConfigVersion.cmake
  DESTINATION ${CMAKE_INSTALL_LIBDIR}/cmake/WCMCore)

add_executable(wcm_size_report tools/size_report.c)
target_link_libraries(wcm_size_report PRIVATE WCM::Core)
wcm_enable_warnings(wcm_size_report)

if(WCM_BUILD_TESTS)
  enable_testing()
  add_executable(wcm_tests tests/test_main.c)
  target_include_directories(wcm_tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/tests/support)
  target_link_libraries(wcm_tests PRIVATE WCM::Core)
  wcm_enable_warnings(wcm_tests)
  add_test(NAME wcm_core_tests COMMAND wcm_tests)

  if(WCM_BUILD_POSIX_PORT)
    add_executable(wcm_posix_stress tests/posix_stress.c)
    target_link_libraries(wcm_posix_stress PRIVATE WCM::Core WCM::PortPOSIX Threads::Threads)
    wcm_enable_warnings(wcm_posix_stress)
    add_test(NAME wcm_posix_stress COMMAND wcm_posix_stress)
  endif()
endif()

if(WCM_BUILD_INTEGRATION_EXAMPLE)
  add_executable(wcm_integration_skeleton examples/integration_skeleton.c)
  target_link_libraries(wcm_integration_skeleton PRIVATE WCM::Core)
  wcm_enable_warnings(wcm_integration_skeleton)
  if(WCM_BUILD_TESTS)
    add_test(NAME wcm_integration_skeleton COMMAND wcm_integration_skeleton)
  endif()
endif()

if(WCM_BUILD_REFERENCE)
  add_executable(wcm_reference_loop examples/reference_control_loop.c)
  target_link_libraries(wcm_reference_loop PRIVATE WCM::Core)
  wcm_enable_warnings(wcm_reference_loop)
  if(WCM_BUILD_TESTS)
    add_test(NAME wcm_reference_loop COMMAND wcm_reference_loop)
  endif()
endif()
