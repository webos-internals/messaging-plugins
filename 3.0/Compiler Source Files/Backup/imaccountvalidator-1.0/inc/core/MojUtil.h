/* ============================================================
 * Date  : Jan 12, 2009
 * Copyright 2009 Palm, Inc. All rights reserved.
 * ============================================================ */

#ifndef MOJUTIL_H_
#define MOJUTIL_H_

#include "core/MojCoreDefs.h"

template<class T>
void MojConstruct(T* p, MojSize numElems);
template<class T>
void MojDestroy(T* p, MojSize numElems);
template<class T>
void MojDeleteRange(T iter, T iterEnd);

template<class T>
T MojPostIncrement(T& t);
template<class T>
T MojPostDecrement(T& t);

template<class T>
T MojAbs(const T& t);

template<class T>
T& MojMin(T& t1, T& t2);
template<class T>
const T& MojMin(const T& t1, const T& t2);

template<class T>
T& MojMax(T& t1, T& t2);
template<class T>
const T& MojMax(const T& t1, const T& t2);

template<class T>
void MojSwap(T& t1, T& t2);

template<class T, class COMP>
MojSize MojBinarySearch(T key, const T* array, MojSize numElems);
template<class T>
MojSize MojBinarySearch(T key, const T* array, MojSize numElems);

template<class T, class COMP>
void MojQuickSort(T* array, MojSize numElems, const COMP& comp = COMP());
template<class T>
void MojQuickSort(T* array, MojSize numElems);

bool MojFlagGet(MojUInt32 flags, MojUInt32 mask);
void MojFlagSet(MojUInt32& flags, MojUInt32 mask, bool val);

MojSize MojHash(const void* p, MojSize size);
MojSize MojHash(const MojChar* str);

inline MojSize MojBase64EncodedLenMax(MojSize srcSize) { return ((srcSize + 2) / 3) * 4; }
inline MojSize MojBase64DecodedSizeMax(MojSize encodedLen) { return ((encodedLen + 3) / 4) * 3; }
MojErr MojBase64Encode(const MojByte* src, MojSize srcSize, MojChar* bufOut, MojSize bufLen, MojSize& lenOut, bool pad = true);
MojErr MojBase64EncodeMIME(const MojByte* src, MojSize srcSize, MojChar* bufOut, MojSize bufLen, MojSize& lenOut, bool pad = true);
MojErr MojBase64Decode(const MojChar* src, MojSize srcLen, MojByte* bufOut, MojSize bufSize, MojSize& sizeOut);
MojErr MojBase64DecodeMIME(const MojChar* src, MojSize srcLen, MojByte* bufOut, MojSize bufSize, MojSize& sizeOut);


int MojLexicalCompare(const MojByte* data1, MojSize size1, const MojByte* data2, MojSize size2);
MojSize MojPrefixSize(const MojByte* data1, MojSize size1, const MojByte* data2, MojSize size2);

MojErr MojCreateDirIfNotPresent(const MojChar* path);
MojErr MojRmDirRecursive(const MojChar* path);
MojErr MojFileToString(const MojChar* path, MojString& strOut);
MojErr MojFileFromString(const MojChar* path, const MojChar* data);
const MojChar* MojFileNameFromPath(const MojChar* path);

#include "core/internal/MojUtilInternal.h"

#endif /* MOJUTIL_H_ */
