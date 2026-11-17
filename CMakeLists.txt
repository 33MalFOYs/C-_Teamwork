cmake_minimum_required(VERSION 3.21)
project(ExpenseTracker VERSION 0.4.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")

option(EXPENSE_TRACKER_BUILD_GUI "Build Qt account and expense workspaces" ON)
include(CTest)
include(cmake/persistence-dependencies.cmake)
set(EXPENSE_TRACKER_INTEGRATION_CONFIG "" CACHE FILEPATH "Isolated expense_tracker_test_* config for database tests")

function(expense_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive-)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endfunction()

add_library(expense_tracker_core
    main/core/phase_status.cpp
    main/core/expense_tracker.cpp
    main/core/memory_cli.cpp
    main/core/validation.cpp
    main/core/expense.cpp
    main/core/expense_list.cpp
    main/services/expense_filter.cpp
    main/services/report_service.cpp
    main/services/memory_expense_service.cpp
    main/services/session.cpp
    main/services/file_manager.cpp
    main/services/config.cpp
    main/sql.cpp
    main/services/auth_service.cpp
    main/services/expense_service.cpp
)
target_include_directories(expense_tracker_core PUBLIC main/include)
target_link_libraries(expense_tracker_core PUBLIC expense_mysql_connector expense_sodium)
expense_warnings(expense_tracker_core)

add_executable(expense_tracker_cli main/cli.cpp)
target_link_libraries(expense_tracker_cli PRIVATE expense_tracker_core)
expense_warnings(expense_tracker_cli)

if(BUILD_TESTING)
    add_executable(expense_tracker_core_smoke tests/core_smoke.cpp)
    target_link_libraries(expense_tracker_core_smoke PRIVATE expense_tracker_core)
    expense_warnings(expense_tracker_core_smoke)
    add_test(NAME core_smoke COMMAND expense_tracker_core_smoke)

    foreach(test_name domain_tests services_tests cli_phase2_tests auth_policy_tests config_tests)
        add_executable(expense_tracker_${test_name} tests/${test_name}.cpp)
        target_link_libraries(expense_tracker_${test_name} PRIVATE expense_tracker_core)
        expense_warnings(expense_tracker_${test_name})
        add_test(NAME ${test_name} COMMAND expense_tracker_${test_name})
        set_tests_properties(${test_name} PROPERTIES TIMEOUT 30)
    endforeach()
    foreach(test_name persistence_tests cli_persistence_tests csv_tests)
        add_executable(expense_tracker_${test_name} tests/${test_name}.cpp)
        target_link_libraries(expense_tracker_${test_name} PRIVATE expense_tracker_core)
        expense_warnings(expense_tracker_${test_name})
        add_test(NAME ${test_name} COMMAND expense_tracker_${test_name} "${EXPENSE_TRACKER_INTEGRATION_CONFIG}")
        set_tests_properties(${test_name} PROPERTIES SKIP_RETURN_CODE 77 TIMEOUT 90 RUN_SERIAL TRUE)
    endforeach()
endif()

if(EXPENSE_TRACKER_BUILD_GUI)
    find_package(Qt6 6.5 REQUIRED COMPONENTS Widgets)
    qt_add_resources(EXPENSE_BRAND_SOURCES main/gui/brand.qrc)
    add_library(expense_tracker_widgets
        ${EXPENSE_BRAND_SOURCES}
        main/gui/theme.cpp
        main/gui/chart_widgets.cpp
        main/gui/login_window.cpp
        main/gui/expense_page.cpp
        main/gui/reports_page.cpp
        main/gui/memory_window.cpp
        main/gui/persistent_controller.cpp
        main/gui.cpp
    )
    target_include_directories(expense_tracker_widgets PUBLIC main/gui)
    target_link_libraries(expense_tracker_widgets PUBLIC expense_tracker_core Qt6::Widgets)
    expense_warnings(expense_tracker_widgets)

    add_executable(expense_tracker_gui WIN32 main/main.cpp)
    target_link_libraries(expense_tracker_gui PRIVATE expense_tracker_widgets)
    expense_warnings(expense_tracker_gui)

    # Deliberately separate from the production login executable. No session or
    # database access is granted by this explicitly temporary Phase 2 workspace.
    add_executable(expense_tracker_memory_gui WIN32 main/gui/memory_main.cpp)
    target_link_libraries(expense_tracker_memory_gui PRIVATE expense_tracker_widgets)
    expense_warnings(expense_tracker_memory_gui)

    if(BUILD_TESTING)
        find_package(Qt6 REQUIRED COMPONENTS Test)
        add_executable(expense_tracker_gui_smoke tests/gui_smoke.cpp)
        target_link_libraries(expense_tracker_gui_smoke PRIVATE expense_tracker_widgets Qt6::Test)
        expense_warnings(expense_tracker_gui_smoke)
        add_test(NAME gui_smoke COMMAND expense_tracker_gui_smoke)
        set_tests_properties(gui_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 20)

        add_executable(expense_tracker_gui_phase2_tests tests/gui_phase2_tests.cpp)
        target_link_libraries(expense_tracker_gui_phase2_tests PRIVATE expense_tracker_widgets Qt6::Test)
        expense_warnings(expense_tracker_gui_phase2_tests)
        add_test(NAME gui_phase2_tests COMMAND expense_tracker_gui_phase2_tests)
        set_tests_properties(gui_phase2_tests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" TIMEOUT 30)

        foreach(test_name gui_phase3_tests gui_persistence_tests)
            add_executable(expense_tracker_${test_name} tests/${test_name}.cpp)
            target_link_libraries(expense_tracker_${test_name} PRIVATE expense_tracker_widgets Qt6::Test)
            expense_warnings(expense_tracker_${test_name})
            add_test(NAME ${test_name} COMMAND expense_tracker_${test_name} "${EXPENSE_TRACKER_INTEGRATION_CONFIG}")
            set_tests_properties(${test_name} PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen" SKIP_RETURN_CODE 77 TIMEOUT 90 RUN_SERIAL TRUE)
        endforeach()
    endif()
endif()

