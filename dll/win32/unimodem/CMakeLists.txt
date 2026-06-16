spec2def(unimodem.tsp unimodem.spec)

list(APPEND SOURCE
    comm.c
    enum.c
    line.c
    main.c
    provider.c
    precomp.h
    ${CMAKE_CURRENT_BINARY_DIR}/unimodem_stubs.c)

add_library(unimodem MODULE
    ${SOURCE}
    ${CMAKE_CURRENT_BINARY_DIR}/unimodem.def)

set_module_type(unimodem win32dll)
set_target_properties(unimodem PROPERTIES SUFFIX ".tsp")
target_link_libraries(unimodem wine)
add_importlibs(unimodem advapi32 kernel32 msvcrt ntdll)
add_pch(unimodem precomp.h SOURCE)
add_cd_file(TARGET unimodem DESTINATION reactos/system32 FOR all)