function(add_plugin NAME)
  if (ARGV1)
    message(STATUS "Ignoring plugin ${NAME}")
  else()
    message(STATUS "Configuring plugin ${NAME}...")


    get_all_sources(SOURCES "${CMAKE_CURRENT_SOURCE_DIR}")
    add_library(${NAME} SHARED ${SOURCES})
    set_target_properties(${NAME} PROPERTIES SUFFIX ".clap" PREFIX "")

    target_link_libraries(${NAME} common)

    # Print a message during compilation
    add_custom_target(${NAME}_print_build_message
      COMMAND ${CMAKE_COMMAND} -E echo "Plugin ${NAME} compiled."
      COMMENT "Compiling plugin ${NAME}..."
    )
    add_dependencies(${NAME} ${NAME}_print_build_message)

    # Copy the plugin to plugin directory
    if (CLAP_DIR)
      message(STATUS "will copy to ${CLAP_DIR}/${NAME}.clap")
      # Copy clap plugin to destination
      set(CLAP_DESTINATION ${CLAP_DIR}/${NAME}.clap)
      add_custom_command(
        OUTPUT ${CLAP_DESTINATION}
        DEPENDS $<TARGET_FILE:${NAME}>
        COMMAND ${CMAKE_COMMAND} -E copy $<TARGET_FILE:${NAME}> ${CLAP_DESTINATION})

    # Print a message during compilation
      set(COPY_TO_DEST_TARGET "${NAME}_copy_plugin_to_destination")
      add_custom_target(
        ${COPY_TO_DEST_TARGET} ALL
        DEPENDS ${CLAP_DESTINATION}
        COMMAND ${CMAKE_COMMAND} -E echo "Plugin ${NAME}"
        COMMENT "Copying plugin ${NAME} -> ${CLAP_DESTINATION}..."
      )
    endif()

    message(STATUS "${NAME} configured.")
  endif()
endfunction()
