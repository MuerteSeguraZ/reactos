list(APPEND HAL_X2APIC_ASM_SOURCE
    x2apic/x2apictrap.S)

list(APPEND HAL_X2APIC_SOURCE
    x2apic/x2apic.c
    x2apic/x2apicsmp.c)

add_asm_files(lib_hal_x2apic_asm ${HAL_X2APIC_ASM_SOURCE})
add_library(lib_hal_x2apic OBJECT ${HAL_X2APIC_SOURCE} ${lib_hal_x2apic_asm})
add_dependencies(lib_hal_x2apic asm bugcodes xdk)