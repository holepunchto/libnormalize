list(APPEND benches
  normalize
)

foreach(bench IN LISTS benches)
  add_executable(bench_${bench} ${bench}.c)

  target_link_libraries(
    bench_${bench}
    PRIVATE
      normalize_static
  )
endforeach()
