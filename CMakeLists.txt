cmake_minimum_required(VERSION 3.12)

project(QVKToolkit CXX)

set_property(GLOBAL PROPERTY USE_FOLDERS ON)

find_package(QT NAMES Qt6 Qt5 COMPONENTS Widgets REQUIRED)
find_package(Qt${QT_VERSION_MAJOR} COMPONENTS Widgets REQUIRED)

add_definitions(-DVK_NO_PROTOTYPES)

add_subdirectory(3rdParty/glslang)

set(CMAKE_AUTOMOC ON)
set(CMAKE_INCLUDE_CURRENT_DIR ON)

file(GLOB_RECURSE PROJECT_SOURCE FILES
	Core/*.h  Core/*.cpp
	Effect/*.h  Effect/*.cpp
	Shape/*.h  Shape/*.cpp
)

add_executable(QVKToolkit ${moc_QVKWindow} ${PROJECT_SOURCE} 	main.cpp)

source_group(TREE ${CMAKE_CURRENT_SOURCE_DIR} FILES ${PROJECT_SOURCE})

target_link_libraries(QVKToolkit PRIVATE Qt${QT_VERSION_MAJOR}::Widgets Qt${QT_VERSION_MAJOR}::WidgetsPrivate glslang SPIRV glslang-default-resource-limits)

