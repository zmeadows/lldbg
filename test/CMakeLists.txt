project(lldbgtest LANGUAGES CXX C)

include(CTest)
enable_testing()

file(GLOB TEST_SOURCES "src/*.cpp")

# TODO: target_link_libraries(test_config PRIVATE lldbg_warnings)

foreach(TEST_SOURCE ${TEST_SOURCES})
  get_filename_component(TEST_NAME ${TEST_SOURCE} NAME_WE)
  add_executable(${TEST_NAME} ${TEST_SOURCE})
  target_include_directories(${TEST_NAME} PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/)
  target_link_libraries(${TEST_NAME} PRIVATE lldbgui_lib)
  add_test(NAME ${TEST_NAME} COMMAND ${TEST_NAME})
  set_tests_properties(${TEST_NAME} PROPERTIES WORKING_DIRECTORY
                                               ${CMAKE_SOURCE_DIR}/test)
endforeach()
