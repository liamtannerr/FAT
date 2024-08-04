#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>

uint16_t getUInt16(const unsigned char *data, int offset) {
    return data[offset] | (data[offset + 1] << 8);
}

uint32_t getUInt32(const unsigned char *data, int offset) {
    return data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16) | (data[offset + 3] << 24);
}

void formatDate(uint16_t date, char *buffer) {
    int year = ((date >> 9) & 0x7F) + 1980;
    int month = (date >> 5) & 0x0F;
    int day = date & 0x1F;
    sprintf(buffer, "%04d-%02d-%02d", year, month, day);
}

void formatTime(uint16_t time, char *buffer) {
    int hours = (time >> 11) & 0x1F;
    int minutes = (time >> 5) & 0x3F;
    int seconds = (time & 0x1F) * 2;
    sprintf(buffer, "%02d:%02d:%02d", hours, minutes, seconds);
}

void readDirectory(FILE *disk, uint32_t dirStart, uint16_t bytesPerSector, const char *dirName, uint16_t sectorsPerCluster) {
    printf("%s\n", dirName);
    printf("==================\n");

    uint8_t dirEntries[512]; // Max entries in root directory (224 entries * 32 bytes each)
    fseek(disk, dirStart * bytesPerSector, SEEK_SET);
    fread(dirEntries, sizeof(dirEntries), 1, disk);

    // First loop to print the directory/file information
    for (uint32_t i = 0; i < sizeof(dirEntries); i += 32) {
        if (dirEntries[i] == 0x00) break; // End of directory
        if (dirEntries[i + 26] == 0x00 || dirEntries[i + 26] == 0x01) continue; // Skip invalid clusters
        if (dirEntries[i] == 0xE5) continue; // Skip deleted entries

        char name[12];
        strncpy(name, (char *)dirEntries + i, 11);
        name[11] = '\0';

        // Skip '.' and '..' entries
        if ((name[0] == '.' && name[1] == '\0') || (name[0] == '.' && name[1] == '.' && name[2] == '\0')) {
            continue;
        }

        uint8_t attributes = dirEntries[i + 11];
        uint32_t fileSize = getUInt32(dirEntries, i + 28);
        uint16_t date = getUInt16(dirEntries, i + 24);
        uint16_t time = getUInt16(dirEntries, i + 22);

        char dateBuffer[11], timeBuffer[9];
        formatDate(date, dateBuffer);
        formatTime(time, timeBuffer);

        printf("%c %10u %20s %s %s\n", 
               (attributes & 0x10) ? 'D' : 'F',
               fileSize,
               name,
               dateBuffer,
               timeBuffer);
    }

    // Second loop to recursively call readDirectory for subdirectories
    for (uint32_t i = 0; i < sizeof(dirEntries); i += 32) {
        if (dirEntries[i] == 0x00) break; // End of directory
        if (dirEntries[i + 26] == 0x00 || dirEntries[i + 26] == 0x01) continue; // Skip invalid clusters
        if (dirEntries[i] == 0xE5) continue; // Skip deleted entries

        char name[12];
        strncpy(name, (char *)dirEntries + i, 11);
        name[11] = '\0';

        // Skip '.' and '..' entries
        if ((name[0] == '.' && name[1] == '\0') || (name[0] == '.' && name[1] == '.' && name[2] == '\0')) {
            continue;
        }

        uint8_t attributes = dirEntries[i + 11];
        uint16_t firstCluster = getUInt16(dirEntries, i + 26);

        if (attributes & 0x10 && firstCluster >= 2 && name[0] != '.') { // Directory and valid cluster
            char subDirName[20];
            snprintf(subDirName, sizeof(subDirName), "%s/%s", dirName, name);

            uint32_t subDirStart = 33 + (firstCluster - 2) * sectorsPerCluster;
            if (subDirStart != dirStart) { // Avoid infinite loop by ensuring the subdirectory is different
                readDirectory(disk, subDirStart, bytesPerSector, subDirName, sectorsPerCluster);
            }
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <disk image file>\n", argv[0]);
        return 1;
    }

    FILE *disk = fopen(argv[1], "rb");
    if (!disk) {
        perror("fopen");
        return 1;
    }

    uint8_t bootSector[512];
    fread(bootSector, sizeof(bootSector), 1, disk);

    uint16_t bytesPerSector = getUInt16(bootSector, 11);
    uint16_t sectorsPerCluster = bootSector[13];
    uint16_t reservedSectors = getUInt16(bootSector, 14);
    uint8_t numFATs = bootSector[16];
    uint16_t rootEntries = getUInt16(bootSector, 17);
    uint16_t totalSectors = getUInt16(bootSector, 19);
    uint16_t sectorsPerFAT = getUInt16(bootSector, 22);
    uint32_t rootDirSectors = ((rootEntries * 32) + (bytesPerSector - 1)) / bytesPerSector;

    uint32_t rootStart = reservedSectors + (numFATs * sectorsPerFAT);

    readDirectory(disk, rootStart, bytesPerSector, "/", sectorsPerCluster);

    fclose(disk);
    return 0;
}





