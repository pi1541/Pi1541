#ifndef _Singleton_h_
#define _Singleton_h_

#include "types.h"

template <typename T> 
class Singleton
{
public:
	static T* Instance()
	{
		if (m_pInstance == 0) 
			m_pInstance = new T;

		//assert(m_pInstance != 0);

		return m_pInstance;
	};

	static void DestroyInstance()
	{
		delete m_pInstance;
		m_pInstance = 0;
	};

protected:

	Singleton()
	{
	};

	virtual ~Singleton()
	{
	};

	static T* m_pInstance;

private:

	Singleton(const Singleton& source)
	{
	};

};
 
template <typename T> T* Singleton<T>::m_pInstance = 0;
 
#endif



