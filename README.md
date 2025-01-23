# checksum
A command line tool to check and correct the Portable Executable (PE) file checksums.

### Usage
```
Usage: checksum [--check] [--quiet] [--no-backup] <file> [[file] ...]

checksum fixes the Portable Executable (PE) checksum for 32-bit and 64-bit binaries.

Options must be provided before the list of files:
    --check     - perform checksum validation only.
    --quiet     - suppress non-error output.
    --no-backup - suppress creation of backup files.

The exit code meaning:
    Number of files with invalid checksum that were not updated.
    Number of files with invalid checksum when running with --check.
```

### Use Cases
1. Correct invalid checksums of portable executable (PE) files after binary patching.
