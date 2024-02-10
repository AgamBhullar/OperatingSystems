#include <stdio.h>
#include <stdlib.h>

#define BUFFER_LEN 64

void display_file_contents(const char *filename) 
{
    char content[BUFFER_LEN];
    FILE *filePtr = fopen(filename, "r");

    if (filePtr == NULL) 
    {
        printf("Unable to open file\n");
        exit(1);
    }

    while (fgets(content, BUFFER_LEN, filePtr)) 
    {
        printf("%s", content);
    }
    printf("\n");

    fclose(filePtr);
}

int main(int argument_count, char *argument_vector[]) 
{
    if (argument_count == 1) 
    {
        return 0;
    }

    for (int index = 1; index < argument_count; index++) 
    {
        display_file_contents(argument_vector[index]);
    }

    return 0;
}