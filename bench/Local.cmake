# The unit names, the spell-out rules, the display names, the dates,
# the zone names and relative time are data files of their own (records
# mloc-0024, mloc-0034, mloc-0036 and mloc-0039 to mloc-0043); the
# benchmark reads them from the source tree. The datetime part's and the
# message part's and the grammar part's cases are files of their own.
target_sources(${PROJECT_NAME}_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/bench_dates.c
                                             ${CMAKE_CURRENT_SOURCE_DIR}/bench_messages.c
                                             ${CMAKE_CURRENT_SOURCE_DIR}/bench_grammar.c)
target_compile_definitions(${PROJECT_NAME}_bench
    PRIVATE MLOC_UNITS_FILE="${PROJECT_SOURCE_DIR}/src/generated/units.mloc"
            MLOC_SPELLOUT_FILE="${PROJECT_SOURCE_DIR}/src/generated/spellout.mloc"
            MLOC_LOCALE_NAMES_FILE="${PROJECT_SOURCE_DIR}/src/generated/names.mloc"
            MLOC_DATES_FILE="${PROJECT_SOURCE_DIR}/src/generated/dates.mloc"
            MLOC_ZONE_NAMES_FILE="${PROJECT_SOURCE_DIR}/src/generated/zone-names.mloc"
            MLOC_RELATIVE_FILE="${PROJECT_SOURCE_DIR}/src/generated/relative.mloc")
