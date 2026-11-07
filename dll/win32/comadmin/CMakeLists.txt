spec2def(comadmin.dll comadmin.spec)

list(APPEND SOURCE
    catalog.c
    collection.c
    main.c
    object.c
    store.c
    typelib.c
    comadmin_private.h)

add_library(comadmin MODULE
    ${SOURCE}
    comadmin.rc
    ${CMAKE_CURRENT_BINARY_DIR}/comadmin.def)

set_module_type(comadmin win32dll)
add_idl_reg_scripts(comadmin regtypelib ${REACTOS_SOURCE_DIR}/sdk/include/psdk/comadmin.idl)
target_link_libraries(comadmin uuid)
add_importlibs(comadmin oleaut32 ole32 advapi32 msvcrt kernel32 ntdll)
add_cd_file(TARGET comadmin DESTINATION reactos/system32 FOR all)