DefinitionBlock ("SSDT-IT8893E.aml", "SSDT", 2, "CUSTOM", "IT8893E", 0x00000001)
{
    External (\_SB.PCI0, DeviceObj)
    
    Scope (\_SB.PCI0)
    {
        Device (PCIE)  
        {
            Name (_ADR, 0x001C0003)  // PCI address 0:1C:3
            // You need to change this line to the address where which ever bridge exists.
            // Adjust the path as needed for your system
            // Windows use:
            // wmic path win32_pnpentity get name,deviceid | findstr "8893"
            // Gives you something like : PCI\VEN_1283&DEV_8893&SUBSYS_309717AA&REV_41\4&3B34F50C&0&00E3
            // The important part is the end: 00E3
                // This translates to:
                    // Bus: 0 (implied/default)
                    // Device: 1C (28 in decimal, which is E in hexadecimal when shifted)
                    // Function: 3
            // This is how you get 0x001C0003
            // Windows can also use device manager, select bridge, properties, details, location paths
            // You'll see something like PCIROOT(0)#PCI(1C03)#PCI(0000)
            // 1C03 is the important bit.

            Method (_INI, 0, NotSerialized)
            {
                // Initialize bridge settings
            }
            
            // Configure PCI Express capability registers
            Method (_DSM, 4, NotSerialized)
            {
                If (LEqual (Arg0, ToUUID ("a0b5b7c6-1318-441c-b0c9-fe695eaf949b")))
                {
                    // Function 0: Query support
                    If (LEqual (Arg2, Zero))
                    {
                        Return (Buffer (One) { 0x3F })  // Functions 0-5 supported
                    }
                    
                    // Function 1: Configure Legacy Mode
                    If (LEqual (Arg2, One))
                    {
                        // Enable Legacy Mode (Transaction Layer Control Register bit 13)
                        OperationRegion (PXC1, SystemMemory, 0xE0000000, 0x1000)  // Adjust base address
                        Field (PXC1, AnyAcc, NoLock, Preserve)
                        {
                            Offset (0x50),  // TLCR - Transaction Layer Control Register
                            , 13,
                            LMEN, 1        // Legacy Mode Enable
                        }
                        Store (One, LMEN)  // Enable Legacy Mode
                        Return (Zero)
                    }
                    
                    // Function 2: Set Maximum Payload Size
                    If (LEqual (Arg2, 2))
                    {
                        // Set Max Payload Size to 256 bytes
                        OperationRegion (PXC2, SystemMemory, 0xE0000000, 0x1000)  // Adjust base address
                        Field (PXC2, AnyAcc, NoLock, Preserve)
                        {
                            Offset (0x78),  // Device Control Register
                            , 5,            // Skip 5 bits
                            MPLS, 3         // Maximum Payload Size
                        }
                        Store (0x01, MPLS)  // 001b: 256 bytes max
                        Return (Zero)
                    }
                    
                    // Function 3: Configure PCI Arbiter
                    If (LEqual (Arg2, 3))
                    {
                        // Configure PCI Arbiter
                        OperationRegion (PXC3, SystemMemory, 0xE0000000, 0x1000)  // Adjust base address
						
                        Field (PXC3, AnyAcc, NoLock, Preserve)
                        {
                            Offset (0x64),  // PCI Control Register
                            , 8,            // Skip 8 bits
                            PCIA, 8         // PCI Arbiter Priority
                        }
                        Store (0xAA, PCIA)  // Default priority setting
                        Return (Zero)
                    }
                    
                    // Function 4: Configure memory ranges
                    If (LEqual (Arg2, 4))
                    {
                        // Configure memory base/limit registers
                        Return (Zero)
                    }
                    
                    // Function 5: Configure I/O ranges
                    If (LEqual (Arg2, 5))
                    {
                        // Configure I/O base/limit registers
                        Return (Zero)
                    }
                }
                Return (Buffer (One) { 0x00 })
            }
        }
    }
}