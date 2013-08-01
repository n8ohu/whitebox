find_package(PythonInterp REQUIRED)

add_custom_command(
    OUTPUT modem.h modem.c
    COMMAND ${PYTHON_EXECUTABLE} ${CMAKE_SOURCE_DIR}/util/mvic.py modem/modem.c modem/modem.h
    DEPENDS ${CMAKE_SOURCE_DIR}/util/mvic.py
)

include_directories(${CMAKE_CURRENT_BINARY_DIR})

set(MODEM_SRCS
    modemd.c
    modem.c
)

add_executable(modemd ${MODEM_SRCS})
target_link_libraries(modemd ${CMAKE_REQUIRED_LIBRARIES} whitebox)

set(MODEM_PUBLIC_HEADERS
    modemd.h
    modem.h
)

install(FILES ${WHITEBOX_PUBLIC_HEADERS}
    DESTINATION include/whitebox
)

install(TARGETS
    modemd
	LIBRARY DESTINATION lib${LIB_SUFFIX}
	ARCHIVE DESTINATION lib${LIB_SUFFIX}
	RUNTIME DESTINATION bin
)
