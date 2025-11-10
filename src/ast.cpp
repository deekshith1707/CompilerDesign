#include "ast.h"
#include <cstdlib>
#include <cstring>

extern int yylineno;

TreeNode* createNode(NodeType type, const char* value) {
    TreeNode* node = static_cast<TreeNode*>(malloc(sizeof(TreeNode)));
    node->type = type;
    node->value = value ? strdup(value) : nullptr;
    node->dataType = nullptr;
    node->isLValue = 0;
    node->children = nullptr;
    node->childCount = 0;
    node->childCapacity = 0;
    node->lineNumber = yylineno;
    return node;
}

void addChild(TreeNode* parent, TreeNode* child) {
    if (!parent || !child) return;
    if (parent->childCount >= parent->childCapacity) {
        parent->childCapacity = parent->childCapacity == 0 ? 4 : parent->childCapacity * 2;
        parent->children = static_cast<TreeNode**>(realloc(parent->children,
                                               parent->childCapacity * sizeof(TreeNode*)));
    }
    parent->children[parent->childCount++] = child;
}

void freeNode(TreeNode* node) {
    if (!node) return;
    if (node->value) free(node->value);
    if (node->dataType) free(node->dataType);
    
    for (int i = 0; i < node->childCount; i++) {
        freeNode(node->children[i]);
    }
    if (node->children) free(node->children);
    free(node);
}