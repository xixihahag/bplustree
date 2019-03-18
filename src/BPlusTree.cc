/*
 * @file BPlusTree.cc
 * @brief 
 * 
 * @author gyz
 * @email mni_gyz@163.com
 */
#include "BPlusTree.h"

int setConfig(pConfig config)
{
    // 从屏幕中读
    memset(config->fileName, 0, sizeof(config->fileName));
    printf("filename:(default /tmp/bptree)");

    char i;
    switch (i = getchar())
    {
    case '\n':
        strcpy(config->fileName, "/tmp/bptree");
        break;
    default:
        ungetc(i, stdin);
        fscanf(stdin, "%s", config->fileName);
        getchar();
    }

    bool flag = false;
    while (!flag)
    {
        printf("blocksize:(default 4096)");
        switch (i = getchar())
        {
        case '\n':
            // config->blockSize = 4096;
            // TODO: 最后改回去
            config->blockSize = 76;
            flag = true;
            break;
        default:
            ungetc(i, stdin);
            fscanf(stdin, "%d", &config->blockSize);
            getchar();
            if (config->blockSize >= 76)
                flag = true;
            else
            {
                printf("blocksize too small\n");
            }
        }
    }

    return S_OK;
}

int initTree(pTree tree, char *fileName, int blockSize)
{
    printf("initTree\n");

    blockSize_ = blockSize;
    // 这两个暂时设置成一样的，默认存的是数字
    // 指的都是data/subchild的个数
    maxOrder_ = (blockSize - sizeof(struct bPlusNode)) / (sizeof(int) + sizeof(ssize_t));
    maxEntries_ = (blockSize - sizeof(struct bPlusNode)) / (sizeof(int) + sizeof(ssize_t));

    // printf("bPlusNode = %d\n", sizeof(struct bPlusNode));
    // printf("sum size = %d\n", sizeof(struct bPlusNode) + 3 * (sizeof(int) + sizeof( ssize_t)));
    // printf("maxOrder_ = %d\n", maxOrder_);
    // printf("maxEntries_ = %d\n", maxEntries_);

    memset(tree->fileName, 0, sizeof(tree->fileName));

    strcpy(tree->fileName, fileName);

    tree->root = INVALID_OFFSET; // 初始化根节点
    // 初始化缓冲区
    tree->caches = (char *)malloc(blockSize * MIN_CACHE_NUM);
    // 初始化空闲块链表为空
    tree->freeBlocks.prev = &tree->freeBlocks;
    tree->freeBlocks.next = &tree->freeBlocks;

    tree->fileSize = 0;

    // TODO: O_DIRECT 读写操作的传输数据大小和缓冲区地址都需要按照一定的规则对齐
    // https://www.cnblogs.com/ziziwu/p/4126244.html
    // tree->fd = open(tree->fileName, O_CREAT | O_RDWR | O_TRUNC | O_DIRECT | O_SYNC, 0644);

    tree->fd = open(tree->fileName, O_CREAT | O_RDWR | O_TRUNC, 0644);

    // FIXME:
    // FILE *file = fopen(strcat(tree->fileName, ".build"), "r");
    FILE *file = fopen("//tmp//bptree.build", "r");
    if (file != NULL)
    {
        int key;
        ssize_t data;
        while (fscanf(file, "%d %lu", &key, &data) != EOF)
        {
            insert(tree, key, data);
            // deleteNode(tree, key);
        }
    }
    else
    {
        printf("file not exist\n");
    }

    // printf("insert end\n");

    return S_OK;
}

// 删除主逻辑
int deleteNode(pTree tree, int key)
{
    pNode node = getNode(tree, tree->root);
    while (node != NULL)
    {
        if (isLeaf(node))
        {
            return leafRemove(tree, node, key);
        }
        else
        {
            int i = keyBinarySearch(node, key);
            if (i >= 0)
            {
                node = getNode(tree, *subi(node, i + 1));
            }
            else
            {
                i *= -1;
                node = getNode(tree, *subi(node, i));
            }
        }
    }
    return S_FALSE;
}

int leafRemove(pTree tree, pNode node, int key)
{
    int pos = keyBinarySearch(node, key);
    int half = (maxEntries_ + 1) / 2;
    if (pos < 0)
    {
        // 尝试删除不存在的节点
        return S_FALSE;
    }

    if (node->parent == INVALID_OFFSET)
    {
        if (node->count == 1)
        {
            tree->root = INVALID_OFFSET;
            tree->level = 0;
            nodeDelete(tree, node, NULL, NULL);
        }
        else
        {
            // leafSimpleRemove(tree, node, pos);
            nodeFlush(tree, node);
        }
    }
    else if (node->count <= half)
    {
        // leafSimpleRemove(tree, node, pos);
        if (findSiblingNode(tree, node))
        {
            // 借一个节点过来-处理双亲节点-结束
            int preKey = *keyi(node, 0);
            int newKey;

            // 右移 空出第一个位置
            memmove(keyi(node, 1), keyi(node, 0), node->count * sizeof(int));
            memmove(datai(node, 1), datai(node, 0), node->count * sizeof(ssize_t));

            // 把兄弟节点的关键字拿过来
            pNode sibling = getNode(tree, node->prev);
            memmove(keyi(node, 0), keyi(sibling, sibling->count - 1), sizeof(int));
            memmove(datai(node, 0), datai(sibling, sibling->count - 1), sizeof(ssize_t));

            // 处理双亲节点的问题
            newKey = *keyi(node, 0);
            noLeafReplace(tree, node->parent, preKey, newKey);

            // 数据写回硬盘
            nodeFlush(tree, node);
            nodeFlush(tree, sibling);
        }
        else
        {
            // 与兄弟节点合并-处理双亲节点-视情况结束
            int preK = *keyi(node, 0);
            pNode sibling = getNode(tree, node->prev);

            // 合并到兄弟节点
            memmove(keyi(sibling, sibling->count), keyi(node, 0), node->count * sizeof(int));
            memmove(datai(sibling, sibling->count), datai(node, 0), node->count * sizeof(ssize_t));

            nodeDelete(tree, node, NULL, NULL);

            // 处理双亲节点
            // noLeafRemove(tree, getNode(tree, node->parent), preK);
        }
    }
    else
    {
        // leafSimpleRemove(tree, node, pos);
        nodeFlush(tree, node);
    }
}

//   int  noLeafRemove(pTree tree, pNode node, int key)
// {
//     int pos = keyBinarySearch(node, key);
//     int half = (node->count + 1) / 2;

//     if (node->parent == INVALID_OFFSET)
//     {
//         if (node->count == 2)
//         {
//             pNode root = getNode(tree,tiInt(node,0));
//             root->parent = INVALID_OFFSET;
//             tree->root = root->self;
//             tree->level--;
//             nodeDelete(tree, node, INVALID_OFFSET, INVALID_OFFSET);
//             nodeFlush(tree, node);
//         }
//         else
//         {
//             noLeafSimpleRemove(tree, node, key);
//             nodeFlush(tree, node);
//         }
//     }
//     else if (node->count <= half)
//     {
//         // noleafSimpleRemove(tree, node, pos);
//         if (findSiblingNode(getNode(tree,node->prev))
//         {
//             // 向兄弟节点借关键字-解决父母节点的问题
//             int preK = keyi(node,0);
//             int newK;

//             memmove(&keyi(node,1), &keyi(node,0), node->count * sizeof(int));
//             memmove(&subi(node,1), &subi(node,0), node->count * sizeof( ssize_t));

//             // FIXME: 两次get同一个node 会不会出问题
//             pNode sibling = getNode(tree, node->prev);
//             memmove(&keyi(node,0), &key(sibling)[sibling->count - 1], sizeof(int));
//             memmove(&keyi(node,0), &key(sibling)[sibling->count - 1], sizeof( ssize_t));

//             // TODO: 处理父母节点的问题
//             newK = keyi(node,0);
//             noLeafReplace(tree, node->parent, preK, newK);

//             nodeFlush(tree, node);
//             nodeFlush(tree, sibling);
//         }
//         else
//         {
//             // 拉父母节点的关键字下来合并
//             int posi = keyBinarySearch(node->parent, key);
//             if (posi < 0)
//             {
//                 posi *= -1;
//                 posi--;
//             }

//             pNode sibling = getNode(tree, node->prev);
//             pNode parent = getNode(tree, node->parent);

//             // 把父母节点的关键字拉下来
//             memmove(&key(sibling)[sibling->count - 1], &keyi(parent,posi), sizeof(int));

//             // 合并兄弟节点
//             memmove(&key(sibling)[sibling->count], &keyi(node,0), node->count * sizeof(int));
//             memmove(&sub(sibling)[sibling->count], &subi(node,0), node->count * sizeof(int));

//             // 维护双亲节点
//             noLeafRemove(tree, parent, posi);
//         }
//     }
//     else
//     {
//         // noLeafSimpleRemove(tree, node, key);
//         nodeFlush(tree, node);
//     }
// }

int noLeafSimpleRemove(pTree tree, pNode node, int pos)
{
    memmove(keyi(node, pos), keyi(node, pos + 1), (node->count - pos - 2) * sizeof(int));
    memmove(subi(node, pos), subi(node, pos + 1), (node->count - pos - 1) * sizeof(ssize_t));
    node->count--;

    return S_OK;
}

int noLeafReplace(pTree tree, ssize_t parent, int preK, int newK)
{
    pNode node = getNode(tree, parent);
    int pos = keyBinarySearch(node, preK);
    if (pos < 0)
        return S_FALSE;

    *keyi(node, pos) = newK;

    nodeFlush(tree, node);
    freeCache(tree, node);
    return S_OK;
}

// 查找兄弟节点是否有多余关键字
bool findSiblingNode(pTree tree, pNode node)
{
    if (node == NULL)
        return false;
    pNode leftNode = getNode(tree, node->prev);
    if (leftNode->count > ((maxEntries_ + 1) / 2))
        return true;
    return false;
}

int nodeDelete(pTree tree, pNode node, pNode lch, pNode rch)
{
}

// int  leafSimpleRemove(pTree tree, pNode node, int pos)
// {
//     memmove(&keyi(node,remove), &keyi(node,remove+1), (node->count - pos - 1) * sizeof(int));
//     memmove(&datai(node,remove), &datai(node,remove+1), (node->count - pos - 1) * sizeof( ssize_t));
//     node->count--;

//     return S_OK;
// }

// TODO: 插入主逻辑
int insert(pTree tree, int key, ssize_t data)
{
    printf("insert\n");

    pNode node = getNode(tree, tree->root);
    pNode oldNode;
    while (NULL != node)
    {
        if (isLeaf(node))
        {
            return leafInsert(tree, node, key, data);
        }
        else
        {
            int i = keyBinarySearch(node, key);
            oldNode = node;
            if (i >= 0)
            {
                node = getNode(tree, *subi(node, i + 1));
            }
            else
            {
                i += 1;
                i *= -1;
                node = getNode(tree, *subi(node, i));
            }
        }
        freeCache(tree, oldNode);
    }
    // printf("new node\n");

    // 新节点加入
    pNode root = newLeaf(tree);

    *keyi(root, 0) = key;
    *datai(root, 0) = data;

    root->count = 1;
    tree->root = append2Tree(tree, root);
    tree->level = 1;

    // 写入到硬盘
    nodeFlush(tree, root);

    // printf("insert end\n");
    return S_OK;
}

int leafInsert(pTree tree, pNode node, int key, ssize_t data)
{
    printf("leafinsert\n");
    int insert = keyBinarySearch(node, key);

    // 限制插入重复节点
    if (insert >= 0)
    {
        return S_FALSE;
    }

    // 应该插入的位置
    insert += 1;
    insert *= -1;
    int splitKey;

    // 分裂新节点
    if ((node->count) == maxEntries_)
    {
        // 从这个点分裂成两个
        int split = (maxEntries_ + 1) / 2;
        pNode sibling = newLeaf(tree);

        if (insert < split)
        {
            // 分裂节点 并把新分裂节点key偏移量返回来
            splitKey = leafSplitLeft(tree, node, sibling, key, data, insert);
            // return buildParentNode(tree, sibling, node, splitKey);
        }
        else
        {
            // printf("key = %d\n", key);
            splitKey = leafSplitRight(tree, node, sibling, key, data, insert);
        }

        // showNode(node);
        // showNode(sibling);
        // printf("0000\n");

        return buildParentNode(tree, node, sibling, splitKey);
    }
    else
    {
        // 直接插入
        leafSimpleInsert(tree, node, key, data, insert);
        nodeFlush(tree, node);
    }
}

int buildParentNode(pTree tree, pNode left, pNode right, int key)
{
    printf("buildParentNode\n");

    // 左面和右面新加节点的情况统一在一起了 所以要单独判断左右的节点是否有parent
    if (left->parent == INVALID_OFFSET && right->parent == INVALID_OFFSET)
    {
        pNode parent = newNoLeaf(tree);
        *keyi(parent, 0) = key;

        // 与子节点相关联

        // printf("left->self = %lu\n", left->self);
        // printf("right->self = %lu\n", right->self);

        *subi(parent, 0) = left->self;
        *subi(parent, 1) = right->self;
        parent->count = 2;

        // tree->root 永远指向最高的节点
        tree->root = append2Tree(tree, parent);

        left->parent = parent->self;
        right->parent = parent->self;
        tree->level++;

        // 把节点写到磁盘
        nodeFlush(tree, parent);
        nodeFlush(tree, left);
        nodeFlush(tree, right);

        // printf("tree->root = %lu\n", tree->root);
        // showNode(getNode(tree, tree->root));
        // printf("***\n");
        // printf("parent->self = %lu\n", parent->self);
        // showNode(parent);
        // showNode(left);
        // showNode(right);

        return S_OK;
    }
    else if (right->parent == INVALID_OFFSET && left->parent != INVALID_OFFSET)
    {
        // printf("key = %d\n", key);
        return noLeafInsert(tree, getNode(tree, left->parent), left, right, key);
    }
}

int noLeafInsert(pTree tree, pNode node, pNode left, pNode right, int key)
{
    printf("noLeafInsert\n");
    // printf("key = %d\n", key);

    int insert = keyBinarySearch(node, key);

    insert += 1;
    insert *= -1;

    if (node->count == maxOrder_)
    {
        int splitKey;
        int split = (node->count + 1) / 2;
        pNode sibling = newNoLeaf(tree);
        int relativeSplit = split - 1;
        if (insert < relativeSplit)
        {
            splitKey = noLeafSplitLeft(tree, node, sibling, left, right, key, split);
        }
        else if (insert == relativeSplit)
        {
            splitKey = noLeafSplitRight(tree, node, sibling, left, right, key, split);
        }
        else
        {
            splitKey = noLeafSplitRight1(tree, node, sibling, left, right, key, split);
        }
        return buildParentNode(tree, node, sibling, splitKey);
    }
    else
    {
        noLeafSimpleInsert(tree, node, left, right, key, insert);
        nodeFlush(tree, node);

        // printf("offset = %d\n", *subi(node, 2));
        // showNode(getNode(tree, *subi(node, 2)));
        // printf("end\n");
    }

    return S_OK;
}

int noLeafSplitRight(pTree tree, pNode node, pNode right, pNode lch, pNode rch, int key, int split)
{
    printf("noLeafSplitRight\n");
    rightNodeAdd(tree, node, right);

    node->count = split;
    // maxOrder_ - (split - 1) - 1
    right->count = maxOrder_ - split + 1;

    memmove(keyi(right, 0), keyi(node, split - 1), (right->count - 1) * sizeof(int));
    memmove(subi(right, 1), subi(node, split), (right->count - 1) * sizeof(int));

    subNodeUpdate(tree, right, 0, rch);
    nodeFlush(tree, lch);

    for (int i = 1; i < right->count; i++)
    {
        subNodeFlush(tree, right, i);
    }

    return key;
}

int noLeafSplitRight1(pTree tree, pNode node, pNode right, pNode lch, pNode rch, int key, int split)
{
    printf("noLeafSplitRight1\n");
    rightNodeAdd(tree, node, right);

    int splitKey = *keyi(node, split - 1);
    node->count = split;
    // maxOrder_ - (split - 1) - 1
    right->count = maxOrder_ - split + 1;

    *keyi(right, 0) = key;
    subNodeUpdate(tree, right, 0, lch);
    subNodeUpdate(tree, right, 1, rch);

    memmove(keyi(right, 1), keyi(node, split), (right->count - 1) * sizeof(int));
    memmove(subi(right, 2), subi(node, split + 1), (right->count - 1) * sizeof(ssize_t));

    for (int i = 2; i < right->count; i++)
    {
        subNodeFlush(tree, right, i);
    }

    return splitKey;
}

int subNodeFlush(pTree tree, pNode parent, ssize_t offset)
{
    printf("subNodeFlush\n");
    pNode subNode = getNode(tree, offset);
    subNode->parent = parent->self;
    nodeFlush(tree, subNode);

    return S_OK;
}

int noLeafSplitLeft(pTree tree, pNode node, pNode right, pNode lch, pNode rch, int key, int split)
{
    printf("noLeafSplitLeft\n");

    rightNodeAdd(tree, node, right);

    int insert = keyBinarySearch(node, key);
    insert += 1;
    insert *= -1;

    // printf("insert = %d\n", insert);

    int splitKey = *keyi(node, split - 2);

    node->count = split;
    right->count = maxOrder_ - split + 1;

    // 先搞定右边的
    memmove(keyi(right, 0), keyi(node, split - 1), (right->count - 1) * sizeof(int));
    memmove(subi(right, 0), subi(node, split - 1), right->count * sizeof(ssize_t));

    for (int i = 0; i < right->count; i++)
    {
        subNodeFlush(tree, right, i);
    }

    // 再移左面的部分
    memmove(keyi(node, insert + 1), keyi(node, insert), (node->count - insert - 2) * sizeof(int));
    memmove(subi(node, insert + 2), keyi(node, insert + 1), (node->count - insert - 2) * sizeof(ssize_t));

    *keyi(node, insert) = key;
    subNodeUpdate(tree, node, insert + 1, rch);

    nodeFlush(tree, lch);

    return splitKey;

    // --

    // memmove(keyi(node, insert + 1), keyi(node, insert), (split - insert - 1) * sizeof(int));
    // memmove(subi(node, insert + 2), subi(node, insert + 1), (split - insert - 1) * sizeof(ssize_t));

    // *keyi(node, insert) = key;
    // subNodeUpdate(tree, node, insert + 1, rch);

    // //更新子节点
    // for (int i = 0; i < right->count; i++)
    // {
    //     subNodeFlush(tree, right, *subi(right, i));
    // }

    // nodeFlush(tree, lch);

    // return splitKey;
}

int noLeafSimpleInsert(pTree tree, pNode node, pNode left, pNode right, int key, int insert)
{
    printf("noLeafSimpleInsert\n");
    memmove(keyi(node, insert + 1), keyi(node, insert), (node->count - insert) * sizeof(int));
    memmove(subi(node, insert + 2), subi(node, insert + 1), (node->count - insert) * sizeof(ssize_t));

    *keyi(node, insert) = key;
    subNodeUpdate(tree, node, insert + 1, right);
    nodeFlush(tree, left);

    // showNode(getNode(tree,tiInt(node,2)));

    node->count++;

    return S_OK;
}

// 先建立父子关系，再写到硬盘上
int subNodeUpdate(pTree tree, pNode parent, int insert, pNode child)
{
    printf("subNodeUpdate\n");
    *subi(parent, insert) = child->self;
    child->parent = parent->self;
    nodeFlush(tree, child);

    return S_OK;
}

// pass
int leafSimpleInsert(pTree tree, pNode leaf, int key, ssize_t data, int insert)
{
    printf("leafSimpleInsert\n");
    memmove(keyi(leaf, insert + 1), keyi(leaf, insert), (leaf->count - insert) * sizeof(int));
    memmove(datai(leaf, insert + 1), datai(leaf, insert), (leaf->count - insert) * sizeof(ssize_t));
    *keyi(leaf, insert) = key;
    *datai(leaf, insert) = data;
    leaf->count++;

    return S_OK;
}

int leafSplitLeft(pTree tree, pNode leaf, pNode right, int key, ssize_t data, int insert)
{
    printf("leafSplitLeft\n");
    int split = (leaf->count + 1) / 2;
    rightNodeAdd(tree, leaf, right);

    int pivot = insert;
    leaf->count = split;
    right->count = maxEntries_ - split + 1;

    memmove(keyi(right, 0), keyi(leaf, split - 1), (right->count) * sizeof(int));
    memmove(datai(right, 0), datai(leaf, split - 1), (right->count) * sizeof(ssize_t));

    memmove(keyi(leaf, pivot + 1), keyi(leaf, pivot), (split - pivot - 1) * sizeof(int));
    memmove(datai(leaf, pivot + 1), datai(leaf, pivot), (split - pivot - 1) * sizeof(ssize_t));

    *keyi(leaf, pivot) = key;
    *datai(leaf, pivot) = data;

    return *keyi(right, 0);
}

// leafSplitRight(tree, node, sibling, key, data, insert);
int leafSplitRight(pTree tree, pNode leaf, pNode right, int key, ssize_t data, int insert)
{
    printf("leafSplitRight\n");
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
    memmove(keyi(right, 0), keyi(leaf, split), pivot * sizeof(int));
    memmove(datai(right, 0), datai(leaf, split), pivot * sizeof(ssize_t));

    // 把新数据加上去
    *keyi(right, pivot) = key;
    *datai(right, pivot) = data;

    // 如果插入的值在中间的话 需要把右面的值也加上去
    memmove(keyi(right, pivot + 1), keyi(leaf, insert), (maxEntries_ - insert) * sizeof(int));
    memmove(datai(right, pivot + 1), datai(leaf, insert), (maxEntries_ - insert) * sizeof(ssize_t));

    // 返回的是新加入的节点key的首地址
    return *keyi(right, 0);
}

// 把左侧新new的节点融入这颗树
int leftNodeAdd(pTree tree, pNode node, pNode left)
{
    printf("leftNodeAdd\n");
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

int rightNodeAdd(pTree tree, pNode node, pNode right)
{
    printf("rightNodeAdd\n");
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

int isLeaf(pNode node)
{
    // printf("isLeaf\n");
    return (node->type == BPLUS_TREE_LEAF);
}

int nodeFlush(pTree tree, pNode node)
{
    if (NULL != node)
    {
        pwrite(tree->fd, node, blockSize_, node->self);
        freeCache(tree, node);
    }
    return S_OK;
}

int freeCache(pTree tree, pNode node)
{
    // printf("freeCache\n");
    char *buf = (char *)node;
    int i = (buf - tree->caches) / blockSize_;
    tree->used[i] = false;

    return S_OK;
}

ssize_t append2Tree(pTree tree, pNode node)
{
    if (listEmpty(&tree->freeBlocks))
    {
        printf("append2Tree\n");
        node->self = tree->fileSize;
        tree->fileSize += blockSize_;
    }
    else
    {
        // TODO: 放到空闲块列表中
        struct freeBlock *block;
    }

    return node->self;
}

int listEmpty(struct listHead *head)
{
    return head->next == head;
}

// 从硬盘中把数据读到内存
pNode getNode(pTree tree, ssize_t offset)
{
    // printf("getNode\n");
    if (offset == INVALID_OFFSET)
        return NULL;

    for (int i = 0; i < MIN_CACHE_NUM; i++)
    {
        if (!tree->used[i])
        {

            tree->used[i] = true;
            char *buf = tree->caches + blockSize_ * i;
            pread(tree->fd, buf, blockSize_, offset);

            // printf("offset =  %lu\n", ((pNode)buf)->self);
            return (pNode)buf;
        }
    }

    printf("find no buf\n");
    return NULL;
}

pNode newLeaf(pTree tree)
{
    pNode node = newNode(tree);
    node->type = BPLUS_TREE_LEAF;
    return node;
}

pNode newNoLeaf(pTree tree)
{
    pNode node = newNode(tree);
    node->type = BPLUS_TREE_NO_LEAF;
    return node;
}

pNode newNode(pTree tree)
{
    pNode node = getCache(tree);
    node->self = INVALID_OFFSET;
    node->parent = INVALID_OFFSET;
    node->prev = INVALID_OFFSET;
    node->next = INVALID_OFFSET;
    node->count = 0;
    return node;
}

pNode getCache(pTree tree)
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
ssize_t search(pTree tree, int key)
{
    pNode node = getNode(tree, tree->root);
    while (node != NULL)
    {
        int i = keyBinarySearch(node, key);
        if (isLeaf(node))
        {
            return (i > 0 ? *datai(node, i) : -1);
        }
        else
        {
            if (i >= 0)
            {
                node = getNode(tree, *subi(node, i + 1));
            }
            else
            {
                i *= -1;
                node = getNode(tree, *subi(node, i));
            }
        }
    }
}

// 二分搜索
// 应该返回的值是以0开始的
// 如果找到的话返回找到的位置
// 如果未找到的话返回应该插入的位置
int keyBinarySearch(pNode node, int target)
{
    // printf("keyBinarySearch\n");
    int *arr = keyi(node, 0);
    int len;
    if (isLeaf(node))
    {
        len = node->count;
    }
    else
    {
        len = node->count - 1;
    }

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

    return -low - 1;
}

void show(pTree tree)
{
    // printf("show\n");

    if (tree->root == INVALID_OFFSET)
    {
        printf("tree empty\n");
        return;
    }

    // printf("noleaf: offset = %d ", tree->root);
    // pNode tnode = getNode(tree, tree->root);
    // showNode(tnode);

    // printf("\nleaf: offset = %d ", *subi(tnode, 0));
    // showNode(getNode(tree, *subi(tnode, 0)));
    // freeCache(tree, tnode);

    // printf("\nleaf: offset = %d ", *subi(tnode, 1));
    // showNode(getNode(tree, *subi(tnode, 1)));
    // freeCache(tree, tnode);

    // printf("\nleaf: offset = %d ", *subi(tnode, 2));
    // showNode(getNode(tree, *subi(tnode, 2)));
    // freeCache(tree, tnode);

    // printf("****\n");

    // pNode node;
    //  ssize_t offset;
    // for (int i = 0; i <= 3; i++)
    // {
    //     offset = blockSize_ * i;
    //     node = getNode(tree, offset);
    //     showNode(node);
    //     freeCache(tree, node);
    //     printf("\n");
    // }

    // --

    std::queue<ssize_t> q;
    q.push(tree->root);

    int len, curCount = 1, nexCount = 0;
    ssize_t t;
    pNode node;
    while (!q.empty())
    {
        t = q.front();
        q.pop();

        node = getNode(tree, t);
        if (NULL == node)
        {
            printf("node == null\n");
        }

        if (isLeaf(node))
        {
            printf("Leaf: offset = %lu\n", t);
            len = node->count;
        }
        else
        {
            printf("noLeaf: offset = %lu\n", t);
            len = node->count - 1;

            // 把子女节点加入到queue中
            if (!isLeaf(node))
            {
                for (int i = 0; i < node->count; i++)
                {
                    // printf("i = %d, offset = %lu\n", i, *subi(node, i));

                    q.push(*subi(node, i));
                }
            }

            nexCount += node->count;
        }

        // 打印node节点中的key
        for (int i = 0; i < len; i++)
        {
            printf("i = %d, key = %d\n", i, *keyi(node, i));
        }

        freeCache(tree, node);

        if ((--curCount) == 0)
        {
            printf("\n");
            curCount = nexCount;
            nexCount = 0;
        }
    }
}

void showNode(pNode node)
{
    // printf("showNode\n");

    int len;
    if (isLeaf(node))
    {
        // printf("***\n");
        len = node->count;
    }
    else
    {
        len = node->count - 1;
    }

    printf("len = %d\n", len);
    for (int i = 0; i < len; i++)
    {
        printf("i = %d,key = %d\n", i, *keyi(node, i));
    }
}

char *offsetPtr(pNode node)
{
    return ((char *)(node) + sizeof(struct bPlusNode));
}

char *sub(pNode node)
{
    return (offsetPtr(node) + (maxOrder_ - 1) * sizeof(int));
}

ssize_t *subi(pNode node, int i)
{
    return (ssize_t *)(sub(node) + i * sizeof(ssize_t));
}

char *key(pNode node)
{
    return offsetPtr(node);
}

int *keyi(pNode node, int i)
{
    return (int *)(key(node) + i * sizeof(int));
}

char *data(pNode node)
{
    return (offsetPtr(node) + maxEntries_ * sizeof(int));
}

ssize_t *datai(pNode node, int i)
{
    return (ssize_t *)(data(node) + i * sizeof(int));
}