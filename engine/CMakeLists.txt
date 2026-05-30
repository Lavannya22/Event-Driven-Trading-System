add_library(engine STATIC
    events/Event.cpp
    orderbook/OrderBook.cpp
    matching/MatchingEngine.cpp
    strategy/StrategyEngine.cpp
    replay/SequentialReplayController.cpp
    persistence/PostgresWriter.cpp
)

target_include_directories(engine PUBLIC ${CMAKE_SOURCE_DIR})

# PostgreSQL persistence — optional; no-ops gracefully when libpq-dev is absent.
# Install on Ubuntu: sudo apt install -y libpq-dev
find_package(PostgreSQL)
if(PostgreSQL_FOUND)
    target_link_libraries(engine PUBLIC PostgreSQL::PostgreSQL)
    target_compile_definitions(engine PUBLIC WITH_POSTGRES=1)
    message(STATUS "PostgreSQL found — persistence layer enabled")
else()
    message(STATUS "PostgreSQL not found — persistence layer compiled as no-ops (install libpq-dev to enable)")
endif()
