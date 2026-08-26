/*
 * PROJECT:     ReactOS HAL
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     x2APIC spurious interrupt entrypoint
 * COPYRIGHT:   Copyright 2026 Alex Mendoza <05alex.mendozaa@gmail.com>
 */

/* INCLUDES *******************************************************************/

#include <asm.inc>

#ifdef _M_AMD64
.code

PUBLIC X2ApicSpuriousService
X2ApicSpuriousService:
    iretq

#else
.code

PUBLIC _X2ApicSpuriousService
_X2ApicSpuriousService:
    iret

#endif

END