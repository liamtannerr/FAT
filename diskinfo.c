#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <stdint.h> 


int main (){
    FILE* disk = fopen("disk.IMA", "r");
    if(disk == NULL){
        perror("Error opening file");
        return 0;
    }
    unsigned char bootSector[512];
    fread(bootSector, sizeof(bootSector), 1, disk);
    char OSname[9];
    strncpy(osName, (char *)(bootSector + 3), 8);
    osName[8] = '\0';
    
    char diskLabel[12];
    strncpy(diskLabel, (char *)(bootSector + 43), 11);
    diskLabel[11] = '\0';
    

    fclose(disk);

    return 0;
}

