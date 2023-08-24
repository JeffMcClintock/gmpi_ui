#pragma once
#include <memory>
#include <string>
#include <assert.h>

// Holds a BLOB (owning the data) convertible to RawView
struct RawData
{
private:
	std::shared_ptr<std::string> value;

public:
	RawData()
	{
	}

	RawData(const void* data, size_t size)
	{
		value = std::make_shared<std::string>(reinterpret_cast<const char*>(data), size);
	}

	template <typename T> RawData(const T& v)
	{
        const auto data = (const char*)&v;
        const auto size = sizeof(v);
        
		value = std::make_shared<std::string>(data, size);
	}

	RawData(struct RawView v);

	RawData(std::string v)
    {
		value = std::make_shared<std::string>(v);
    }
                    
	RawData(std::wstring v)
	{
        const auto size = v.size() * sizeof(v[0]);
        const auto data = (const char*)v.data();
        
		value = std::make_shared<std::string>(data, size);
    }
    
	RawData(const char* v)
	{
		value = std::make_shared<std::string>(v);
	}
	const void* data() const
	{
		return value->data();
	}
	size_t size() const
	{
		return value->size();
	}
};

// Points at value without owning or copying it.
// Relies on data not changing or being freed.
struct RawView
{
private:
	const void* data_ = {};
	size_t size_ = 0;

public:
	RawView(){}
	RawView(const void* data, size_t size) : data_(data), size_(size) {}

	template <typename T>
	RawView(const T& v) : data_(&v), size_(sizeof(v)) {}

	// Overloaded constructors for non POD data.
	RawView(const std::string& v) : data_(v.data()), size_(v.size() * sizeof(v[0])) {}
	RawView(const std::wstring& v) : data_(v.data()), size_(v.size() * sizeof(v[0])) {}
	RawView(const char* v) : data_(v), size_(strlen(v)) {}
	RawView(RawData v) : data_(v.data()), size_(v.size()) {}

	// conversion back to types.
	template <typename T>
	explicit operator T() const { assert(size() == sizeof(T));  return *reinterpret_cast<const T*>(data()); }

	explicit operator std::string() const { std::string r(reinterpret_cast<const char*>(data()), size()); return r; }
	explicit operator std::wstring() const { std::wstring r(reinterpret_cast<const wchar_t*>(data()), size() / sizeof(wchar_t)); return r; }

	void reset(const void* data, size_t size)
	{
		data_ = data;
		size_ = size;
	}

	const void* data() const
	{
		return data_;
	}
	size_t size() const
	{
		return size_;
	}
};

inline RawData::RawData(RawView v)
{
	value = std::make_shared<std::string>(reinterpret_cast<const char*>(v.data()), v.size());
}

// Equality operators
inline bool operator==(const RawView& lhs, const RawData& rhs)
{
	return lhs.size() == rhs.size() && memcmp(lhs.data(), rhs.data(), rhs.size()) == 0;
}

inline bool operator!=(const RawView& lhs, const RawData& rhs)
{
	return !(lhs == rhs);
}
