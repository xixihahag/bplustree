#include <cstdio>

int main(int argc, char const *argv[])
{
    char buf[1024];
    int a;
    // int n = fscanf(stdin, "%s", buf);
    // int n = gets(buf);
    fgets(buf, stdin);
    printf("%s", buf);
    // printf("n = %d,buf = \"%s\"\n", n, buf);
    // printf("a = %d\n", a);
    return 0;
}
