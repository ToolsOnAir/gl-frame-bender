add_executable(convert-yuv10-to-v210
  ConvertYUV10ToV210.cpp)

target_link_libraries(convert-yuv10-to-v210 PRIVATE ${Boost_LIBRARIES})

add_executable(v210-frame-comparison
  V210FrameComparison.cpp)

target_link_libraries(v210-frame-comparison PRIVATE gl-frame-bender-lib)

target_include_directories(v210-frame-comparison PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/protobuf_generated)
target_include_directories(v210-frame-comparison PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../external/glad/include)
target_include_directories(v210-frame-comparison PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../external/roots/include)
target_include_directories(v210-frame-comparison PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../gl-frame-bender-tests)

target_link_libraries(v210-frame-comparison PRIVATE ${Boost_LIBRARIES} ${GLM_LIBRARIES} ${DEVIL_LIBRARIES} ${GLFW_LIBRARIES} ${IPP_LIBRARIES})
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	target_link_libraries(v210-frame-comparison PRIVATE debug ${Protobuf_LIBRARIES_DEBUG} optimized ${Protobuf_LIBRARIES})
ELSE(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	target_link_libraries(v210-frame-comparison PRIVATE ${Protobuf_LIBRARIES})
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
