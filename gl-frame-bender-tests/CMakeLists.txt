add_executable(gl-frame-bender-tests
  CircularQueueTests.cpp
  GLHelperTests.cpp
  PipelineTests.cpp
  RenderTests.cpp
  StreamSourceTests.cpp
  TestCommon.cpp
  TestCommon.h
  TestMain.cpp
  TestUtils.h
  PrecompileTest.cpp
  PrecompileTest.h)

add_definitions(-DBOOST_TEST_ALTERNATIVE_INIT_API)
target_link_libraries(gl-frame-bender-tests PRIVATE gl-frame-bender-lib)

target_include_directories(gl-frame-bender-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/protobuf_generated)
target_include_directories(gl-frame-bender-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../external/glad/include)
target_include_directories(gl-frame-bender-tests PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../external/roots/include)

target_link_libraries(gl-frame-bender-tests PRIVATE ${Boost_LIBRARIES} ${GLM_LIBRARIES} ${DEVIL_LIBRARIES} ${GLFW_LIBRARIES} ${IPP_LIBRARIES})
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	target_link_libraries(gl-frame-bender-tests PRIVATE debug ${Protobuf_LIBRARIES_DEBUG} optimized ${Protobuf_LIBRARIES})
ELSE(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	target_link_libraries(gl-frame-bender-tests PRIVATE ${Protobuf_LIBRARIES})
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
