cmake_minimum_required(VERSION 3.28)

project(sampraknet)

option(RAKNET_DLL "Build as shared library" OFF)
option(RAKNET_THREADSAFE "Build threadsafe variant of RakNet" OFF)

set(RAKNET_SOURCES 
	src/BitStream.cpp
	src/CheckSum.cpp
	src/DataBlockEncryptor.cpp
	src/DS_HuffmanEncodingTree.cpp
	src/GetTime.cpp
	src/InternalPacketPool.cpp
	src/NetworkIDGenerator.cpp
	src/NetworkTypes.cpp
	src/PluginInterface.cpp
	src/RakClient.cpp
	src/RakNetworkFactory.cpp
	src/RakPeer.cpp
	src/RakServer.cpp
	src/RakSleep.cpp
	src/Rand.cpp
	src/ReliabilityLayer.cpp
	src/rijndael.cpp
	src/RPCMap.cpp
	src/SAMPAuth.cpp
	src/SAMPNetEncr.cpp
	src/SAMPRPC.cpp
	src/SHA1.cpp
	src/SimpleMutex.cpp
	src/SocketLayer.cpp
	src/StringCompressor.cpp
	src/StringTable.cpp
)

if(RAKNET_DLL)
	add_library(raknet SHARED ${RAKNET_SOURCES})
	target_compile_definitions(raknet PRIVATE _RAKNET_DLL)
else()
	add_library(raknet STATIC ${RAKNET_SOURCES})
endif()

if(RAKNET_THREADSAFE)
	target_compile_definitions(raknet PRIVATE _RAKNET_THREADSAFE)
endif()

if(WIN32)
	target_compile_definitions(raknet PRIVATE _CRT_SECURE_NO_WARNINGS _WINSOCK_DEPRECATED_NO_WARNINGS)
	target_link_libraries(raknet PRIVATE ws2_32)
endif()
target_include_directories(raknet PUBLIC include)