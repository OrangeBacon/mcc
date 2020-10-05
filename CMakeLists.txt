cmake_minimum_required(VERSION 3.9.4)

project(mcc)

set(CFLAGS
    -Wall
    -Wextra
    -Werror
    -pedantic
    -Wno-missing-field-initializers
)

add_executable(mcc
    src/main.c
    src/scanner.c
    src/file.c
    src/token.c
    src/ast.c
    src/memory.c
    src/parser.c
    src/x64.c
    src/symbolTable.c
    src/analysis.c
    src/assemble.c
    src/ir.c
    src/astLower.c
    src/x64Encode.c
    src/argParser.c
    src/driver.c
    src/lex.c
    src/lexString.c
    src/test.c
    src/colorText.c
)
target_include_directories(mcc PRIVATE src)
target_compile_options(mcc PRIVATE ${CFLAGS})

file(GLOB WIN_LIBS "C:/Program Files (x86)/Windows Kits/10/Lib/10.0.*/um/x64/")
list(GET WIN_LIBS -1 WIN_LIBS_LAST)
target_link_libraries(mcc PRIVATE ${WIN_LIBS_LAST}/pathcch.lib)
target_link_libraries(mcc PRIVATE ${WIN_LIBS_LAST}/shlwapi.lib)
set_property(TARGET mcc PROPERTY C_STANDARD 11)