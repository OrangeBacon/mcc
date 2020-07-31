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
)
target_include_directories(mcc PRIVATE src)
target_compile_options(mcc PRIVATE ${CFLAGS})
set_property(TARGET mcc PROPERTY C_STANDARD 11)