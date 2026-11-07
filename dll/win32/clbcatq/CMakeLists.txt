spec2def(clbcatq.dll clbcatq.spec)
add_idl_headers(clbcatq_idlheader clbcatq_internal.idl)

add_library(clbcatq MODULE
    main.c
    clbcatq.rc
    ${CMAKE_CURRENT_BINARY_DIR}/clbcatq.def)

set_module_type(clbcatq win32dll)
add_dependencies(clbcatq clbcatq_idlheader)
add_importlibs(clbcatq msvcrt kernel32 ntdll)
add_cd_file(TARGET clbcatq DESTINATION reactos/system32 FOR all)