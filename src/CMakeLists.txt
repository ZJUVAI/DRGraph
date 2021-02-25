set(Boost_USE_STATIC_LIBS ON)
set(Boost_USE_MULTITHREADED ON)
set(Boost_USE_STATIC_RUNTIME OFF)
find_package(Boost REQUIRED COMPONENTS program_options)
include_directories(${Boost_INCLUDE_DIRS})
include_directories("${PROJECT_SOURCE_DIR}/general")

# set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -g -std=c++11 -fopenmp -fPIC  -funroll-loops -march=native")
SET(CMAKE_BUILD_TYPE "Release")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O3 -msse -msse2 -msse3 -funroll-loops -std=c++11 -Ofast -march=native -ffast-math -fopenmp -fPIC")
SET(EXECUTABLE_OUTPUT_PATH ${PROJECT_SOURCE_DIR})


if(NOT DEFINED ENV{DRGRAPH_GPU_COMPILE})
    add_executable(Vis main.cpp knn.cpp visualizemod.cpp algorithm/kmeans.c evaluation_DR.cpp 
        sp_evaluation.cpp evaluation_GL.cpp timer.cpp genrandom.cpp data.cpp space_partition.cpp quadtree.h quadtree.cpp multilevel.cpp)
    target_link_libraries(Vis pthread gsl gslcblas ${Boost_LIBRARIES})
else()
    enable_language("CUDA")
    find_package(CUDA QUIET REQUIRED)
    SET(CUDA_SEPARABLE_COMPILATION ON)
    SET(CUDA_HOST_COMPILER ${CXX})
    SET(CUDA_PROPAGATE_HOST_FLAGS OFF)
    set(CUDA_NVCC_FLAGS ${CUDA_NVCC_FLAGS};
                        -O3
                        -Xptxas -dlcm=cg
                        -gencode=arch=compute_30,code=sm_30
                        -gencode=arch=compute_35,code=sm_35
                        -gencode=arch=compute_50,code=sm_50
                        -gencode=arch=compute_52,code=sm_52
                        -gencode=arch=compute_60,code=sm_60
                        -gencode=arch=compute_61,code=sm_61
                        -gencode=arch=compute_70,code=sm_70
                        -std=c++11
                        -Xcompiler '-O3'
                        -Xcompiler '-fPIC'
                        -Xcompiler '-fopenmp'
                        -Xcompiler '-msse4'
                        -Xcompiler '-m64'
                        -Xcompiler '-mpopcnt'
                        -Xcompiler '-g'
                        )
    
    add_definitions(-DUSE_CUDA)
    add_executable(Vis main.cpp knn.cpp visualizemod.cu algorithm/kmeans.c evaluation_DR.cpp 
        sp_evaluation.cpp evaluation_GL.cpp timer.cpp genrandom.cpp data.cu space_partition.cpp quadtree.h quadtree.cpp multilevel.cpp)
    set_property(TARGET Vis PROPERTY CUDA_STANDARD 11)
    target_link_libraries(Vis pthread gsl gslcblas ${Boost_LIBRARIES}  ${CUDA_curand_LIBRARY})
endif()
