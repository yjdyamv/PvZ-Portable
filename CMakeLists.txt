cmake_minimum_required(VERSION 3.5)

project(re-plants-vs-zombies)

file(GLOB SOURCES
	*.cpp
	SexyAppFramework/*.cpp
	SexyAppFramework/graphics/*.cpp
	SexyAppFramework/imagelib/*.cpp
	SexyAppFramework/misc/*.cpp
	SexyAppFramework/paklib/*.cpp
	SexyAppFramework/sound/*.cpp
	SexyAppFramework/widget/*.cpp
	#SexyAppFramework/ogg/*.c
	SexyAppFramework/fcaseopen/*.c
	Sexy.TodLib/*.cpp
	Lawn/*.cpp
	Lawn/Widget/*.cpp
	Lawn/System/*.cpp
	SexyAppFramework/LawnProject.rc
)

# these files are included and must not be built in the project
list(REMOVE_ITEM SOURCES
	${CMAKE_CURRENT_SOURCE_DIR}/Sexy.TodLib/TodDrawTriangle.cpp
	${CMAKE_CURRENT_SOURCE_DIR}/Sexy.TodLib/TodDrawTriangleInc.cpp
)


option(CONSOLE "Show console on Windows" ON)

find_package(ZLIB REQUIRED)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)
find_package(SDL2 REQUIRED)
find_package(GLEW REQUIRED)

set(USE_MIDI OFF CACHE BOOL "SDL Mixer X midi" FORCE)
set(USE_MODPLUG OFF CACHE BOOL "SDL Mixer X modplug" FORCE)
set(USE_XMP OFF CACHE BOOL "SDL Mixer X xmp" FORCE)
set(USE_OPUS OFF CACHE BOOL "SDL Mixer X opus" FORCE)
set(USE_DRFLAC OFF CACHE BOOL "SDL Mixer X flac" FORCE)
set(USE_PXTONE OFF CACHE BOOL "SDL Mixer X pxtone" FORCE)
add_subdirectory(SexyAppFramework/sound/SDL-Mixer-X)

add_executable(re-plants-vs-zombies ${SOURCES})
target_include_directories(re-plants-vs-zombies PRIVATE
	${PROJECT_SOURCE_DIR}/SexyAppFramework
	${PROJECT_SOURCE_DIR}/SexyAppFramework/sound/SDL-Mixer-X/include
	${SDL2_INCLUDE_DIRS}
)
target_compile_definitions(re-plants-vs-zombies PRIVATE _DEBUG)

target_link_libraries(re-plants-vs-zombies jpeg png z SDL2_mixer_ext_Static ${SDL2_LIBRARIES} ${GLEW_LIBRARIES})

if (WIN32)
	target_link_options(re-plants-vs-zombies PRIVATE -static -static-libgcc -static-libstdc++)
	target_link_libraries(re-plants-vs-zombies opengl32 ws2_32 ddraw dinput8 dxguid dxerr8 user32 gdi32 winmm imm32 shlwapi)
	target_compile_definitions(re-plants-vs-zombies PRIVATE WINDOWS)

	if(NOT CONSOLE)
		target_link_options(re-plants-vs-zombies PRIVATE -mwindows)
	endif()
else()
	target_link_libraries(re-plants-vs-zombies GL)
endif()
