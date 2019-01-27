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

// TODO: 插入主逻辑
int BPlusTree::insert(pTree tree, int key, ssize_t data)
{
    pNode node = getNode(tree, tree->root);
    if (NULL != node)
    {
        if (isLeaf(node))
        {
            return leafInsert(tree, node, key, data);
        }
        else
        {
            int i = keyBinarySearch(node, key);
            if (i >= 0)
            {
                node = getNode(tree, sub(node)[i + 1]);
            }
            else
            {
                i = -i;
                node = getNode(tree, sub(node)[i]);
            }
        }
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

int BPlusTree::leafInsert(pTree tree, pNode node, int key, ssize_t data)
{
    int insert = keyBinarySearch(node, key);
    if (insert >= 0)
    {
        // 已经存在
        return S_FALSE;
    }

    // 应该插入的位置
    insert *= -1;
    int splitKey;

    // getCache(tree);

    if (node->count == maxEntries_)
    {
        // 从这个点分裂成两个
        int split = (maxEntries_ + 1) / 2;
        pNode sibling = newLeaf(tree);

        if (insert < split)
        {
            // 分裂节点 并把新分裂节点key偏移量返回来
            splitKey = leafSplitLeft(tree, node, sibling, key, data, insert);
            return buildParentNode(tree, sibling, leaf, splitKey);
        }
        else
        {
            splitKey = leafSplitRight(tree, node, sibling, key, data, insert);
            return buildParentNode(tree, leaf, sibling, splitKey);
        }
    }
    else
    {
        leafSimpleInsert(tree, node, key, data, insert);
        nodeFlush(tree, node);
    }
}

int BPlusTree::buildParentNode(pTree tree, pNode left, pNode right, int key)
{
    // 左面和右面新加节点的情况统一在一起了 所以要单独判断左右的节点是否有parent
    if (left->parent == INVALID_OFFSET && right->parent == INVALID_OFFSET)
    {
        pNode parent = newNoLeaf(tree);
        key(parent)[0] = key;

        // 与子节点相关联
        sub(parent)[0] = left->self;
        sub(parent)[1] = right->self;
        parent->count = 2;

        // tree->root 永远指向最高的节点
        tree->root = append2Tree(tree, parent);

        left->parent = parent->self;
        right->parent = parent->self;
        tree->level++;

        // 把节点写到磁盘
        nodeFlush(tree, left);
        nodeFlush(tree, right);
        nodeFlush(tree, parent);

        return S_OK;
    }
    else if (left->parent == INVALID_OFFSET)
    {
        return noLeafInsert(tree, getNode(tree, right->parent), left, right, key);
    }
    else if (right->parent == INVALID_OFFSET)
    {
        return noLeafInsert(tree, getNode(tree, left->parent), left, right, key);
    }
}

int BPlusTree::noLeafInsert(pTree tree, pNode node, pNode left, pNode right, int key)
{
    int insert = keyBinarySearch(tree, key);
    insert *= -1;

    if (node->count == maxOrder_)
    {
        int splitKey;
        int split = (node->count + 1) / 2;
        pNode sibling = newNoLeaf(tree);
        if (insert < split)
        {
            splitKey = noLeafSplitLeft(tree, node, sibling, left, right, key, split);
        }
        else if (insert == split)
        {
            splitKey = noLeafSplitLeft();
        }
        else
        {
            splitKey = noLeafSplitLeft();
        }
    }
    else
    {
        noLeafSimpleInsert(tree, node, left, right, insert);
        nodeFlush(tree, node);
    }

    return S_OK;
}

int BPlusTree::noLeafSplitLeft(pTree tree, pNode node, pNode left, pNode lch, pNode rch, int key, int insert)
{
    int splitKey;
    int split = (maxOrder_ + 1) / 2;

    leftNodeAdd(tree, node, left);

    int pivot = insert;
    left->count = split;
    node->count = maxOrder_ - split + 1;

    memmove(&key(left)[0], &key(node)[0], pivot * sizeof(int));
    memmove(&sub(left)[0], &sub(node)[0], pivot * sizeof(ssize_t));

    memmove(&key(left)[pivot + 1], &key(left)[pivot], (split - pivot - 1) * sizeof(int));
    memmove(&sub(left)[pivot + 1], &sub(left)[pivot], (split - pivot - 1) * sizeof(ssize_t));
}

int BPlusTree::noLeafSimpleInsert(pTree tree, pNode node, pNode left, pNode right, int insert)
{
    memmove(&key(node)[insert + 1], &key(node)[insert], (node->count - insert - 1) * sizeof(int));
    key(node)[insert] = key;

    memmove(&data(node)[insert + 2], &data(node)[insert + 1], (node->count - insert - 1) * sizeof(ssize_t));
    subNodeUpdate(tree, node, insert, left);
    subNodeUpdate(tree, node, insert + 1, right);

    node->count++;

    return S_OK;
}

int BPlusTree::subNodeUpdate(pTree tree, pNode parent, int insert, pNode child)
{
    sub(parent)[insert] = child->self;
    child->parent = parent->self;
    nodeFlush(tree, child);

    return S_OK;
}

int BPlusTree::leafSimpleInsert(pTree tree, pNode leaf, int key, ssize_t data, int insert)
{
    memmove(&key(leaf)[insert + 1], &key(leaf)[insert], (leaf->count - insert) * sizeof(int));
    memmove(&data(leaf)[insert + 1], &data(leaf)[insert], (leaf->count - insert) * sizeof(ssize_t));
    key(leaf)[insert] = key;
    data(leaf)[insert] = data;
    leaf->count++;

    return S_OK;
}

int BPlusTree::leafSplitLeft(pTree tree, pNode leaf, pNode left, int key, ssize_t data, int insert)
{
    int split = (leaf->count + 1) / 2;

    leftNodeAdd(tree, leaf, left);

    int pivot = insert;
    left->count = split;
    leaf->count = maxEntries_ - split + 1;

    memmove(&key(left)[0], &key(leaf)[0], pivot * sizeof(int));
    memmove(&data(left)]0,&data(leaf)[0],pivot*sizeof(ssize_t));

    key(leaf)[pivot] = key;
    data(leaf)[pivot] = data;

    memmove(&key(left)[pivot + 1], &key(leaf)[pivot], (split - pivot - 1) * sizeof(int));
    memmove(&data(left)[pivot + 1], &data(leaf)[pivot], (split - pivot - 1) * sizeof(ssize_t));

    // 比right多一步前移的操作
    memmove(&key(leaf)[0], &key(leaf)[split - 1], leaf->count * sizeof(int));
    memmove(&data(leaf)[0], &data(leaf)[split - 1], leaf->count * sizeof(ssize_t));

    return S_OK;
}

// leafSplitRight(tree, node, sibling, key, data, insert);
int BPlusTree::leafSplitRight(pTree tree, pNode leaf, pNode right, int key, ssize_t data, int insert)
{
    // 要分裂的位置
    int split = (leaf->count + 1) / 2;

    // 将新申请的节点right和当前节点相关联 还未分元素出去
    rightNodeAdd(tree, leaf, right);

    // 分元素出去
    // 计算两边剩余元素数量
    int pivot = insert - split;
    leaf->count = split;
    right->count = maxEntries_ - split + 1;

    // 放数据过去
    memmove(&key(right)[0], &key(leaf)[split], pivot * sizeof(int));
    memmove(&data(right)[0], &data(leaf)[split], pivot * sizeof(ssize_t));

    // 把新数据加上去
    key(right)[pivot] = key;
    data(right)[pivot] = data;

    // 如果插入的值在中间的话 需要把右面的值也加上去
    memmove(&key(right)[pivot + 1], &key(leaf)[insert], (maxEntries_ - insert) * sizeof(int));
    memmove(&data(right)[pivot + 1], &data(leaf)[insert], (maxEntries_ - insert) * sizeof(ssize_t));

    // 返回的是新加入的节点key的首地址
    return key(right)[0];
}

int BPlusTree::leftNodeAdd(pTree tree, pNode node, pNode left)
{
    append2Tree(tree, left);
    pNode prev = getNode(tree, node->prev);
    if (NULL != prev)
    {
        prev->next = left->self;
        left->prev = prev->self;
        // 把prev写入磁盘 以后不需要对这个节点进行操作
        nodeFlush(tree, prev);
    }
    else
    {
        left->prev = INVALID_OFFSET;
    }

    left->next = node->self;
    node->prev = left->self;

    return S_OK;
}

int BPlusTree::rightNodeAdd(pTree tree, pNode node, pNode right)
{
    append2Tree(tree, right);
    pNode next = getNode(tree, node->next);

    if (NULL != next)
    {
        next->prev = right->self;
        right->next = next->self;
        nodeFlush(tree, next);
    }
    else
    {
        right->next = INVALID_OFFSET;
    }

    right->prev = node->self;
    node->next = right->self;

    return S_OK;
}

int BPlusTree::isLeaf(pNode node)
{
    return node->type == BPLUS_TREE_LEAF;
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

int BPlusTree::newNoLeaf(pTree tree)
{
    pNode node = newNode(tree);
    node->type = BPLUS_TREE_NO_LEAF;
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

pNode BPlusTree::getCache(pTree tree)
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

// 搜索部分
int BPlusTree::search(pTree tree, int key)
{
    pNode node = getNode(tree, tree->root);
    while (node != NULL)
    {
        int i = keyBinarySearch(node, key);
    }
}

// 二分搜索 TODO: 自己写的，看看会不会有问题
// 返回值为负的话是当前节点中没有该元素，其应该插入的位置
int BPlusTree::keyBinarySearch(pNode node, int target)
{
    int *arr = key(node);
    int len = node->count;

    int low = 0;
    int high = len - 1;
    int flag = 1;
    while (low <= high)
    {
        int mid = (low + high) / 2;
        if (target == arr[mid])
        {
            return mid;
        }
        if (target > arr[mid])
        {
            low = mid + 1;
            flag = 0;
        }
        else
        {
            high = mid - 1;
            flag = 1;
        }
    }
    if (flag)
        return high < 0 ? high : -high;
    else
        return -low;
}