# RuuviAir B0 (NSIB) Bootloader

The *B0* (also known as *NSIB*) is a secure, immutable first-stage bootloader developed and maintained by Nordic Semiconductor.  
This repository provides **Ruuvi-specific extensions** to the NSIB through the use of available extension hooks.

## Key Features

- **Factory restore support**: Restore the original factory firmware stored in external flash memory by pressing and holding the button during startup.  
- Designed to integrate seamlessly with the standard NSIB boot process.  
- Maintains compatibility with Nordic’s secure boot flow.

