#CXX
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -fopenmp -fPIC  -funroll-loops -march=native")
SET(CMAKE_BUILD_TYPE "Debug")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -msse -msse2 -msse3 -funroll-loops -std=c++11 -Ofast -march=native -ffast-math -fopenmp -fPIC")



#Boost
set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost REQUIRED COMPONENTS program_options)
if(NOT Boost_FOUND)
    message("Boost not found")
endif()
include_directories(${Boost_INCLUDE_DIRS})

#Progress Bar
include_directories("${PROJECT_SOURCE_DIR}/include")

#include_directories("${PROJECT_SOURCE_DIR}/general")

add_executable(Vis main.cpp timer.cpp random.cpp data.cpp graph.cpp multilevel.cpp aliasSampler.cpp optimization.cpp)
target_link_libraries(Vis ${Boost_LIBRARIES} pthread gsl gslcblas)
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR})

