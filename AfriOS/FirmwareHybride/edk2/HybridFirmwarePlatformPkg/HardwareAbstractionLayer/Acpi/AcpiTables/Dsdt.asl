/** @file
  Differentiated System Description Table (DSDT) for Hybrid Firmware.

  Declares:
    - Scope(\_SB) container
    - CPU device (one CPU object per logical processor)
    - HPET device (High-Precision Event Timer)
    - RTC device (Real-Time Clock)
    - AfriOS platform device (\_SB.AFRIOS) with method _MSG returning a
      vendor string for OS-side identification.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

DefinitionBlock ("Dsdt.aml", "DSDT", 2, "AFRIOS", "HYBRID", 0x00000001)
{
    //
    // System Bus scope. All platform devices live under here.
    //
    Scope (\_SB)
    {
        //
        // PCI Host Bridge — present on PC-class platforms (X64/IA32).
        //
        Device (PCI0)
        {
            Name (_HID, EisaId ("PNP0A03"))
            Name (_ADR, 0x00)

            Method (_BBN, 0, NotSerialized)
            {
                Return (0x00)
            }

            Method (_CRS, 0, NotSerialized)
            {
                Return (ConcatenateResourceTemplate (
                            CreateDWordField (Buffer (0x07) { 0x47, 0x01, 0x00, 0x00, 0xFF, 0x00, 0x00 }, 0),
                            Buffer (0x17) { 0x88, 0x0D, 0x00, 0x02, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0xFF, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                            0x79, 0x00 })
                            ))
            }
        }

        //
        // CPU device — represents the BSP. APs are added dynamically by
        // the platform's ACPI SSDT (see AcpiTableGenerator.c).
        //
        Device (CPU0)
        {
            Name (_HID, "ACPI0007")
            Name (_UID, 0x00)

            Method (_STA, 0, NotSerialized)
            {
                Return (0x0F)
            }

            //
            // Power states for the CPU. Real values are filled in by the
            // platform's _CST/_PSS methods.
            //
            Name (_CST, Package (0x02)
            {
                0x01,
                Package (0x04) { 0x01, 0x08, 0x00, 0x00 }
            })
        }

        //
        // High-Precision Event Timer (HPET). Mandatory for modern OSes.
        //
        Device (HPET)
        {
            Name (_HID, EisaId ("PNP0103"))
            Name (_UID, 0x00)
            Name (_STA, 0x0F)

            Method (_CRS, 0, NotSerialized)
            {
                Return (Buffer (0x17) {
                    0x86, 0x09, 0x00, 0x01, 0x00, 0xD0, 0xFE, 0x00,  // 32-bit MMIO @ 0xFED00000
                    0x04, 0x00,
                    0x79, 0x00
                })
            }
        }

        //
        // Real-Time Clock (RTC). Required for legacy time-of-day services.
        //
        Device (RTC)
        {
            Name (_HID, EisaId ("PNP0B00"))
            Name (_UID, 0x00)
            Name (_STA, 0x0F)

            Method (_CRS, 0, NotSerialized)
            {
                Return (Buffer (0x12) {
                    0x47, 0x01, 0x70, 0x00, 0x70, 0x00, 0x02, 0x00,  // IO 0x70..0x71
                    0x22, 0x40, 0x00,                                  // IRQ 8
                    0x79, 0x00
                })
            }
        }

        //
        // Power Button device.
        //
        Device (PWRB)
        {
            Name (_HID, EisaId ("PNP0C0C"))
            Name (_UID, 0x01)
            Name (_STA, 0x0F)
        }

        //
        // Hybrid Power Source device. Custom AfriOS ID — solar/battery/AC.
        //
        Device (APWR)
        {
            Name (_HID, "AFRI0001")
            Name (_UID, 0x01)
            Name (_STA, 0x0F)

            //
            // _PSR — power source report. 0 = battery, 1 = AC, 2 = solar.
            //
            Method (_PSR, 0, Serialized)
            {
                Return (0x02)   // Solar by default for the hybrid firmware.
            }
        }

        //
        // AfriOS Platform Device. OS-side driver matches on this ACPI object
        // to identify the platform and read vendor metadata via _MSG.
        //
        Device (AFRIOS)
        {
            Name (_HID, "AFRI0000")
            Name (_UID, 0x00)
            Name (_STA, 0x0F)

            //
            // _MSG — returns a UTF-16 vendor string. The OS uses this to
            // identify the platform (e.g., to apply quirks).
            //
            Method (_MSG, 0, Serialized)
            {
                Return (Unicode ("AfriOS Hybrid Firmware v0.1"))
            }

            //
            // _UID helpers exposed to the OS-side AfriOS platform driver.
            //
            Method (SLOT, 0, Serialized)
            {
                Return (0x00)   // Active A/B slot (0=A, 1=B). See ABSlotManager.
            }
        }
    }

    //
    // Top-level power-management scope — mirrors the EDK2 reference DSDT.
    //
    Scope (\_GPE)
    {
        Method (_L0D, 0, NotSerialized)
        {
            // Notify the AfriOS platform device that a power event occurred.
            Notify (\_SB.AFRIOS, 0x80)
        }
    }

    //
    // Root-level system info methods.
    //
    Method (\_S0, 0, NotSerialized)
    {
        Return (Package (0x02) { 0x00, 0x00 })
    }
}
