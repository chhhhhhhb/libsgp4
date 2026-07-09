cmake_minimum_required(VERSION 3.14)
project(libsgp4
    VERSION 1.0.0
    DESCRIPTION "SGP4 satellite orbit propagation library"
    LANGUAGES CXX
)

# Output directories
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib)

# C++17 required for: nested namespace definitions, std::chrono, inline variables
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Source files
set(SRCS
    src/Eci.cpp
    src/OrbitalElements.cpp
    src/SGP4.cpp
    src/Tle.cpp
    src/Util.cpp
)

# Public headers
set(INCS
    include/CoordGeodetic.h
    include/DateTime.h
    include/DecayedException.h
    include/Eci.h
    include/Globals.h
    include/OrbitalElements.h
    include/SatelliteException.h
    include/SGP4.h
    include/TimeSpan.h
    include/Tle.h
    include/TleException.h
    include/Util.h
    include/Vector.h
)

# Static library
add_library(sgp4 STATIC ${SRCS} ${INCS})
target_include_directories(sgp4 PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Shared library
add_library(sgp4s SHARED ${SRCS} ${INCS})
target_include_directories(sgp4s PUBLIC ${CMAKE_CURRENT_SOURCE_DIR}/include)

# Set shared library version properties and output name
set_target_properties(sgp4s PROPERTIES
    VERSION ${PROJECT_VERSION}
    SOVERSION ${PROJECT_VERSION_MAJOR}
    RUNTIME_OUTPUT_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}/lib
    OUTPUT_NAME "sgp4"
)

# Static library output name
set_target_properties(sgp4 PROPERTIES
    OUTPUT_NAME "sgp4"
)

# # Test executable
# if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/test/test.cpp)
#     add_executable(sgp4_test ${CMAKE_CURRENT_SOURCE_DIR}/test/test.cpp)
#     target_link_libraries(sgp4_test PRIVATE sgp4)
# endif()

# Install rules
include(GNUInstallDirs)
install(TARGETS sgp4s LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(TARGETS sgp4 ARCHIVE DESTINATION ${CMAKE_INSTALL_LIBDIR})
install(FILES ${INCS} DESTINATION ${CMAKE_INSTALL_INCLUDEDIR}/libsgp4)