# execute_process(
#     COMMAND npm install
#     WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
#     RESULT_VARIABLE npm_install_result
#     OUTPUT_VARIABLE npm_install_output
#     ERROR_VARIABLE npm_install_error
# )

# if (npm_install_result)
#     message(FATAL_ERROR "npm install failed: ${npm_install_error}")
# else()
#     message(STATUS "npm install succeeded: ${npm_install_output}")
# endif()

# execute_process(
#     COMMAND npm run staging
#     WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
#     RESULT_VARIABLE npm_run_staging_result
#     OUTPUT_VARIABLE npm_run_staging_output
#     ERROR_VARIABLE npm_run_staging_error
# )

# if (npm_run_staging_result)
#     message(FATAL_ERROR "npm run staging failed: ${npm_run_staging_error}")
# else()
#     message(STATUS "npm run staging succeeded: ${npm_run_staging_output}")
# endif()

# file(COPY ${CMAKE_CURRENT_LIST_DIR}/dist/
#      DESTINATION ${CMAKE_CURRENT_LIST_DIR}/../rootfs/usr/share/supervisor/www)

# message(STATUS "Files copied to rootfs/usr/share/supervisor/www")

add_custom_target(
    npm_install
    COMMAND npm install
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMENT "Installing npm dependencies"
)

add_custom_target(
    npm_run_staging
    COMMAND npm run staging
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMENT "Running npm run staging"
)

add_custom_target(
    deploy_web
    COMMAND rm -rf ${CMAKE_CURRENT_LIST_DIR}/../rootfs/usr/share/supervisor/www/*
    COMMAND mkdir -p ${CMAKE_CURRENT_LIST_DIR}/../rootfs/usr/share/supervisor/www/
    COMMAND cp -r ${CMAKE_CURRENT_LIST_DIR}/dist/* ${CMAKE_CURRENT_LIST_DIR}/../rootfs/usr/share/supervisor/www/
    WORKING_DIRECTORY ${CMAKE_CURRENT_LIST_DIR}
    COMMENT "Deploying web files"
)

add_dependencies(npm_run_staging npm_install)

add_dependencies(deploy_web npm_run_staging)

add_dependencies(${PROJECT_NAME} deploy_web)