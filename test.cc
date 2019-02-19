#include <cstdio>

typedef struct A
{
    int a;
} mA, *pmA;

pmA getA()
{
    struct A a;
    return &a;
}

int main(int argc, char const *argv[])
{
    struct A *mainA = getA();
    return 0;
}
