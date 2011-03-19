/* ============================================================
 * Date  : Jan 9, 2009
 * Copyright 2009 Palm, Inc. All rights reserved.
 * ============================================================ */

#ifndef MOJVECTORINTERNAL_H_
#define MOJVECTORINTERNAL_H_

// specialization for pointer types
template<>
class MojVector<class T*> : private MojVector<void*>
{
public:
	typedef T* ValueType;
	typedef ValueType* Iterator;
	typedef const ValueType* ConstIterator;

	MojVector() : Base() {}
	MojVector(const MojVector& v) : Base(v) {}

	MojSize size() const { return Base::size(); }
	MojSize capacity() const { return Base::capacity(); }
	bool empty() const { return Base::empty(); }

	ConstIterator begin() const { return (ConstIterator) Base::begin(); }
	ConstIterator end() const { return (ConstIterator) Base::end(); }

	const ValueType& at(MojSize idx) const { return (ValueType&) Base::at(idx); }
	const ValueType& front() const { return (ValueType&) Base::front(); }
	const ValueType& back() const { return (ValueType&) Base::back(); }

	MojSize find(const ValueType& t, MojSize startIdx = 0) const { return Base::find(t, startIdx); }

	void clear() { Base::clear(); }
	void swap(MojVector& v) { Base::swap(v); }
	void assign(const MojVector& v) { Base::assign(v); }
	MojErr assign(ConstIterator rangeBegin, ConstIterator rangeEnd) { return Base::assign((Base::ConstIterator) rangeBegin, (Base::ConstIterator) rangeEnd); }
	int compare(const MojVector& v) const { return Base::compare(v); }

	MojErr begin(Iterator& i) { return Base::begin((Base::Iterator&) i); }
	MojErr end(Iterator& i) { return Base::end((Base::Iterator&) i); }
	MojErr reserve(MojSize numElems) { return Base::reserve(numElems); }
	MojErr resize(MojSize numElems, const ValueType& val = NULL) { return Base::resize(numElems, val); }
	MojErr push(const ValueType& t) { return Base::push(t); }
	MojErr pop() { return Base::pop(); }
	MojErr setAt(MojSize idx, const ValueType& val) { return Base::setAt(idx, val); }
	MojErr append(ConstIterator rangeBegin, ConstIterator rangeEnd) { return Base::append((Base::ConstIterator) rangeBegin, (Base::ConstIterator) rangeEnd); }
	MojErr insert(MojSize idx, MojSize numElems, const ValueType& val) { return Base::insert(idx, numElems, val); }
	MojErr insert(MojSize idx, ConstIterator rangeBegin, ConstIterator rangeEnd) { return Base::insert(idx, (Base::ConstIterator) rangeBegin, (Base::ConstIterator) rangeEnd); }
	MojErr erase(MojSize idx, MojSize numElems = 1) { return Base::erase(idx, numElems); }

	MojVector& operator=(const MojVector& rhs) { Base::assign(rhs); return *this; }
	const ValueType& operator[](MojSize idx) const { return (const ValueType&) Base::operator[](idx); }
	bool operator==(const MojVector& rhs) const { return Base::operator==(rhs); }
	bool operator!=(const MojVector& rhs) const { return Base::operator!=(rhs); }
	bool operator<(const MojVector& rhs) const { return Base::operator<(rhs); }
	bool operator<=(const MojVector& rhs) const { return Base::operator<=(rhs); }
	bool operator>(const MojVector& rhs) const { return Base::operator>(rhs); }
	bool operator>=(const MojVector& rhs) const { return Base::operator>=(rhs); }

private:
	typedef MojVector<void*> Base;
};

template<class T, class EQ, class COMP>
const MojSize MojVector<T, EQ, COMP>::InitialSize = 8;

template<class T, class EQ, class COMP>
MojVector<T, EQ, COMP>::MojVector(const MojVector& v)
: m_begin(v.m_begin),
  m_end(v.m_end),
  m_endAlloc(v.m_endAlloc)
{
	if (m_begin)
		MojRefCountRetain(m_begin);
}

template<class T, class EQ, class COMP>
MojSize MojVector<T, EQ, COMP>::find(const T& t, MojSize startIdx) const
{
	MojAssert(startIdx == 0 || startIdx < size());

	for (ConstIterator i = begin() + startIdx; i < end(); ++i) {
		if (EQ()(*i, t))
			return i - begin();
	}
	return MojInvalidIndex;
}

template<class T, class EQ, class COMP>
void MojVector<T, EQ, COMP>::clear()
{
	if (empty())
		return;
	if (MojRefCountGet(m_begin) == 1) {
		// we can only safely cleanup and reuse the existing array
		// if we have the only reference
		MojDestroy(m_begin, size());
		m_end = m_begin;
	} else {
		reset(NULL, NULL, NULL);
	}
}

template<class T, class EQ, class COMP>
void MojVector<T, EQ, COMP>::assign(const MojVector& v)
{
	reset(v.m_begin, v.m_end, v.m_endAlloc);
	if (m_begin)
		MojRefCountRetain(m_begin);
}

template<class T, class EQ, class COMP>
MojErr MojVector<T, EQ, COMP>::assign(ConstIterator rangeBegin, ConstIterator rangeEnd)
{
	MojAssert((rangeBegin && rangeEnd >= rangeBegin) || (rangeBegin == NULL && rangeEnd == NULL));

	clear();
	MojErr err = append(rangeBegin, rangeEnd);
	MojErrCheck(err);

	return MojErrNone;
}

template<class T, class EQ, class COMP>
int MojVector<T, EQ, COMP>::compare(const MojVector& comp) const
{
	ConstIterator iter = begin();
	ConstIterator compIter = comp.begin();
	for (;;) {
		if (iter == end())
			return compIter == comp.end() ? 0 : -1;
		if (compIter == comp.end())
			return 1;
		int comp = COMP()(*iter, *compIter);
		if (comp != 0)
			return comp;
		++iter;
		++compIter;
	}
	MojAssertNotReached();
	return 0;
}

template<class T, class EQ, class COMP>
MojErr MojVector<T, EQ, COMP>::begin(Iterator& i)
{
	i = NULL;
	MojErr err = ensureWritable();
	MojErrCheck(err);
	i = m_begin;
	return MojErrNone;
}

template<class T, class EQ, class COMP>
MojErr MojVector<T, EQ, COMP>::end(Iterator& i)
{
	i = NULL;
	MojErr err = ensureWritable();
	MojErrCheck(err);
	i = m_end;
	return MojErrNone;
}

template<class T, class EQ, class COMP>
MojErr MojVector<T, EQ, COMP>::reserve(MojSize numElems)
{
	if (numElems > capacity()) {
		MojErr err = realloc(numElems);
		MojErrCheck(err);
	}
	return MojErrNone;
}

template<class T, class EQ, class COMP>
MojErr MojVector<T, EQ, COMP>::resize(MojSize numElems, const T& val)
{
	MojSize curSize = size();
	if (numElems > curSize) {
		MojErr err = insert(curSize, numElems - curSize, val);
		MojErrCheck(err);
	} else if (numElems < curSize) {
		MojErr err = erase(numElems, curSize - numElems);
		MojErrCheck(err);
	}
	return MojErrNone;
}

template<class T, class EQ, class COMP>
MojErr MojVector<T, EQ, COMP>::push(const T& val) {
	MojErr err = ensureSpace(size() + 1);
	MojErrCheck(err);
	new(m_end) T(val);
	m_end++;
	return MojErrNone;
}

template<class T, class EQ, class COMP>
MojErr MojVector<T, EQ, COMP>::setAt(MojSize idx, const T& val)
{
	MojAssert(idx < size());
	MojErr err = ensureWritable();
	MojErrCheck(err);
	m_begin[idx] = val;
	return MojErrNone;
}

template<class T, class EQ, class COMP>
MojErr MojVector<T, EQ, COMP>::append(ConstIterator rangeBegin, ConstIterator rangeEnd)
{
	MojAssert((rangeBegin && rangeEnd >= rangeBegin) || (rangeBegin == NULL && rangeEnd == NULL));
	MojErr err = ensureSpace(size() + (rangeEnd - rangeBegin));
	MojErrCheck(err);
	for (ConstIterator i = rangeBegin; i < rangeEnd; ++i, ++m_end) {
		new(m_end) T(*i);
	}
	return MojErrNone;
}

template<class T, class EQ, class COMP>
MojErr MojVector<T, EQ, COMP>::insert(MojSize idx, MojSize numElems, const T& val)
{
	MojAssert(idx <= size());

	MojErr err = shift(idx, numElems);
	MojErrCheck(err);
	Iterator pos = m_begin + idx;
	Iterator end = pos + numElems;
	for (; pos < end; ++pos) {
		*pos = val;
	}
	return MojErrNone;
}

template<class T, class EQ, class COMP>
MojErr MojVector<T, EQ, COMP>::insert(MojSize idx, ConstIterator rangeBegin, ConstIterator rangeEnd)
{
	MojAssert(idx <= size());
	MojAssert((rangeBegin && rangeEnd >= rangeBegin) || (rangeBegin == NULL && rangeEnd == NULL));

	MojErr err = shift(idx, rangeEnd - rangeBegin);
	MojErrCheck(err);
	Iterator pos = m_begin + idx;
	for (ConstIterator i = rangeBegin; i < rangeEnd; ++i, ++pos) {
		*pos = *i;
	}
	return MojErrNone;
}

template<class T, class EQ, class COMP>
MojErr MojVector<T, EQ, COMP>::erase(MojSize idx, MojSize numElems)
{
	MojAssert(idx + numElems <= size());

	// may invalidate iters
	MojErr err = ensureWritable();
	MojErrCheck(err);
	MojSize numMove = size() - (idx + numElems);
	Iterator pos = m_begin + idx;
	// do moves
	for (MojSize i = 0; i < numMove; ++i, ++pos) {
		*pos = *(pos + numElems);
	}
	// then destroy elems hanging off the end
	m_end -= numElems;
	MojDestroy(m_end, numElems);

	return MojErrNone;
}

template<class T, class EQ, class COMP>
MojErr MojVector<T, EQ, COMP>::reverse()
{
	if (size() > 1) {
		MojErr err = ensureWritable();
		MojErrCheck(err);
		Iterator i1 = m_begin;
		Iterator i2 = m_end - 1;
		while (i2 > i1) {
			MojSwap(*i1, *i2);
			++i1;
			--i2;
		}
	}
	return MojErrNone;
}

template<class T, class EQ, class COMP>
MojErr MojVector<T, EQ, COMP>::sort()
{
	if (size() > 1) {
		MojErr err = ensureWritable();
		MojErrCheck(err);
		MojQuickSort<T, COMP>(m_begin, size());
	}
	return MojErrNone;
}

template<class T, class EQ, class COMP>
bool MojVector<T, EQ, COMP>::operator==(const MojVector& rhs) const
{
	if (size() != rhs.size())
		return false;
	if (m_begin == rhs.m_begin)
		return true;
	ConstIterator i = begin();
	ConstIterator rhi = rhs.begin();
	while (i != end()) {
		if (!EQ()(*i, *rhi))
			return false;
		++i;
		++rhi;
	}
	return true;
}

template<class T, class EQ, class COMP>
MojErr MojVector<T, EQ, COMP>::shift(MojSize idx, MojSize numElems)
{
	MojAssert(idx <= size());
	MojSize numMove = size() - idx;
	MojSize numCopyConstruct = MojMin(numElems, numMove);
	MojSize numDefaultConstruct = numElems - numCopyConstruct;
	MojSize numAssign = numMove - numCopyConstruct;

	// make space (possibly invalidating iterator)
	MojSize err = ensureSpace(size() + numElems);
	MojErrCheck(err);
	Iterator elem = m_end - 1;
	// incrementing end as we go to make sure  newly created elems
	// get destroyed if a constructor throws
	for (MojSize i = 0; i < numDefaultConstruct; ++i, ++m_end) {
		new(m_end) T();
	}
	for (MojSize i = 0; i < numCopyConstruct; ++i, ++m_end) {
		ConstIterator src = m_end - numElems;
		new(m_end) T(*src);
	}
	for (MojSize i = 0; i < numAssign; ++i, --elem) {
		ConstIterator src = elem - numElems;
		*elem = *src;
	}

	return MojErrNone;
}

template<class T, class EQ, class COMP>
MojErr MojVector<T, EQ, COMP>::ensureWritable()
{
	if (m_begin && MojRefCountGet(m_begin) > 1) {
		MojErr err = realloc(capacity());
		MojErrCheck(err);
	}
	return MojErrNone;
}

template<class T, class EQ, class COMP>
MojErr MojVector<T, EQ, COMP>::ensureSpace(MojSize numElems)
{
	if (numElems > capacity()) {
		MojSize newSize = MojMax(InitialSize, capacity() * 2);
		newSize = MojMax(numElems, newSize);
		MojErr err = realloc(newSize);
		MojErrCheck(err);
		MojAssert(capacity() >= numElems);
	}
	MojErr err = ensureWritable();
	MojErrCheck(err);

	return MojErrNone;
}

template<class T, class EQ, class COMP>
MojErr MojVector<T, EQ, COMP>::realloc(MojSize numElems)
{
	// we use a temporary vector and keep it valid at all times to make
	// sure that proper cleanup happens if a constructor throws an exception
	MojVector<T, EQ, COMP> tmp;
	tmp.m_begin = static_cast<T*>(MojRefCountAlloc(numElems * sizeof(T)));
	MojAllocCheck(tmp.m_begin);
	tmp.m_end = tmp.m_begin;
	tmp.m_endAlloc = tmp.m_begin + numElems;
	for (ConstIterator i = begin(); i < end(); ++i, ++tmp.m_end) {
		new(tmp.m_end) T(*i);
	}
	swap(tmp);

	return MojErrNone;
}

template<class T, class EQ, class COMP>
void MojVector<T, EQ, COMP>::reset(Iterator begin, Iterator end, Iterator endAlloc)
{
	release();
	m_begin = begin;
	m_end = end;
	m_endAlloc = endAlloc;
}

template<class T, class EQ, class COMP>
inline void MojVector<T, EQ, COMP>::release()
{
	MojRefCountRelease(m_begin, size());
}

#endif /* MOJVECTORINTERNAL_H_ */
