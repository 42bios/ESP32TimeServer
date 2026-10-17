# U8g2 Arduino tag 2.36.17, pinned by the 3rdparty Git submodule.
set(U8G2_SRC "${CMAKE_SOURCE_DIR}/3rdparty/U8g2_Arduino/src")
file(GLOB U8G2_C_SOURCES "${U8G2_SRC}/clib/*.c")
idf_component_register(SRCS ${U8G2_C_SOURCES} "${U8G2_SRC}/U8g2lib.cpp" "${U8G2_SRC}/U8x8lib.cpp"
    INCLUDE_DIRS "${U8G2_SRC}" "${U8G2_SRC}/clib" REQUIRES arduino-esp32)
