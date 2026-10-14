# The unit names, the spell-out rules and the display names are data
# files of their own (records mloc-0024, mloc-0034 and mloc-0036); the
# benchmark reads them from the source tree.
target_compile_definitions(${PROJECT_NAME}_bench
    PRIVATE MLOC_UNITS_FILE="${PROJECT_SOURCE_DIR}/src/generated/units.mloc"
            MLOC_SPELLOUT_FILE="${PROJECT_SOURCE_DIR}/src/generated/spellout.mloc"
            MLOC_LOCALE_NAMES_FILE="${PROJECT_SOURCE_DIR}/src/generated/names.mloc")
