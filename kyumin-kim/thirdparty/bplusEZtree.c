/*
 * bplusEZtree.c
 *
 * 읽기 위한 학습용 B+Tree다. 기존 bplustree.c처럼 파일 저장, offset,
 * cache, boot 파일을 다루지 않는다. 교과서 그림처럼 보이도록 메모리
 * 포인터와 배열만 사용한다.
 *
 * 핵심 규칙:
 * - internal node는 길 안내만 한다.
 * - 실제 value는 leaf node에만 저장한다.
 * - leaf node끼리는 next 포인터로 linked list처럼 연결된다.
 * - delete는 일부러 생략했다. insert/search/range scan만 본다.
 *
 * 단독 실행:
 *   cc -std=c11 -Wall -Wextra -pedantic -DBPLUS_SIMPLE_DEMO thirdparty/bplusEZtree.c -o /tmp/bplus_simple
 *   /tmp/bplus_simple
 */

#include <stdio.h>
#include <stdlib.h>

/*
 * order 4:
 * - internal node는 child를 최대 4개 가진다.
 * - key는 최대 3개 가진다.
 * - insert 중 split 직전에는 잠깐 key 4개까지 들어갈 수 있다.
 */
#define ORDER 4
#define MAX_KEYS (ORDER - 1)

typedef struct Node {
    int is_leaf;
    int key_count;
    int keys[MAX_KEYS + 1];
    long values[MAX_KEYS + 1];
    struct Node *children[ORDER + 1];
    struct Node *next;
} Node;

typedef struct {
    Node *root;
} Tree;

typedef struct {
    int happened;
    int key_for_parent;
    Node *right;
} Split;

static Node *new_node(int is_leaf);
static void free_node(Node *node);
static int lower_bound(const Node *node, int key);
static int child_slot(const Node *node, int key);
static Node *find_leaf(const Tree *tree, int key);
static Node *leftmost_leaf(const Tree *tree);
static void insert_into_leaf(Node *leaf, int key, long value);
static Split split_leaf(Node *leaf);
static void insert_child(Node *parent, int slot, Split child_split);
static Split split_internal(Node *node);
static Split insert_recursive(Node *node, int key, long value);
static void dump_node(const Node *node, int depth);

/* 빈 tree를 만든다. */
Tree *tree_new(void)
{
    return calloc(1, sizeof(Tree));
}

/* tree 전체를 메모리에서 해제한다. */
void tree_free(Tree *tree)
{
    if (tree == NULL) {
        return;
    }

    free_node(tree->root);
    free(tree);
}

/* key/value를 넣는다. 이미 있는 key면 value만 바꾼다. */
void tree_insert(Tree *tree, int key, long value)
{
    Split split;

    if (tree == NULL) {
        return;
    }

    if (tree->root == NULL) {
        tree->root = new_node(1);
    }

    split = insert_recursive(tree->root, key, value);
    if (split.happened) {
        Node *new_root = new_node(0);

        new_root->keys[0] = split.key_for_parent;
        new_root->children[0] = tree->root;
        new_root->children[1] = split.right;
        new_root->key_count = 1;
        tree->root = new_root;
    }
}

/* key 하나를 찾는다. 없으면 -1을 반환한다. */
long search(const Tree *tree, int key)
{
    Node *leaf = find_leaf(tree, key);
    int index;

    if (leaf == NULL) {
        return -1;
    }

    index = lower_bound(leaf, key);
    if (index < leaf->key_count && leaf->keys[index] == key) {
        return leaf->values[index];
    }

    return -1;
}

/* start_key부터 end_key까지 leaf linked list를 따라가며 value를 모은다. */
int scan_range(const Tree *tree, int start_key, int end_key, long *out, int out_size)
{
    Node *leaf;
    int index;
    int count = 0;

    if (tree == NULL || start_key > end_key || out_size < 0 || (out_size > 0 && out == NULL)) {
        return -1;
    }

    leaf = find_leaf(tree, start_key);
    if (leaf == NULL || out_size == 0) {
        return 0;
    }

    index = lower_bound(leaf, start_key);
    while (leaf != NULL && count < out_size) {
        while (index < leaf->key_count && count < out_size) {
            if (leaf->keys[index] > end_key) {
                return count;
            }
            out[count++] = leaf->values[index++];
        }

        leaf = leaf->next;
        index = 0;
    }

    return count;
}

/* start_key가 들어갈 leaf부터 leaf_count개 leaf를 next로 따라가며 value를 모은다. */
int scan_leafs_from(const Tree *tree, int start_key, int leaf_count, long *out, int out_size)
{
    Node *leaf;
    int index;
    int count = 0;

    if (tree == NULL || leaf_count <= 0 || out_size < 0 || (out_size > 0 && out == NULL)) {
        return -1;
    }

    leaf = find_leaf(tree, start_key);
    if (leaf == NULL || out_size == 0) {
        return 0;
    }

    index = lower_bound(leaf, start_key);
    for (int visited = 0; leaf != NULL && visited < leaf_count && count < out_size; visited++) {
        while (index < leaf->key_count && count < out_size) {
            out[count++] = leaf->values[index++];
        }

        leaf = leaf->next;
        index = 0;
    }

    return count;
}

/* leaf linked list만 한 줄로 출력한다. */
void print_leaves(const Tree *tree)
{
    Node *leaf = leftmost_leaf(tree);

    printf("leaves: ");
    while (leaf != NULL) {
        printf("[");
        for (int i = 0; i < leaf->key_count; i++) {
            printf("%s%d", i == 0 ? "" : " ", leaf->keys[i]);
        }
        printf("]");
        if (leaf->next != NULL) {
            printf(" -> ");
        }
        leaf = leaf->next;
    }
    printf("\n");
}

/* tree 모양을 들여쓰기 형태로 출력한다. */
void dump_tree(const Tree *tree)
{
    if (tree == NULL || tree->root == NULL) {
        printf("(empty)\n");
        return;
    }

    dump_node(tree->root, 0);
}

/* node 하나를 만든다. */
static Node *new_node(int is_leaf)
{
    Node *node = calloc(1, sizeof(*node));
    if (node != NULL) {
        node->is_leaf = is_leaf;
    }
    return node;
}

/* node와 자식 node들을 지운다. */
static void free_node(Node *node)
{
    if (node == NULL) {
        return;
    }

    if (!node->is_leaf) {
        for (int i = 0; i <= node->key_count; i++) {
            free_node(node->children[i]);
        }
    }

    free(node);
}

/* key가 node->keys 안에서 들어갈 첫 위치를 찾는다. */
static int lower_bound(const Node *node, int key)
{
    int index = 0;

    while (index < node->key_count && node->keys[index] < key) {
        index++;
    }

    return index;
}

/* internal node에서 key가 내려갈 child 번호를 찾는다. */
static int child_slot(const Node *node, int key)
{
    int slot = 0;

    while (slot < node->key_count && key >= node->keys[slot]) {
        slot++;
    }

    return slot;
}

/* root부터 내려가 key가 들어갈 leaf를 찾는다. */
static Node *find_leaf(const Tree *tree, int key)
{
    Node *node;

    if (tree == NULL) {
        return NULL;
    }

    node = tree->root;
    while (node != NULL && !node->is_leaf) {
        node = node->children[child_slot(node, key)];
    }

    return node;
}

/* leaf linked list의 시작점인 가장 왼쪽 leaf를 찾는다. */
static Node *leftmost_leaf(const Tree *tree)
{
    Node *node;

    if (tree == NULL) {
        return NULL;
    }

    node = tree->root;
    while (node != NULL && !node->is_leaf) {
        node = node->children[0];
    }

    return node;
}

/* leaf에 key/value를 정렬 순서대로 넣는다. */
static void insert_into_leaf(Node *leaf, int key, long value)
{
    int index = lower_bound(leaf, key);

    if (index < leaf->key_count && leaf->keys[index] == key) {
        leaf->values[index] = value;
        return;
    }

    for (int i = leaf->key_count; i > index; i--) {
        leaf->keys[i] = leaf->keys[i - 1];
        leaf->values[i] = leaf->values[i - 1];
    }

    leaf->keys[index] = key;
    leaf->values[index] = value;
    leaf->key_count++;
}

/*
 * leaf split:
 * - 왼쪽 leaf와 오른쪽 leaf가 value를 나눠 가진다.
 * - 부모로 올릴 key는 오른쪽 leaf의 첫 key다.
 * - leaf끼리는 next로 이어 둔다.
 */
static Split split_leaf(Node *leaf)
{
    Split split = {0};
    Node *right = new_node(1);
    int mid = leaf->key_count / 2;

    for (int i = mid; i < leaf->key_count; i++) {
        int right_index = i - mid;
        right->keys[right_index] = leaf->keys[i];
        right->values[right_index] = leaf->values[i];
        right->key_count++;
    }

    leaf->key_count = mid;
    right->next = leaf->next;
    leaf->next = right;

    split.happened = 1;
    split.key_for_parent = right->keys[0];
    split.right = right;
    return split;
}

/* 자식 split 결과를 parent의 key/child 배열에 끼워 넣는다. */
static void insert_child(Node *parent, int slot, Split child_split)
{
    for (int i = parent->key_count; i > slot; i--) {
        parent->keys[i] = parent->keys[i - 1];
    }

    for (int i = parent->key_count + 1; i > slot + 1; i--) {
        parent->children[i] = parent->children[i - 1];
    }

    parent->keys[slot] = child_split.key_for_parent;
    parent->children[slot + 1] = child_split.right;
    parent->key_count++;
}

/*
 * internal split:
 * - 가운데 key는 부모로 올라간다.
 * - 그 key는 왼쪽/오른쪽 internal node 안에는 남지 않는다.
 */
static Split split_internal(Node *node)
{
    Split split = {0};
    Node *right = new_node(0);
    int mid = node->key_count / 2;
    int right_key_count = node->key_count - mid - 1;

    split.happened = 1;
    split.key_for_parent = node->keys[mid];
    split.right = right;

    for (int i = 0; i < right_key_count; i++) {
        right->keys[i] = node->keys[mid + 1 + i];
    }

    for (int i = 0; i <= right_key_count; i++) {
        right->children[i] = node->children[mid + 1 + i];
    }

    right->key_count = right_key_count;
    node->key_count = mid;
    return split;
}

/* 재귀적으로 insert하고, split이 생기면 부모에게 알려준다. */
static Split insert_recursive(Node *node, int key, long value)
{
    Split split = {0};

    if (node->is_leaf) {
        insert_into_leaf(node, key, value);
        if (node->key_count > MAX_KEYS) {
            return split_leaf(node);
        }
        return split;
    }

    int slot = child_slot(node, key);
    Split child_split = insert_recursive(node->children[slot], key, value);

    if (!child_split.happened) {
        return split;
    }

    insert_child(node, slot, child_split);
    if (node->key_count > MAX_KEYS) {
        return split_internal(node);
    }

    return split;
}

/* tree를 사람이 읽기 좋게 출력한다. */
static void dump_node(const Node *node, int depth)
{
    for (int i = 0; i < depth; i++) {
        printf("  ");
    }

    printf("%s:", node->is_leaf ? "leaf" : "internal");
    for (int i = 0; i < node->key_count; i++) {
        printf(" %d", node->keys[i]);
    }
    printf("\n");

    if (!node->is_leaf) {
        for (int i = 0; i <= node->key_count; i++) {
            dump_node(node->children[i], depth + 1);
        }
    }
}

#ifdef BPLUS_SIMPLE_DEMO
int main(void)
{
    Tree *tree = tree_new();
    long out[10];

    for (int id = 1; id <= 20; id++) {
        tree_insert(tree, id, id * 64L);
    }

    dump_tree(tree);
    print_leaves(tree);

    printf("search 7 = %ld\n", search(tree, 7));

    int count = scan_range(tree, 6, 12, out, 10);
    printf("range 6..12:");
    for (int i = 0; i < count; i++) {
        printf(" %ld", out[i]);
    }
    printf("\n");

    count = scan_leafs_from(tree, 6, 2, out, 10);
    printf("2 leafs from 6:");
    for (int i = 0; i < count; i++) {
        printf(" %ld", out[i]);
    }
    printf("\n");

    tree_free(tree);
    return 0;
}
#endif
