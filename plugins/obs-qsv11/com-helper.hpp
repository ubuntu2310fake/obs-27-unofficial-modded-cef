#pragma once

#include <unknwn.h>

template <class T>
class CComPtr {
public:
	T *p;

	CComPtr() : p(nullptr) {}
	CComPtr(T *lp) : p(lp) {
		if (p) p->AddRef();
	}
	CComPtr(const CComPtr<T> &lp) : p(lp.p) {
		if (p) p->AddRef();
	}
	~CComPtr() {
		if (p) p->Release();
	}

	void Release() {
		if (p) {
			T *temp = p;
			p = nullptr;
			temp->Release();
		}
	}

	operator T*() const {
		return p;
	}

	T& operator*() const {
		return *p;
	}

	T** operator&() {
		return &p;
	}

	T* operator->() const {
		return p;
	}

	T* operator=(T *lp) {
		if (p != lp) {
			if (lp) lp->AddRef();
			if (p) p->Release();
			p = lp;
		}
		return lp;
	}

	CComPtr<T>& operator=(const CComPtr<T> &lp) {
		if (p != lp.p) {
			if (lp.p) lp.p->AddRef();
			if (p) p->Release();
			p = lp.p;
		}
		return *this;
	}

	bool operator!() const {
		return (p == nullptr);
	}

	bool operator==(T *pT) const {
		return p == pT;
	}
};

template <class T>
class CComQIPtr : public CComPtr<T> {
public:
	CComQIPtr() {}
	CComQIPtr(IUnknown *lp) {
		if (lp) {
			lp->QueryInterface(__uuidof(T), (void**)&this->p);
		}
	}
	template <class U>
	CComQIPtr(const CComPtr<U> &lp) {
		if (lp.p) {
			lp.p->QueryInterface(__uuidof(T), (void**)&this->p);
		}
	}
};
