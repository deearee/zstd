#include <stdio.h>
#include "common/fse.h"
#include "common/error_private.h"
#include "common/mem.h"

#define ZDICT_STATIC_LINKING_ONLY
#include "ANSToolkit.cpp"
#include "common/zstd_internal.h"

#define MAX_SYMBOLS 4096
#define MAX_TABLE_LOG 20

BYTE scratchBuffer[FSE_BUILD_DTABLE_WKSP_SIZE(MAX_TABLE_LOG, MAX_SYMBOLS)] = {};
FSE_CTable dtable[FSE_DTABLE_SIZE_U32(MAX_TABLE_LOG)] = {};

void swap(unsigned *a, unsigned *b) {
    int temp = *a;
    *a = *b;
    *b = temp;
}

void shuffle(unsigned* array, int n) {
    for (int i = n - 1; i > 0; i--) {
        int j = rand() % (i + 1);
        swap(&array[i], &array[j]);
    }
}

void printStats(int lowProb) {
    const size_t numSymbols = 256;

    short norm[numSymbols];
    unsigned count[numSymbols];
    unsigned const tableLog = 12;
    size_t total = 0;
    unsigned const maxSymbolValue = numSymbols - 1;
    size_t i;

    srand(1);

    for (i = 0; i < numSymbols; ++i) count[i] = (int) ((1.0 / (double) (i + 1)) * 10000000);
        shuffle(count, numSymbols);

//        for (i = 0; i < numSymbols; ++i) count[i] = rand();

//    for (i = 0; i < numSymbols; ++i) printf("%d\n", count[i]);

    for (i = 0; i < numSymbols; i++) total += count[i];
    FSE_normalizeCount(norm, tableLog, count, total, maxSymbolValue, lowProb);

//    for (i = 0; i < numSymbols; ++i) printf("%hd\n", norm[i]);

    size_t const outBufSize = (1 << tableLog) * 4;
    BYTE *outBuf = (BYTE *) malloc(outBufSize * sizeof(BYTE));
    size_t compressedTableSize = FSE_writeNCount(outBuf, outBufSize, norm, maxSymbolValue, tableLog);

    FSE_buildDTable_wksp(dtable, norm, maxSymbolValue, tableLog, scratchBuffer, sizeof(scratchBuffer));

    {
        U32 const tableSize = 1 << tableLog;
        void *const tdPtr = dtable + 1;   /* because *dt is unsigned, 32-bits aligned on 32-bits */
        FSE_DECODE_TYPE *const tableDecode = (FSE_DECODE_TYPE *) (tdPtr);
        U32 u;
//        for (u=0; u<tableSize; u++) {
//            printf("%hu %hu %hu\n", tableDecode[u].symbol, tableDecode[u].newState, tableDecode[u].nbBits);
//        }

        ANS tmp;
        tmp.m = numSymbols;
        tmp.L = tableSize;
        tmp.p = new prec[tmp.m];
        tmp.q = new tvar[tmp.m];
        tmp.s = new avar[tmp.L];

        for (i = 0; i < numSymbols; ++i) tmp.p[i] = (double) count[i] / (double) total;
        for (i = 0; i < numSymbols; ++i) { tmp.q[i] = (norm[i] > 0) ? norm[i] : -norm[i]; }
        for (i = 0; i < tableSize; ++i) tmp.s[i] = tableDecode[i].symbol;

        tmp.calc_h();
        tmp.find_sp();

        if (lowProb == 2)
            printf("(%d,bitScaling=%d,smallValues=%d)=(%zu,%f)\n", tableSize, enableScaling, enableSmallValues >> 1,
                   compressedTableSize*8, (tmp.hANS - tmp.h) / tmp.h);
        else
            printf("(%d,lowProb=%d)=(%zu,%f)\n", tableSize, lowProb,
                   compressedTableSize*8, (tmp.hANS - tmp.h) / tmp.h);
    }
}

int main(int argc, char** args){
    printStats(1);
    printStats(0);

    for (int f = 0; f < 4; f++) {
        enableScaling = f&1;
        enableSmallValues = f&2;

        printStats(2);
    }
}