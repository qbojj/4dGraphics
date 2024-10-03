include(../CMake/CommonMacros.cmake)
project(4dGraphics)

file(GLOB SOURCES CONFIGURE_DEPENDS *.cpp)
file(GLOB HEADERS CONFIGURE_DEPENDS *.hpp *.h)
file(GLOB MODULE_FILES CONFIGURE_DEPENDS *.cppm)

add_executable(4dGraphics WIN32)
target_sources(4dGraphics
PRIVATE  ${SOURCES}
PUBLIC FILE_SET HEADERS BASE_DIRS ${CMAKE_CURRENT_SOURCE_DIR} FILES
       ${HEADERS}
)

add_dependencies(4dGraphics Shaders)
target_compile_features(4dGraphics PRIVATE cxx_std_23)
target_link_libraries(4dGraphics PRIVATE v4dg)

install(TARGETS 4dGraphics)
