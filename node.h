#pragma once
#include <iostream>
#include <string>
using namespace std;

class Node {
public:
    string prefix; // �����Ƚ� �����
    int port;      // ��Ʈ��ȣ �����
    Node* left;
    Node* right;

    Node(string prefix = "", int port = -1) {  // port�� -1�̸� �� ����� ��
        this->prefix = prefix;
        this->port = port;
        left = NULL;
        right = NULL;
    }

    // �ش� ����� ��� ����Ʈ�� ��� (root���� ȣ�� �� ��� ��� ���)
    void print(string a = "") {
        cout << a << "\t" << this->prefix << "*\t(" << this->port << ")\n";
        if (this->left != NULL) this->left->print(a + "0");
        if (this->right != NULL) this->right->print(a + "1");
    }
};

class hashEntry {
public:
    bool type;  // 1�̸� prefix node, 0�̸� internal node
    string prefix;
    int length;
    int port;
    hashEntry* next;  // ���� ����Ʈ�� ���� ��带 ����Ŵ

    hashEntry(bool type, string prefix = "", int port = -1, hashEntry* next = NULL) {
        this->type = type;
        this->prefix = prefix;
        this->port = port;
        this->length = prefix.length();
        this->next = next;
    }

};

class hashTable {
public:
    hashEntry* head = NULL;

    void append(bool type, string prefix, int port) {  // ��� �߰�
        this->head = new hashEntry(type, prefix, port, this->head);
    }

    void print() {
        hashEntry* p = this->head;
        while (p) {
            cout << p->type << '\t' << p->prefix << '\t' << p->port << endl;
            p = p->next;
        }
    }
};