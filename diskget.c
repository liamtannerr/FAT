#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <ctype.h>

#define SECTOR_SIZE 512

uint16_t getUInt16(const unsigned char *data, int offset) {
    return data[offset] | (data[offset + 1] << 8);
}

uint32_t getUInt32(const unsigned char *data, int offset) {
    return data[offset] | (data[offset + 1] << 8) | (data[offset + 2] << 16) | (data[offset + 3] << 24);
}

void toUpperCase(char *str) {
    for (int i = 0; str[i]; i++) {
        str[i] = toupper(str[i]);
    }
}

void formatFileName(const unsigned char *entry, char *formattedName) {
    strncpy(formattedName, (char *)entry, 8);
    formattedName[8] = '\0';
    for (int i = 7; i >= 0; i--) {
        if (formattedName[i] == ' ') {
            formattedName[i] = '\0';
        } else {
            break;
        }
    }

    if (entry[8] != ' ') {
        strcat(formattedName, ".");
        strncat(formattedName, (char *)entry + 8, 3);
    }
}

void readFATTable(FILE *disk, uint16_t *FAT, uint32_t fatStart, uint16_t sectorsPerFAT) {
    fseek(disk, fatStart * SECTOR_SIZE, SEEK_SET);
    fread(FAT, SECTOR_SIZE * sectorsPerFAT, 1, disk);
}

uint16_t getNextCluster(uint16_t *FAT, uint16_t cluster) {
    uint16_t nextCluster = FAT[cluster];
    if (nextCluster >= 0xFF8) {
        return 0xFFFF; // End of chain
    }
    return nextCluster;
}

void readFileData(FILE *disk, uint16_t *FAT, uint16_t startCluster, uint32_t fileSize, uint16_t bytesPerCluster, FILE *outputFile) {
    uint16_t currentCluster = startCluster;
    uint32_t bytesLeft = fileSize;

    while (currentCluster < 0xFF8 && bytesLeft > 0) {
        uint32_t clusterStart = (31 + currentCluster - 2) * bytesPerCluster;
        uint8_t clusterData[bytesPerCluster];

        fseek(disk, clusterStart, SEEK_SET);
        fread(clusterData, bytesPerCluster, 1, disk);

        fwrite(clusterData, 1, bytesLeft > bytesPerCluster ? bytesPerCluster : bytesLeft, outputFile);

        bytesLeft -= bytesPerCluster;
        currentCluster = getNextCluster(FAT, currentCluster);
    }
}

void copyFileFromRoot(FILE *disk, uint32_t rootStart, uint16_t bytesPerSector, uint16_t sectorsPerCluster, uint16_t sectorsPerFAT, const char *fileName) {
    uint8_t rootEntries[224 * 32];
    fseek(disk, rootStart * bytesPerSector, SEEK_SET);
    fread(rootEntries, sizeof(rootEntries), 1, disk);

    uint16_t *FAT = (uint16_t *)malloc(SECTOR_SIZE * sectorsPerFAT);
    if (!FAT) {
        perror("malloc");
        exit(EXIT_FAILURE);
    }
    readFATTable(disk, FAT, 1, sectorsPerFAT);

    char searchName[12];
    strcpy(searchName, fileName);
    toUpperCase(searchName);

    for (uint32_t i = 0; i < sizeof(rootEntries); i += 32) {
        if (rootEntries[i] == 0x00) break; // End of directory
        if (rootEntries[i] == 0xE5) continue; // Skip deleted entries

        char entryName[13];
        formatFileName(rootEntries + i, entryName);

        if (strcmp(entryName, searchName) == 0) {
            uint16_t firstCluster = getUInt16(rootEntries, i + 26);
            uint32_t fileSize = getUInt32(rootEntries, i + 28);

            FILE *outputFile = fopen(fileName, "wb");
            if (!outputFile) {
                perror("fopen");
                free(FAT);
                exit(EXIT_FAILURE);
            }

            readFileData(disk, FAT, firstCluster, fileSize, bytesPerSector * sectorsPerCluster, outputFile);

            fclose(outputFile);
            printf("File copied successfully.\n");

            free(FAT);
            return;
        }
    }

    printf("File not found.\n");
    free(FAT);
}

int main(int argc, char *argv[]) {
    if (argc != 3) {
        fprintf(stderr, "Usage: %s <disk image file> <filename>\n", argv[0]);
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

    copyFileFromRoot(disk, rootStart, bytesPerSector, sectorsPerCluster, sectorsPerFAT, argv[2]);

    fclose(disk);
    return 0;
}
