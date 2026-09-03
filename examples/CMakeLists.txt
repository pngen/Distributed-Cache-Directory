set(EXAMPLES
  example_01_cache_identity
  example_02_state_registration
  example_03_multi_replica
  example_04_multi_domain
  example_05_local_vs_remote
  example_06_health_ranking
  example_07_reachability
  example_08_lease_liveness
  example_09_invalidation
  example_10_supersession
  example_11_tombstone
  example_12_historical_query
  example_13_persistence_recovery
  example_14_multiprocess_authority
  example_15_cuda_location
)

foreach(prefix ${EXAMPLES})
  string(REGEX REPLACE "^example_" "" name ${prefix})
  add_executable(${prefix} ${name}.cpp)
  target_link_libraries(${prefix} PRIVATE DistributedCacheDirectory::distributedcachedirectory)
  add_test(NAME ${prefix} COMMAND ${prefix})
endforeach()
