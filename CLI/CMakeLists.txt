project(obs-cli)

option(ENABLE_CLI "Enables the OBS CLI interfaces" ON)
if(DISABLE_CLI)
	message(STATUS "CLI disabled")
	return()
endif()
include_directories(SYSTEM "${CMAKE_SOURCE_DIR}/libobs"
		${OBS_JANSSON_INCLUDE_DIRS})
if(UNIX)
	set(obs_PLATFORM_SOURCES
		platform-x11.cpp)

	set(obs_PLATFORM_LIBRARIES)
endif()

set(obs-cli_SOURCES
	${obs-PLATFORM_SOURCES}
	obs-cli.c
	list.c
)

add_executable(obs-cli
	${obs-cli_SOURCES}
)

target_link_libraries(obs-cli
	libobs
	libff
	${OBS_JANSSON_IMPORT}
	${obs_PLATFORM_LIBRARIES})

install_obs_core(obs-cli)
