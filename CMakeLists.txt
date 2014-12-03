project(HandmadeHero)
cmake_minimum_required(VERSION 2.8)
aux_source_directory(./src SRC_LIST)
include_directories(./src /usr/local/include)

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake/")

set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -Wall -g -std=c11")
#set(SDL_BUILDING_LIBRARY)
find_package(SDL2)


add_executable(${PROJECT_NAME} ${SRC_LIST})

message("Test ${SDL2_LIBRARY}") 
target_link_libraries(${PROJECT_NAME} ${SDL2_LIBRARY})

