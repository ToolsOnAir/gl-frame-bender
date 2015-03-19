SOURCE_GROUP(shaders REGULAR_EXPRESSION "${CMAKE_SOURCE_DIR}/glsl/*")
SOURCE_GROUP(resources REGULAR_EXPRESSION "${CMAKE_SOURCE_DIR}/resources/*")

add_library (gl-frame-bender-lib
  ByPassDownloadStage.h
  ByPassUploadStage.cpp
  ByPassUploadStage.h
  ChronoUtils.h
  CircularFifo.h
  CircularFifoHelpers.h
  Context.cpp
  Context.h
  CopyHostToMappedPBOStage.cpp
  CopyHostToMappedPBOStage.h
  CopyMappedPBOToHostStage.cpp
  CopyMappedPBOToHostStage.h
  DemoStreamRenderer.cpp
  DemoStreamRenderer.h
  protobuf_generated/fbt_format.pb.cc
  protobuf_generated/fbt_format.pb.h
  FormatConverterStage.cpp
  FormatConverterStage.h
  FormatOptions.h
  FrameBender.h
  FrameCompositionInputStage.cpp
  FrameCompositionInputStage.h
  FrameCompositionOutputStage.cpp
  FrameCompositionOutputStage.h
  Frame.cpp
  Frame.h
  ImageFormat.cpp
  ImageFormat.h
  Init.cpp
  Init.h
  Logging.cpp
  Logging.h
  MapPBOStage.cpp
  MapPBOStage.h
  MathConstants.h
  PackTextureToPBOStage.cpp
  PackTextureToPBOStage.h
  PipelineStage.h
  Precompile.h
  Precompile.cpp
  ProgramOptions.cpp
  ProgramOptions.h
  Quad.cpp
  Quad.h
  RenderStage.cpp
  RenderStage.h
  Semantics.h
  StageDataTypes.h
  Stage.h
  Stage.inl.h
  StageSampler.cpp
  StageSampler.h
  StreamComposition.cpp
  StreamComposition.h
  StreamDispatch.cpp
  StreamDispatch.h
  StreamRenderer.cpp
  StreamRenderer.h
  StreamSource.cpp
  StreamSource.h
  StringifyEnums.cpp
  StringifyEnums.h
  FrameTime.h
  TimeSampler.cpp
  TimeSampler.h
  TraceFormat.cpp
  TraceFormat.h
  UnmapPBOStage.cpp
  UnmapPBOStage.h
  UnpackPBOToTextureStage.cpp
  UnpackPBOToTextureStage.h
  UtilsGL.cpp
  UtilsGL.h
  Utils.h
  Window.cpp
  Window.h
  ${CMAKE_CURRENT_SOURCE_DIR}/../external/glad/src/glad.c
  ${ShaderFilesAbsolute}
  ${ResourceFilesAbsolute})

target_include_directories(gl-frame-bender-lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/protobuf_generated)
target_include_directories(gl-frame-bender-lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../external/glad/include)
target_include_directories(gl-frame-bender-lib PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/../external/roots/include)

target_include_directories (gl-frame-bender-lib PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

target_link_libraries(gl-frame-bender-lib PRIVATE ${Boost_LIBRARIES} ${GLM_LIBRARIES} ${PROTOBUF_LIBRARIES} ${DevIL_LIBRARIES} ${GLFW_LIBRARIES})
