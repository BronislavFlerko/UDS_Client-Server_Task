cmake_minimum_required(VERSION 3.5)

project(UDS_Server LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

add_definitions(-DMULTIMETER_CHANNELS=2)

add_executable(UDS_Server server.cpp
    multimeter.h
    multimeter.cpp
    client.cpp
    main.cpp
    server.h
    client.h


)

include(GNUInstallDirs)
install(TARGETS UDS_Server
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
