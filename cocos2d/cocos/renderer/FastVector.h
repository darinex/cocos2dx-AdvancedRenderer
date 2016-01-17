#pragma once

#include <memory>

// this class is a fast implementation of a vector list for a case where you just need to add and get data from list and nothing else
// NOTE: before calling push_back you should make sure to call reserveElements with the count of elements youre going to add
template<class T> class FastVector {
public:
	FastVector() {
		listSize = 10;
		list = (T*)malloc(sizeof(T) * listSize);
		elementCount = 0;
		reserveIndex = 0;
	}
	~FastVector() {
		free(list);
	}
	void reserveElements(size_t count) {
		if (reserveIndex + count > listSize) {
			listSize = reserveIndex + count;
			list = (T*)realloc(list, sizeof(T) * listSize);
		}
		reserveIndex += count;
	}
	inline void push_back(T obj) {
		list[elementCount++] = obj;
		reserveIndex = MAX(reserveIndex, elementCount);
	}
	inline T at(size_t index) const {
		return list[index];
	}

	void clear() {
		reserveIndex = 0;
		elementCount = 0;
	}

	const T* cbegin() const {
		return list;
	}

	const T* cend() const {
		return list + elementCount;
	}

protected:
	T* list;
	size_t elementCount;
	size_t listSize;
	size_t reserveIndex;
};