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

set(NO_GLEW OFF)
set(USE_EGL OFF)
set(USE_GLES OFF)
if(NINTENDO_SWITCH)
	set(NO_GLEW ON)
	set(USE_EGL ON)
	set(USE_GLES ON)
	list(APPEND SOURCES
		${CMAKE_CURRENT_SOURCE_DIR}/SexyAppFramework/graphics/platform/switch/Window.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/SexyAppFramework/graphics/platform/switch/GLInterface.cpp
	)
else()
	list(APPEND SOURCES
		${CMAKE_CURRENT_SOURCE_DIR}/SexyAppFramework/graphics/platform/pc/Window.cpp
		${CMAKE_CURRENT_SOURCE_DIR}/SexyAppFramework/graphics/platform/pc/GLInterfaceModern.cpp
	)
endif()

option(CONSOLE "Show console on Windows" ON)

find_package(ZLIB REQUIRED)
find_package(JPEG REQUIRED)
find_package(PNG REQUIRED)
find_package(SDL2 REQUIRED)
if (NOT NO_GLEW)
	find_package(GLEW REQUIRED)
endif()

set(SDL_MIXER_X_STATIC ON CACHE BOOL "Build static library of SDL Mixer X" FORCE)
set(SDL_MIXER_X_SHARED OFF CACHE BOOL "Build shared library of SDL Mixer X" FORCE)
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

if (NO_GLEW)
	target_compile_definitions(re-plants-vs-zombies PRIVATE NO_GLEW)
endif()
if (USE_EGL)
	target_compile_definitions(re-plants-vs-zombies PRIVATE USE_EGL)
endif()
if (USE_GLES)
	target_compile_definitions(re-plants-vs-zombies PRIVATE USE_GLES)
endif()

target_link_libraries(re-plants-vs-zombies jpeg png z SDL2_mixer_ext_Static openmpt mpg123 vorbisfile vorbis ogg ${SDL2_LIBRARIES} ${GLEW_LIBRARIES})

if (WIN32)
	target_link_options(re-plants-vs-zombies PRIVATE -static -static-libgcc -static-libstdc++)
	target_link_libraries(re-plants-vs-zombies opengl32 ws2_32 ddraw dinput8 dxguid dxerr8 user32 gdi32 winmm imm32 shlwapi)
	target_compile_definitions(re-plants-vs-zombies PRIVATE WINDOWS)

	if(NOT CONSOLE)
		target_link_options(re-plants-vs-zombies PRIVATE -mwindows)
	endif()
elseif (NINTENDO_SWITCH)
	target_link_libraries(re-plants-vs-zombies glad EGL glapi drm_nouveau)
	nx_generate_nacp(re-plants-vs-zombies "Plants vs. Zombies" "PopCap Games, Unofficial Switch port by Headshotnoby" "0.1")
	nx_create_nro(re-plants-vs-zombies)
else()
	target_link_libraries(re-plants-vs-zombies GL)
endif()
