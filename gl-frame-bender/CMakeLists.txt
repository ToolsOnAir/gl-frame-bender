add_executable(gl-frame-bender
  main.cpp)

target_link_libraries(gl-frame-bender PRIVATE gl-frame-bender-lib)

target_include_directories(gl-frame-bender PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/protobuf_generated)
target_include_directories(gl-frame-bender PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../external/glad/include)
target_include_directories(gl-frame-bender PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../external/roots/include)

target_link_libraries(gl-frame-bender PRIVATE ${Boost_LIBRARIES} ${GLM_LIBRARIES} ${DEVIL_LIBRARIES} ${GLFW_LIBRARIES} ${IPP_LIBRARIES})
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	target_link_libraries(gl-frame-bender PRIVATE debug ${Protobuf_LIBRARIES_DEBUG} optimized ${Protobuf_LIBRARIES})
ELSE(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	target_link_libraries(gl-frame-bender PRIVATE ${Protobuf_LIBRARIES})
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")

