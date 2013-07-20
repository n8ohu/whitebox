include_directories(${CMAKE_CURRENT_BINARY_DIR})

add_executable(modemd modemd.c)
target_link_libraries(modemd ${CMAKE_REQUIRED_LIBRARIES} whitebox)

install(TARGETS
    modemd
	LIBRARY DESTINATION lib${LIB_SUFFIX}
	ARCHIVE DESTINATION lib${LIB_SUFFIX}
	RUNTIME DESTINATION bin
)
