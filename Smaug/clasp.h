#pragma once
//!
//! clasp
//!
//! A runtime lifetime checker
//! clasp<T> is like unique_ptr<T>
//! clasp_ref<T> is like T*, but will throw a runtime exception if dangled
//!
//!  Magnus Larsen, 2021

#include <memory>
#include <exception>
#include <type_traits>
#include <vector>

#ifndef NDEBUG
#define CLASP_CHECK
#endif
// TODO: Add alternative checking method, where clasp_refs throw exceptions rather than clasps
// #define CLASP_ALT_CHECK

template<class T>
class clasp_ref;
template<class T>
class enable_borrow_this;


// Function to allow cycles to be broken during destruction.
// Implementations should call this on all their members
// Only called when CLASP_CHECK is on
// (default impls later)
template<class T>
void breakClaspCycles(T&);

// type trait to allow storing weak_ptr to this in clasp
template <class T, class = void>
struct _type_enabled_borrow_this : std::false_type {};
template <class T>
struct _type_enabled_borrow_this<T, std::void_t<decltype(std::declval<T&>().__clasp_h_weak_this)>> : std::true_type {};


/// An owned reference to an object similar to unique_ptr.
///
/// Caveat: cannot safely be passed to binaries with differing NDEBUG flag.
/// NOTE: Feel no need to null-check clasps, because although clasp can be
///   null, you really shouldn't expose other code to such values.
template<class T>
class clasp {
	using owned = clasp<T>;
	using cref = clasp_ref<T>;
#ifdef CLASP_CHECK
	std::shared_ptr<T> m_inner;
	clasp(std::shared_ptr<T>&& inner) : m_inner(std::move(inner)) {
		if constexpr (_type_enabled_borrow_this<T>::value)
			m_inner->__clasp_h_weak_this = m_inner;
	}
#else
	std::unique_ptr<T> m_inner;
	clasp(std::unique_ptr<T>&& inner) : m_inner(std::move(inner)) {}
#endif

	template <class T2, class... Types>
	friend clasp<T2> make_clasp(Types&&... args);

	template <class T2>
	friend class clasp;

	friend cref;

public:
	template<class T2>
	clasp(clasp<T2>&& o) : m_inner(o.m_inner) { o.m_inner = nullptr; }

	template<class T2>
	clasp(clasp<T2>& o) : m_inner(o.m_inner) { o.m_inner = nullptr; }

	// construct a null clasp. shouldn't live long, these are dangerous
	clasp(std::nullptr_t = nullptr) { }

	~clasp() noexcept(false);

	cref borrow() noexcept;

	void reset() noexcept { m_inner.reset(); }
	void swap(owned& rhs) noexcept;

	bool operator ==(const T& b) const { return &b == get(); }
	bool operator ==(const cref& b) const { return b.get() == get(); }
	explicit operator bool() const noexcept { return !!m_inner; }
	T& operator*() const { return *m_inner; }
	T* operator->() const noexcept { return &*m_inner; }
	T* get() const { return &**this; };

	// TODO: Ensure clasp was valid before overwriting with other clasp
	owned& operator =(owned& o) { m_inner = o.m_inner; o.m_inner = nullptr; return *this; }
	owned& operator =(owned&& o) noexcept { m_inner = o.m_inner; o.m_inner = nullptr; return *this; }

	// This ain't exactly safe, but you shouldn't be nulling clasps long-term.
	operator T& () const { return *m_inner; }
};

/// A reference to a owned_clasp, the owned_clasp must outlive this reference.
/// Lifetimes are only checked in debug builds.
template<class T>
class clasp_ref {
	using owned = clasp<T>;
	using cref = clasp_ref<T>;
#ifdef CLASP_CHECK
	using inner_t = std::shared_ptr<T>;
#else
	using inner_t = T*;
#endif
	inner_t m_inner;

	template<class T2>
	friend class clasp_ref;

public:
	clasp_ref(std::nullptr_t = nullptr) : m_inner(nullptr) { }
	clasp_ref(owned const&) noexcept;
	template<class T2>
	clasp_ref(clasp_ref<T2> const& o) noexcept : m_inner(o.m_inner) {}

private:
	clasp_ref(inner_t inner) noexcept : m_inner(inner) {};

public:
	T& deref() const;

	void swap(cref& rhs) noexcept;

	explicit operator bool() const noexcept { return !!m_inner; }

	T& operator*() const { return deref(); }
	T* operator->() const noexcept { return &deref(); }
	T* get() const { return &deref(); };
	// clasp_ref's must always be valid, and so can safely decay into a reference
	operator T& () const { return deref(); }
#ifdef CLASP_CHECK
	template<class T2> clasp_ref<T2> cast() const { return clasp_ref<T2>::clasp_ref(std::static_pointer_cast<T2>(m_inner)); }
#else
	template<class T2> clasp_ref<T2> cast() const { return clasp_ref<T2>::clasp_ref(static_cast<clasp_ref<T2>::inner_t>(m_inner)); }
#endif

	bool operator ==(const T& b) const { return &b == get(); }
	bool operator ==(const cref& o) const { return get() == o.get(); };

	friend class enable_borrow_this<T>;
};

/// Exception that is thrown in debug mode when an expired clasp is dereferenced or dropped.
class bad_clasp_ref : public std::exception {
public:
	bad_clasp_ref() noexcept {}

	virtual const char* what() const noexcept override {
		return "dangling clasp reference";
	}
};


/// Exception that is thrown in debug mode when borrow_this is
///  called on an object allocated outside of a clasp
class bad_borrow_this : public std::exception {
public:
	bad_borrow_this() noexcept {}

	virtual const char* what() const noexcept override {
		return "cannot call borrow_this on object allocated outside of clasp";
	}
};

/// Analogue of enable_shared_from_this for clasps.
/// Similarly, you should only be calling borrow_from_this() on objects stored in clasps.
/// The T is the curiously recurring template pattern.
#ifdef CLASP_CHECK
template<class T>
class enable_borrow_this
{
	std::weak_ptr<T> __clasp_h_weak_this;
public:
	clasp_ref<T> borrow_this()
	{
		if (__clasp_h_weak_this.expired()) throw bad_borrow_this{};
		return clasp_ref<T>(__clasp_h_weak_this);
	}

	friend class clasp<T>;
	friend struct _type_enabled_borrow_this<T>;
};
#else
template<class T>
class enable_borrow_this
{
public:
	clasp_ref<T> borrow_this()
	{
		return { this };
	}
};
#endif

//
// impl clasp
//

/// Allocate a clasp on the heap using the default allocator
// TODO: Allow for custom allocators via allocate_clasp function
template <class T, class... Types>
clasp<T> make_clasp(Types&&... args)
{
	return clasp<T>(
#ifdef CLASP_CHECK
		std::make_shared<T>(args...)
#else
		std::make_unique<T>(args...)
#endif
		);
}


template<class T>
clasp<T>::~clasp() noexcept(false)
{
#ifdef CLASP_CHECK
	if (m_inner)
	{
		breakClaspCycles(*m_inner);

		std::weak_ptr<T> weak = m_inner;
		m_inner = nullptr;
		if (!weak.expired())
		{
			throw bad_clasp_ref{};
		}
	}
#endif
}

template<class T>
clasp_ref<T> clasp<T>::borrow() noexcept
{
	return clasp_ref(*this);
}

template<class T>
void clasp<T>::swap(owned& rhs) noexcept
{
	std::swap(m_inner, rhs.m_inner);
}

template<class T>
void swap(clasp<T>& lhs, clasp<T>& rhs) noexcept
{
	lhs.swap(rhs);
}

//
// impl clasp_ref
//

template<class T>
clasp_ref<T>::clasp_ref(owned const& o) noexcept {
#ifdef CLASP_CHECK
	m_inner = o.m_inner;
#else
	m_inner = &*o.m_inner;
#endif
}

template<class T>
T& clasp_ref<T>::deref() const {
	return *m_inner;
}

template<class T>
void clasp_ref<T>::swap(cref& rhs) noexcept
{
	std::swap(m_inner, rhs.m_inner);
}

template<class T>
void swap(clasp_ref<T>& lhs, clasp_ref<T>& rhs) noexcept
{
	lhs.swap(rhs);
}

// Default implementation
template<class T>
void breakClaspCycles(T& t) {}

template<class T>
void breakClaspCycles(std::vector<T>& v)
{
	for (auto& x : v)
		breakClaspCycles(x);
}
template<class T>
void breakClaspCycles(clasp<T>& c)
{
	if (c)
		breakClaspCycles(*c);
}
template<class T>
void breakClaspCycles(clasp_ref<T>& c)
{
	c = nullptr;
}
