#include "BPlusTree.h"

int BPlusTree::setConfig(pBPlusTreeConfig config)
{
    // 从屏幕中读
    printf("filename:(default /tmp/pbtree)");
    char i;
    switch (i = getchar())
    {
    case '\n':
        strcpy(config->filename, "/tmp/pbtree");
        break;
    default:
        ungetc(i, stdin);
        fscanf(stdin, "%s", config->fileName);
    }

    printf("blocksize:");
    switch (i = getchar())
    {
    case '\n':
        config->blockSize = 4096;
        break;
    default:
        ungetc(i, stdin);
        fscanf(stdin, "%d", config->blockSize);
    }

    return S_OK;
}

int BPlusTree::initTree(pTree tree, char *fileName, int blockSize)
{
    blockSize_ = blockSize;
    // 这两个暂时设置成一样的，默认存的是数字
    maxOrder_ = (blockSize - sizeof(bPlusNode)) / (sizeof(int) + sizeof(ssize_t));
    maxEntries_ = (blockSize - sizeof(bPlusNode)) / (sizeof(int) + sizeof(ssize_t));

    tree->fileName = fileName;
    tree->root = INVALID_OFFSET; // 初始化根节点
    // 初始化缓冲区
    tree->caches = (char *)malloc(blockSize * MIN_CACHE_NUM);
    // 初始化被删除的空闲块为空
    tree->freeBlocks->prev = tree->freeBlocks;
    tree->freeBlocks->next = tree->freeBlocks;

    // 从文件读命令 进行建树
    int fd = open(strcat(tree->filename, ".build"), O_RDWR, 0644);
    if (fd > 0)
    {
        tree->fd = fd;
        ssize_t key;
        while (fscanf(fd, "%d", &d) != EOF)
        {
            insert(tree, key, key);
        }
    }

    return S_OK;
}

int BPlusTree::insert(pTree tree, int key, ssize_t data)
{
    pNode node = getNode(tree, tree->root);
    if (NULL != node)
    {
    }

    // 新节点加入
    pNode root = newLeaf(tree);
    key(root)[0] = key;
    data(root)[0] = data;
    root->count = 1;
    tree->root = append2Tree(tree, root);
    tree->level = 1;

    // 写入到硬盘
    nodeFlush(tree, root);
    return S_OK;
}

int BPlusTree::nodeFlush(pTree tree, pNode node)
{
    if (NULL != node)
    {
        pwrite(tree->fd, node, blockSize_, node->self);
        freeCache(tree, node);
    }
    return S_OK;
}

int BPlusTree::freeCache(pTree tree, pNode node)
{
    char *buf = (char *)node;
    int i = (buf - tree->caches) / blockSize_;
    tree->used[i] = 0;

    return S_OK;
}

int BPlusTree::append2Tree(pTree tree, pNode node)
{
    if (listEmpty(&tree->freeBlocks))
    {
        node->self = tree->fileSize;
        tree->fileSize += blockSize_;
    }
    else
    {
        // TODO: 放到空闲块中
        struct freeBlock *block;
    }
}

int BPlusTree::listEmpty(struct listHead *head)
{
    return head->next == head;
}

// 从硬盘中把数据读到内存
int BPlusTree::getNode(pTree tree, ssize_t offset)
{
    if (offset == INVALID_OFFSET)
        return NULL;

    for (int i = 0; i < MIN_CACHE_NUM; i++)
    {
        if (!tree->used[i])
        {
            char *buf = tree->caches + blockSize_ * i;
            pread(tree->fd, buf, blockSize_, offset);
            return (pNode)buf;
        }
    }
}

int BPlusTree::newLeaf(pTree tree)
{
    pNode node = newNode(tree);
    node->type = BPLUS_TREE_LEAF;
    return node;
}

int BPlusTree::newNode(pTree tree)
{
    pNode node = getCache(tree);
    node->self = INVALID_OFFSET;
    node->parent = INVALID_OFFSET;
    node->prev = INVALID_OFFSET;
    node->next = INVALID_OFFSET;
    node->count = 0;
    return node;
}

int BPlusTree::getCache(pTree tree)
{
    for (int i = 0; i < MIN_CACHE_NUM; i++)
    {
        if (!tree->used[i])
        {
            tree->used[i] = 1;
            char *buf = tree->caches + blockSize_ * i;
            return (pNode)buf;
        }
    }
}
