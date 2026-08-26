/*
 * PROJECT:     ReactOS HAL
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     SMP specific x2APIC code
 * COPYRIGHT:   Copyright 2026 Alex Mendoza <05alex.mendozaa@gmail.com>
 */

/* INCLUDES *******************************************************************/

#include <hal.h>
#include "x2apicp.h"
#include <smp.h>

#define NDEBUG
#include <debug.h>

extern PPROCESSOR_IDENTITY HalpProcessorIdentity;

/* INTERNAL FUNCTIONS *********************************************************/

/**
 * @brief
 * Sends an IPI to one or more processors. The x2APIC ICR
 * write is a single 64 bit MSR write defined to never fail, so there is
 * no idle/pending check here.
 *
 * @param[in] DestinationProcessor
 * 32 bit x2APIC ID of the target processor, ignored for a shorthand.
 *
 * @param[in] Vector
 * Interrupt vector to deliver.
 *
 * @param[in] MessageType
 * Delivery mode.
 *
 * @param[in] TriggerMode
 * APIC_TGM_Edge or APIC_TGM_Level.
 *
 * @param[in] DestinationShortHand
 * Where to send the interrupt.
 *
 * @return
 * None.
 **/
VOID
NTAPI
X2ApicRequestGlobalInterrupt(
    _In_ ULONG DestinationProcessor,
    _In_ UCHAR Vector,
    _In_ APIC_MT MessageType,
    _In_ APIC_TGM TriggerMode,
    _In_ APIC_DSH DestinationShortHand)
{
    ULONG Flags;
    APIC_INTERRUPT_COMMAND_REGISTER Icr;

    /* Disable interrupts while we build and send the ICR */
    Flags = __readeflags();
    _disable();

    Icr.LongLong = 0;
    Icr.Vector = Vector;
    Icr.MessageType = MessageType;
    Icr.DestinationMode = APIC_DM_Physical;
    Icr.DeliveryStatus = 0;
    Icr.Level = 0;
    Icr.TriggerMode = TriggerMode;
    Icr.DestinationShortHand = DestinationShortHand;
    Icr.Destination = DestinationProcessor;

    /* Single 64 bit write sends the interrupt */
    X2ApicWriteIcr(Icr);

    if (Flags & EFLAGS_INTERRUPT_MASK)
    {
        _enable();
    }
}

/* SMP SUPPORT FUNCTIONS ******************************************************/

VOID
NTAPI
X2ApicStartApplicationProcessor(
    _In_ ULONG NTProcessorNumber,
    _In_ PHYSICAL_ADDRESS StartupLoc)
{
    ASSERT(StartupLoc.HighPart == 0);
    ASSERT((StartupLoc.QuadPart & 0xFFF) == 0);
    ASSERT((StartupLoc.QuadPart & 0xFFF00FFF) == 0);

    /* Init IPI */
    X2ApicRequestGlobalInterrupt(HalpProcessorIdentity[NTProcessorNumber].LapicId, 0,
        APIC_MT_INIT, APIC_TGM_Edge, APIC_DSH_Destination);

    /* De-Assert Init IPI */
    X2ApicRequestGlobalInterrupt(HalpProcessorIdentity[NTProcessorNumber].LapicId, 0,
        APIC_MT_INIT, APIC_TGM_Level, APIC_DSH_Destination);

    /* Stall execution for a bit to give APIC time */
    KeStallExecutionProcessor(200);

    /* Startup IPI */
    X2ApicRequestGlobalInterrupt(HalpProcessorIdentity[NTProcessorNumber].LapicId, (StartupLoc.LowPart) >> 12,
        APIC_MT_Startup, APIC_TGM_Edge, APIC_DSH_Destination);
}

/* EOF */