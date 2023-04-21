// brief: a singleton base class offering an easy way to create singleton
#include <iostream>
#include <unistd.h>

template<typename T>
class Singleton{
public:
    static T& get_instance(){
        static T instance;
        return instance;
    }
    virtual ~Singleton(){
        std::cout<<"destructor called!"<<std::endl;
    }
    Singleton(const Singleton&)=delete;
    Singleton& operator =(const Singleton&)=delete;
protected:
    Singleton(){
        std::cout<<"constructor called!"<<std::endl;
    }

};
/********************************************/
// Example:
// 1.friend class declaration is requiered!
// 2.constructor should be private


class DerivedSingle:public Singleton<DerivedSingle>{
   // !!!! attention!!!
   // needs to be friend in order to
   // access the private constructor/destructor
   friend class Singleton<DerivedSingle>;
public:
   DerivedSingle(const DerivedSingle&)=delete;
   DerivedSingle& operator =(const DerivedSingle&)= delete;
private:
   DerivedSingle()=default;
};

int main(int argc, char* argv[]){
    DerivedSingle& instance1 = DerivedSingle::get_instance();
    DerivedSingle& instance2 = DerivedSingle::get_instance();
    sleep(300);
    return 0;
}

