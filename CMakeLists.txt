cmake_minimum_required( VERSION 3.0 )

set( CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build )
set( CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_HOME_DIRECTORY}/build )
set(ASSIMP_NO_EXPORT ON CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(ASSIMP_BUILD_ASSIMP_TOOLS OFF CACHE BOOL "" FORCE)

project( smaug )
set(CMAKE_CXX_STANDARD 14)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(BIGG_EXAMPLES OFF CACHE BOOL "" FORCE)
set(BIGG_ASSET_GEN ON CACHE BOOL "" FORCE)

option(SKIP_PERFORMANCE_COMPARISON "Skip building performance comparison (which requires boost)" ON)

add_subdirectory( thirdparty/bigg )
add_subdirectory( thirdparty/portable-file-dialogs )
add_subdirectory( thirdparty/keyvalues )
add_subdirectory( thirdparty/assimp )
add_subdirectory( thirdparty/cereal )

include_directories( thirdparty )
add_subdirectory( Smaug )
