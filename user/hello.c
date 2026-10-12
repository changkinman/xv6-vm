#include "kernel/types.h"
#include "kernel/stat.h"
#include "user/user.h"

int main(int argc, char *argv[])
{
    printf("Hello, xv6 world!\n");
    printf("My student ID is: 2024312111462\n");
    
    if(argc > 1) {
        printf("Arguments: ");
        for(int i = 1; i < argc; i++) {
            printf("%s ", argv[i]);
        }
        printf("\n");
    }
    exit(0);
}
