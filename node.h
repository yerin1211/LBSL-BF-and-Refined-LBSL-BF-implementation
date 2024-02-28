#pragma once
#include <iostream>
#include <string>
using namespace std;

class Node {
public:
    string prefix; // 프리픽스 저장용
    int port;      // 포트번호 저장용
    Node* left;
    Node* right;

    Node(string prefix = "", int port = -1) {  // port가 -1이면 빈 노드라는 뜻
        this->prefix = prefix;
        this->port = port;
        left = NULL;
        right = NULL;
    }

    // 해당 노드의 모든 서브트리 출력 (root에서 호출 시 모든 노드 출력)
    void print(string a = "") {
        cout << a << "\t" << this->prefix << "*\t(" << this->port << ")\n";
        if (this->left != NULL) this->left->print(a + "0");
        if (this->right != NULL) this->right->print(a + "1");
    }
};

class hashEntry {
public:
    bool type;  // 1이면 prefix node, 0이면 internal node
    string prefix;
    int length;
    int port;
    hashEntry* next;  // 연결 리스트의 다음 노드를 가리킴

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

    void append(bool type, string prefix, int port) {  // 요소 추가
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