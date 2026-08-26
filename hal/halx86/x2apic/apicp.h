/*
 * PROJECT:     ReactOS Kernel
 * LICENSE:     MIT (https://spdx.org/licenses/MIT)
 * PURPOSE:     Private header file for x2APIC hal
 * COPYRIGHT:   Copyright 2026 Alex Mendoza <05alex.mendozaa@gmail.com>
 */

#pragma once

#define MSR_APIC_BASE 0x0000001B
#define X2APIC_MSR_BASE 0x00000800
#define CPUID_X2APIC_FEATURE_BIT 21
#define X2APIC_MSR_ICR 0x00000830
#define X2APIC_MSR_SELF_IPI 0x0000083F
#define X2APIC_SPURIOUS_VECTOR 0xDF

#include <pshpack1.h>
typedef union _APIC_BASE_ADDRESS_REGISTER
{
    UINT64 LongLong;
    struct
    {
        UINT64 Reserved1:8;
        UINT64 BootStrapCPUCore:1;
        UINT64 Reserved2:1;
        UINT64 EnableX2Apic:1;
        UINT64 Enable:1;
        UINT64 BaseAddress:40;
        UINT64 ReservedMBZ:12;
    };
} APIC_BASE_ADDRESS_REGISTER;
#include <poppack.h>

typedef enum _APIC_REGISTER
{
    APIC_ID = 0x0020,
    APIC_EOI = 0x00B0,
    APIC_SIVR = 0x00F0,
    APIC_ICR0 = 0x0300,
} APIC_REGISTER;

#include <pshpack1.h>
typedef union _APIC_INTERRUPT_COMMAND_REGISTER
{
    UINT64 LongLong;
    struct
    {
        UINT64 Vector:8;
        UINT64 MessageType:3;
        UINT64 DestinationMode:1;
        UINT64 DeliveryStatus:1;
        UINT64 ReservedMBZ:1;
        UINT64 Level:1;
        UINT64 TriggerMode:1;
        UINT64 ReservedMBZ2:2;
        UINT64 DestinationShortHand:2;
        UINT64 ReservedMBZ3:12;
        UINT64 Destination:32;
    };
} APIC_INTERRUPT_COMMAND_REGISTER;
#include <poppack.h>

FORCEINLINE
APIC_REGISTER
X2ApicMsrFromRegister(APIC_REGISTER Register)
{
    return (APIC_REGISTER)(X2APIC_MSR_BASE + (Register / 0x10));
}

FORCEINLINE
ULONG
X2ApicRead(_In_ APIC_REGISTER Register)
{
    return (ULONG)__readmsr(X2ApicMsrFromRegister(Register));
}

FORCEINLINE
VOID
X2ApicWrite(_In_ APIC_REGISTER Register, _In_ ULONG Value)
{
    __writemsr(X2ApicMsrFromRegister(Register), Value);
}

FORCEINLINE
VOID
X2ApicWriteIcr(_In_ APIC_INTERRUPT_COMMAND_REGISTER IcrValue)
{
    __writemsr(X2APIC_MSR_ICR, IcrValue.LongLong);
}

BOOLEAN
NTAPI
X2ApicIsSupported(VOID);

VOID
NTAPI
X2ApicEnable(VOID);

#include <pshpack1.h>
typedef union _APIC_SPURIOUS_INERRUPT_REGISTER
{
    UINT32 Long;
    struct
    {
        UINT32 Vector:8;
        UINT32 SoftwareEnable:1;
        UINT32 ReservedMBZ:23;
    };
} APIC_SPURIOUS_INERRUPT_REGISTER;
#include <poppack.h>

VOID __cdecl X2ApicSpuriousService(VOID);

VOID
NTAPI
X2ApicInitializeLocalApic(ULONG Cpu);

typedef enum _APIC_MT
{
    APIC_MT_Fixed = 0,
    APIC_MT_LowestPriority = 1,
    APIC_MT_SMI = 2,
    APIC_MT_RemoteRead = 3,
    APIC_MT_NMI = 4,
    APIC_MT_INIT = 5,
    APIC_MT_Startup = 6,
    APIC_MT_ExtInt = 7,
} APIC_MT;

/* Trigger Mode */
typedef enum _APIC_TGM
{
    APIC_TGM_Edge,
    APIC_TGM_Level
} APIC_TGM;

typedef enum _APIC_DM
{
    APIC_DM_Physical,
    APIC_DM_Logical
} APIC_DM;

/* Destination Short Hand */
typedef enum _APIC_DSH
{
    APIC_DSH_Destination,
    APIC_DSH_Self,
    APIC_DSH_AllIncludingSelf,
    APIC_DSH_AllExcludingSelf
} APIC_DSH;

VOID
NTAPI
X2ApicRequestGlobalInterrupt(
    _In_ ULONG DestinationProcessor,
    _In_ UCHAR Vector,
    _In_ APIC_MT MessageType,
    _In_ APIC_TGM TriggerMode,
    _In_ APIC_DSH DestinationShortHand);

VOID
NTAPI
X2ApicStartApplicationProcessor(
    _In_ ULONG NTProcessorNumber,
    _In_ PHYSICAL_ADDRESS StartupLoc);