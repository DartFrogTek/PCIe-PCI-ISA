## TODO
- Replace the FILE_GUID in the .inf with a unique GUID via uuidgen.
- Confirm build environment EDK2 links against MdePkg.
- Use a shell to load/test dynamically.

---

### **Test Dynamically with a UEFI Shell**
It's possible to load a DXE driver **manually** from a USB stick or virtual disk with the [UEFI Shell](https://github.com/tianocore/edk2-shell).

Steps:
1. Build your driver to a `.efi` binary (e.g., `IsaIoForwardingDxe.efi`).
2. Copy it to a FAT32 USB drive.
3. Boot into the UEFI shell (some BIOSes let you add it as a boot option).
4. At the shell prompt:
   ```shell
   fs0:
   load IsaIoForwardingDxe.efi
   ```
5. If successful, the `ISA_MOTHERBOARD_IO` attribute will be set immediately. You can even call `drivers` or `devtree` to confirm it's loaded.

---

## 1. Using UEFI Shell to Load the Driver

This is the simplest approach for testing:

1. Build the .efi driver binary using EDK2 build tools:
   ```bash
   build -p MdePkg/MdePkg.dsc -m IsaIoForwardingPkg/IsaIoForwardingDxe/IsaIoForwardingDxe.inf -a X64 -t GCC5
   ```

2. Copy the resulting .efi file to a FAT32-formatted USB drive

3. Boot to UEFI Shell and load it:
   ```
   Shell> load IsaIoForwardingDxe.efi
   ```

## 2. Create a Simple UEFI Application to Load It

See `IsaIoLoader.c`

## 4. Add to the ESP for Automatic Loading

Place your driver in a special location on the EFI System Partition for automatic loading:

```
/EFI/OEM/Drivers/IsaIoForwardingDxe.efi
```

Many UEFI implementations will check certain paths for drivers during boot.

## M93p Testing
1. Build the driver using EDK2
2. Create a bootable USB with UEFI Shell
3. Boot to UEFI Shell
4. Load your driver manually
5. Use UEFI Shell commands like `iotest` to verify I/O forwarding is working
6. Once verified, create a more permanent solution
