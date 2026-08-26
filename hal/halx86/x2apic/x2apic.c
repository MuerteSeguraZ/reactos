/*
 * PROJECT:     ReactOS HAL
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     x2APIC detection and mode enable
 * COPYRIGHT:   Copyright 2026 Alex Mendoza <05alex.mendozaa@gmail.com>
 */

/* INCLUDES *******************************************************************/

#include <hal.h>
#include "x2apicp.h"

#define NDEBUG
#include <debug.h>

/* FUNCTIONS ******************************************************************/

/**
 * @brief
 * Checks whether the current processor supports x2APIC mode.
 *
 * @return
 * TRUE if CPUID.01H:ECX.x2APIC[bit 21] is set, FALSE otherwise.
 **/
BOOLEAN
NTAPI
X2ApicIsSupported(VOID)
{
    INT CpuInfo[4];

    __cpuid(CpuInfo, 1);

    return (CpuInfo[2] & (1 << CPUID_X2APIC_FEATURE_BIT)) != 0;
}

/**
 * @brief
 * Switches the current processor's local APIC into x2APIC mode.
 * 
 * @remarks
 * This must only be called after verifying support with X2ApicIsSupported,
 * and before you try to do any MSR based register access.
 *
 * @return
 * None.
 **/
VOID
NTAPI
X2ApicEnable(VOID)
{
    APIC_BASE_ADDRESS_REGISTER BaseRegister;

    BaseRegister.LongLong = __readmsr(MSR_APIC_BASE);

    /* xAPIC (enable) must already be set before x2APIC can be enabled */
    BaseRegister.Enable = 1;
    BaseRegister.EnableX2Apic = 1;

    __writemsr(MSR_APIC_BASE, BaseRegister.LongLong);
}

/**
 * @brief
 * Brings up the local x2APIC on the current processor. To do so, it enables x2APIC
 * mode, registers the spurious interrupt handler, then programs the
 * spurious vector register. The handler is registered before SIVR is
 * written so there's never a window where SIVR points at an unregistered
 * vector.
 *
 * @param[in] Cpu
 * Index of the current processor.
 *
 * @return
 * None.
 **/
VOID
NTAPI
X2ApicInitializeLocalApic(ULONG Cpu)
{
    APIC_SPURIOUS_INERRUPT_REGISTER SpIntRegister;

    UNREFERENCED_PARAMETER(Cpu);

    X2ApicEnable();

    /* Register the spurious ISR before SIVR ever points at it */
    KeRegisterInterruptHandler(X2APIC_SPURIOUS_VECTOR, X2ApicSpuriousService);

    /* Set spurious vector and SoftwareEnable */
    SpIntRegister.Long = X2ApicRead(APIC_SIVR);
    SpIntRegister.Vector = X2APIC_SPURIOUS_VECTOR;
    SpIntRegister.SoftwareEnable = 1;
    X2ApicWrite(APIC_SIVR, SpIntRegister.Long);
}