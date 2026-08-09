/** @file
  Fixed ACPI Description Table (FADT) for Hybrid Firmware.

  Copyright (c) 2026, AfriOS. All rights reserved.
**/

#include <IndustryStandard/Acpi.h>

EFI_ACPI_6_3_FIXED_ACPI_DESCRIPTION_TABLE Fadt = {
  {
    EFI_ACPI_6_3_FIXED_ACPI_DESCRIPTION_TABLE_SIGNATURE,
    sizeof (EFI_ACPI_6_3_FIXED_ACPI_DESCRIPTION_TABLE),
    EFI_ACPI_6_3_FIXED_ACPI_DESCRIPTION_TABLE_REVISION,
    0,                          // Checksum will be updated at runtime
    {'A', 'F', 'R', 'I', 'O', 'S'},
    0,                          // OEM Table ID
    1,                          // OEM Revision
    0,                          // Creator ID
    0                           // Creator Revision
  },
  0,                            // Firmware Ctrl (FACS) - updated at runtime
  0,                            // DSDT - updated at runtime
  0,                            // Reserved
  EFI_ACPI_6_3_PM_PROFILE_MOBILE,
  0,                            // SCI_INT
  0,                            // SMI_CMD
  0,                            // ACPI_ENABLE
  0,                            // ACPI_DISABLE
  0,                            // S4BIOS_REQ
  0,                            // PSTATE_CNT
  0,                            // PM1a_EVT_BLK
  0,                            // PM1b_EVT_BLK
  0,                            // PM1a_CNT_BLK
  0,                            // PM1b_CNT_BLK
  0,                            // PM2_CNT_BLK
  0,                            // PM_TMR_BLK
  0,                            // GPE0_BLK
  0,                            // GPE1_BLK
  0,                            // PM1_EVT_LEN
  0,                            // PM1_CNT_LEN
  0,                            // PM2_CNT_LEN
  0,                            // PM_TMR_LEN
  0,                            // GPE0_LEN
  0,                            // GPE1_LEN
  0,                            // GPE1_BASE
  0,                            // CST_CNT
  0,                            // P_LVL2_LAT
  0,                            // P_LVL3_LAT
  0,                            // FLUSH_SIZE
  0,                            // FLUSH_STRIDE
  0,                            // DUTY_OFFSET
  0,                            // DUTY_WIDTH
  0,                            // DAY_ALRM
  0,                            // MON_ALRM
  0,                            // CENTURY
  0,                            // IAPC_BOOT_ARCH
  0,                            // Reserved
  EFI_ACPI_6_3_FIXED_FEATURE_FLAGS,
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // Reset Reg
  0,                            // Reset Value
  0,                            // ARM_BOOT_ARCH
  EFI_ACPI_6_3_FIXED_ACPI_DESCRIPTION_TABLE_REVISION,
  0,                            // X_FIRMWARE_CTRL
  0,                            // X_DSDT
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // X_PM1a_EVT_BLK
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // X_PM1b_EVT_BLK
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // X_PM1a_CNT_BLK
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // X_PM1b_CNT_BLK
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // X_PM2_CNT_BLK
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // X_PM_TMR_BLK
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // X_GPE0_BLK
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // X_GPE1_BLK
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}, // Sleep Control Reg
  {0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0}  // Sleep Status Reg
};
