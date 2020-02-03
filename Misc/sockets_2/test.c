
#include <stdio.h> 
#include <dirent.h> 
#include <string.h>
  
int main(int argc, char *argv[]) {
    
    char fullpath_name[64] = "image/";
    char *subdir_name = "im1";
    strcat(fullpath_name, subdir_name);

    struct dirent *de;
    DIR *dr = opendir(fullpath_name); 

    if (dr == NULL) { 
        fprintf(stderr, "Could not open current directory\n" ); 
        return 1; 
    } 

    int no_of_files = 0;
    while ((de = readdir(dr)) != NULL) {
        if(strcmp(de->d_name, ".")!=0 && strcmp(de->d_name, "..")!=0) {
            printf("%s\n", de->d_name);
            no_of_files++;
        }
    }
    rewinddir(dr);
    while ((de = readdir(dr)) != NULL) {
        if(strcmp(de->d_name, ".")!=0 && strcmp(de->d_name, "..")!=0) {
            printf("%s\n", de->d_name);
            no_of_files++;
        }
    }

    closedir(dr);  
  
    
    return 0; 
} 
