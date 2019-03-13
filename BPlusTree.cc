/*
 * @file BPlusTree.cc
 * @brief 
 * 
 * @author gyz
 * @email mni_gyz@163.com
 */
#include "BPlusTree.h"

int BPlusTree::setConfig(pConfig config)
{
    // 从屏幕中读
    printf("filename:(default /tmp/pbtree)");
    char i;
    switch (i = getchar())
    {
    case '\n':
        strcpy(config->fileName, "/tmp/pbtree");
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

    // tree->fileName = fileName;
    memcpy(tree->fileName, fileName, strlen(fileName));
    tree->root = INVALID_OFFSET; // 初始化根节点
    // 初始化缓冲区
    tree->caches = (char *)malloc(blockSize * MIN_CACHE_NUM);
    // 初始化空闲块链表为空
    tree->freeBlocks.prev = &tree->freeBlocks;
    tree->freeBlocks.next = &tree->freeBlocks;

    // 从文件读命令 进行建树
    int fd = open(strcat(tree->fileName, ".build"), O_RDWR, 0644);
    if (fd > 0)
    {
        tree->fd = fd;
        ssize_t key;
        // FIXME:  这里输入格式有点问题
        // 删除部分
        // while (fscanf(fd, "%d", &data) != EOF)
        // {
        //     insert(tree, key, data);
        //     deleteNode(tree, key);
        // }
    }

    return S_OK;
}

// 删除主逻辑
int BPlusTree::deleteNode(pTree tree, int key)
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
                node = getNode(tree, sub(node)[i + 1]);
            }
            else
            {
                i *= -1;
                node = getNode(tree, sub(node)[i]);
            }
        }
    }
    return S_FALSE;
}

int BPlusTree::leafRemove(pTree tree, pNode node, int key)
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
            leafSimpleRemove(tree, node, pos);
            nodeFlush(tree, node);
        }
    }
    else if (node->count <= half)
    {
        leafSimpleRemove(tree, node, pos);
        if (findSiblingNode(node))
        {
            // 借一个节点过来-处理双亲节点-结束
            int preKey = key(node)[0];
            int newKey;

            // 右移 空出第一个位置
            memmove(&key(node)[1], &key(node)[0], node->count * sizeof(int));
            memmove(&data(node)[1], &data(node)[0], node->count * sizeof(ssize_t));

            // 把兄弟节点的关键字拿过来
            pNode sibling = getNode(tree, node->prev);
            memmove(&key(node)[0], &key(sibling)[sibling->count - 1], sizeof(int));
            memmove(&data(node)[0], &data(sibling)[sibling->count - 1], sizeof(ssize_t));

            // 处理双亲节点的问题
            newKey = key(node)[0];
            noLeafReplace(tree, node->parent, preKey, newKey);

            // 数据写回硬盘
            nodeFlush(tree, node);
            nodeFlush(tree, sibling);
        }
        else
        {
            // 与兄弟节点合并-处理双亲节点-视情况结束
            int preK = key(node)[0];
            pNode sibling = getNode(tree, node->prev);

            // 合并到兄弟节点
            memmove(&key(sibling)[sibling->count], &key(node)[0], node->count * sizeof(int));
            memmove(&data(sibling)[sibling->count], &data(node)[0], node->count * sizeof(ssize_t));

            nodeDelete(tree, node, NULL, NULL);

            // 处理双亲节点
            noLeafRemove(tree, node->parent, preK);
        }
    }
    else
    {
        leafSimpleRemove(tree, node, pos);
        nodeFlush(tree, node);
    }
}

int BPlusTree::noLeafRemove(pTree tree, pNode node, int key)
{
    int pos = keyBinarySearch(node, key);
    int half = (node->count + 1) / 2;

    if (node->parent == NULL)
    {
        if (node->count == 2)
        {
            pNode root = getNode(tree, sub(node)[0]);
            root->parent = NULL;
            tree->root = root;
            tree->level--;
            nodeDelete(tree, node, NULL, NULL);
            nodeFlush(tree, node);
        }
        else
        {
            noLeafSimpleRemove(tree, node, key);
            nodeFlush(tree, node);
        }
    }
    else if (node->count <= half)
    {
        noleafSimpleRemove(tree, node, pos);
        if (findSiblingNode(node->prev))
        {
            // 向兄弟节点借关键字-解决父母节点的问题
            int preK = key(node)[0];
            int newK;

            memmove(&key(node)[1], &key(node)[0], node->count * sizeof(int));
            memmove(&sub(node)[1], &sub(node)[0], node->count * sizeof(ssize_t));

            // FIXME: 两次get同一个node 会不会出问题
            pNode sibling = getNode(node->prev);
            memmove(&key(node)[0], &key(sibling)[sibling->count - 1], sizeof(int));
            memmove(&key(node)[0], &key(sibling)[sibling->count - 1], sizeof(ssize_t));

            // TODO: 处理父母节点的问题
            newK = key(node)[0];
            noLeafReplace(tree, node->parent, preK, newK);

            nodeFlush(tree, node);
            nodeFlush(tree, sibling);
        }
        else
        {
            // 拉父母节点的关键字下来合并
            int posi = keyBinarySearch(node->parent, key);
            if (posi < 0)
            {
                posi *= -1;
                posi--;
            }

            pNode sibling = getNode(tree, node->prev);
            pNode parent = getNode(tree, node->parent);

            // 把父母节点的关键字拉下来
            memmove(&key(sibling)[sibling->count - 1], &key(parent)[posi], sizeof(int));

            // 合并兄弟节点
            memmove(&key(sibling)[sibling->count], &key(node)[0], node->count * sizeof(int));
            memmove(&sub(sibling)[sibling->count], &sub(node)[0], node->count * sizeof(int));

            // 维护双亲节点
            noLeafRemove(tree, parent, posi);
        }
    }
    else
    {
        noLeafSimpleRemove(tree, node, key);
        nodeFlush(tree, node);
    }
}

int BPlusTree::noLeafSimpleRemove(pTree tree, pNode node, int pos)
{
    memmove(&key(node)[pos], &key(node)[pos + 1], (node->count - pos - 2) * sizeof(int));
    memmove(&sub(node)[pos], &sub(node)[pos + 1], (node->count - pos - 1) * sizeof(ssize_t));
    node->count--;

    return S_OK;
}

int BPlusTree::noLeafReplace(pTree tree, ssize_t parent, int preK, int newK)
{
    pNode node = getNode(tree, parent);
    int pos = keyBinarySearch(node, preK);
    if (pos < 0)
        return S_FALSE;

    key(node)[pos] = newK;

    nodeFlush(tree, node);
    freeCache(tree, node);
    return S_OK;
}

// 查找兄弟节点是否有多余关键字
bool BPlusTree::findSiblingNode(pNode node)
{
    if (node == NULL)
        return false;
    pNode leftNode = getNode(node->prev);
    if (leftNode->count > ((maxEntries_ + 1) / 2))
        return true;
    return false;
}

int BPlusTree::nodeDelete(pTree tree, pNode node, pNode lch, pNode rch)
{
}

int BPlusTree::leafSimpleRemove(pTree tree, pNode node, int pos)
{
    memmove(&key(node)[remove], &key(node)[remove + 1], (node->count - pos - 1) * sizeof(int));
    memmove(&data(node)[remove], &data(node)[remove + 1], (node->count - pos - 1) * sizeof(ssize_t));
    node->count--;

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
                i += 1;
                i *= -1;
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

    // 限制插入重复节点
    if (insert >= 0)
    {
        // 已经存在
        return S_FALSE;
    }

    // 应该插入的位置
    insert += 1;
    insert *= -1;
    int splitKey;

    // 分裂新节点
    if (node->count == maxEntries_)
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
            splitKey = leafSplitRight(tree, node, sibling, key, data, insert);
        }

        return buildParentNode(tree, node, sibling, splitKey);
    }
    else
    {
        // 直接插入
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
    // else if (left->parent == INVALID_OFFSET)
    // {
    //     return noLeafInsert(tree, getNode(tree, right->parent), left, right, key);
    // }
    else if (right->parent == INVALID_OFFSET)
    {
        return noLeafInsert(tree, getNode(tree, left->parent), left, right, key);
    }
}

int BPlusTree::noLeafInsert(pTree tree, pNode node, pNode left, pNode right, int key)
{
    int insert = keyBinarySearch(node, key);
    insert += 1;
    insert *= -1;

    if (node->count == maxOrder_)
    {
        int splitKey;
        int split = (node->count + 1) / 2;
        pNode sibling = newNoLeaf(tree);
        if (insert < split)
        {
            splitKey = noLeafSplitLeft(tree, node, sibling, left, right, key, split);
            // return buildParentNode(tree, sibling, node, splitKey);
        }
        else if (insert == split)
        {
            // 非叶子节点分裂和叶子节点分裂不同，所以需要两个
            splitKey = noLeafSplitRight(tree, node, sibling, left, right, key, split);
            // return buildParentNode(tree, node, sibling, splitKey);
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
    }

    return S_OK;
}

int BPlusTree::noLeafSplitRight(pTree tree, pNode node, pNode right, pNode lch, pNode rch, int key, int insert)
{
    int split = (maxOrder_ + 1) / 2;
    rightNodeAdd(tree, node, right);
    int splitKey = key(node)[split - 1];

    int pivot = 0;
    node->count = split;
    right->count = maxOrder_ - split + 1;

    key(right)[0] = key;
    subNodeUpdate(tree, right, pivot, lch);
    subNodeUpdate(tree, right, pivot + 1, rch);

    memmove(&key(right)[pivot + 1], &key(node)[split], (right->count - 2) * sizeof(int));
    memmove(&sub(right)[pivot + 2], &sub(node)[split + 1], (right->count - 2) * sizeof(int));

    for (int i = 2; i < right->count; i++)
    {
        subNodeFlush(tree, right, i);
    }

    return S_OK;
}

int BPlusTree::noLeafSplitRight1(pTree tree, pNode node, pNode right, pNode lch, pNode rch, int key, int insert)
{
    int split = (maxOrder_ + 1) / 2;
    rightNodeAdd(tree, node, right);
    int splitKey = key(node)[split];

    int pivot = insert - split - 1;
    node->count = split + 1;
    right->count = maxOrder_ - split;

    // 这里key的位置也是split+1是因为split这个位置的key要被提上去
    memmove(&key(right)[0], &key(node)[split + 1], pivot * sizeof(int));
    memmove(&sub(right)[0], &sub(node)[split + 1], pivot * sizeof(ssize_t));

    key(right)[pivot] = key;

    memmove(&key(right)[pivot + 1], &key(node)[insert], (maxOrder_ - insert - 1) * sizeof(int));
    memmove(&sub(right)[pivot + 2], &sub(node)[insert + 1], (maxOrder_ - insert - 1) * sizeof(ssize_t));

    for (int i = 0; i < right->count; i++)
    {
        if (i != pivot && i != pivot + 1)
        {
            subNodeFlush(tree, right, sub(right)[i]);
        }
    }

    return splitKey;
}

int BPlusTree::subNodeFlush(pTree tree, pNode parent, ssize_t offset)
{
    pNode subNode = getNode(tree, offset);
    subNode->parent = parent->self;
    nodeFlush(tree, subNode);

    return S_OK;
}

int BPlusTree::noLeafSplitLeft(pTree tree, pNode node, pNode right, pNode lch, pNode rch, int key, int split)
{
    int splitKey;

    rightNodeAdd(tree, node, right);
    // leftNodeAdd(tree, node, left);

    // int pivot = insert;
    int insert = keyBinarySearch(node, key);
    // left->count = split;
    node->count = split;
    right->count = maxOrder_ - split;
    splitKey = key(leaf)[split - 1];

    memmove(&key(right)[0], &key(node)[split], (right->count) * sizeof(int));
    memmove(&data(right)[0], &data(node)[split], ((right->count) + 1) * sizeof(ssize_t));

    memmove(&key(node)[insert + 1], &key(node)[insert], (split - insert - 1) * sizeof(int));
    memmove(&data(node)[insert + 2], &data(node)[insert], (split - insert - 1) * sizeof(ssize_t));

    key(node)[insert] = key;
    data(node)[insert + 1] = data;

    //更新子节点
    for (int i = 0; i < right->count; i++)
    {
        subNodeFlush(tree, right, sub(right)[i]);
    }

    // int splitkey;
    return splitkey;
}

int BPlusTree::noLeafSimpleInsert(pTree tree, pNode node, pNode left, pNode right, int key, int insert)
{
    memmove(&key(node)[insert + 1], &key(node)[insert], (node->count - insert - 1) * sizeof(int));
    key(node)[insert] = key;

    memmove(&data(node)[insert + 2], &data(node)[insert + 1], (node->count - insert - 1) * sizeof(ssize_t));
    subNodeUpdate(tree, node, insert, left);
    subNodeUpdate(tree, node, insert + 1, right);

    node->count++;

    return S_OK;
}

// 先建立父子关系，再写到硬盘上
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

int BPlusTree::leafSplitLeft(pTree tree, pNode leaf, pNode right, int key, ssize_t data, int insert)
{

    int split = (leaf->count + 1) / 2;
    rightNodeAdd(tree, leaf, right);

    int pivot = insert;
    leaf->count = split;
    right->count = maxEntries_ - split + 1;

    memmove(&key(right)[0], &key(leaf)[split - 1], (right->count) * sizeof(int));
    memmove(&data(right)[0], &data(leaf)[split - 1], (right->count) * sizeof(ssize_t));

    memmove(&key(leaf)[pivot + 1], &key(leaf)[pivot], (split - pivot - 1) * sizeof(int));
    memmove(&data(leaf)[pivot + 1], &data(leaf)[pivot], (split - pivot - 1) * sizeof(ssize_t));

    key(leaf)[pivot] = key;
    data(leaf)[pivot] = data;

    return key(right)[0];
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

// 把左侧新new的节点融入这颗树
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

ssize_t BPlusTree::append2Tree(pTree tree, pNode node)
{
    if (listEmpty(&tree->freeBlocks))
    {
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

int BPlusTree::listEmpty(struct listHead *head)
{
    return head->next == head;
}

// 从硬盘中把数据读到内存
pNode BPlusTree::getNode(pTree tree, ssize_t offset)
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

pNode BPlusTree::newLeaf(pTree tree)
{
    pNode node = newNode(tree);
    node->type = BPLUS_TREE_LEAF;
    return node;
}

pNode BPlusTree::newNoLeaf(pTree tree)
{
    pNode node = newNode(tree);
    node->type = BPLUS_TREE_NO_LEAF;
    return node;
}

pNode BPlusTree::newNode(pTree tree)
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
ssize_t BPlusTree::search(pTree tree, int key)
{
    pNode node = getNode(tree, tree->root);
    while (node != NULL)
    {
        int i = keyBinarySearch(node, key);
        if (isLeaf(node))
        {
            return (i > 0 ? data(node)[i] : -1);
        }
        else
        {
            if (i >= 0)
            {
                node = getNode(tree, sub(node)[i + 1]);
            }
            else
            {
                i *= -1;
                node = getNode(tree, sub(node)[i]);
            }
        }
    }
}

// 二分搜索
// 应该返回的值是以0开始的
// 如果找到的话返回找到的位置
// 如果未找到的话返回应该插入的位置
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

    return -low - 1;
}