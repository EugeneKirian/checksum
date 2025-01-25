/*
Copyright (c) 2025 Eugene Kirian <eugenekirian@gmail.com>

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#define _CRT_SECURE_NO_WARNINGS

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN

#include <windows.h>
#include <imagehlp.h>

#pragma comment(lib, "imagehlp.lib")

#include <stdio.h>
#include <stdlib.h>

#pragma warning(disable : 4996)

typedef struct
{
    int backup;
    int check;
    int quiet;
} context;

static int process_file(context* ctx, const char* path) {
    HANDLE file = CreateFileA(path,
        ctx->check ? GENERIC_READ : GENERIC_READ | GENERIC_WRITE,
        FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);

    if (file == INVALID_HANDLE_VALUE) {
        printf("Unable to open file %s\n", path);
        return FALSE;
    }

    DWORD size = GetFileSize(file, NULL);

    if (size == INVALID_FILE_SIZE) {
        printf("Unable to get file size for %s\n", path);
        CloseHandle(file);
        return FALSE;
    }

    HANDLE mapping = CreateFileMappingA(file, NULL, ctx->check ? PAGE_READONLY : PAGE_READWRITE, 0, 0, NULL);

    if (mapping == NULL) {
        printf("Unable to create file mapping for %s\n", path);
        CloseHandle(file);
        return FALSE;
    }

    void* view = MapViewOfFile(mapping, ctx->check ? FILE_MAP_READ : FILE_MAP_ALL_ACCESS, 0, 0, 0);

    if (view == NULL) {
        printf("Unable to create file mapping view for %s\n", path);
        CloseHandle(mapping);
        CloseHandle(file);
        return FALSE;
    }

    // All ImageHlp functions, such as this one, are single threaded.
    // Therefore, calls from more than one thread to this function
    // will likely result in unexpected behavior or memory corruption.
    // To avoid this, you must synchronize all concurrent calls from more than one thread to this function.

    DWORD actual = 0, correct = 0;
    PIMAGE_NT_HEADERS header = CheckSumMappedFile(view, size, &actual, &correct);

    if (header == NULL) {
        printf("Unable to obtain executable header and checksum for %s\n", path);
        UnmapViewOfFile(view);
        CloseHandle(mapping);
        CloseHandle(file);
        return FALSE;
    }

    if (header->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC
        || header->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC)
    {
        if (actual == correct) {
            if (!ctx->quiet) {
                printf("File %s has correct checksum 0x%x\n", path, actual);
            }

            UnmapViewOfFile(view);
            CloseHandle(mapping);
            CloseHandle(file);
            return TRUE;
        }

        if (!ctx->quiet) {
            printf("File %s checksum 0x%x, correct checksum 0x%x\n", path, actual, correct);
        }

        // Create backup file.
        if (!ctx->check && ctx->backup) {
            char backup[MAX_PATH];
            strcpy_s(backup, MAX_PATH, path);
            strcat_s(backup, MAX_PATH, ".bak");

            if (!CopyFileA(path, backup, FALSE)) {
                printf("Unable to create backup file %s for file %s\n", backup, path);
            }

            if (!ctx->quiet) {
                printf("Backup file %s created for file %s\n", backup, path);
            }
        }

        // Update the checksum.
        if (header->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR64_MAGIC) {
            if (!ctx->check) {
                ((IMAGE_NT_HEADERS64*)header)->OptionalHeader.CheckSum = correct;
            }
        }
        else if (header->OptionalHeader.Magic == IMAGE_NT_OPTIONAL_HDR32_MAGIC) {
            if (!ctx->check) {
                ((IMAGE_NT_HEADERS32*)header)->OptionalHeader.CheckSum = correct;
            }
        }

        // Save the file.
        if (!ctx->check) {
            FlushViewOfFile(view, 0);
            FlushFileBuffers(file);
        }

        UnmapViewOfFile(view);
        CloseHandle(mapping);
        CloseHandle(file);
        return TRUE;
    }

    printf("File %s is not a valid executable file\n", path);

    UnmapViewOfFile(view);
    CloseHandle(mapping);
    CloseHandle(file);

    return FALSE;
}

static void print_usage() {
    printf("\n"
        "Copyright (c) 2025 Eugene Kirian <eugenekirian@gmail.com>\n\n"
        "Usage: checksum [--check] [--quiet] [--no-backup] <file> [[file] ...]\n\n"
        "checksum fixes the Portable Executable (PE) checksum for 32-bit and 64-bit binaries.\n\n"
        "Options must be provided before the list of files:\n"
        "    --check     - perform checksum validation only.\n"
        "    --quiet     - suppress non-error output.\n"
        "    --no-backup - suppress creation of backup files.\n\n"
        "The exit code meaning:\n"
        "    Number of files with invalid checksum that were not updated.\n"
        "    Number of files with invalid checksum when running with --check.\n"
    );
}

int main(int argc, char** argv) {
    if (argc < 2) {
        print_usage();
        return ERROR_SUCCESS;
    }

    context ctx;
    ctx.check = FALSE;
    ctx.backup = TRUE;
    ctx.quiet = FALSE;

    int errors = 0, processing = 0;

    for (int i = 1; i < argc; i++) {
        if (!processing) {
            if (strcmpi("--check", argv[i]) == 0) { ctx.check = TRUE; }
            else if (strcmpi("--quiet", argv[i]) == 0) { ctx.quiet = TRUE; }
            else if (strcmpi("--silent", argv[i]) == 0) { ctx.quiet = TRUE; }
            else if (strcmpi("--no-backup", argv[i]) == 0) { ctx.backup = FALSE; }
            else {
                if (!ctx.quiet) {
                    printf("Copyright (c) 2025 Eugene Kirian <eugenekirian@gmail.com>\n");
                }

                processing = TRUE;
            }
        }

        if (processing) {
            if (!process_file(&ctx, argv[i])) { errors++; }
        }
    }

    return errors;
}