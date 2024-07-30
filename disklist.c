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

    uint8_t dirEntries[224 * 32]; // Max entries in root directory (224 entries * 32 bytes each)
    fseek(disk, dirStart * bytesPerSector, SEEK_SET);
    fread(dirEntries, sizeof(dirEntries), 1, disk);

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
        uint16_t firstCluster = getUInt16(dirEntries, i + 26);
        uint16_t date = getUInt16(dirEntries, i + 24);
        uint16_t time = getUInt16(dirEntries, i + 22);

        char dateStr[11];
        char timeStr[9];
        formatDate(date, dateStr);
        formatTime(time, timeStr);

        if (attributes & 0x10) { // Directory
            printf("D %-10u %-20s %s %s\n", fileSize, name, dateStr, timeStr);
            if (firstCluster > 1) {
                uint32_t subDirStart = (firstCluster - 2) * sectorsPerCluster + dirStart + 1; // Adjust for data area
                char subDirName[256];
                snprintf(subDirName, sizeof(subDirName), "%s/%s", dirName, name);
                readDirectory(disk, subDirStart, bytesPerSector, subDirName, sectorsPerCluster);
            }
        } else { // Regular file
            printf("F %-10u %-20s %s %s\n", fileSize, name, dateStr, timeStr);
        }
    }
}

int main(int argc, char *argv[]) {
    if (argc != 2) {
        fprintf(stderr, "Usage: %s <disk image>\n", argv[0]);
        return 1;
    }

    FILE *disk = fopen(argv[1], "rb");
    if (disk == NULL) {
        perror("Error opening file");
        return 1;
    }

    unsigned char bootSector[512];
    if (fread(bootSector, sizeof(bootSector), 1, disk) != 1) {
        perror("Error reading boot sector");
        fclose(disk);
        return 1;
    }

    uint16_t bytesPerSector = getUInt16(bootSector, 11);
    uint16_t sectorsPerFAT = getUInt16(bootSector, 22);
    uint8_t numberOfFATs = bootSector[16];
    uint32_t fatStart = getUInt16(bootSector, 14);
    uint16_t sectorsPerCluster = bootSector[13];
    uint32_t rootDirStart = fatStart + numberOfFATs * sectorsPerFAT;

    // Start reading the root directory
    readDirectory(disk, rootDirStart, bytesPerSector, "/", sectorsPerCluster);

    fclose(disk);
    return 0;
}

