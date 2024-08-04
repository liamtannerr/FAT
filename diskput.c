#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <time.h>
#include <ctype.h>

#define SECTOR_SIZE 512

uint16_t getUInt16(const unsigned char *data, int offset) {
    return data[offset] | (data[offset + 1] << 8);
}

void setUInt16(unsigned char *data, int offset, uint16_t value) {
    data[offset] = value & 0xFF;
    data[offset + 1] = (value >> 8) & 0xFF;
}

void setUInt32(unsigned char *data, int offset, uint32_t value) {
    data[offset] = value & 0xFF;
    data[offset + 1] = (value >> 8) & 0xFF;
    data[offset + 2] = (value >> 16) & 0xFF;
    data[offset + 3] = (value >> 24) & 0xFF;
}

void toUpperCase(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = toupper(str[i]);
    }
}

void formatFileNameForEntry(const char *fileName, unsigned char *entry) {
    char name[9] = {0};
    char ext[4] = {0};
    char *dot = strrchr(fileName, '.');

    if (dot) {
        strncpy(name, fileName, dot - fileName);
        strncpy(ext, dot + 1, 3);
    } else {
        strncpy(name, fileName, 8);
    }

    for (int i = 0; i < 8; i++) {
        entry[i] = (i < strlen(name)) ? toupper(name[i]) : ' ';
    }

    for (int i = 0; i < 3; i++) {
        entry[8 + i] = (i < strlen(ext)) ? toupper(ext[i]) : ' ';
    }
}

int findFreeRootEntry(FILE *disk, uint32_t rootStart, uint16_t bytesPerSector) {
    uint8_t rootEntries[224 * 32];
    fseek(disk, rootStart * bytesPerSector, SEEK_SET);
    fread(rootEntries, sizeof(rootEntries), 1, disk);

    for (uint32_t i = 0; i < sizeof(rootEntries); i += 32) {
        if (rootEntries[i] == 0x00 || rootEntries[i] == 0xE5) {
            return i / 32; // Return the index of the free entry
        }
    }
    return -1; // No free entry found
}

uint16_t findFreeCluster(uint16_t *FAT, uint16_t totalClusters) {
    for (uint16_t i = 2; i < totalClusters; i++) {
        if (FAT[i] == 0x0000) {
            return i; // Return the index of the free cluster
        }
    }
    return 0xFFFF; // No free cluster found
}

void writeFATEntry(uint16_t *FAT, uint16_t cluster, uint16_t value) {
    FAT[cluster] = value;
}

void writeDataToClusters(FILE *disk, uint16_t *FAT, uint16_t startCluster, const unsigned char *fileData, uint32_t fileSize, uint16_t bytesPerCluster) {
    uint16_t currentCluster = startCluster;
    uint32_t bytesLeft = fileSize;
    const unsigned char *dataPtr = fileData;

    while (bytesLeft > 0) {
        uint32_t clusterStart = (31 + currentCluster - 2) * bytesPerCluster;
        fseek(disk, clusterStart, SEEK_SET);
        fwrite(dataPtr, 1, bytesLeft > bytesPerCluster ? bytesPerCluster : bytesLeft, disk);

        bytesLeft -= bytesPerCluster;
        dataPtr += bytesPerCluster;

        if (bytesLeft > 0) {
            uint16_t nextCluster = findFreeCluster(FAT, 2847);
            if (nextCluster == 0xFFFF) {
                fprintf(stderr, "No more free clusters available.\n");
                exit(EXIT_FAILURE);
            }
            writeFATEntry(FAT, currentCluster, nextCluster);
            currentCluster = nextCluster;
        } else {
            writeFATEntry(FAT, currentCluster, 0xFFF); // Mark as end of file
        }
    }
}

void updateRootDirectoryEntry(FILE *disk, uint32_t rootStart, uint16_t bytesPerSector, int entryIndex, const char *fileName, uint16_t startCluster, uint32_t fileSize) {
    unsigned char entry[32] = {0};
    formatFileNameForEntry(fileName, entry);
    entry[11] = 0x20; // File attribute (0x20 = archive)

    // Set creation and last modified times (using current time for simplicity)
    time_t t = time(NULL);
    struct tm *tm = localtime(&t);
    uint16_t timeVal = (tm->tm_hour << 11) | (tm->tm_min << 5) | (tm->tm_sec / 2);
    uint16_t dateVal = ((tm->tm_year - 80) << 9) | ((tm->tm_mon + 1) << 5) | tm->tm_mday;

    setUInt16(entry, 14, timeVal);
    setUInt16(entry, 16, dateVal);
    setUInt16(entry, 24, timeVal);
    setUInt16(entry, 26, dateVal);
    setUInt16(entry, 26, startCluster);
    setUInt32(entry, 28, fileSize);

    fseek(disk, rootStart * bytesPerSector + entryIndex * 32, SEEK_SET);
    fwrite(entry, 1, 32, disk);
}

void copyFileToDisk(FILE *disk, uint32_t rootStart, uint16_t bytesPerSector, uint16_t sectorsPerCluster, uint16_t sectorsPerFAT, const char *fileName) {
    FILE *inputFile = fopen(fileName, "rb");
    if (!inputFile) {
        perror("fopen");
        exit(EXIT_FAILURE);
    }

    fseek(inputFile, 0, SEEK_END);
    uint32_t fileSize = ftell(inputFile);
    rewind(inputFile);

    unsigned char *fileData = (unsigned char *)malloc(fileSize);
    if (!fileData) {
        perror("malloc");
        fclose(inputFile);
        exit(EXIT_FAILURE);
    }

    fread(fileData, 1, fileSize, inputFile);
    fclose(inputFile);

    uint16_t *FAT = (uint16_t *)malloc(SECTOR_SIZE * sectorsPerFAT);
    if (!FAT) {
        perror("malloc");
        free(fileData);
        exit(EXIT_FAILURE);
    }
    fseek(disk, SECTOR_SIZE, SEEK_SET);
    fread(FAT, SECTOR_SIZE * sectorsPerFAT, 1, disk);

    int freeEntryIndex = findFreeRootEntry(disk, rootStart, bytesPerSector);
    if (freeEntryIndex == -1) {
        fprintf(stderr, "No free entry in root directory.\n");
        free(fileData);
        free(FAT);
        exit(EXIT_FAILURE);
    }

    uint16_t startCluster = findFreeCluster(FAT, 2847);
    if (startCluster == 0xFFFF) {
        fprintf(stderr, "No free clusters available.\n");
        free(fileData);
        free(FAT);
        exit(EXIT_FAILURE);
    }

    writeDataToClusters(disk, FAT, startCluster, fileData, fileSize, bytesPerSector * sectorsPerCluster);

    updateRootDirectoryEntry(disk, rootStart, bytesPerSector, freeEntryIndex, fileName, startCluster, fileSize);

    fseek(disk, SECTOR_SIZE, SEEK_SET);
    fwrite(FAT, SECTOR_SIZE * sectorsPerFAT, 1, disk);

    free(fileData);
    free(FAT);

    printf("File copied successfully.\n");
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <disk image file> <filename>\n", argv[0]);
        return 1;
    }

    FILE *disk = fopen(argv[1], "rb+");
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

    copyFileToDisk(disk, rootStart, bytesPerSector, sectorsPerCluster, sectorsPerFAT, argv[2]);

    fclose(disk);
    return 0;
}

