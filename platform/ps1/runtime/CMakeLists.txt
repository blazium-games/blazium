# Guest-only PSn00bSDK target. Host compilers and packers are spawned by
# blazium-toolchain (ps1 build). Do not cmake the sibling PSn00bSDK tree.

cmake_minimum_required(VERSION 3.21)

project(
	blazium_ps1_runtime
	LANGUAGES C CXX ASM
	VERSION 0.1.0
	DESCRIPTION "Blazium PS1 guest hello"
)

psn00bsdk_add_executable(runtime GPREL main.cpp)
target_compile_options(runtime PRIVATE -fno-exceptions -fno-rtti)
target_compile_definitions(runtime PRIVATE BLAZIUM_PS1_COOK_ABI=2)

set(BLAZIUM_PS1_TIM "" CACHE FILEPATH "Optional cooked TIM to embed")
if(BLAZIUM_PS1_TIM)
	configure_file("${BLAZIUM_PS1_TIM}" "${CMAKE_CURRENT_BINARY_DIR}/TEX00.TIM" COPYONLY)
	psn00bsdk_target_incbin(runtime PRIVATE cooked_tim "${CMAKE_CURRENT_BINARY_DIR}/TEX00.TIM")
	target_compile_definitions(runtime PRIVATE BLAZIUM_PS1_HAS_TIM=1)
endif()

set(BLAZIUM_PS1_MESH "" CACHE FILEPATH "Optional cooked SVECTOR mesh to embed")
if(BLAZIUM_PS1_MESH)
	configure_file("${BLAZIUM_PS1_MESH}" "${CMAKE_CURRENT_BINARY_DIR}/MESH00.BIN" COPYONLY)
	psn00bsdk_target_incbin(runtime PRIVATE cooked_mesh "${CMAKE_CURRENT_BINARY_DIR}/MESH00.BIN")
	target_compile_definitions(runtime PRIVATE BLAZIUM_PS1_HAS_MESH=1)
endif()

set(BLAZIUM_PS1_VAG "" CACHE FILEPATH "Optional cooked VAG to embed")
if(BLAZIUM_PS1_VAG)
	configure_file("${BLAZIUM_PS1_VAG}" "${CMAKE_CURRENT_BINARY_DIR}/SFX00.VAG" COPYONLY)
	psn00bsdk_target_incbin(runtime PRIVATE cooked_vag "${CMAKE_CURRENT_BINARY_DIR}/SFX00.VAG")
	target_compile_definitions(runtime PRIVATE BLAZIUM_PS1_HAS_VAG=1)
endif()

set(BLAZIUM_PS1_SPRITE "" CACHE FILEPATH "Optional cooked SPRITE table to embed")
if(BLAZIUM_PS1_SPRITE)
	configure_file("${BLAZIUM_PS1_SPRITE}" "${CMAKE_CURRENT_BINARY_DIR}/SPRITE00.BIN" COPYONLY)
	psn00bsdk_target_incbin(runtime PRIVATE cooked_sprite "${CMAKE_CURRENT_BINARY_DIR}/SPRITE00.BIN")
	target_compile_definitions(runtime PRIVATE BLAZIUM_PS1_HAS_SPRITE=1)
endif()

set(BLAZIUM_PS1_SCRIPT "" CACHE FILEPATH "Optional cooked SCRIPT.IR to embed")
if(BLAZIUM_PS1_SCRIPT)
	configure_file("${BLAZIUM_PS1_SCRIPT}" "${CMAKE_CURRENT_BINARY_DIR}/SCRIPT.IR" COPYONLY)
	psn00bsdk_target_incbin(runtime PRIVATE cooked_script "${CMAKE_CURRENT_BINARY_DIR}/SCRIPT.IR")
	target_compile_definitions(runtime PRIVATE BLAZIUM_PS1_HAS_SCRIPT=1)
endif()
