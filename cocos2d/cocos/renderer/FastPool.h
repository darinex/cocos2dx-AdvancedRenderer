#pragma once

#include <memory>

// requires the class used for this list to have a default public constructor
template<class T> class FastPool {
public:

	// a direct function pointer is used here as it is alot faster than std::function
	FastPool(T(*createDelegate)()) {
		_listSize = 10;
		_poolList = (T*) malloc(_listSize * sizeof(T));
		_currentElementCount = 0;
		_createDelegate = createDelegate;
	}

	~FastPool() {
		free(_poolList);
	}

	inline T pop() {
		if (_currentElementCount <= 0) {
			return _createDelegate();
		}
		else {
			return _poolList[--_currentElementCount];
		}
	}

	inline void push(T obj) {
		if (_currentElementCount + 1 > _listSize) {
			_listSize *= 1.3f;
			_poolList = (T*)realloc(_poolList, _listSize * sizeof(T));
		}
		_poolList[_currentElementCount++] = obj;
	}

	int getElementCount() {
		return _currentElementCount;
	}

protected:
	T(*_createDelegate)();

	T* _poolList;
	int _currentElementCount;
	int _listSize;
};