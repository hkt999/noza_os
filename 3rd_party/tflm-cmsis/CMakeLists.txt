cmake_minimum_required(VERSION 3.12)
set(CMAKE_SYSTEM_NAME Generic)
set(CMAKE_SYSTEM_PROCESSOR arm)
set(CMAKE_TRY_COMPILE_TARGET_TYPE STATIC_LIBRARY)
set(CMAKE_EXE_LINKER_FLAGS "-static")
set(HOST_PLATFORM "rtos")
set(CMAKE_VERBOSE_MAKEFILE OFF)

project(tflm-pico C CXX ASM)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_BUILD_TYPE Release)

set(CMAKE_C_COMPILER arm-none-eabi-gcc)
set(CMAKE_CXX_COMPILER arm-none-eabi-g++)
set(CMAKE_ASM_COMPILER arm-none-eabi-as)
set(CMAKE_AR arm-none-eabi-ar)
set(CMAKE_RANLIB arm-none-eabi-ranlib)

set(CMAKE_C_FLAGS "-mcpu=cortex-m0plus -march=armv6-m -mthumb -O3 \
	-ffunction-sections -fdata-sections -fno-exceptions -fno-unwind-tables \
	-fno-asynchronous-unwind-tables -DCMSIS_NN -DPROJECT_GENERATION")

set(CMAKE_CXX_FLAGS "-mcpu=cortex-m0plus -march=armv6-m -mthumb -O3 \
	-ffunction-sections -fdata-sections -fno-exceptions -fno-rtti -fno-unwind-tables \
	-fno-asynchronous-unwind-tables -fno-threadsafe-statics -DCMSIS_NN -DPROJECT_GENERATION")

add_library(
	tflm_cmsis
	tensorflow/lite/micro/kernels/elu.cc
	tensorflow/lite/micro/kernels/concatenation.cc
	tensorflow/lite/micro/kernels/squeeze.cc
	tensorflow/lite/micro/kernels/read_variable.cc
	tensorflow/lite/micro/kernels/svdf_common.cc
	tensorflow/lite/micro/kernels/batch_to_space_nd.cc
	tensorflow/lite/micro/kernels/select.cc
	tensorflow/lite/micro/kernels/fully_connected_common.cc
	tensorflow/lite/micro/kernels/broadcast_to.cc
	tensorflow/lite/micro/kernels/cumsum.cc
	tensorflow/lite/micro/kernels/leaky_relu_common.cc
	tensorflow/lite/micro/kernels/l2_pool_2d.cc
	tensorflow/lite/micro/kernels/depthwise_conv_common.cc
	tensorflow/lite/micro/kernels/floor_mod.cc
	tensorflow/lite/micro/kernels/if.cc
	tensorflow/lite/micro/kernels/exp.cc
	tensorflow/lite/micro/kernels/mul_common.cc
	tensorflow/lite/micro/kernels/space_to_batch_nd.cc
	tensorflow/lite/micro/kernels/var_handle.cc
	tensorflow/lite/micro/kernels/round.cc
	tensorflow/lite/micro/kernels/conv_common.cc
	tensorflow/lite/micro/kernels/strided_slice.cc
	tensorflow/lite/micro/kernels/prelu.cc
	tensorflow/lite/micro/kernels/unidirectional_sequence_lstm.cc
	tensorflow/lite/micro/kernels/split.cc
	tensorflow/lite/micro/kernels/transpose.cc
	tensorflow/lite/micro/kernels/assign_variable.cc
	tensorflow/lite/micro/kernels/kernel_util.cc
	tensorflow/lite/micro/kernels/expand_dims.cc
	tensorflow/lite/micro/kernels/dequantize_common.cc
	tensorflow/lite/micro/kernels/transpose_conv.cc
	tensorflow/lite/micro/kernels/while.cc
	tensorflow/lite/micro/kernels/pad.cc
	tensorflow/lite/micro/kernels/sub_common.cc
	tensorflow/lite/micro/kernels/dequantize.cc
	tensorflow/lite/micro/kernels/resize_nearest_neighbor.cc
	tensorflow/lite/micro/kernels/unpack.cc
	tensorflow/lite/micro/kernels/arg_min_max.cc
	tensorflow/lite/micro/kernels/shape.cc
	tensorflow/lite/micro/kernels/gather_nd.cc
	tensorflow/lite/micro/kernels/circular_buffer.cc
	tensorflow/lite/micro/kernels/prelu_common.cc
	tensorflow/lite/micro/kernels/sub.cc
	tensorflow/lite/micro/kernels/activations_common.cc
	tensorflow/lite/micro/kernels/reshape.cc
	tensorflow/lite/micro/kernels/zeros_like.cc
	tensorflow/lite/micro/kernels/depth_to_space.cc
	tensorflow/lite/micro/kernels/activations.cc
	tensorflow/lite/micro/kernels/detection_postprocess.cc
	tensorflow/lite/micro/kernels/logistic.cc
	tensorflow/lite/micro/kernels/reduce_common.cc
	tensorflow/lite/micro/kernels/softmax_common.cc
	tensorflow/lite/micro/kernels/call_once.cc
	tensorflow/lite/micro/kernels/squared_difference.cc
	tensorflow/lite/micro/kernels/quantize_common.cc
	tensorflow/lite/micro/kernels/ceil.cc
	tensorflow/lite/micro/kernels/add_n.cc
	tensorflow/lite/micro/kernels/hard_swish.cc
	tensorflow/lite/micro/kernels/add_common.cc
	tensorflow/lite/micro/kernels/lstm_eval.cc
	tensorflow/lite/micro/kernels/elementwise.cc
	tensorflow/lite/micro/kernels/floor.cc
	tensorflow/lite/micro/kernels/log_softmax.cc
	tensorflow/lite/micro/kernels/cast.cc
	tensorflow/lite/micro/kernels/gather.cc
	tensorflow/lite/micro/kernels/circular_buffer_common.cc
	tensorflow/lite/micro/kernels/micro_tensor_utils.cc
	tensorflow/lite/micro/kernels/mirror_pad.cc
	tensorflow/lite/micro/kernels/broadcast_args.cc
	tensorflow/lite/micro/kernels/maximum_minimum.cc
	tensorflow/lite/micro/kernels/ethosu.cc
	tensorflow/lite/micro/kernels/pack.cc
	tensorflow/lite/micro/kernels/split_v.cc
	tensorflow/lite/micro/kernels/logistic_common.cc
	tensorflow/lite/micro/kernels/slice.cc
	tensorflow/lite/micro/kernels/fill.cc
	tensorflow/lite/micro/kernels/cmsis_nn/conv.cc
	tensorflow/lite/micro/kernels/cmsis_nn/depthwise_conv.cc
	tensorflow/lite/micro/kernels/cmsis_nn/softmax.cc
	tensorflow/lite/micro/kernels/cmsis_nn/fully_connected.cc
	tensorflow/lite/micro/kernels/cmsis_nn/mul.cc
	tensorflow/lite/micro/kernels/cmsis_nn/pooling.cc
	tensorflow/lite/micro/kernels/cmsis_nn/svdf.cc
	tensorflow/lite/micro/kernels/cmsis_nn/add.cc
	tensorflow/lite/micro/kernels/space_to_depth.cc
	tensorflow/lite/micro/kernels/leaky_relu.cc
	tensorflow/lite/micro/kernels/comparisons.cc
	tensorflow/lite/micro/kernels/reduce.cc
	tensorflow/lite/micro/kernels/quantize.cc
	tensorflow/lite/micro/kernels/div.cc
	tensorflow/lite/micro/kernels/logical.cc
	tensorflow/lite/micro/kernels/tanh.cc
	tensorflow/lite/micro/kernels/logical_common.cc
	tensorflow/lite/micro/kernels/l2norm.cc
	tensorflow/lite/micro/kernels/resize_bilinear.cc
	tensorflow/lite/micro/kernels/pooling_common.cc
	tensorflow/lite/micro/kernels/neg.cc
	tensorflow/lite/micro/kernels/hard_swish_common.cc
	tensorflow/lite/micro/kernels/kernel_runner.cc
	tensorflow/lite/micro/kernels/floor_div.cc
	tensorflow/lite/micro/test_helpers.cc
	tensorflow/lite/micro/mock_micro_graph.cc
	tensorflow/lite/micro/micro_graph.cc
	tensorflow/lite/micro/micro_allocation_info.cc
	tensorflow/lite/micro/tflite_bridge/micro_error_reporter.cc
	tensorflow/lite/micro/tflite_bridge/flatbuffer_conversions_bridge.cc
	tensorflow/lite/micro/tflite_bridge/op_resolver_bridge.cc
	tensorflow/lite/micro/all_ops_resolver.cc
	tensorflow/lite/micro/micro_profiler.cc
	tensorflow/lite/micro/micro_context.cc
	tensorflow/lite/micro/micro_resource_variable.cc
	tensorflow/lite/micro/system_setup.cc
	tensorflow/lite/micro/test_helper_custom_ops.cc
	tensorflow/lite/micro/cortex_m_generic/debug_log.cc
	tensorflow/lite/micro/cortex_m_generic/micro_time.cc
	tensorflow/lite/micro/micro_log.cc
	tensorflow/lite/micro/arena_allocator/non_persistent_arena_buffer_allocator.cc
	tensorflow/lite/micro/arena_allocator/recording_single_arena_buffer_allocator.cc
	tensorflow/lite/micro/arena_allocator/persistent_arena_buffer_allocator.cc
	tensorflow/lite/micro/arena_allocator/single_arena_buffer_allocator.cc
	tensorflow/lite/micro/recording_micro_allocator.cc
	tensorflow/lite/micro/micro_string.cc
	tensorflow/lite/micro/micro_interpreter.cc
	tensorflow/lite/micro/fake_micro_context.cc
	tensorflow/lite/micro/memory_planner/greedy_memory_planner.cc
	tensorflow/lite/micro/memory_planner/non_persistent_buffer_planner_shim.cc
	tensorflow/lite/micro/memory_planner/linear_memory_planner.cc
	tensorflow/lite/micro/micro_utils.cc
	tensorflow/lite/micro/flatbuffer_utils.cc
	tensorflow/lite/micro/micro_allocator.cc
	tensorflow/lite/micro/memory_helpers.cc
	tensorflow/lite/kernels/kernel_util.cc
	tensorflow/lite/kernels/internal/tensor_utils.cc
	tensorflow/lite/kernels/internal/portable_tensor_utils.cc
	tensorflow/lite/kernels/internal/reference/portable_tensor_utils.cc
	tensorflow/lite/kernels/internal/quantization_util.cc
	tensorflow/lite/experimental/microfrontend/lib/fft.cc
	tensorflow/lite/experimental/microfrontend/lib/kiss_fft_int16.cc
	tensorflow/lite/experimental/microfrontend/lib/fft_util.cc
	tensorflow/lite/core/api/error_reporter.cc
	tensorflow/lite/core/api/tensor_utils.cc
	tensorflow/lite/core/api/flatbuffer_conversions.cc
	tensorflow/lite/core/api/op_resolver.cc
	tensorflow/lite/core/c/common.cc
	tensorflow/lite/schema/schema_utils.cc
	tensorflow/lite/experimental/microfrontend/lib/log_lut.c
	tensorflow/lite/experimental/microfrontend/lib/pcan_gain_control_util.c
	tensorflow/lite/experimental/microfrontend/lib/frontend.c
	tensorflow/lite/experimental/microfrontend/lib/window_util.c
	tensorflow/lite/experimental/microfrontend/lib/frontend_util.c
	tensorflow/lite/experimental/microfrontend/lib/log_scale.c
	tensorflow/lite/experimental/microfrontend/lib/filterbank.c
	tensorflow/lite/experimental/microfrontend/lib/noise_reduction.c
	tensorflow/lite/experimental/microfrontend/lib/noise_reduction_util.c
	tensorflow/lite/experimental/microfrontend/lib/filterbank_util.c
	tensorflow/lite/experimental/microfrontend/lib/log_scale_util.c
	tensorflow/lite/experimental/microfrontend/lib/pcan_gain_control.c
	tensorflow/lite/experimental/microfrontend/lib/window.c
	third_party/kissfft/kiss_fft.c
	third_party/kissfft/tools/kiss_fftr.c
	third_party/cmsis_nn/Source/PoolingFunctions/arm_avgpool_s8.c
	third_party/cmsis_nn/Source/PoolingFunctions/arm_max_pool_s16.c
	third_party/cmsis_nn/Source/PoolingFunctions/arm_avgpool_s16.c
	third_party/cmsis_nn/Source/PoolingFunctions/arm_max_pool_s8.c
	third_party/cmsis_nn/Source/NNSupportFunctions/arm_nn_depthwise_conv_nt_t_s8.c
	third_party/cmsis_nn/Source/NNSupportFunctions/arm_nn_vec_mat_mult_t_svdf_s8.c
	third_party/cmsis_nn/Source/NNSupportFunctions/arm_nn_mat_mul_core_1x_s8.c
	third_party/cmsis_nn/Source/NNSupportFunctions/arm_nn_vec_mat_mult_t_s8.c
	third_party/cmsis_nn/Source/NNSupportFunctions/arm_nn_depthwise_conv_nt_t_padded_s8.c
	third_party/cmsis_nn/Source/NNSupportFunctions/arm_nntables.c
	third_party/cmsis_nn/Source/NNSupportFunctions/arm_nn_mat_mul_core_4x_s8.c
	third_party/cmsis_nn/Source/NNSupportFunctions/arm_nn_mat_mult_nt_t_s8.c
	third_party/cmsis_nn/Source/NNSupportFunctions/arm_nn_vec_mat_mult_t_s16.c
	third_party/cmsis_nn/Source/NNSupportFunctions/arm_nn_depthwise_conv_nt_t_s16.c
	third_party/cmsis_nn/Source/NNSupportFunctions/arm_nn_mat_mul_kernel_s16.c
	third_party/cmsis_nn/Source/NNSupportFunctions/arm_q7_to_q15_with_offset.c
	third_party/cmsis_nn/Source/FullyConnectedFunctions/arm_fully_connected_s8.c
	third_party/cmsis_nn/Source/FullyConnectedFunctions/arm_fully_connected_s16.c
	third_party/cmsis_nn/Source/ConcatenationFunctions/arm_concatenation_s8_x.c
	third_party/cmsis_nn/Source/ConcatenationFunctions/arm_concatenation_s8_w.c
	third_party/cmsis_nn/Source/ConcatenationFunctions/arm_concatenation_s8_y.c
	third_party/cmsis_nn/Source/ConcatenationFunctions/arm_concatenation_s8_z.c
	third_party/cmsis_nn/Source/SoftmaxFunctions/arm_softmax_u8.c
	third_party/cmsis_nn/Source/SoftmaxFunctions/arm_nn_softmax_common_s8.c
	third_party/cmsis_nn/Source/SoftmaxFunctions/arm_softmax_s8.c
	third_party/cmsis_nn/Source/SoftmaxFunctions/arm_softmax_s16.c
	third_party/cmsis_nn/Source/SoftmaxFunctions/arm_softmax_s8_s16.c
	third_party/cmsis_nn/Source/SVDFunctions/arm_svdf_state_s16_s8.c
	third_party/cmsis_nn/Source/SVDFunctions/arm_svdf_s8.c
	third_party/cmsis_nn/Source/ActivationFunctions/arm_relu_q15.c
	third_party/cmsis_nn/Source/ActivationFunctions/arm_relu6_s8.c
	third_party/cmsis_nn/Source/ActivationFunctions/arm_relu_q7.c
	third_party/cmsis_nn/Source/BasicMathFunctions/arm_elementwise_mul_s16.c
	third_party/cmsis_nn/Source/BasicMathFunctions/arm_elementwise_add_s8.c
	third_party/cmsis_nn/Source/BasicMathFunctions/arm_elementwise_mul_s8.c
	third_party/cmsis_nn/Source/BasicMathFunctions/arm_elementwise_add_s16.c
	third_party/cmsis_nn/Source/ConvolutionFunctions/arm_depthwise_conv_s16.c
	third_party/cmsis_nn/Source/ConvolutionFunctions/arm_convolve_s16.c
	third_party/cmsis_nn/Source/ConvolutionFunctions/arm_convolve_1x1_s8_fast.c
	third_party/cmsis_nn/Source/ConvolutionFunctions/arm_convolve_wrapper_s16.c
	third_party/cmsis_nn/Source/ConvolutionFunctions/arm_depthwise_conv_3x3_s8.c
	third_party/cmsis_nn/Source/ConvolutionFunctions/arm_nn_mat_mult_s8.c
	third_party/cmsis_nn/Source/ConvolutionFunctions/arm_depthwise_conv_wrapper_s8.c
	third_party/cmsis_nn/Source/ConvolutionFunctions/arm_nn_depthwise_conv_s8_core.c
	third_party/cmsis_nn/Source/ConvolutionFunctions/arm_convolve_1_x_n_s8.c
	third_party/cmsis_nn/Source/ConvolutionFunctions/arm_depthwise_conv_s8_opt.c
	third_party/cmsis_nn/Source/ConvolutionFunctions/arm_convolve_fast_s16.c
	third_party/cmsis_nn/Source/ConvolutionFunctions/arm_depthwise_conv_fast_s16.c
	third_party/cmsis_nn/Source/ConvolutionFunctions/arm_convolve_s8.c
	third_party/cmsis_nn/Source/ConvolutionFunctions/arm_depthwise_conv_s8.c
	third_party/cmsis_nn/Source/ConvolutionFunctions/arm_convolve_wrapper_s8.c
	third_party/cmsis_nn/Source/ConvolutionFunctions/arm_depthwise_conv_wrapper_s16.c
	third_party/cmsis_nn/Source/ConvolutionFunctions/arm_nn_mat_mult_kernel_s8_s16.c
	third_party/cmsis_nn/Source/ReshapeFunctions/arm_reshape_s8.c
)

target_include_directories(
	tflm_cmsis
	PUBLIC
	${CMAKE_CURRENT_LIST_DIR}
	third_party/cmsis/CMSIS/Core/Include
	third_party/cmsis_nn/Include
	third_party/cmsis_nn
	third_party/ruy
	third_party/gemmlowp
	third_party/kissfft
	third_party/flatbuffers
	third_party/flatbuffers/include
)

# make examples
# hello_world
add_library(
	hello_world
	examples/hello_world/constants.cc
	examples/hello_world/hello_world_model_data.cc
	examples/hello_world/hw_main.cc
	examples/hello_world/hw_main_functions.cc
	examples/hello_world/hw_output_handler.cc
)

# person_detection
add_library(
	person_detection
	examples/person_detection/detection_responder.cc
	examples/person_detection/image_provider.cc
	examples/person_detection/pd_main.cc
	examples/person_detection/pd_main_functions.cc
	examples/person_detection/pd_model_settings.cc
	examples/person_detection/person_detect_model_data.cc
)

# magic wand
add_library(
	magic_wand
	examples/magic_wand/accelerometer_handler.cc
	examples/magic_wand/gesture_predictor.cc
	examples/magic_wand/magic_wand_model_data.cc
	examples/magic_wand/mw_main.cc
	examples/magic_wand/mw_main_functions.cc
	examples/magic_wand/mw_output_handler.cc
)

# micro speech
add_library(
	micro_speech
	examples/micro_speech/audio_provider.cc
	examples/micro_speech/command_responder.cc
	examples/micro_speech/feature_provider.cc
	examples/micro_speech/ms_main.cc
	examples/micro_speech/ms_main_functions.cc
	examples/micro_speech/micro_speech_model_data.cc
	examples/micro_speech/recognize_commands.cc
	examples/micro_speech/micro_features/micro_features_generator.cc
	examples/micro_speech/micro_features/micro_model_settings.cc
)

set(
	TARGET_LIST
	hello_world
	person_detection
	magic_wand
	micro_speech
)

foreach(TARGET ${TARGET_LIST})
	target_include_directories(${TARGET} PRIVATE
		${CMAKE_CURRENT_LIST_DIR}
		third_party/ruy
		third_party/gemmlowp
		third_party/kissfft
		third_party/flatbuffers
		third_party/flatbuffers/include
	)
endforeach()

#foreach(TARGET ${TARGET_LIST})
#	target_link_libraries(
#		${TARGET}
#		tflm_cmsis
#	)
#endforeach()
		
