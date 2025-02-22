cmake_minimum_required(VERSION 3.4.3)

set(CMAKE_VERBOSE_MAKEFILE ON)

set(CMAKE_CXX_STANDARD 11)

set (HOMEKIT_ADK "HomeKitADK")
set (fdk-aac "fdk-aac")

# Set the project name
project(positron)

# Tell CMake where to look for header files
include_directories(${CMAKE_SOURCE_DIR}/)
include_directories(${CMAKE_SOURCE_DIR}/include/imp_sys)
include_directories( "include"
    "Camera"
	"DiffTests"
    "${fdk-aac}/libAACenc/include"
    "${fdk-aac}/libAACdec/include"
    "${fdk-aac}/libArithCoding/include"
    "${fdk-aac}/libDRCdec/include"
    "${fdk-aac}/libSBRenc/include"
    "${fdk-aac}/libSBRdec/include"
    "${fdk-aac}/libFDK/include"
    "${fdk-aac}/libSYS/include"
	"${fdk-aac}/libMpegTPEnc/include"
	"${fdk-aac}/libMpegTPDec/include"
	"${fdk-aac}/libPCMutils/include"
	"${fdk-aac}/libSACenc/include"
	"${fdk-aac}/libSACdec/include"
    "${HOMEKIT_ADK}/HAP"
    "${HOMEKIT_ADK}/PAL"
    "${HOMEKIT_ADK}/PAL/Linux"
    "${HOMEKIT_ADK}/External/Base64"
    "${HOMEKIT_ADK}/External/JSON"
    "${HOMEKIT_ADK}/External/HTTP"
    )

# turbojpeg
find_package( PkgConfig )
#pkg_check_modules(JPEG_LIB REQUIRED libturbojpeg)
find_library( JPEG_LIB turbojpeg )

include_directories(${JPEG_LIB_INCLUDE_DIRS})

# dns_sd
find_library( DNS_SD_LIB dns_sd )

include_directories(${DNS_SD_LIB_INCLUDE_DIRS})

# pkg_check_modules(AVAHI REQUIRED libdns_sd)
# include_directories(${AVAHI_INCLUDE_DIRS})
# find_library(avahi-client-lib avahi-client REQUIRED)
# find_library(avahi-common-lib avahi-common REQUIRED)
# find_path(avahi-client-includes avahi-client/client.h REQUIRED)
# find_path(avahi-common-includes avahi-common/defs.h REQUIRED)


# mbedtls

find_library(MBEDTLS_LIBRARY mbedtls)
find_library(MBEDX509_LIBRARY mbedx509)
find_library(MBEDCRYPTO_LIBRARY mbedcrypto)

set(MBEDTLS_LIBRARIES "${MBEDTLS_LIBRARY}" "${MBEDX509_LIBRARY}" "${MBEDCRYPTO_LIBRARY}")

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(MBEDTLS DEFAULT_MSG
    MBEDTLS_INCLUDE_DIRS MBEDTLS_LIBRARY MBEDX509_LIBRARY MBEDCRYPTO_LIBRARY)

mark_as_advanced(MBEDTLS_INCLUDE_DIRS MBEDTLS_LIBRARY MBEDX509_LIBRARY MBEDCRYPTO_LIBRARY)




# The source files are all the *.c files
file(GLOB POSITRON_SRC_FILES 
	"Camera/*"
    "${fdk-aac}/libAACenc/src/*.cpp"
    "${fdk-aac}/libAACdec/src/*.cpp"
    "${fdk-aac}/libArithCoding/src/*.cpp"
    "${fdk-aac}/libDRCdec/src/*.cpp"
    "${fdk-aac}/libSBRenc/src/*.cpp"
    "${fdk-aac}/libSBRdec/src/*.cpp"
    "${fdk-aac}/libFDK/src/*.cpp"
    "${fdk-aac}/libSYS/src/*.cpp"
    "${fdk-aac}/libMpegTPEnc/src/*.cpp"
    "${fdk-aac}/libMpegTPDec/src/*.cpp"
    "${fdk-aac}/libPCMutils/src/*.cpp"
    "${fdk-aac}/libSACenc/src/*.cpp"
    "${fdk-aac}/libSACdec/src/*.cpp"
	"${HOMEKIT_ADK}/PAL/Linux/HAPPlatform.c"
	"${HOMEKIT_ADK}/PAL/Linux/HAPPlatformAbort.c"
	"${HOMEKIT_ADK}/PAL/Linux/HAPPlatformAccessorySetup.c"
	"${HOMEKIT_ADK}/PAL/Linux/HAPPlatformAccessorySetupDisplay.c"
	"${HOMEKIT_ADK}/PAL/Linux/HAPPlatformAccessorySetupNFC.c"
	"${HOMEKIT_ADK}/PAL/Linux/HAPPlatformBLEPeripheralManager.c"
	"${HOMEKIT_ADK}/PAL/Linux/HAPPlatformClock.c"
	"${HOMEKIT_ADK}/PAL/Linux/HAPPlatformKeyValueStore.c"
	"${HOMEKIT_ADK}/PAL/Linux/HAPPlatformLog.c"
	"${HOMEKIT_ADK}/PAL/Linux/HAPPlatformMFiHWAuth.c"
	"${HOMEKIT_ADK}/PAL/Linux/HAPPlatformMFiTokenAuth.c"
	"${HOMEKIT_ADK}/PAL/Linux/HAPPlatformRandomNumber.c"
	"${HOMEKIT_ADK}/PAL/Linux/HAPPlatformRunLoop.c"
	"${HOMEKIT_ADK}/PAL/Linux/HAPPlatformServiceDiscovery.c"
	"${HOMEKIT_ADK}/PAL/Linux/HAPPlatformTCPStreamManager.c"
	"${HOMEKIT_ADK}/PAL/Linux/HAPPlatformFileManager.c"
	"${HOMEKIT_ADK}/PAL/HAPAssert.c"
	"${HOMEKIT_ADK}/PAL/HAPBase+Crypto.c"
	"${HOMEKIT_ADK}/PAL/HAPBase+Double.c"
	"${HOMEKIT_ADK}/PAL/HAPBase+Float.c"
	"${HOMEKIT_ADK}/PAL/HAPBase+Int.c"
	"${HOMEKIT_ADK}/PAL/HAPBase+MACAddress.c"
	"${HOMEKIT_ADK}/PAL/HAPBase+RawBuffer.c"
	"${HOMEKIT_ADK}/PAL/HAPBase+Sha1Checksum.c"
	"${HOMEKIT_ADK}/PAL/HAPBase+String.c"
	"${HOMEKIT_ADK}/PAL/HAPBase+UTF8.c"
	"${HOMEKIT_ADK}/PAL/HAPLog.c"
	"${HOMEKIT_ADK}/PAL/HAPPlatformSystemInit.c"
	"${HOMEKIT_ADK}/PAL/Crypto/MbedTLS/HAPMbedTLS.c"
	"${HOMEKIT_ADK}/HAP/HAPAccessory+Info.c"
	"${HOMEKIT_ADK}/HAP/HAPAccessoryServer+Reset.c"
	"${HOMEKIT_ADK}/HAP/HAPAccessoryServer.c"
	"${HOMEKIT_ADK}/HAP/HAPAccessorySetup.c"
	"${HOMEKIT_ADK}/HAP/HAPAccessorySetupInfo.c"
	"${HOMEKIT_ADK}/HAP/HAPAccessoryValidation.c"
	"${HOMEKIT_ADK}/HAP/HAPBLEAccessoryServer+Advertising.c"
	"${HOMEKIT_ADK}/HAP/HAPBLEAccessoryServer+Broadcast.c"
	"${HOMEKIT_ADK}/HAP/HAPBLEAccessoryServer.c"
	"${HOMEKIT_ADK}/HAP/HAPBLECharacteristic+Broadcast.c"
	"${HOMEKIT_ADK}/HAP/HAPBLECharacteristic+Configuration.c"
	"${HOMEKIT_ADK}/HAP/HAPBLECharacteristic+Signature.c"
	"${HOMEKIT_ADK}/HAP/HAPBLECharacteristic.c"
	"${HOMEKIT_ADK}/HAP/HAPBLECharacteristicParseAndWriteValue.c"
	"${HOMEKIT_ADK}/HAP/HAPBLECharacteristicReadAndSerializeValue.c"
	"${HOMEKIT_ADK}/HAP/HAPBLEPDU+TLV.c"
	"${HOMEKIT_ADK}/HAP/HAPBLEPDU.c"
	"${HOMEKIT_ADK}/HAP/HAPBLEPeripheralManager.c"
	"${HOMEKIT_ADK}/HAP/HAPBLEProcedure.c"
	"${HOMEKIT_ADK}/HAP/HAPBLEProtocol+Configuration.c"
	"${HOMEKIT_ADK}/HAP/HAPBLEService+Signature.c"
	"${HOMEKIT_ADK}/HAP/HAPBLESession.c"
	"${HOMEKIT_ADK}/HAP/HAPBLETransaction.c"
	"${HOMEKIT_ADK}/HAP/HAPBitSet.c"
	"${HOMEKIT_ADK}/HAP/HAPCharacteristic.c"
	"${HOMEKIT_ADK}/HAP/HAPCharacteristicTypes.c"
	"${HOMEKIT_ADK}/HAP/HAPDeviceID.c"
	"${HOMEKIT_ADK}/HAP/HAPIP+ByteBuffer.c"
	"${HOMEKIT_ADK}/HAP/HAPIPAccessory.c"
	"${HOMEKIT_ADK}/HAP/HAPIPAccessoryProtocol.c"
	"${HOMEKIT_ADK}/HAP/HAPIPAccessoryServer.c"
	"${HOMEKIT_ADK}/HAP/HAPIPCharacteristic.c"
	"${HOMEKIT_ADK}/HAP/HAPIPSecurityProtocol.c"
	"${HOMEKIT_ADK}/HAP/HAPIPServiceDiscovery.c"
	"${HOMEKIT_ADK}/HAP/HAPJSONUtils.c"
	"${HOMEKIT_ADK}/HAP/HAPLegacyImport.c"
	"${HOMEKIT_ADK}/HAP/HAPMACAddress.c"
	"${HOMEKIT_ADK}/HAP/HAPMFiHWAuth.c"
	"${HOMEKIT_ADK}/HAP/HAPMFiTokenAuth.c"
	"${HOMEKIT_ADK}/HAP/HAPPDU.c"
	"${HOMEKIT_ADK}/HAP/HAPPairing.c"
	"${HOMEKIT_ADK}/HAP/HAPPairingBLESessionCache.c"
	"${HOMEKIT_ADK}/HAP/HAPPairingPairSetup.c"
	"${HOMEKIT_ADK}/HAP/HAPPairingPairVerify.c"
	"${HOMEKIT_ADK}/HAP/HAPPairingPairings.c"
	"${HOMEKIT_ADK}/HAP/HAPRequestHandlers+AccessoryInformation.c"
	"${HOMEKIT_ADK}/HAP/HAPRequestHandlers+HAPProtocolInformation.c"
	"${HOMEKIT_ADK}/HAP/HAPRequestHandlers+Pairing.c"
	"${HOMEKIT_ADK}/HAP/HAPRequestHandlers.c"
	"${HOMEKIT_ADK}/HAP/HAPServiceTypes.c"
	"${HOMEKIT_ADK}/HAP/HAPSession.c"
	"${HOMEKIT_ADK}/HAP/HAPStringBuilder.c"
	"${HOMEKIT_ADK}/HAP/HAPTLV.c"
	"${HOMEKIT_ADK}/HAP/HAPTLVMemory.c"
	"${HOMEKIT_ADK}/HAP/HAPTLVReader.c"
	"${HOMEKIT_ADK}/HAP/HAPTLVWriter.c"
	"${HOMEKIT_ADK}/HAP/HAPUUID.c"
	"${HOMEKIT_ADK}/HAP/HAPVersion.c"
	"${HOMEKIT_ADK}/External/JSON/util_json_reader.c"
	"${HOMEKIT_ADK}/External/HTTP/util_http_reader.c"
	"${HOMEKIT_ADK}/External/Base64/util_base64.c")


set (LOG_LEVEL_Debug 3)
set (LOG_LEVEL_Info 2)
set (LOG_LEVEL_Test 1)
set (LOG_LEVEL_Release 0)

add_definitions(-DHAP_LOG_LEVEL=${LOG_LEVEL_Debug})
add_definitions(-DIP=1)
add_definitions(-Werror)

######################
# Target Executables #
######################

add_executable(positron ${POSITRON_SRC_FILES})

#########################
# Linking Configuration #
#########################


set(CMAKE_THREAD_PREFER_PTHREAD TRUE)
set(THREADS_PREFER_PTHREAD_FLAG TRUE)
find_package (Threads)
target_link_libraries (positron Threads::Threads) 

# static link of stdc++ if available
if (STATICSTDCPP)
  include(CheckCXXCompilerFlag)
  CHECK_CXX_COMPILER_FLAG("-static-libgcc" CXX_SUPPORTS_STATIC_LIBGCC)
  if (CXX_SUPPORTS_STATIC_LIBGCC) 
    target_link_libraries (positron -static-libgcc)
  endif()    
  CHECK_CXX_COMPILER_FLAG("-static-libstdc++" CXX_SUPPORTS_STATIC_LIBSTDCPP)
  if (CXX_SUPPORTS_STATIC_LIBSTDCPP) 
    target_link_libraries (positron -static-libstdc++)
  endif()
endif()


target_link_libraries(positron ${CMAKE_SOURCE_DIR}/lib/imp_sys/uclibc/libimp.so )
target_link_libraries(positron ${CMAKE_SOURCE_DIR}/lib/imp_sys/uclibc/libsysutils.so )
target_link_libraries(positron ${CMAKE_SOURCE_DIR}/lib/imp_sys/uclibc/libalog.so )


target_link_libraries(positron ${MBEDTLS_LIBRARIES})
target_link_libraries(positron ${JPEG_LIB})
target_link_libraries(positron ${DNS_SD_LIB})
target_link_libraries(positron ${avahi-client-lib} ${avahi-common-lib})

# ALSA Sound Library
#find_library (LIB_ALSA_SOUND NAMES asound)
#target_link_libraries(positron ${LIB_ALSA_SOUND})
#message(STATUS "libalsasound.so path: ${LIB_ALSA_SOUND}")

set_property(TARGET positron PROPERTY C_STANDARD 99)

#get_cmake_property(_variableNames VARIABLES)
#list (SORT _variableNames)
#foreach (_variableName ${_variableNames})
#    message(STATUS "${_variableName}=${${_variableName}}")
#endforeach()

install(TARGETS positron DESTINATION bin)


