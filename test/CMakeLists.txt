set(TESTS
  test_identity
  test_record_model
  test_cache_model
  test_indexes
  test_query
  test_ranking
  test_health
  test_reachability
  test_leases
  test_invalidation
  test_tombstones
  test_history
  test_persistence
  test_protocol
  test_property
  test_concurrency
  test_adversarial
  test_multiprocess
)

foreach(t ${TESTS})
  add_executable(${t} ${t}.cpp)
  target_link_libraries(${t} PRIVATE DistributedCacheDirectory::distributedcachedirectory)
  if(t STREQUAL "test_multiprocess")
    target_compile_definitions(${t} PRIVATE
      DCD_COORDINATOR_EXE="$<TARGET_FILE:distributed-cache-directory-coordinator>"
      DCD_WORKER_EXE="$<TARGET_FILE:distributed-cache-directory-worker>")
  endif()
  add_test(NAME ${t} COMMAND ${t})
endforeach()
