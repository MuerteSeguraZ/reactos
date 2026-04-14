#include <ntdef.h>
#include <winddi.h>

extern PPALETTE PALETTE_ShareLockPalette(HPALETTE hPal);
extern VOID PALETTE_ShareUnlockPalette(PPALETTE ppal);

ENGAPI
ULONG
APIENTRY
EngQueryPalette(
    _In_ HPALETTE hPal,
    _Out_ ULONG *piMode,
    _In_ ULONG cColors,
    _Out_writes_opt_(cColors) ULONG *pulColors)
{
    PPALETTE ppal;
    ULONG cRet = 0;

    if (!hPal || !piMode)
        return 0;

    ppal = PALETTE_ShareLockPalette(hPal);
    if (!ppal)
        return 0;

    *piMode = ppal->flFlags & (PAL_INDEXED | PAL_BITFIELDS | PAL_RGB | PAL_BGR | PAL_CMYK);

    if (pulColors && cColors > 0)
    {
        if (*piMode & PAL_INDEXED)
        {
            cRet = min(cColors, ppal->NumColors);
            RtlCopyMemory(pulColors, ppal->IndexedColors, cRet * sizeof(PALETTEENTRY));
        }
        else if (*piMode & PAL_BITFIELDS)
        {
            if (cColors >= 3)
            {
                pulColors[0] = ppal->RedMask;
                pulColors[1] = ppal->GreenMask;
                pulColors[2] = ppal->BlueMask;
                cRet = 3;
            }
        }
    }

    PALETTE_ShareUnlockPalette(ppal);
    return cRet;
}