find_package(Qt6 REQUIRED COMPONENTS Test)

function(toupie_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE ${ARGN} Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

toupie_add_test(tst_core toupie_core)
toupie_add_test(tst_battle toupie_core)
toupie_add_test(tst_tournament toupie_core)
toupie_add_test(tst_net toupie_net)
toupie_add_test(tst_gui toupie_ui)
set_tests_properties(tst_gui PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
