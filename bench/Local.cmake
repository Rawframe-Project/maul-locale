# The unit names are a data file of their own (record mloc-0024); the
# benchmark reads it from the source tree.
target_compile_definitions(${PROJECT_NAME}_bench
    PRIVATE MLOC_UNITS_FILE="${PROJECT_SOURCE_DIR}/src/generated/units.mloc")
