#ifndef SIGMA_MISC
#define SIGMA_MISC

#include <Sigma/common.h>

template<size_t S, size_t A>
struct alignas(A) aligned_storage {
    uint8_t bytes[S];
};


template<typename T>
class lazy_initializer {
    public:
    template<typename... Args>
    void init(Args&&... args){
        if(this->_initialized) return;
        new (&_storage) T(args...);
        this->_initialized = true;
    }

    void deinit(){
        _initialized = false;
    }

    operator bool(){
        return this->_initialized;
    }
    
    T* operator ->(){
		return get();
	}
	T& operator *(){
		return *get();
	}

    T* get(){
		if(_initialized){
            return reinterpret_cast<T *>(&_storage);
        }
        PANIC("Tried to access unintialized lazy variable");
        return nullptr; // Unreachable?
	}

    private:

    bool _initialized = false;
    aligned_storage<sizeof(T), alignof(T)> _storage;
};


#endif