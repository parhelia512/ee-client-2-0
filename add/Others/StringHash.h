#ifndef __STRINGHASH_H__
#define __STRINGHASH_H__

#include <hash_map>
#include <string>

//¿í×Ö·û´®hash
unsigned int hash(const wchar_t *key);
struct widestring_hash_compare : public stdext::hash_compare<std::wstring>
{ 
	size_t operator()(const std::wstring&  A)const
	{
		return hash(A.c_str());
	}
	bool operator()(const std::wstring&  a1, const std::wstring&  a2)const
	{
		return  a1 != a2;
	}
};

//×Ö·û´®hash
unsigned int hash(const char *str);
struct string_hash_compare : public stdext::hash_compare<std::string>
{ 
	size_t operator()(const std::string&  A)const
	{
		return hash(A.c_str());
	}

	bool operator()(const std::string&  a1, const std::string&  a2)const
	{
		return  a1 != a2;
	}
};

#endif