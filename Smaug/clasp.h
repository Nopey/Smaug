#pragma once
#include "sassert.h"
#include <memory>

template<class T>
class clasp_ref;

/// <summary>
/// An owned reference to an object similar to unique_ptr.
/// 
/// Caveat: cannot safely be passed to binaries with differing NDEBUG flag.
/// </summary>
template<class T>
class clasp_owned {
	using owned = clasp_owned<T>;
	using cref = clasp_ref<T>;
#ifdef NDEBUG
	std::unique_ptr<T> m_inner;
	clasp_owned(std::unique_ptr<T> inner): m_inner(std::move(inner)) {}
#else
	std::shared_ptr<T> m_inner;
	clasp_owned(std::shared_ptr<T> inner) : m_inner(inner) {}
#endif

	template <class T, class... Types>
	friend clasp_owned<T> make_clasp(Types&&... args);

	friend class cref;

public:

	cref borrow() noexcept;

	void reset() noexcept { m_inner.reset(); }

	explicit operator bool() const noexcept { return !!m_inner; }
	T& operator*() const { return *m_inner; }
	T* operator->() const noexcept { return m_inner; }
};

/// <summary>
/// A reference to a owned_clasp, the owned_clasp must outlive this reference.
/// Lifetimes are only checked in debug builds.
/// </summary>
template<class T>
class clasp_ref {
	using owned = clasp_owned<T>;
	using cref = clasp_ref<T>;
#ifdef NDEBUG
	T *m_inner;
#else
	std::weak_ptr<T> m_inner;
#endif

public:
	clasp_ref() = delete;
	clasp_ref(owned &) noexcept;
	~clasp_ref();

	T &deref() const;

	void swap(cref& rhs) noexcept;
	
	explicit operator bool() const noexcept { return !!m_inner; }
	T& operator*() const { return deref(); }
	T* operator->() const noexcept { return &deref(); }
};

class bad_clasp_ref : public std::exception {
public:
	bad_clasp_ref() noexcept {}

	virtual const char* what() const noexcept override {
		return "dangling clasp reference";
	}
};

//
// impl clasp_owned
//

/// <summary>
/// 
/// </summary>
template <class T, class... Types>
clasp_owned<T> make_clasp(Types&&... args)
{
	return clasp_owned<T>(
#ifdef NDEBUG
		std::make_unique<T>(args...)
#else
		std::make_shared<T>(args...)
#endif
	);
}

template<class T>
clasp_ref<T> clasp_owned<T>::borrow() noexcept
{
	return clasp_ref(*this);
}

//
// impl clasp_ref
//

template<class T>
clasp_ref<T>::clasp_ref(owned& o) noexcept {
#ifdef NDEBUG
	m_inner = &*o.m_inner;
#else
	m_inner = o.m_inner;
#endif
}

template<class T>
clasp_ref<T>::~clasp_ref() {
#ifndef NDEBUG
	if (!m_inner.lock())
	{
		// Can't throw exception from destructor
		SASSERT(!"bad clasp ref being dropped");
	}
#endif
}

template<class T>
T &clasp_ref<T>::deref() const {
#ifdef NDEBUG
	return *m_inner;
#else
	auto p = m_inner.lock();
	if(!p) throw bad_clasp_ref {};
	return *p;
#endif
}


template<class T>
void clasp_ref<T>::swap(cref& rhs) noexcept
{
#ifdef NDEBUG
	std::swap(m_inner, rhs.m_inner);
#else
	m_inner.swap(rhs.m_inner);
#endif
}

template<class T>
void swap(clasp_ref<T>& lhs, clasp_ref<T>& rhs) noexcept
{
	lhs.swap(rhs);
}
