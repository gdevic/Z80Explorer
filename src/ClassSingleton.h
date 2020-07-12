#ifndef CLASS_SINGLETON_H
#define CLASS_SINGLETON_H

template <class T>
class Singleton
{
public:
    static T& Instance()
    {
        static T _instance;             // create static instance of our class
        return _instance;               // return it
    }

private:
    Singleton();                        // hide constructor
    ~Singleton();                       // hide destructor
    Singleton(const Singleton &);       // hide copy constructor
    Singleton& operator=(const Singleton &); // hide assign op
};

#endif // CLASS_SINGLETON_H
