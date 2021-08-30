#Boost
find_package(Boost REQUIRED COMPONENTS program_options)
if(NOT Boost_FOUND)
    message("Not found Boost")
endif()
include_directories(${Boost_INCLUDE_DIRS})

#set(Boost_USE_STATIC_LIBS ON)
#set(Boost_USE_MULTITHREADED ON)
#set(Boost_USE_STATIC_RUNTIME OFF)
#include_directories("${PROJECT_SOURCE_DIR}/general")

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -fopenmp -fPIC  -funroll-loops -march=native")
SET(CMAKE_BUILD_TYPE "Release")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -msse -msse2 -msse3 -funroll-loops -std=c++11 -Ofast -march=native -ffast-math -fopenmp -fPIC")


add_executable(Vis main.cpp data.cpp)
target_link_libraries(Vis ${Boost_LIBRARIES} pthread gsl gslcblas)
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR})

