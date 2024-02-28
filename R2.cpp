#include <Windows.h>  // sleep
#include <algorithm>  // reverse
#include <iostream>
#include <fstream>
#include <string>
#include <bitset>
#include <cmath>
#include <ctime>
#include "node.h"  // Ʈ���� ���� ���⿡�� ����
using namespace std;

#define MAX_PREFIX_L 32           // �ִ� prefix ����

#define CRC_LEN 32                // CRC ����
#define CRC_INIT "11111111111111111111111111111110"  // crc �ʱⰪ (���Ƿ� ����) string ���� (���̰� 32���� ��!!)
#define POLY "11101101101110001000001100100000"  // CRC polynomial (���Ƿ� ����), 0xED88320 (reverse of  0x04C11DB7) 

#define ALPHA 2  // �������ũ��󸶳�ũ�����������ִ°�, 2�� �¼����� ��
#define BF_SIZE T * ALPHA  // Bloom filter ũ��
#define BF_BIT_SIZE log2(BF_SIZE)  // Bloom filter �ε��� ũ��

int nodeNum;  // �� ��� ����, �ؽ� �Լ� ���� ������ ���� �ʿ���.
int K;        // �ؽ� �Լ� ����, K = ln2 * m / n
int T;        // T' = 2^(log2T) ; T = the number of nodes in valid levels in a trie
int minLv = MAX_PREFIX_L;  // prefix node�� �����ϴ� �ּ� ����
int maxLv = 0;             // prefix node�� �����ϴ� �ִ� ����

int hashCnt_LBSL, hashCnt_R2; // �ؽ� ���̺� ���� �� �� �ߴ��� ������

/* prefix�� port�� ���� �ؽ�Ʈ ������ �ҷ��ͼ� Ʈ���̸� ���� */
void makeTrie(Node* root, fstream& fin) {
    string prefixtmp;  // �����Ƚ� ����� �ӽú���
    int porttmp;       // ��Ʈ��ȣ ����� �ӽú���

    while (!fin.eof()) {
        fin >> prefixtmp >> porttmp;  // �ؽ�Ʈ ���Ͽ��� �о��
        Node* n = root;  // �̵���ų ������ ����
        string p = "";   // �����Ƚ� �ʱⰪ ������ �ӽú���

        for (auto c : prefixtmp) {  // ���ڿ����� ���� �ϳ��ϳ��� ���� c�� �ְ� �˻��� (c : char����)
            if (c == '0') {  // 0�̸� ��������
                p += "0";
                if (!n->left) {  // �̵��ؾ� �Ǵ� ��ġ�� ��尡 ������ �ϳ� ����� ���� �̵�
                    n->left = new Node(p);
                }
                n = n->left;  // �������� ������
            }
            else {  // 1�̸� ����������
                p += "1";
                if (!n->right) {  // �̵��ؾ� �Ǵ� ��ġ�� ��尡 ������ �ϳ� ����� ���� �̵�
                    n->right = new Node(p);
                }
                n = n->right;  // ���������� ������
            }
        }
        n->port = porttmp; // ������ ��ġ���� ������ �̵��������ϱ� �� �������
    }
}

/* WBSL ������ ���� pre-computation
 * �θ� ����� port�� �� �ڽ� ��忡 ������ */
void preComputation(Node* root) {
    if (root->left) {  // ���� �ڽ��� ������ ���
        if (root->left->port == -1) {
            root->left->port = root->port;  // ���� �ڽ��� port�� default�̸� �ڽ��� port�� ��������
        }
        preComputation(root->left);  // �������� ���� ��������� Ž��
    }
    if (root->right) {  // ������ �ڽ��� ������ ���
        if (root->right->port == -1) {
            root->right->port = root->port;  // ������ �ڽ��� port�� default�̸� �ڽ��� port�� ��������
        }
        preComputation(root->right);  // ���������� ���� ��������� Ž��
    }
}

/* LBSL ������ ���� leaf-pushing
 * ���� ���� ��� ��尡 leaf node�� ��ġ�ϵ��� �� */
void leafPush(Node* root) {
    if (!root->left && !root->right) return;  // leaf ����� ��� : �׳� ���ư�

    if (root->port != -1) {  // ���ִ³�� : �� ��Ʈ��ȣ�� �Ʒ������� push����� ��
        //����
        if (!root->left) {           // �ڽ� ��尡 ������ ���� ���� ��Ʈ��ȣ �־���
            root->left = new Node(root->prefix + "0", root->port);
        }
        else if (root->left->port == -1) {  // �� �ڽ� ��尡 ������ ��Ʈ��ȣ �־���
            root->left->port = root->port;
        } // �� �ڽ� ��忡�� �ƹ��͵� ����
        //������
        if (!root->right) {           // �ڽ� ��尡 ������ ���� ���� ��Ʈ��ȣ �־���
            root->right = new Node(root->prefix + "1", root->port);
        }
        else if (root->right->port == -1) {  // �� �ڽ� ��尡 ������ ��Ʈ��ȣ �־���
            root->right->port = root->port;
        }

        root->port = -1;  // ��Ʈ��ȣ �� push�����ϱ� �� ��Ʈ��ȣ ���
    }

    if (root->left) leafPush(root->left); // �ڽ� ��� Ž�� ����
    if (root->right) leafPush(root->right);
}

/* ��尡 �� �� ������ ������ */
int countNode(Node* root) {
    if (!root) return 0;
    return countNode(root->left) + countNode(root->right) + 1;
}

/* valid level  ������
 * ��� �ϳ� �̻��� prefix node�� �����ϴ� level�� �ּ�, �ִ� ��ȯ
 * �� ��ȿ������.... */
void getValidLevels(Node* root, int& minLevel, int& maxLevel) {
    if (root->port != -1) {
        int tmp = root->prefix.length();
        minLevel = min(tmp, minLevel);
        maxLevel = max(tmp, maxLevel);
    }

    if (root->left) getValidLevels(root->left, minLevel, maxLevel);
    if (root->right) getValidLevels(root->right, minLevel, maxLevel);
}

/* valid level ���� �ȿ� �����ϴ� ��� ����� �� ���� */
int getValidNodes(Node* root, int length = 0) {
    int cnt = 0;
    if (minLv <= length && length <= maxLv) cnt += 1;

    if (root->left) cnt += getValidNodes(root->left, length + 1);
    if (root->right) cnt += getValidNodes(root->right, length + 1);

    return cnt;
}

/* ���Ǵ� ���� ���� ������ �迭ũ�ⰰ����
 * ��� ���� ũ��, �ؽ� �Լ� ����, valid level min, max ���ϱ� */
void getSize(Node* root) {
    getValidLevels(root, minLv, maxLv);
    int validNodeCnt = getValidNodes(root);
    T = 1 << (int)ceil(log2(validNodeCnt));  // 2 ^ (log2T)
    K = floor((log(2) * BF_SIZE / countNode(root)) + 0.5);  // ln2 * m / n �ݿø�-> �ؽ��Լ� ����

    /*
    cout << "K = " << K << endl;
    cout << "T = " << T << endl;
    cout << "validNodeCnt = " << validNodeCnt << endl;
    cout << "minLv = " << minLv << endl;
    cout << "maxLv = " << maxLv << endl;
    cout << "BF_SIZE = " << BF_SIZE << endl;
    cout << "BF_BIT_SIZE = " << BF_BIT_SIZE << endl;
    cout << "====================\n";
    */
}


/* CRC �ؽ��Լ� (string ��� ����) */
string CRC32(const string& in) {
    string crc = CRC_INIT;
    string prev;  // ����crc�� ����

    for (auto c : in) {  // input�� �� bit�� ��� --> input�� ���̸�ŭ �ݺ�
        prev = crc;
        for (int i = 0; i < 32; i++) {
            if (POLY[i] - '0') crc[i] = (c - '0') ^ prev[(i + 31) % 32];  // polynomial�� 1�� bit�� input�� xor�����Ͽ� shift
            else crc[i] = prev[(i + 31) % 32];  // poly�� 0�� �� �׳� shift��
        }
    }

    return crc;  //���̰� 32�� �ؽ� �ڵ带 binary string���� return
}

/* Bloom filter ���α׷���
 * Ʈ������ ��� ��� ������ ���Ϳ� ������� */
void makeBF(Node* root, bool* BF, const int& k, string a = "") {
    if (a != "") {  // ��Ŵ°Ÿ���
        string crc = CRC32(a);  // ���� ����� prefix�� CRC�ڵ带 ����

        for (int i = 0; i < k; i++) {
            // (crc-�ε�������)�� k���� ������ �ű⼭���� �ε������̸�ŭ�� �߶� �ε����� ���
            BF[stoi(crc.substr((i * (CRC_LEN - BF_BIT_SIZE) / k), BF_BIT_SIZE), 0, 2)] = 1;
        }
    }

    if (root->left) makeBF(root->left, BF, k, a + '0');
    if (root->right) makeBF(root->right, BF, k, a + '1');
}

/* hash table ����� : ������ Ʈ���� ������ �ؽ� ���̺� ����
 * makeHT_LBSL�� ��� ��带 �ؽ� ���̺� ����
 * makeHT_R2�� prefix node�� single-child internal node�� �ؽ� ���̺� ���� */
void makeHT_LBSL(Node* root, hashTable* HT) {
    if (root->port != -1) HT[root->prefix.length()].append(1, root->prefix, root->port); // �����
    else HT[root->prefix.length()].append(0, root->prefix, root->port);                  // ����

    if (root->left) makeHT_LBSL(root->left, HT);  // ��� Ž�� ����
    if (root->right) makeHT_LBSL(root->right, HT);
}

void makeHT_R2(Node* root, hashTable* HT) {
    if (root->port != -1) HT[root->prefix.length()].append(1, root->prefix, root->port); // �����
    else if (root->left && !root->right) HT[root->prefix.length()].append(0, root->prefix, root->port); // single-child internal nodes
    else if (!root->left && root->right) HT[root->prefix.length()].append(0, root->prefix, root->port);

    if (root->left) makeHT_R2(root->left, HT);  // ��� Ž�� ����
    if (root->right) makeHT_R2(root->right, HT);
}

/* �ؽ� ���̺��� ��� ��� ��� */
void printHT(hashTable* HT) {
    cout << "�� printHT start\n";
    hashEntry* p;
    for (int i = 1; i <= MAX_PREFIX_L; i++) {
        if (!HT[i].head) continue;  // ��� �ִ� ����Ʈ�� ��� �� ��
        cout << i << endl;
        HT[i].print();
        cout << endl;
    }
    cout << "�� printHT end\n";
}

/* search */
hashEntry* search_LBSL(const string& in, const bool* BF, const int& k, const hashTable* HT) {
    int i, j, bml = 0;
    int low = minLv, high = maxLv, mid;  // binary search �� ���� �ε���
    string crc;

    // ��� ���͸� �̿��� best matching length ã��
    while (low <= high) {
        mid = (low + high) / 2;  // low�� high�� �߰����� length�� ���� �˻�
        cout << "mid : " << mid << endl;

        crc = CRC32(in.substr(0, mid));  // input �� �տ������� mid-bit��ŭ �߶� crc ����
        for (j = 0; j < k; j++) {  // Bloom filter membership test
            // (crc-�ε�������)�� k���� ������ �ű⼭���� �ε������̸�ŭ�� �߶� �ε����� ���
            if (!BF[stoi(crc.substr((j * (CRC_LEN - BF_BIT_SIZE) / k), BF_BIT_SIZE), 0, 2)]) break;  // negative�� �� ���� �ߴ�
        }

        if (j == k) {  // for������ break�� �߻����� �ʾ����Ƿ� positive -> �ؽ� ���̺� ����
            cout << "positive: " << mid << endl;

            hashEntry* p = HT[mid].head;
            hashCnt_LBSL++;
            while (p) {  // ���̰� mid�� ��� �ؽ����̺� ��Ʈ�� �˻�
                //Sleep(10);  // Off-chip data�� �����Ѵٰ� �����Ͽ� ���Ƿ� �ణ�� �����̸� ��
                cout << p->prefix << '\t' << p->length << '\t' << p->port << endl;

                if (in.substr(0, mid) == p->prefix) { // true positive : ��ġ�Ǵ� ��带 ����
                    if (p->port != -1) { // prefix node
                        cout << "bml : " << p->length << endl;
                        return p;
                    }
                    else { // internal node
                        low = mid + 1;
                        break;
                    }
                }
                p = p->next;
            }
            if (!p) {  // �ؽ����̺� ������ �ôµ� �´°Ծ��� -> false positive
                high = mid - 1;
            }
        }
        else {  // break �߻������� negative
            high = mid - 1;  // Ž�������� ���ʹ�����
        };
    }
    return 0;  // Ž���� ���´µ� �´°Ծ��� no matching
}

/* search */
hashEntry* search_R2(const string& in, const bool* BF, const int& k, const hashTable* HT) {
    int i, j, bml = 0;
    string crc;

    // ��� ���͸� �̿��� best matching length ã��
    for (i = minLv; i <= maxLv && i <= in.length(); i++) {
        crc = CRC32(in.substr(0, i));  // input �� �տ������� i-bit��ŭ �߶� crc ����

        for (j = 0; j < k; j++) {
            // (crc-�ε�������)�� k���� ������ �ű⼭���� �ε������̸�ŭ�� �߶� �ε����� ���
            if (!BF[stoi(crc.substr((j * (CRC_LEN - BF_BIT_SIZE) / k), BF_BIT_SIZE), 0, 2)]) break;  // negative�� �� ���� �ߴ�
        }
        if (j == k) bml = i; // for������ break�� �߻����� �ʾ����Ƿ� positive -> bml ������Ʈ
        else break; // break �߻������� negative�ϱ� ���� �ߴ�
    }

    cout << "bml : " << bml << endl;
    while (bml) {
        cout << bml << endl;
        hashEntry* p = HT[bml].head; // �ؽ� ���̺� ����
        hashCnt_R2++;
        while (p) {
            //Sleep(10);  // Off-chip data�� �����Ѵٰ� �����Ͽ� ���Ƿ� �ణ�� �����̸� ��
            cout << p->prefix << '\t' << p->length << '\t' << p->port << endl;

            if (in.substr(0, bml) == p->prefix) { // type : prefix node�̸� 1, internal node�̸� 0
                if (p->port != -1) return p;  // ��ġ�ϴ� prefix�� ����
                else return 0;          // ��ġ�ϴ� prefix�� ����
            }
            p = p->next;
        }
        bml--;
    }
    cout << "error\n"; // ������� �� �� �ȿ;ߵ�...
    return 0;  
}

/* Main */
int main() {
    clock_t start[5], finish[5];
    start[0] = clock();

    fstream fin("./in.txt", ios::in);  // ������ ������ ������ ����ִ� �ؽ�Ʈ ����
    if (!fin) cout << "fin open failed.\n";

    Node* root = new Node();  // Ʈ���� ����

    makeTrie(root, fin);
    leafPush(root);
    getSize(root);  // ��� ���� ũ��, �ؽ� �Լ� ����, valid level min, max ���ϱ�
    bool* BF = new bool[BF_SIZE] {};  // ��� ���� ����
    hashTable HT_R2[MAX_PREFIX_L + 1];  // �����Ƚ� ���� �ϳ� �� ���Ḯ��Ʈ �ϳ���
    hashTable HT_LBSL[MAX_PREFIX_L + 1];

    makeBF(root, BF, K);
    makeHT_R2(root, HT_R2);
    makeHT_LBSL(root, HT_LBSL); 

    finish[0] = clock();
    
    /////////////////////////////////////search
    string addr = "110110";

    start[1] = clock();
    cout << "========search_R2========\n";
    hashCnt_R2 = 0;
    // ��ġ�ϴ� prefix�� �ִٸ� res�� ���� ����, �ƴϸ� nullptr
    hashEntry* res = search_R2(addr, BF, K, HT_R2);  
    if (res) cout << "result : " << res->port << endl;
    else cout << "No matching prefix\n";
    cout << "�ؽ� ���̺� ����Ƚ�� : " << hashCnt_R2 << endl;
    cout << "=========================\n";
    finish[1] = clock();

    cout << endl;

    start[2] = clock();
    cout << "=======search_LBSL=======\n";
    hashCnt_LBSL = 0;
    hashEntry* res2 = search_LBSL(addr, BF, K, HT_LBSL);
    if (res2) cout << "result : " << res2->port << endl;
    else cout << "No matching prefix\n";
    cout << "�ؽ� ���̺� ����Ƚ�� : " << hashCnt_LBSL << endl;
    cout << "=========================\n";
    finish[2] = clock();

    //���� �����ʹ� �ʹ� �۾� ���ǹ��� �ð� ���̰� ������ ����
    cout << "\n\n";
    cout << "programming : " << finish[0] - start[0] << "ns\n";
    cout << "search_R2   : " << finish[1] - start[1] << "ns\n";
    cout << "search_LBSL : " << finish[2] - start[2] << "ns\n";
}