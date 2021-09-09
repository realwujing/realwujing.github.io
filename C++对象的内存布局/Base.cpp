#include <iostream>

// 单一类
class Base
{
public:
    Base() : m_base(0) {}
    virtual ~Base() {}
    virtual void print() const { std::cout << "Base print()" << std::endl; }
    static int GetStaticVal();

protected:
    int m_base;
    static int m_tmp;
};

// 单继承无重写
class DerivedSimple : public Base{
public:
    DerivedSimple(){}
    virtual ~DerivedSimple(){}
    virtual void DerivedSimplePrint() { std::cout << "DerivedSimplePrint()" << std::endl; }
private:
    int m_derivedSimple;
};

// 单继承有重写
class DerivedRewrite : public Base{
public:
    DerivedRewrite(){}
    virtual ~DerivedRewrite(){}
    virtual void DerivedRewritePrint() { std::cout << "DerivedRewritePrint()" << std::endl; }
    virtual void print() const { std::cout << "DerivedRewrite print()" << std::endl; };
private:
    int m_derivedRewrite;
};

// 多继承
class BaseMult{
public:
    BaseMult(){}
    virtual ~BaseMult(){}
    virtual void print() const { std::cout << "BaseMult print()" << std::endl; }

protected:
    int m_baseMult;	
};

class DerivedMult : public Base,public BaseMult{
public:
    DerivedMult(){}
    virtual ~DerivedMult(){}
    virtual void DerivedMultPrint() { std::cout << "DerivedMultPrint()" << std::endl; }
private:
    int derivedMult;
};

// 虚继承
class DerivedVirtual : virtual public Base{
public:
    DerivedVirtual(){}
    virtual ~DerivedVirtual(){}
    virtual void DerivedVirtualPrint() { std::cout << "DerivedVirtualPrint()" << std::endl; }
    virtual void print() const { std::cout << "DerivedVirtual print()" << std::endl; };
private:
    int m_derivedVirtual;
};

// 菱形继承
class DerivedVirtual1: virtual public Base{
public:
    DerivedVirtual1(){}
    virtual ~DerivedVirtual1(){}
    virtual void DerivedVirtual1Print() { std::cout << "DerivedVirtual1Print()" << std::endl; }
 
private:
    int m_derivedVirtual1;
};
class DerivedVirtual2: virtual public Base{
public:
    DerivedVirtual2(){}
    virtual ~DerivedVirtual2(){}
    virtual void DerivedVirtual2Print() { std::cout << "DerivedVirtual2Print()" << std::endl; }

private:
    int m_derivedVirtual2;
};
class DerivedLast : public DerivedVirtual1, public DerivedVirtual2{
public:
    DerivedLast(){}
    virtual ~DerivedLast(){}
    virtual void DerivedLastPrint() { std::cout <<"DerivedLastPrint()" << std::endl; }
 
private:
    int m_derivedLast;
};

int Base::m_tmp = 1;

int main()
{
    // 单一类
    Base base;

    // 单继承无重写
    DerivedSimple derivedSimple;

    // 单继承有重写
    DerivedRewrite derivedRewrite;

    // 多继承
    DerivedMult derivedMult;

    // 虚继承
    DerivedVirtual derivedVirtual;

    // 菱形继承
    DerivedLast derivedLast;

    return 0;
}
