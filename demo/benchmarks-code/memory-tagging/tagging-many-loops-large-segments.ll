; File: segment_stack_test.ll

declare ptr @llvm.wasm.segment.stack.new(ptr, i64)
declare void @llvm.wasm.segment.stack.free(ptr, ptr, i64)

define i32 @__main_void() {
entry:
  %static_size_array = alloca [10000 x i32], align 16
  %iteration_count = alloca i32
  store i32 40000, i32* %iteration_count
  
  br label %loop_start

loop_start:
  %count = load i32, i32* %iteration_count
  %is_done = icmp eq i32 %count, 0
  br i1 %is_done, label %loop_end, label %loop_body

loop_body:
  %segment_ptr = call ptr @llvm.wasm.segment.stack.new(ptr %static_size_array, i64  40000)
  call void @llvm.wasm.segment.stack.free(ptr %segment_ptr, ptr %static_size_array, i64  40000)

  %new_count = sub i32 %count, 1
  store i32 %new_count, i32* %iteration_count
  br label %loop_start

loop_end:
  ret i32 0
}
