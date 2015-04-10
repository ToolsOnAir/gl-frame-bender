add_executable(gl-frame-bender
  main.cpp)

target_link_libraries(gl-frame-bender PRIVATE gl-frame-bender-lib)

target_include_directories(gl-frame-bender PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/protobuf_generated)
target_include_directories(gl-frame-bender PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../external/glad/include)
target_include_directories(gl-frame-bender PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../external/roots/include)

target_link_libraries(gl-frame-bender PRIVATE ${Boost_LIBRARIES} ${GLM_LIBRARIES} ${DEVIL_LIBRARIES} ${GLFW_LIBRARIES} ${IPP_LIBRARIES})
IF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	target_link_libraries(gl-frame-bender PRIVATE debug ${Protobuf_LIBRARIES_DEBUG} optimized ${Protobuf_LIBRARIES})

	add_custom_command(TARGET gl-frame-bender POST_BUILD
	    COMMAND ${CMAKE_COMMAND} -E copy_if_different        
	    "${PROJECT_SOURCE_DIR}/external/devil/lib/msvc_x64/DevIL.dll"        
	    $<TARGET_FILE_DIR:gl-frame-bender>)

	add_custom_command(TARGET gl-frame-bender POST_BUILD
	    COMMAND ${CMAKE_COMMAND} -E copy_if_different        
	    "${PROJECT_SOURCE_DIR}/external/devil/lib/msvc_x64/ILU.dll"        
	    $<TARGET_FILE_DIR:gl-frame-bender>)

	add_custom_command(TARGET gl-frame-bender POST_BUILD
	    COMMAND ${CMAKE_COMMAND} -E copy_if_different        
	    "${PROJECT_SOURCE_DIR}/external/devil/lib/msvc_x64/ILUT.dll"        
	    $<TARGET_FILE_DIR:gl-frame-bender>)

ELSE(${CMAKE_SYSTEM_NAME} MATCHES "Windows")
	target_link_libraries(gl-frame-bender PRIVATE ${Protobuf_LIBRARIES})
ENDIF(${CMAKE_SYSTEM_NAME} MATCHES "Windows")

add_custom_command(TARGET gl-frame-bender POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_if_different        
    "${PROJECT_SOURCE_DIR}/resources/PlayerPreset.cfg"        
    $<TARGET_FILE_DIR:gl-frame-bender>)

add_custom_command(TARGET gl-frame-bender POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory       
    "${PROJECT_SOURCE_DIR}/glsl"        
    $<TARGET_FILE_DIR:gl-frame-bender>/glsl)

add_custom_command(TARGET gl-frame-bender POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory       
    "${PROJECT_SOURCE_DIR}/test_data"        
    $<TARGET_FILE_DIR:gl-frame-bender>/test_data)

add_custom_command(TARGET gl-frame-bender POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory       
    "${PROJECT_SOURCE_DIR}/textures"        
    $<TARGET_FILE_DIR:gl-frame-bender>/textures)

