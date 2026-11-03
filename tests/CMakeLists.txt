find_package(Qt6 REQUIRED COMPONENTS Test)

function(toupie_add_test name)
    add_executable(${name} ${name}.cpp)
    target_link_libraries(${name} PRIVATE toupie_core Qt6::Test)
    add_test(NAME ${name} COMMAND ${name})
endfunction()

toupie_add_test(tst_core)
toupie_add_test(tst_battle)
