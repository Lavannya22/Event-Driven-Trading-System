include(FetchContent)

# ── uSockets (C transport layer for uWebSockets) ─────────────────────────────
FetchContent_Declare(usockets_src
    GIT_REPOSITORY https://github.com/uNetworking/uSockets.git
    GIT_TAG        v0.8.8
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(usockets_src)

# Collect platform sources: core + epoll/kqueue eventing, skip SSL and libuv.
file(GLOB USOCKETS_SOURCES
    "${usockets_src_SOURCE_DIR}/src/*.c"
    "${usockets_src_SOURCE_DIR}/src/eventing/epoll_kqueue.c"
)
list(FILTER USOCKETS_SOURCES EXCLUDE REGEX ".*(libuv|openssl|wolfssl|boringssl|gcd).*")

add_library(usockets STATIC ${USOCKETS_SOURCES})
target_include_directories(usockets PUBLIC "${usockets_src_SOURCE_DIR}/src")
target_compile_definitions(usockets PUBLIC LIBUS_NO_SSL=1)
set_target_properties(usockets PROPERTIES C_STANDARD 11)

# ── uWebSockets (header-only C++ layer) ──────────────────────────────────────
FetchContent_Declare(uwebsockets_src
    GIT_REPOSITORY https://github.com/uNetworking/uWebSockets.git
    GIT_TAG        v20.62.0
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(uwebsockets_src)

add_library(uwebsockets INTERFACE)
target_include_directories(uwebsockets INTERFACE "${uwebsockets_src_SOURCE_DIR}/src")
# Propagate LIBUS_NO_SSL so DashboardServer.cpp sees it when including <App.h>
target_compile_definitions(uwebsockets INTERFACE LIBUS_NO_SSL=1)
target_link_libraries(uwebsockets INTERFACE usockets z pthread)

# ── nlohmann/json ─────────────────────────────────────────────────────────────
set(JSON_BuildTests OFF CACHE BOOL "" FORCE)
set(JSON_Install    OFF CACHE BOOL "" FORCE)
FetchContent_Declare(nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG        v3.11.3
    GIT_SHALLOW    TRUE
)
FetchContent_MakeAvailable(nlohmann_json)
# Creates target: nlohmann_json::nlohmann_json

# ── dashboard_server executable ───────────────────────────────────────────────
add_executable(dashboard_server
    DashboardServer.cpp
    main.cpp
)

target_include_directories(dashboard_server PRIVATE ${CMAKE_SOURCE_DIR})

target_link_libraries(dashboard_server
    PRIVATE engine
    PRIVATE uwebsockets
    PRIVATE nlohmann_json::nlohmann_json
)
