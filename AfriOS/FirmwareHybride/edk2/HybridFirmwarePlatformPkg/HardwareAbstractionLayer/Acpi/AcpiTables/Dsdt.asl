/** @file
  Differentiated System Description Table (DSDT) for Hybrid Firmware.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

DefinitionBlock ("Dsdt.aml", "DSDT", 2, "AFRIOS", "HYBRID", 0x00000001)
{
    Scope (_SB)
    {
        // System Bus
        Device (PCI0)
        {
            Name (_HID, EisaId ("PNP0A03"))
            Name (_ADR, 0x00)
            
            // PCI Host Bridge and Devices would be defined here
        }

        // Power Management Object
        Device (PWRB)
        {
            Name (_HID, EisaId ("PNP0C0C"))
            Name (_UID, 0x01)
        }

        // Hybrid Power Source Object
        Device (APWR)
        {
            Name (_HID, "AFRI0001") // Custom AfriOS Power ID
            Name (_UID, 0x01)

            /**
              Power Source Method (_PSR)
              Returns: 
                0 = Off-line (Battery/AC)
                1 = On-line (Solar Optimal)
            **/
            Method (_PSR, 0, Serialized)
            {
                // In a real implementation, this would read from a PMIO register or a PCD
                // For demonstration, we assume it's controlled by a global variable
                // defined in the firmware.
                Return (0x01) 
            }
        }
    }
}
