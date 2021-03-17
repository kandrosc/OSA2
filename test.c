#include "notapp.h"
#include <stdio.h>
#include <sys/time.h>



int main() {
    double f = 3.5;
    int i = (int)f;
    printf("%f",(f-i)*1000000000L);



}