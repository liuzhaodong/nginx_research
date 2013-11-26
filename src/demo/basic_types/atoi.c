#include <stdio.h>
#include "../../core/ngx_config.h"
#include "../../core/ngx_string.c"

int main()
{
    u_char a[] = "abcd";
    ngx_int_t b;
    b = ngx_atoi(&a);
    printf ("%d", b);
    return 0;
}
