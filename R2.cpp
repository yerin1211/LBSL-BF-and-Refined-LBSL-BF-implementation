#include <Windows.h>  // sleep
#include <algorithm>  // reverse
#include <iostream>
#include <fstream>
#include <string>
#include <bitset>
#include <cmath>
#include <ctime>
#include "node.h"  // 트라이 노드는 여기에서 정의
using namespace std;

#define MAX_PREFIX_L 32           // 최대 prefix 길이

#define CRC_LEN 32                // CRC 길이
#define CRC_INIT "11111111111111111111111111111110"  // crc 초기값 (임의로 설정) string 버전 (길이가 32여야 함!!)
#define POLY "11101101101110001000001100100000"  // CRC polynomial (임의로 설정), 0xED88320 (reverse of  0x04C11DB7) 

#define ALPHA 2  // 블룸필터크기얼마나크게할지정해주는거, 2의 승수여야 함
#define BF_SIZE T * ALPHA  // Bloom filter 크기
#define BF_BIT_SIZE log2(BF_SIZE)  // Bloom filter 인덱스 크기

int nodeNum;  // 총 노드 개수, 해시 함수 개수 결정을 위해 필요함.
int K;        // 해시 함수 개수, K = ln2 * m / n
int T;        // T' = 2^(log2T) ; T = the number of nodes in valid levels in a trie
int minLv = MAX_PREFIX_L;  // prefix node가 존재하는 최소 레벨
int maxLv = 0;             // prefix node가 존재하는 최대 레벨

int hashCnt_LBSL, hashCnt_R2; // 해시 테이블 접근 몇 번 했는지 보려고

/* prefix와 port가 적힌 텍스트 파일을 불러와서 트라이를 만듦 */
void makeTrie(Node* root, fstream& fin) {
    string prefixtmp;  // 프리픽스 저장용 임시변수
    int porttmp;       // 포트번호 저장용 임시변수

    while (!fin.eof()) {
        fin >> prefixtmp >> porttmp;  // 텍스트 파일에서 읽어옴
        Node* n = root;  // 이동시킬 포인터 변수
        string p = "";   // 프리픽스 초기값 지정용 임시변수

        for (auto c : prefixtmp) {  // 문자열에서 문자 하나하나씩 떼서 c에 넣고 검사함 (c : char변수)
            if (c == '0') {  // 0이면 왼쪽으로
                p += "0";
                if (!n->left) {  // 이동해야 되는 위치에 노드가 없으면 하나 만들고 나서 이동
                    n->left = new Node(p);
                }
                n = n->left;  // 왼쪽으로 움직임
            }
            else {  // 1이면 오른쪽으로
                p += "1";
                if (!n->right) {  // 이동해야 되는 위치에 노드가 없으면 하나 만들고 나서 이동
                    n->right = new Node(p);
                }
                n = n->right;  // 오른쪽으로 움직임
            }
        }
        n->port = porttmp; // 삽입할 위치까지 포인터 이동시켰으니까 값 집어넣음
    }
}

/* WBSL 구현을 위한 pre-computation
 * 부모 노드의 port를 빈 자식 노드에 복사함 */
void preComputation(Node* root) {
    if (root->left) {  // 왼쪽 자식이 존재할 경우
        if (root->left->port == -1) {
            root->left->port = root->port;  // 왼쪽 자식의 port가 default이면 자신의 port를 복사해줌
        }
        preComputation(root->left);  // 왼쪽으로 가서 재귀적으로 탐색
    }
    if (root->right) {  // 오른쪽 자식이 존재할 경우
        if (root->right->port == -1) {
            root->right->port = root->port;  // 오른쪽 자식의 port가 default이면 자신의 port를 복사해줌
        }
        preComputation(root->right);  // 오른쪽으로 가서 재귀적으로 탐색
    }
}

/* LBSL 구현을 위한 leaf-pushing
 * 값을 가진 모든 노드가 leaf node에 위치하도록 함 */
void leafPush(Node* root) {
    if (!root->left && !root->right) return;  // leaf 노드일 경우 : 그냥 돌아감

    if (root->port != -1) {  // 차있는노드 : 내 포트번호를 아래쪽으로 push해줘야 함
        //왼쪽
        if (!root->left) {           // 자식 노드가 없으면 새로 만들어서 포트번호 넣어줌
            root->left = new Node(root->prefix + "0", root->port);
        }
        else if (root->left->port == -1) {  // 빈 자식 노드가 있으면 포트번호 넣어줌
            root->left->port = root->port;
        } // 찬 자식 노드에는 아무것도 안함
        //오른쪽
        if (!root->right) {           // 자식 노드가 없으면 새로 만들어서 포트번호 넣어줌
            root->right = new Node(root->prefix + "1", root->port);
        }
        else if (root->right->port == -1) {  // 빈 자식 노드가 있으면 포트번호 넣어줌
            root->right->port = root->port;
        }

        root->port = -1;  // 포트번호 다 push했으니까 내 포트번호 비움
    }

    if (root->left) leafPush(root->left); // 자식 노드 탐색 진행
    if (root->right) leafPush(root->right);
}

/* 노드가 총 몇 개인지 구해줌 */
int countNode(Node* root) {
    if (!root) return 0;
    return countNode(root->left) + countNode(root->right) + 1;
}

/* valid level  구해줌
 * 적어도 하나 이상의 prefix node가 존재하는 level의 최소, 최대 반환
 * 좀 비효율적임.... */
void getValidLevels(Node* root, int& minLevel, int& maxLevel) {
    if (root->port != -1) {
        int tmp = root->prefix.length();
        minLevel = min(tmp, minLevel);
        maxLevel = max(tmp, maxLevel);
    }

    if (root->left) getValidLevels(root->left, minLevel, maxLevel);
    if (root->right) getValidLevels(root->right, minLevel, maxLevel);
}

/* valid level 범위 안에 존재하는 모든 노드의 수 구함 */
int getValidNodes(Node* root, int length = 0) {
    int cnt = 0;
    if (minLv <= length && length <= maxLv) cnt += 1;

    if (root->left) cnt += getValidNodes(root->left, length + 1);
    if (root->right) cnt += getValidNodes(root->right, length + 1);

    return cnt;
}

/* 사용되는 여러 값들 구하자 배열크기같은거
 * 블룸 필터 크기, 해시 함수 개수, valid level min, max 구하기 */
void getSize(Node* root) {
    getValidLevels(root, minLv, maxLv);
    int validNodeCnt = getValidNodes(root);
    T = 1 << (int)ceil(log2(validNodeCnt));  // 2 ^ (log2T)
    K = floor((log(2) * BF_SIZE / countNode(root)) + 0.5);  // ln2 * m / n 반올림-> 해시함수 개수

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


/* CRC 해시함수 (string 사용 버전) */
string CRC32(const string& in) {
    string crc = CRC_INIT;
    string prev;  // 이전crc값 저장

    for (auto c : in) {  // input의 한 bit씩 계산 --> input의 길이만큼 반복
        prev = crc;
        for (int i = 0; i < 32; i++) {
            if (POLY[i] - '0') crc[i] = (c - '0') ^ prev[(i + 31) % 32];  // polynomial이 1인 bit에 input을 xor연산하여 shift
            else crc[i] = prev[(i + 31) % 32];  // poly가 0인 건 그냥 shift만
        }
    }

    return crc;  //길이가 32인 해시 코드를 binary string으로 return
}

/* Bloom filter 프로그래밍
 * 트라이의 모든 노드 정보를 필터에 집어넣음 */
void makeBF(Node* root, bool* BF, const int& k, string a = "") {
    if (a != "") {  // 빈거는거르자
        string crc = CRC32(a);  // 현재 노드의 prefix로 CRC코드를 만듦

        for (int i = 0; i < k; i++) {
            // (crc-인덱스길이)를 k개로 나눠서 거기서부터 인덱스길이만큼을 잘라 인덱스로 사용
            BF[stoi(crc.substr((i * (CRC_LEN - BF_BIT_SIZE) / k), BF_BIT_SIZE), 0, 2)] = 1;
        }
    }

    if (root->left) makeBF(root->left, BF, k, a + '0');
    if (root->right) makeBF(root->right, BF, k, a + '1');
}

/* hash table 만들기 : 변형된 트라이 가지고 해시 테이블 생성
 * makeHT_LBSL은 모든 노드를 해시 테이블에 저장
 * makeHT_R2는 prefix node와 single-child internal node만 해시 테이블에 저장 */
void makeHT_LBSL(Node* root, hashTable* HT) {
    if (root->port != -1) HT[root->prefix.length()].append(1, root->prefix, root->port); // 찬노드
    else HT[root->prefix.length()].append(0, root->prefix, root->port);                  // 빈노드

    if (root->left) makeHT_LBSL(root->left, HT);  // 재귀 탐색 진행
    if (root->right) makeHT_LBSL(root->right, HT);
}

void makeHT_R2(Node* root, hashTable* HT) {
    if (root->port != -1) HT[root->prefix.length()].append(1, root->prefix, root->port); // 찬노드
    else if (root->left && !root->right) HT[root->prefix.length()].append(0, root->prefix, root->port); // single-child internal nodes
    else if (!root->left && root->right) HT[root->prefix.length()].append(0, root->prefix, root->port);

    if (root->left) makeHT_R2(root->left, HT);  // 재귀 탐색 진행
    if (root->right) makeHT_R2(root->right, HT);
}

/* 해시 테이블의 모든 요소 출력 */
void printHT(hashTable* HT) {
    cout << "▶ printHT start\n";
    hashEntry* p;
    for (int i = 1; i <= MAX_PREFIX_L; i++) {
        if (!HT[i].head) continue;  // 비어 있는 리스트는 출력 안 함
        cout << i << endl;
        HT[i].print();
        cout << endl;
    }
    cout << "◀ printHT end\n";
}

/* search */
hashEntry* search_LBSL(const string& in, const bool* BF, const int& k, const hashTable* HT) {
    int i, j, bml = 0;
    int low = minLv, high = maxLv, mid;  // binary search 를 위한 인덱스
    string crc;

    // 블룸 필터를 이용해 best matching length 찾기
    while (low <= high) {
        mid = (low + high) / 2;  // low와 high의 중간지점 length에 대해 검사
        cout << "mid : " << mid << endl;

        crc = CRC32(in.substr(0, mid));  // input 중 앞에서부터 mid-bit만큼 잘라서 crc 돌림
        for (j = 0; j < k; j++) {  // Bloom filter membership test
            // (crc-인덱스길이)를 k개로 나눠서 거기서부터 인덱스길이만큼을 잘라 인덱스로 사용
            if (!BF[stoi(crc.substr((j * (CRC_LEN - BF_BIT_SIZE) / k), BF_BIT_SIZE), 0, 2)]) break;  // negative일 시 루프 중단
        }

        if (j == k) {  // for문에서 break가 발생하지 않았으므로 positive -> 해시 테이블 접근
            cout << "positive: " << mid << endl;

            hashEntry* p = HT[mid].head;
            hashCnt_LBSL++;
            while (p) {  // 길이가 mid인 모든 해시테이블 엔트리 검사
                //Sleep(10);  // Off-chip data에 접근한다고 가정하여 임의로 약간의 딜레이를 줌
                cout << p->prefix << '\t' << p->length << '\t' << p->port << endl;

                if (in.substr(0, mid) == p->prefix) { // true positive : 매치되는 노드를 만남
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
            if (!p) {  // 해시테이블 끝까지 봤는데 맞는게없음 -> false positive
                high = mid - 1;
            }
        }
        else {  // break 발생했으면 negative
            high = mid - 1;  // 탐색범위를 앞쪽반절로
        };
    }
    return 0;  // 탐색을 끝냈는데 맞는게없음 no matching
}

/* search */
hashEntry* search_R2(const string& in, const bool* BF, const int& k, const hashTable* HT) {
    int i, j, bml = 0;
    string crc;

    // 블룸 필터를 이용해 best matching length 찾기
    for (i = minLv; i <= maxLv && i <= in.length(); i++) {
        crc = CRC32(in.substr(0, i));  // input 중 앞에서부터 i-bit만큼 잘라서 crc 돌림

        for (j = 0; j < k; j++) {
            // (crc-인덱스길이)를 k개로 나눠서 거기서부터 인덱스길이만큼을 잘라 인덱스로 사용
            if (!BF[stoi(crc.substr((j * (CRC_LEN - BF_BIT_SIZE) / k), BF_BIT_SIZE), 0, 2)]) break;  // negative일 시 루프 중단
        }
        if (j == k) bml = i; // for문에서 break가 발생하지 않았으므로 positive -> bml 업데이트
        else break; // break 발생했으면 negative니까 루프 중단
    }

    cout << "bml : " << bml << endl;
    while (bml) {
        cout << bml << endl;
        hashEntry* p = HT[bml].head; // 해시 테이블 접근
        hashCnt_R2++;
        while (p) {
            //Sleep(10);  // Off-chip data에 접근한다고 가정하여 임의로 약간의 딜레이를 줌
            cout << p->prefix << '\t' << p->length << '\t' << p->port << endl;

            if (in.substr(0, bml) == p->prefix) { // type : prefix node이면 1, internal node이면 0
                if (p->port != -1) return p;  // 일치하는 prefix가 있음
                else return 0;          // 일치하는 prefix가 없음
            }
            p = p->next;
        }
        bml--;
    }
    cout << "error\n"; // 여기까지 안 옴 안와야됨...
    return 0;  
}

/* Main */
int main() {
    clock_t start[5], finish[5];
    start[0] = clock();

    fstream fin("./in.txt", ios::in);  // 삽입할 노드들의 정보가 들어있는 텍스트 파일
    if (!fin) cout << "fin open failed.\n";

    Node* root = new Node();  // 트라이 생성

    makeTrie(root, fin);
    leafPush(root);
    getSize(root);  // 블룸 필터 크기, 해시 함수 개수, valid level min, max 구하기
    bool* BF = new bool[BF_SIZE] {};  // 블룸 필터 생성
    hashTable HT_R2[MAX_PREFIX_L + 1];  // 프리픽스 길이 하나 당 연결리스트 하나씩
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
    // 일치하는 prefix가 있다면 res에 정보 들어옴, 아니면 nullptr
    hashEntry* res = search_R2(addr, BF, K, HT_R2);  
    if (res) cout << "result : " << res->port << endl;
    else cout << "No matching prefix\n";
    cout << "해시 테이블 접근횟수 : " << hashCnt_R2 << endl;
    cout << "=========================\n";
    finish[1] = clock();

    cout << endl;

    start[2] = clock();
    cout << "=======search_LBSL=======\n";
    hashCnt_LBSL = 0;
    hashEntry* res2 = search_LBSL(addr, BF, K, HT_LBSL);
    if (res2) cout << "result : " << res2->port << endl;
    else cout << "No matching prefix\n";
    cout << "해시 테이블 접근횟수 : " << hashCnt_LBSL << endl;
    cout << "=========================\n";
    finish[2] = clock();

    //예시 데이터는 너무 작아 유의미한 시간 차이가 보이지 않음
    cout << "\n\n";
    cout << "programming : " << finish[0] - start[0] << "ns\n";
    cout << "search_R2   : " << finish[1] - start[1] << "ns\n";
    cout << "search_LBSL : " << finish[2] - start[2] << "ns\n";
}