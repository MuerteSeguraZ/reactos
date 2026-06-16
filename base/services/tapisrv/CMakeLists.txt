include_directories(${REACTOS_SOURCE_DIR}/sdk/include/reactos/idl)
add_rpc_files(server ${REACTOS_SOURCE_DIR}/sdk/include/reactos/idl/tapisrv.idl)

spec2def(tapisrv.dll tapisrv.spec ADD_IMPORTLIB)

list(APPEND SOURCE
    handles.c
    provider.c
    rpcserver.c
    tapisrv.c
    precomp.h
    ${CMAKE_CURRENT_BINARY_DIR}/tapisrv_s.c
    ${CMAKE_CURRENT_BINARY_DIR}/tapisrv_stubs.c)

add_library(tapisrv MODULE
    ${SOURCE}
    tapisrv.rc
    ${CMAKE_CURRENT_BINARY_DIR}/tapisrv.def)

set_module_type(tapisrv win32dll UNICODE)
target_link_libraries(tapisrv wine ${PSEH_LIB})
add_importlibs(tapisrv advapi32 rpcrt4 kernel32 msvcrt ntdll)
add_pch(tapisrv precomp.h SOURCE)
add_cd_file(TARGET tapisrv DESTINATION reactos/system32 FOR all)