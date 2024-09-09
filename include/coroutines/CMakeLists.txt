file(GLOB CORO_FILES *.cpp)
foreach (CORO_FILE ${CORO_FILES})
    get_filename_component(CORO_NAME ${CORO_FILE} NAME_WE)
    add_executable(${CORO_NAME} ${CORO_FILE})
endforeach ()

target_link_libraries(coro PRIVATE debugger)