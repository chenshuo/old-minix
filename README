HOW TO GET AND INSTALL
MINIX 2.0.4

More Important note: Version 2.0.4 has been slapped together in a
hurry to have a fixed point to start a new Minix project at. The code
should be OK, but most new things haven't been tested. (So there's
already a fix out.) The documentation is alas far behind all the changes.

Important note: Version 2.0.4 will not run on a system with only 640K!
The halfbaked swapping code does not fully compensate for the
increased memory use of other parts of the system, most notably the
shell. The documentation has not been updated to reflect this, because
we still have the hope that the next version will again run on the XT. 

This directory and subdirectories contains Minix version 2.0.4, a small
educational UNIX-like system for IBM PC's and compatibles. 

Before installing Minix, do the unthinkable and read the installation
manual usage(8). It and the possibly useful boot(8) are in the following
files in two forms: 

  usage.ps   boot.ps    - PostScript
  usage.txt  boot.txt   - Flat ASCII text

Alternatively, with a web browser you can view the whole manual
page set. The page to start with is usage(8). 

These files may also be of interest: 

  md5list               - MD5 checksums of the files here
  crclist               - Checksums using Minix crc command
  crc.c                 - Command to make CRC checksums
  changes.txt           - List of changes between 2.0.0 and 2.0.4
  psman/                - All manuals in PostScript form


Minix for the Intel architecture comes in three flavors: 

  - Regular (for 386, 486, and Pentium machines)
  - Small (for 286 machines)
  - Running under DOS/Windows/Bochs/WMWare(?).

The bits and pieces of each flavor are distributed over the i386, i86, 
src directories, depending on architecture (386+ or 286), or source
files (src). (Take a peek in those directories if you want to know the file
sizes.) You need the following files, and only those files, for each
flavor: 

386, 486, Pentium 

  i386/ROOT.MNX         - Installation root floppy image
  i386/USR.MNX          - Installation /usr floppy image
  i386/NET.TAZ          - Network install files (optional!)
  i386/USR.TAZ          - Binary /usr files (386 executables)
  src/SYS.TAZ           - Operating system sources (libary, kernel, etc.)
  src/CMD.TAZ           - Commands sources (cp, ls, mkdir, etc.)
  src/CMD.TAZ           - Fixed RTL8139 driver

286 with 2M+ memory
286 with a 1.44M floppy drive
286 with two 720K floppy drives 

  i86/ROOT.MNX          - Installation root floppy image
  i86/USR.MNX           - Installation /usr floppy image
  i86/NET.TAZ           - Network install files (optional!)
  i86/USR.TAZ           - Binary /usr files (8086 executables)
  src/SYS.TAZ           - Operating system sources (libary, kernel, etc.)
  src/CMD.TAZ           - Commands sources (cp, ls, mkdir, etc.)

Running under DOS in 386 mode 

  dosminix.txt          - README (also present in ZIP file)
  i386/DOSMINIX.ZIP     - Bootstrap and 50M "disk"

Running under DOS in 8086/286 mode
Running under Windows in a DOS box in
8086 mode (experimental) 

  dosminix.txt          - README (also present in ZIP file)
  i86/DOSMINIX.ZIP      - Bootstrap and 50M "disk"

The files with names ending in .MNX are sector-by-sector copies of
Minix floppies or disks. The .TAZ files are compressed UNIX tar
archives. The .ZIP files are archives that can be unpacked by the
Windows utilities PKZIP or WinZip. 

Notes 

The two installation floppies ROOT.MNX and USR.MNX can be
combined on a 1.2 Mb or 1.44 Mb diskette. The .TAZ files must be
distributed over several floppies. Example for UNIX: 

        cat ROOT.MNX USR.MNX >/dev/floppy
        dd if=USR.TAZ of=/dev/floppy bs=1440k count=1 skip=0
        dd if=USR.TAZ of=/dev/floppy bs=1440k count=1 skip=1
        dd if=USR.TAZ of=/dev/floppy bs=1440k count=1 skip=2

Increase the "skip" count until dd writes a diskette partially. 
/dev/floppy should be the name of the floppy device. You have to find
out what your Operating System names it today. Under MS-DOS you
can use the FDVOL command found in the ../dosutil directory to write
the floppy images. 

Be careful when you install Minix, many of the commands are
potentionally dangerous in a way that a simple typing mistake may
destroy all other data on your system. So make backups first! (The
DosMinix version, however, is relatively safe.) 

Kernel configurations 

The kernel images present in the NET.TAZ collection differ as follows
in the <minix/config.h> include file: 

                                i86     i386
        NR_PROCS                 40       64
        NR_BUFS                         2048
        NR_BUF_HASH                     4096
        ENABLE_CACHE2                      0
        ENABLE_ATAPI              0
        ENABLE_FATFILE            0
        ENABLE_USERBIOS                    1
        ENABLE_PCI                1        1
        DMA_SECTORS              16       64
        ENABLE_DP8390             1        1
        ENABLE_RTL8139                     1
        NR_RS_LINES               2        4
        NR_PTYS                   4        8

