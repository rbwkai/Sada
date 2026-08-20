cmake_minimum_required(VERSION 3.14)
project(sada CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(SQLite3 REQUIRED)

add_executable(sada
    main.cpp
    db.cpp
    dataset.cpp
    engine.cpp
    card.cpp
)
target_link_libraries(sada PRIVATE SQLite3::SQLite3)

# Where `make install` puts the bundled word list; baked into the binary so
# it can find that file on first run (see dataset.cpp). Tracks
# CMAKE_INSTALL_PREFIX unless explicitly overridden with -DSADA_SHARE_DIR=...
if(NOT SADA_SHARE_DIR)
    set(SADA_SHARE_DIR "${CMAKE_INSTALL_PREFIX}/share/sada")
endif()
target_compile_definitions(sada PRIVATE SADA_SHARE_DIR="${SADA_SHARE_DIR}")

install(TARGETS sada RUNTIME DESTINATION bin)
install(FILES data/sada_arabic_2000.csv DESTINATION ${SADA_SHARE_DIR})
