
list(APPEND HAL_X2APIC_SOURCE
    x2apic/x2apic.c)

add_library(lib_hal_x2apic OBJECT ${HAL_X2APIC_SOURCE})
add_dependencies(lib_hal_x2apic bugcodes xdk)