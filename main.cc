/**
 * @file main.cc
 * @brief 
 * 
 * @author gyz
 * @email mni_gyz@163.com
 */
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "BPlusTree.h"

int main(int argc, char const *argv[])
{
    struct config config;
    pTree tree = calloc(1, sizeof(*tree));

    // 输入文件名称 block大小
    setConfig(&config);
    initTree(tree, config.fileName, config.blockSize);

    return 0;
}
