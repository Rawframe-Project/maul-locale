# The unit names, the spell-out rules, the display names, the dates, the
# zone names and relative time are data files of their own; the benchmark
# reads them from the source tree. The datetime part's and the message
# part's, the grammar part's and the collation part's cases are files of
# their own; collation reads its data file too.
target_sources(${PROJECT_NAME}_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/bench_dates.c
                                             ${CMAKE_CURRENT_SOURCE_DIR}/bench_messages.c
                                             ${CMAKE_CURRENT_SOURCE_DIR}/bench_grammar.c
                                             ${CMAKE_CURRENT_SOURCE_DIR}/bench_collation.c)
target_compile_definitions(${PROJECT_NAME}_bench
    PRIVATE MLOC_UNITS_FILE="${PROJECT_SOURCE_DIR}/src/generated/units.mloc"
            MLOC_SPELLOUT_FILE="${PROJECT_SOURCE_DIR}/src/generated/spellout.mloc"
            MLOC_LOCALE_NAMES_FILE="${PROJECT_SOURCE_DIR}/src/generated/names.mloc"
            MLOC_DATES_FILE="${PROJECT_SOURCE_DIR}/src/generated/dates.mloc"
            MLOC_ZONE_NAMES_FILE="${PROJECT_SOURCE_DIR}/src/generated/zone-names.mloc"
            MLOC_RELATIVE_FILE="${PROJECT_SOURCE_DIR}/src/generated/relative.mloc"
            MLOC_COLLATION_FILE="${PROJECT_SOURCE_DIR}/src/generated/collation.mloc")
# The catalog cases read a set of 2,000 keys that the catalog tool
# compiles at build time; without the tools the benchmark has no catalog
# cases.
target_sources(${PROJECT_NAME}_bench PRIVATE ${CMAKE_CURRENT_SOURCE_DIR}/bench_catalog.c)
if(TARGET mloccat)
    set(MLOC_BENCH_CATALOG ${CMAKE_CURRENT_BINARY_DIR}/catalog)
    add_custom_command(
        OUTPUT ${MLOC_BENCH_CATALOG}/en.mlct ${MLOC_BENCH_CATALOG}/fr.mlct
        COMMAND ${CMAKE_COMMAND} -E make_directory ${MLOC_BENCH_CATALOG}
        COMMAND ${CMAKE_COMMAND} -DDIR=${MLOC_BENCH_CATALOG} -P
                ${CMAKE_CURRENT_SOURCE_DIR}/catalog_set.cmake
        COMMAND mloccat compile ${MLOC_BENCH_CATALOG} ${MLOC_BENCH_CATALOG}/en.mf2
                ${MLOC_BENCH_CATALOG}/fr.mf2
        DEPENDS mloccat ${CMAKE_CURRENT_SOURCE_DIR}/catalog_set.cmake
        VERBATIM)
    add_custom_target(${PROJECT_NAME}_bench_catalog
                      DEPENDS ${MLOC_BENCH_CATALOG}/en.mlct ${MLOC_BENCH_CATALOG}/fr.mlct)
    add_dependencies(${PROJECT_NAME}_bench ${PROJECT_NAME}_bench_catalog)
    target_compile_definitions(${PROJECT_NAME}_bench
                               PRIVATE MLOC_BENCH_CATALOG_DIR="${MLOC_BENCH_CATALOG}")
endif()
