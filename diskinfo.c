#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>

// Helper functions to read integers from boot sector in little-endian format
uint16_t getUInt16(const unsigned char *data, int offset) {
    return data[offset] | (data[offset + 1] << 8);
}

uint32_t getUInt32(const unsigned char *data, int offset) {
    return data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16) | (data[offset + 3] << 24);
}

// Function to count free sectors in the FAT
uint32_t countFreeSectors(FILE *disk, uint32_t fatStart, uint16_t sectorsPerFAT, uint16_t bytesPerSector) {
    fseek(disk, fatStart * bytesPerSector, SEEK_SET);
    uint8_t fat[sectorsPerFAT * bytesPerSector];
    fread(fat, sizeof(fat), 1, disk);

    uint32_t freeCount = 0;
    for (uint32_t i = 0; i < sizeof(fat); i += 3) {
        uint16_t entry1 = fat[i] | ((fat[i + 1] & 0x0F) << 8);
        uint16_t entry2 = (fat[i + 1] >> 4) | (fat[i + 2] << 4);
        if (entry1 == 0) freeCount++;
        if (entry2 == 0) freeCount++;
    }
    return freeCount;
}

// Function to find the disk label in the root directory
void findDiskLabel(FILE *disk, uint32_t rootDirStart, uint16_t bytesPerSector, char *diskLabel) {
    fseek(disk, rootDirStart * bytesPerSector, SEEK_SET);
    uint8_t rootDir[224 * 32]; // Max entries in root directory (224 entries * 32 bytes each)
    fread(rootDir, sizeof(rootDir), 1, disk);

    for (uint32_t i = 0; i < sizeof(rootDir); i += 32) {
        if (rootDir[i] == 0x00) break; // End of directory
        if (rootDir[i + 11] == 0x08) { // Volume label attribute
            strncpy(diskLabel, (char *)(rootDir + i), 11);
            diskLabel[11] = '\0';
            return;
        }
    }
    strcpy(diskLabel, "No Label");
}

// Function to count files in the root directory
uint32_t countFiles(FILE *disk, uint32_t rootDirStart, uint16_t bytesPerSector) {
    fseek(disk, rootDirStart * bytesPerSector, SEEK_SET);
    uint8_t rootDir[224 * 32]; // Max entries in root directory (224 entries * 32 bytes each)
    fread(rootDir, sizeof(rootDir), 1, disk);

    uint32_t fileCount = 0;
    for (uint32_t i = 0; i < sizeof(rootDir); i += 32) {
        if (rootDir[i] == 0x00) break; // End of directory
        if (rootDir[i + 11] & 0x08) continue; // Skip volume labels
        if (rootDir[i + 26] == 0x00 || rootDir[i + 26] == 0x01) continue; // Skip invalid clusters
        fileCount++;
    }
    return fileCount;
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

    char osName[9];
    strncpy(osName, (char *)(bootSector + 3), 8);
    osName[8] = '\0';
    printf("OS Name: %s\n", osName);

    uint16_t bytesPerSector = getUInt16(bootSector, 11);
    uint16_t totalSectors = getUInt16(bootSector, 19);
    uint32_t totalSize = bytesPerSector * totalSectors;
    printf("Total size of the disk: %u bytes\n", totalSize);

    uint16_t sectorsPerFAT = getUInt16(bootSector, 22);
    uint8_t numberOfFATs = bootSector[16];
    printf("Number of FAT copies: %u\n", numberOfFATs);
    printf("Sectors per FAT: %u\n", sectorsPerFAT);

    uint32_t fatStart = getUInt16(bootSector, 14);
    uint32_t freeSectors = countFreeSectors(disk, fatStart, sectorsPerFAT, bytesPerSector);
    uint32_t freeSize = freeSectors * bytesPerSector;
    printf("Free size of the disk: %u bytes\n", freeSize);

    // Calculate root directory start
    uint32_t rootDirStart = fatStart + numberOfFATs * sectorsPerFAT;

    // Find disk label in root directory
    char diskLabel[12];
    findDiskLabel(disk, rootDirStart, bytesPerSector, diskLabel);
    printf("Label of the disk: %s\n", diskLabel);

    // Count files in the root directory
    uint32_t fileCount = countFiles(disk, rootDirStart, bytesPerSector);
    printf("The number of files in the disk: %u\n", fileCount);

    fclose(disk);
    return 0;
}



