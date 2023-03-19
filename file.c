#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/stat.h>
#include <stdbool.h>
#include <strings.h>

bool file_exists(char* path)
{
    FILE *fptr = fopen(path, "r");

    if (fptr == NULL) {
        return false;
    }

    fclose(fptr);

    return true;
}

char* get_file_content(char* path)
{
    FILE* ptr;
    char* content = malloc(50000);
    bzero(content, 50000);
 
    ptr = fopen(path, "r");
 
    if (NULL == ptr) {
        return "";
    }

    int i = 0;
    char ch;
    do {
        ch = fgetc(ptr);
        content[i++] = ch;
    } while (ch != EOF);

    content[i-1] = '\0';
 
    fclose(ptr);

    return content;
}

bool is_regular_file(char *path)
{
    struct stat path_stat;
    stat(path, &path_stat);

    return (bool) S_ISREG(path_stat.st_mode);
}

char* get_parent_dir(const char* dir)
{
    char* parent_dir = (char*) malloc(255 * sizeof(char));
    bzero(parent_dir, sizeof(parent_dir));
    int slash_position = -1;
    for (int i = strlen(dir)-2; i >= 0; i--) {
        if (dir[i] == '/') {
            slash_position = i;
            break;
        }
    }
    
    if (slash_position >= 0) {
        strncpy(parent_dir, &dir[0], slash_position + 1);
    }

    return parent_dir;
}