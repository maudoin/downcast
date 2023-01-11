cmake_minimum_required(VERSION 3.5)

project(downcast_winlibs_mingw)
#tested under windows10 with https://github.com/brechtsanders/winlibs_mingw/releases/download/12.2.0-14.0.6-10.0.0-msvcrt-r2/winlibs-x86_64-posix-seh-gcc-12.2.0-llvm-14.0.6-mingw-w64msvcrt-10.0.0-r2.7z
#see https://winlibs.com/
set(WINDOWS_KIT_VERS "10.0.19041.0")
set(WINDOWS_10_KIT_DIR "C:/Program Files (x86)/Windows Kits/10")

if(NOT EXISTS ${WINDOWS_10_KIT_DIR})
    message("Please setup WINDOWS_10_KIT_DIR var manually, there is no autodetection")
endif()

######## compiler setup

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_CXX_LINK_FLAGS                "-static-libgcc -static-libstdc++")
set(CMAKE_CXX_FLAGS_INIT                "-Wall")
set(CMAKE_CXX_FLAGS_DEBUG_INIT          "-g")
set(CMAKE_CXX_FLAGS_MINSIZEREL_INIT     "-Os -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELEASE_INIT        "-O3 -DNDEBUG")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO_INIT "-O2 -g")

find_package(Threads REQUIRED)

######## sokol DX11 setup
ADD_DEFINITIONS(-DSOKOL_D3D11)
######## DX11 setup
find_library(D3D11_LIBRARY d3d11.lib
    PATHS
      "$ENV{DXSDK_DIR}/Lib/x86"
      "${WINDOWS_10_KIT_DIR}/Lib/${WINDOWS_KIT_VERS}/um/x64"
    DOC
      "The directory where d3d11.lib resides"
      )
######## openssl
set(OPENSSL_ROOT_DIR "${CMAKE_SOURCE_DIR}/3rdparties/openssl-1.1.1k-win64-mingw")
set(OPENSSL_USE_STATIC_LIBS TRUE)
find_package(OpenSSL REQUIRED)

######## threads
find_package(Threads REQUIRED)

######## upx
find_program(UPX_EXECUTABLE
 upx.exe
 PATHS
 ${CMAKE_SOURCE_DIR}/3rdparties
)
mark_as_advanced(
  UPX_EXECUTABLE
)

function(pack_target PACK_TGT_NAME)
    if (${UPX_EXECUTABLE} STREQUAL "UPX_EXECUTABLE-NOTFOUND")
        message("Missing exe packer ${UPX_EXECUTABLE}")
    else()
      ADD_CUSTOM_COMMAND(
        DEPENDS ${PACK_TGT_NAME}
        COMMAND $<IF:$<CONFIG:Release>,${UPX_EXECUTABLE},echo>
        ARGS $<$<CONFIG:Debug>:"Skipping upx"> --force $<TARGET_FILE:${PACK_TGT_NAME}>
        TARGET ${PACK_TGT_NAME})
    endif()
endfunction()
######## signer

set(SIGN_CERT_FILE "${CMAKE_SOURCE_DIR}/src/resources/immomedia.pfx")

find_program(SIGNTOOL
 signtool.exe
 PATHS
 "${WINDOWS_10_KIT_DIR}/bin/${WINDOWS_KIT_VERS}/x64"
)
mark_as_advanced(
  SIGNTOOL
)

function(code_sign_target_files PACK_TGT_NAME)
  if (${SIGNTOOL} STREQUAL "SIGNTOOL-NOTFOUND")
    message("Missing signtool")
  else()
    if(DEFINED ENV{SIGN_PASSWORD})
        ADD_CUSTOM_COMMAND(
          DEPENDS ${PACK_TGT_NAME}
          COMMAND $<IF:$<CONFIG:Release>,${SIGNTOOL},echo>
          ARGS $<$<CONFIG:Debug>:"Skipping signing"> sign /f ${SIGN_CERT_FILE} /p $ENV{SIGN_PASSWORD} /fd sha256 /tr http://timestamp.digicert.com /td sha256 /v $<TARGET_FILE:${PACK_TGT_NAME}>
          TARGET ${PACK_TGT_NAME})
    else()
        message("define SIGN_PASSWORD env variable to sign the executable")
    endif()
  endif()
endfunction()

######## projects
add_subdirectory(3rdparties)
add_subdirectory(src)
add_subdirectory(test)
