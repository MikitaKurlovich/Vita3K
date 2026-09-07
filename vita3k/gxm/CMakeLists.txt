add_library(
	gxm
	STATIC
	include/gxm/functions.h
	include/gxm/state.h
	include/gxm/types.h
	src/attributes.cpp
	src/color.cpp
	src/gxp.cpp
	src/stream.cpp
	src/textures.cpp
	src/transfer.cpp
)

target_include_directories(gxm PUBLIC include)
target_link_libraries(gxm PUBLIC util)
target_link_libraries(gxm PRIVATE)

if(BUILD_TESTING AND NOT ANDROID)
	add_executable(
		gxm-tests
		tests/pass_type_tests.cpp
	)
	target_link_libraries(gxm-tests PRIVATE googletest gxm)
	add_test(NAME gxm COMMAND gxm-tests)
endif()
