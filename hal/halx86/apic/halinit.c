/*
 * PROJECT:     ReactOS Hardware Abstraction Layer
 * LICENSE:     GPL-2.0-or-later (https://spdx.org/licenses/GPL-2.0-or-later)
 * PURPOSE:     Initialize the APIC HAL
 * COPYRIGHT:   Copyright 2011 Timo Kreuzer <timo.kreuzer@reactos.org>
 */

/* INCLUDES *****************************************************************/

#include <hal.h>
#include "apicp.h"
#include <smp.h>
#define NDEBUG
#include <debug.h>

VOID
NTAPI
ApicInitializeLocalApic(ULONG Cpu);

BOOLEAN
NTAPI
X2ApicIsSupported(VOID);

VOID
NTAPI
X2ApicInitializeLocalApic(ULONG Cpu);

static ULONG HalpApicMode = HALP_APIC_MODE_LEGACY;

/* FUNCTIONS ****************************************************************/

VOID
NTAPI
HalpInitProcessor(
    IN ULONG ProcessorNumber,
    IN PLOADER_PARAMETER_BLOCK LoaderBlock)
{
    if (ProcessorNumber == 0)
    {
        HalpParseApicTables(LoaderBlock);

        if (X2ApicIsSupported())
        {
            HalpApicMode = HALP_APIC_MODE_X2APIC;
        }
    }

    HalpSetupProcessorsTable(ProcessorNumber);

    /* Initialize the local APIC for this cpu, using whichever mode was
       decided for the whole system above */
    if (HalpApicMode == HALP_APIC_MODE_X2APIC)
    {
        X2ApicInitializeLocalApic(ProcessorNumber);
    }
    else
    {
        ApicInitializeLocalApic(ProcessorNumber);
    }

    /* Initialize profiling data (but don't start it) */
    HalInitializeProfiling();

    /* Initialize the timer */
    //ApicInitializeTimer(ProcessorNumber);
}

VOID
HalpInitPhase0(IN PLOADER_PARAMETER_BLOCK LoaderBlock)
{
    DPRINT1("Using HAL: APIC %s %s %s\n",
            (HalpApicMode == HALP_APIC_MODE_X2APIC) ? "x2APIC" : "xAPIC",
            (HalpBuildType & PRCB_BUILD_UNIPROCESSOR) ? "UP" : "SMP",
            (HalpBuildType & PRCB_BUILD_DEBUG) ? "DBG" : "REL");

    HalpPrintApicTables();

    /* Enable clock interrupt handler */
    HalpEnableInterruptHandler(IDT_INTERNAL,
                               0,
                               APIC_CLOCK_VECTOR,
                               CLOCK2_LEVEL,
                               HalpClockInterrupt,
                               Latched);

    /* Enable profile interrupt handler */
    HalpEnableInterruptHandler(IDT_DEVICE,
                               0,
                               APIC_PROFILE_VECTOR,
                               APIC_PROFILE_LEVEL,
                               HalpProfileInterrupt,
                               Latched);
}

VOID
HalpInitPhase1(VOID)
{
    /* Initialize DMA. NT does this in Phase 0 */
    HalpInitDma();
}

/* EOF */
