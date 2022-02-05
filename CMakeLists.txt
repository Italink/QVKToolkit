cmake_minimum_required(VERSION 3.12)

project(QVKToolkit CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets OpenGLWidgets REQUIRED)

add_definitions(-DVK_NO_PROTOTYPES)

add_subdirectory(3rdParty/glslang)
add_subdirectory(3rdParty/assimp)

macro(get_all_targets_recursive targets dir)
    get_property(subdirectories DIRECTORY ${dir} PROPERTY SUBDIRECTORIES)
    foreach(subdir ${subdirectories})
        get_all_targets_recursive(${targets} ${subdir})
    endforeach()
    get_property(current_targets DIRECTORY ${dir} PROPERTY BUILDSYSTEM_TARGETS)
    list(APPEND ${targets} ${current_targets})
endmacro()
get_all_targets(TARGET_LIST)
foreach(TARGET_ITEM ${TARGET_LIST})
    set_target_properties(${TARGET_ITEM} PROPERTIES FOLDER "3rdParty")
endforeach()

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

file(GLOB_RECURSE PROJECT_SOURCE FILES
	Core/*.h  Core/*.cpp
	Effect/*.h  Effect/*.cpp
	Renderer/*.h  Renderer/*.cpp
)

set(BUILD_LIBRARY ON)

if(BUILD_LIBRARY)
	add_library(QVKToolkit ${PROJECT_SOURCE})
else()
	add_executable(QVKToolkit ${PROJECT_SOURCE} main.cpp)	
endif()

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PROJECT_SOURCE})

target_link_libraries(QVKToolkit PRIVATE 
	Qt${QT_VERSION_MAJOR}::Widgets
	Qt${QT_VERSION_MAJOR}::WidgetsPrivate
	Qt${QT_VERSION_MAJOR}::OpenGL 
	Qt${QT_VERSION_MAJOR}::OpenGLPrivate 
	glslang SPIRV glslang-default-resource-limits
	assimp::assimp
 )

