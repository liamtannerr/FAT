# FAT12 File System Utilities

This project contains utilities for interacting with a FAT12 file system image. The utilities include:

diskinfo: Provides information about the FAT12 file system.
disklist: Lists the contents of the root directory of a FAT12 file system.
diskget: Retrieves a file from the FAT12 image and copies it to the local directory.
diskput: Copies a file from the local directory to the FAT12 image.

# After envoking the makefile:

    Usage:

        diskinfo:
            ./diskinfo <disk image file>
        disklist:
            ./disklist <disk image file>
        diskget:
            ./diskget <disk image file> <filename>
        diskput:
            ./diskput <disk image file> <filename>



