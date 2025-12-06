cmake_minimum_required(VERSION 3.16)
project(IoTPlatform VERSION 2.0.0)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Configuration options
option(WITH_YAML "Enable YAML config file support" OFF)

# Find packages
find_package(Threads REQUIRED)

# Find PostgreSQL
find_package(PostgreSQL REQUIRED)

# Find libpqxx through pkg-config
find_package(PkgConfig REQUIRED)
pkg_check_modules(LIBPQXX REQUIRED libpqxx)

# Other required libraries
find_package(CURL REQUIRED)
find_package(nlohmann_json 3.9.1 REQUIRED)
find_package(OpenSSL REQUIRED)
find_package(cpr REQUIRED)

# Optional yaml-cpp
if(WITH_YAML)
    find_package(yaml-cpp QUIET)
    if(yaml-cpp_FOUND)
        add_definitions(-DHAS_YAML_CPP)
        message(STATUS "Found yaml-cpp: ${yaml-cpp_VERSION}")
    else()
        message(WARNING "yaml-cpp not found, YAML config support disabled")
        set(WITH_YAML OFF)
    endif()
endif()

# Include directories
include_directories(
    ${PROJECT_SOURCE_DIR}/src
    ${PostgreSQL_INCLUDE_DIRS}
    ${LIBPQXX_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
)

# Source files
set(SOURCES
    src/main.cpp
    src/core/Application.cpp
    src/core/ConfigManager.cpp
    src/core/Database.cpp
    src/core/DatabaseMigrator.cpp
    src/core/NotificationService.cpp
    # НОВЫЙ ФАЙЛ:
    src/core/RemoteDatabaseConnection.cpp
    src/engine/RuleEngine.cpp
    src/api/Server.cpp
    src/api/TelemetryServerImpl.cpp
    src/bot/TelegramBotHandler.cpp
    src/services/AlertService.cpp
    src/simulation/DeviceSimulator.cpp
    src/smtp/EmailService.cpp
    src/smtp/EmailConfig.cpp
    src/utils/Formatter.cpp
)

# Add executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Link libraries - ВАЖНЫЙ МОМЕНТ: правильный порядок линковки
target_include_directories(${PROJECT_NAME} PRIVATE
    ${PostgreSQL_INCLUDE_DIRS}
    ${LIBPQXX_INCLUDE_DIRS}
    ${CURL_INCLUDE_DIRS}
)

# Линковка: сначала libpqxx, потом libpq
target_link_libraries(${PROJECT_NAME} PRIVATE
    Threads::Threads
    ${LIBPQXX_LIBRARIES}     # libpqxx ДОЛЖЕН БЫТЬ ПЕРЕД libpq
    ${PostgreSQL_LIBRARIES}  # libpq
    CURL::libcurl
    nlohmann_json::nlohmann_json
    OpenSSL::SSL
    OpenSSL::Crypto
    cpr::cpr
)

# Optional yaml-cpp
if(WITH_YAML AND yaml-cpp_FOUND)
    target_link_libraries(${PROJECT_NAME} PRIVATE yaml-cpp)
endif()

# Installation
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# Copy config files
configure_file(${PROJECT_SOURCE_DIR}/config.yaml ${CMAKE_CURRENT_BINARY_DIR}/config.yaml COPYONLY)
configure_file(${PROJECT_SOURCE_DIR}/.env ${CMAKE_CURRENT_BINARY_DIR}/.env COPYONLY)

# Add compile definitions for PostgreSQL
add_definitions(-DHAVE_PQXX)