#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <limits.h>
#include <libgen.h>

#include "bootimg.h"

typedef unsigned char byte;

FILE *bootimg;
boot_img_hdr hdr;
int total_read = 0;

int read_padding(FILE* f, unsigned int itemsize, int pagesize)
{
    byte* buf = (byte*)malloc(sizeof(byte) * pagesize);
    unsigned pagemask = pagesize - 1;
    unsigned count;

    if((itemsize & pagemask) == 0) {
        free(buf);
        return 0;
    }

    count = pagesize - (itemsize & pagemask);

    fread(buf, count, 1, f);
    free(buf);
    return count;
}

void write_string_to_file(char* file, char* string)
{
    FILE* f = fopen(file, "w");
    fwrite(string, strlen(string), 1, f);
    fwrite("\n", 1, 1, f);
    fclose(f);

    return;
}

void dump_to_file(FILE *target, unsigned int length, FILE *source)
{
    byte* buf = (byte*)malloc(sizeof(byte) * length);
    fread(buf, length, 1, source);
    fwrite(buf, length, 1, target);
    free(buf);

    total_read += length;

    return;
}

int usage() {
    printf("usage: unpackbootimg\n");
    printf("\t-i|--input boot.img\n");
    printf("\t[ -o|--output output_directory]\n");
    printf("\t[ -p|--pagesize <size-in-hexadecimal> ]\n");
    return 0;
}

int main(int argc, char** argv)
{
    char tmp[PATH_MAX];
    char* directory = "./";
    char* filename = NULL;
    int pagesize = 0;

    argc--;
    argv++;
   
    while(argc > 0){
        char *arg = argv[0];
        char *val = argv[1];
        argc -= 2;
        argv += 2;
        if(!strcmp(arg, "--input") || !strcmp(arg, "-i")) {
            filename = val;
        } else if(!strcmp(arg, "--output") || !strcmp(arg, "-o")) {
            directory = val;
        } else if(!strcmp(arg, "--pagesize") || !strcmp(arg, "-p")) {
            pagesize = strtoul(val, 0, 16);
        } else {
            return usage();
        }
    }
    
    if (filename == NULL) {
        return usage();
    }
    
    bootimg = fopen(filename, "rb");

    //printf("Reading hdr...\n");
    fread(&hdr, sizeof(hdr), 1, bootimg);
    printf("BOARD_KERNEL_CMDLINE %s\n", hdr.cmdline);
    printf("BOARD_KERNEL_BASE %08x\n", hdr.kernel_addr - 0x00008000);
    printf("BOARD_PAGE_SIZE %d\n", hdr.page_size);
    
    if (pagesize == 0) {
        pagesize = hdr.page_size;
    }
    
    //printf("cmdline...\n");
    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-cmdline");
    write_string_to_file(tmp, hdr.cmdline);
    
    //printf("base...\n");
    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-base");
    char basetmp[200];
    sprintf(basetmp, "%08x", hdr.kernel_addr - 0x00008000);
    write_string_to_file(tmp, basetmp);

    //printf("pagesize...\n");
    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-pagesize");
    char pagesizetmp[200];
    sprintf(pagesizetmp, "%d", hdr.page_size);
    write_string_to_file(tmp, pagesizetmp);
    
    total_read += sizeof(hdr);
    //printf("total read: %d\n", total_read);
    total_read += read_padding(bootimg, sizeof(hdr), pagesize);

    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-zImage");
    FILE *kernel = fopen(tmp, "wb");
    dump_to_file(kernel, hdr.kernel_size, bootimg);
    fclose(kernel);

    total_read += read_padding(bootimg, hdr.kernel_size, pagesize);

    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-ramdisk.gz");
    FILE *ramdisk = fopen(tmp, "wb");
    dump_to_file(ramdisk, hdr.ramdisk_size, bootimg);
    fclose(ramdisk);
    total_read += read_padding(bootimg, hdr.ramdisk_size, pagesize);

    sprintf(tmp, "%s/%s", directory, basename(filename));
    strcat(tmp, "-dtb.img");
    FILE *dtb = fopen(tmp, "wb");
    dump_to_file(dtb, hdr.dt_size, bootimg);
    fclose(dtb);
    total_read += read_padding(bootimg, hdr.dt_size, pagesize);

    fclose(bootimg);
    
    //printf("Total Read: %d\n", total_read);
    return 0;
}