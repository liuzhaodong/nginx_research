#include <stdio.h>
#include "ngx_config.h"
#include "ngx_conf_file.h"
#include "nginx.h"
#include "ngx_core.h"
#include "ngx_string.h"

int main()
{
    u_char* p = NULL;
    ngx_str_t  dst;
    ngx_str_t mystr = ngx_string("hello, world !");
    int dst_len = ngx_base64_encoded_length(mystr.len);
    printf("source length is %d, destionation length is %d\n", mystr.len, dst_len);
    p = malloc(ngx_base64_encoded_length(mystr.len) + 1);
    dst.data = p;
    ngx_encode_base64(&dst, &mystr);
    printf("source str is %s\ndestionation str is %s\n", mystr.data, dst.data);
    free(p);
    return 0;
}
