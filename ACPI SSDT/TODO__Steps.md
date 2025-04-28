Windows ADK or ACPI Component Architecture Downloads (Windows* Binary Tools)
- https://learn.microsoft.com/en-us/windows-hardware/get-started/adk-install#download-the-adk-101261002454-december-2024
- https://www.intel.com/content/www/us/en/download/774881/acpi-component-architecture-downloads-windows-binary-tools.html


BACKUP ACPI WITH IASL:
- Admin Terminal:
    - ./acpidump -b

NOTE: Your paths will be different.

Windows ADK:
- Download ADK
- cd C:\Program Files (x86)\Windows Kits\10\Tools\10.0.26100.0\x64\ACPIVerify
- Admin Terminal:
    - ./asl.exe "C:\Users\Neptune\Documents\GitHub\PCIe-PCI-ISA\ACPI SSDT\IT8893_ACPI_SSDT.asl"
    - This should output SSDT-IT8893E.aml in C:\Program Files (x86)\Windows Kits\10\Tools\10.0.26100.0\x64\ACPIVerify

IASL Windows:
- Download iasl and extract to some path:
- cd C:\Users\Neptune\Documents\GitHub\PCIe-PCI-ISA\ACPI SSDT\acpitools>
- Admin Terminal:
    - ./iasl.exe -oa ../IT8893_ACPI_SSDT.asl
    - This should output SSDT-IT8893E.aml in C:\Users\Neptune\Documents\GitHub\PCIe-PCI-ISA\ACPI SSDT


EXAMPLE IASL OUTPUT:
```
PS C:\Users\Neptune\Documents\GitHub\PCIe-PCI-ISA\ACPI SSDT\acpitools> ./iasl.exe -oa ../IT8893_ACPI_SSDT.asl

Intel ACPI Component Architecture
ASL+ Optimizing Compiler/Disassembler version 20250404
Copyright (c) 2000 - 2025 Intel Corporation

../IT8893_ACPI_SSDT.asl     31:             Method (_DSM, 4, NotSerialized)
Remark   2120 -                                       ^ Control Method should be made Serialized due to creation of named objects within (\_SB.PCI0.PCIE._DSM)

../IT8893_ACPI_SSDT.asl     33:                 If (LEqual (Arg0, ToUUID ("a0b5b7c6-1318-441c-b0c9-fe695eaf949b")))
Remark   2184 -                                                                           Unknown UUID string ^

../IT8893_ACPI_SSDT.asl     45:                         OperationRegion (PXC1, SystemMemory, 0xE0000000, 0x1000)  // Adjust base address
Warning  3175 -                                                            ^ Static OperationRegion should be declared outside control method

../IT8893_ACPI_SSDT.asl     45:                         OperationRegion (PXC1, SystemMemory, 0xE0000000, 0x1000)  // Adjust base address
Remark   2173 -                                                            ^ Creation of named objects within a method is highly inefficient, use globals or method local variables instead (\_SB.PCI0.PCIE._DSM)

../IT8893_ACPI_SSDT.asl     60:                         OperationRegion (PXC2, SystemMemory, 0xE0000000, 0x1000)  // Adjust base address
Warning  3175 -                                                            ^ Static OperationRegion should be declared outside control method

../IT8893_ACPI_SSDT.asl     60:                         OperationRegion (PXC2, SystemMemory, 0xE0000000, 0x1000)  // Adjust base address
Remark   2173 -                                                            ^ Creation of named objects within a method is highly inefficient, use globals or method local variables instead (\_SB.PCI0.PCIE._DSM)

../IT8893_ACPI_SSDT.asl     75:                         OperationRegion (PXC3, SystemMemory, 0xE0000000, 0x1000)  // Adjust base address
Warning  3175 -                                                            ^ Static OperationRegion should be declared outside control method

../IT8893_ACPI_SSDT.asl     75:                         OperationRegion (PXC3, SystemMemory, 0xE0000000, 0x1000)  // Adjust base address
Remark   2173 -                                                            ^ Creation of named objects within a method is highly inefficient, use globals or method local variables instead (\_SB.PCI0.PCIE._DSM)

ASL Input:     ../IT8893_ACPI_SSDT.asl -    4694 bytes     35 keywords      0 source lines
AML Output:    ../SSDT-IT8893E.aml -     296 bytes     24 opcodes      11 named objects

Compilation successful. 0 Errors, 3 Warnings, 5 Remarks, 0 Optimizations
PS C:\Users\Neptune\Documents\GitHub\PCIe-PCI-ISA\ACPI SSDT\acpitools>
```