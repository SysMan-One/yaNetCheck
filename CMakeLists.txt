#+++
#
#	FACILITY:	YANETCHK - a network path checking utility
#
#	MODULE:		CMakeLists.txt
#
#	ABSTRACT:	Build of the YANETCHK utility and its help library.
#			The StarLet package (UTILITY$ROUTINES) is consumed as
#			an installed CMake package; the HELP package supplies
#			the HELPTRAN librarian that compiles the Markdown help
#			source into the YANETCHK.HLB library and the HELP
#			browser that /HELP delegates to at run time.
#
#	AUTHOR:		Ruslan R. Laishev
#
#	CREATION DATE:	 1-AUG-2026
#
#	MODIFICATION HISTORY:
#
#		 1-AUG-2026	X-01 : Initial version.
#
#		12-NOV-2026	CLD	X.01-28ECO02 : The identification is taken from
#					__IDENT__ of the head module src/yanetchk.c - the
#					project() VERSION (it said 1.1.28) and the /IDENT of
#					the help library (it said X-01) follow from it.
#
#---

cmake_minimum_required(VERSION 3.16)

#
#	The identification of the product is __IDENT__ of the head module,
#	X.aa-bb[ECOcc] (two digits in every field); the version is aa.bb.cc
#	without the leading zeros: X.01-28ECO02 -> 1.28.2.  The older form
#	Xaa-bb[ECOc] is taken as well.
#
file(STRINGS ${CMAKE_CURRENT_SOURCE_DIR}/src/yanetchk.c YNETCHK_IDENT_LINE
	REGEX "^#define[ \t]+__IDENT__[ \t]+\"" LIMIT_COUNT 1)

if ( NOT YNETCHK_IDENT_LINE MATCHES "__IDENT__[ \t]+\"([^\"]+)\"" )
	message(FATAL_ERROR "No __IDENT__ in src/yanetchk.c")
endif()

set(YNETCHK_IDENT "${CMAKE_MATCH_1}")

if ( YNETCHK_IDENT MATCHES "^X\\." AND NOT YNETCHK_IDENT MATCHES "^X\\.[0-9][0-9]-[0-9][0-9](ECO[0-9][0-9])?$" )
	message(FATAL_ERROR "__IDENT__ is '${YNETCHK_IDENT}', which is not of the form X.aa-bb[ECOcc] (two digits each)")
endif()

if ( NOT YNETCHK_IDENT MATCHES "^X\\.?([0-9]+)-([0-9]+)(ECO([0-9]+))?$" )
	message(FATAL_ERROR "__IDENT__ is '${YNETCHK_IDENT}', which is not of the form X.aa-bb[ECOcc]")
endif()

#	Taken out of CMAKE_MATCH_n before the first REGEX REPLACE, which resets them
set(YNETCHK_VER_MAJOR "${CMAKE_MATCH_1}")
set(YNETCHK_VER_MINOR "${CMAKE_MATCH_2}")
set(YNETCHK_VER_PATCH "${CMAKE_MATCH_4}")

if ( YNETCHK_VER_PATCH STREQUAL "" )
	set(YNETCHK_VER_PATCH "0")
endif()

string(REGEX REPLACE "^0+([0-9])" "\\1" YNETCHK_VER_MAJOR "${YNETCHK_VER_MAJOR}")
string(REGEX REPLACE "^0+([0-9])" "\\1" YNETCHK_VER_MINOR "${YNETCHK_VER_MINOR}")
string(REGEX REPLACE "^0+([0-9])" "\\1" YNETCHK_VER_PATCH "${YNETCHK_VER_PATCH}")

project(YANETCHK
	VERSION "${YNETCHK_VER_MAJOR}.${YNETCHK_VER_MINOR}.${YNETCHK_VER_PATCH}"
	LANGUAGES C DESCRIPTION "Network path checking utility")

message(STATUS "YANETCHK ${YNETCHK_IDENT}, version ${PROJECT_VERSION}")

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

#
#	A build with no type named is a release build - otherwise the
#	compiler is given neither optimization nor NDEBUG, and the tracing
#	of the UTILITY$ROUTINES, which NDEBUG silences, reaches the user.
#
if ( NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES )
	set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
	message(STATUS "No build type named, assuming ${CMAKE_BUILD_TYPE}")
endif()

#
#	Product configuration
#
#	The /IDENT of the help library: the ident of the head module, unless
#	-DYNETCHK$K_IDENT=... names another (not cached, so a new __IDENT__
#	reaches an existing build tree)
if ( NOT DEFINED YNETCHK$K_IDENT )
	set(YNETCHK$K_IDENT "${YNETCHK_IDENT}")
endif()

set(YNETCHK$K_LIBDIR  "share/yanetchk"  CACHE STRING "Directory the help library is installed into")

option(YNETCHK$M_STRICT  "Treat the compilation diagnostics as errors"  OFF)
option(BUILD_TESTING     "Build the regression tests"                   ON)

#
#	Translation options
#
add_library(yanetchk_options INTERFACE)

target_compile_options(yanetchk_options INTERFACE
	-Wall -Wextra)

if ( YNETCHK$M_STRICT )
	target_compile_options(yanetchk_options INTERFACE -Werror)
endif()

#
#	__ARCH__NAME__ identifies the target in the banner; the '$'-spelled
#	YNETCHK$K_HLBDIR cannot travel through -D (the Makefile generator
#	expands the '$' away), so it is carried by a generated header.
#
add_compile_definitions(__ARCH__NAME__="${CMAKE_SYSTEM_PROCESSOR}")

set(YNETCHK_HLBDIR "${CMAKE_INSTALL_PREFIX}/${YNETCHK$K_LIBDIR}")

#
#	UTILITY$ROUTINES - the StarLet package, found where it was installed.
#	Build and install it first, then point this build at it with
#	-DCMAKE_PREFIX_PATH=<prefix> when the prefix is not a default one.
#
find_package(StarLet 1.0 QUIET)

if ( NOT StarLet_FOUND )
	message(FATAL_ERROR
		"The StarLet package (UTILITY\$ROUTINES) was not found.\n"
		"Build and install it, then configure this product again:\n"
		"    cmake -S <utility_routines> -B b -DCMAKE_BUILD_TYPE=Release\n"
		"    cmake --build b && cmake --install b --prefix <prefix>\n"
		"    cmake -S . -B build -DCMAKE_PREFIX_PATH=<prefix>\n"
		"StarLet V.01-17 or later is required.")
endif()

message(STATUS "Using StarLet ${StarLet_VERSION}")

#
#	The HELP package: HELPTRAN compiles the help source into the library,
#	the HELP browser reads it at run time.  Both are looked up on the
#	PATH and under CMAKE_PREFIX_PATH; the browser is a run-time
#	dependency, so a missing one is a warning, not an error - /HELP
#	falls back to the built-in table summary without it.
#
#	The librarian may be installed as helptran or vhelptran, the browser
#	as help or vhelp (the HELP package renames them to dodge the shell
#	builtin). Find whatever is present and remember the browser's name so
#	the binary can exec it at run time.
find_program(HELPTRAN_EXECUTABLE NAMES vhelptran helptran)
find_program(HELP_EXECUTABLE     NAMES vhelp help)

if ( HELP_EXECUTABLE )
	get_filename_component(YNETCHK_HELPCMD "${HELP_EXECUTABLE}" NAME)
else()
	set(YNETCHK_HELPCMD "vhelp")
endif()

message(STATUS "HELP browser image: ${YNETCHK_HELPCMD}")

configure_file(
	${CMAKE_CURRENT_SOURCE_DIR}/include/yanetchk_config.h.in
	${CMAKE_CURRENT_BINARY_DIR}/include/yanetchk_config.h  @ONLY)

#
#	POSIX threads
#
set(THREADS_PREFER_PTHREAD_FLAG ON)
find_package(Threads REQUIRED)

#
#	The utility itself
#
add_executable(yanetchk src/yanetchk.c)

target_include_directories(yanetchk PRIVATE
	${CMAKE_CURRENT_BINARY_DIR}/include)

target_link_libraries(yanetchk PRIVATE StarLet::starlet yanetchk_options Threads::Threads)

#
#	The help library, compiled out of the Markdown source by HELPTRAN.
#	When HELPTRAN is absent the library is simply not built - /HELP then
#	serves the built-in table summary.
#
if ( HELPTRAN_EXECUTABLE )
	add_custom_command(
		OUTPUT  ${CMAKE_CURRENT_BINARY_DIR}/yanetchk.hlb
		COMMAND ${HELPTRAN_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/doc/yanetchk.md
			/OUTPUT=${CMAKE_CURRENT_BINARY_DIR}/yanetchk.hlb
			/IDENT=${YNETCHK$K_IDENT}
		DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/doc/yanetchk.md
		COMMENT "Translating doc/yanetchk.md into yanetchk.hlb")

	add_custom_target(yanetchk_hlb ALL DEPENDS ${CMAKE_CURRENT_BINARY_DIR}/yanetchk.hlb)

	install(FILES ${CMAKE_CURRENT_BINARY_DIR}/yanetchk.hlb DESTINATION ${YNETCHK$K_LIBDIR})
else()
	message(WARNING
		"HELPTRAN was not found: the yanetchk.hlb help library will not be built.\n"
		"Install the HELP package and reconfigure to enable the full /HELP.")
endif()

#
#	Installation
#
install(TARGETS yanetchk RUNTIME DESTINATION bin)

#
#	Regression: build the utility and exercise a round of /HELP and a
#	local send/receive loop.
#
if ( BUILD_TESTING )
	enable_testing()
	add_subdirectory(test)
endif()
