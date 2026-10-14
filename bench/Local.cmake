# The unit names and the spell-out rules are data files of their own
# (records mloc-0024 and mloc-0034); the benchmark reads them from the
# source tree.
target_compile_definitions(${PROJECT_NAME}_bench
    PRIVATE MLOC_UNITS_FILE="${PROJECT_SOURCE_DIR}/src/generated/units.mloc"
            MLOC_SPELLOUT_FILE="${PROJECT_SOURCE_DIR}/src/generated/spellout.mloc")
