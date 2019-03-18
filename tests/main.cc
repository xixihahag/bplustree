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
#include <malloc.h>

#include "BPlusTree.h"

int main()
{
    config mConfig;
    pTree tree = (pTree)calloc(1, sizeof(struct bPlusTree));

    // 输入文件名称 block大小
    setConfig(&mConfig);
    initTree(tree, mConfig.fileName, mConfig.blockSize);

    show(tree);

    // 树建好 下面进行搜索
    // int k;
    // printf("please insert key\n");
    // scanf("%d", &k);
    // search(tree, k);

    return 0;
}
