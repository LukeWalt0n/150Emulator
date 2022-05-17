#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stddef.h>


int main(){

    printf("0x%08x", ((0x1F & 1) << 6));
}