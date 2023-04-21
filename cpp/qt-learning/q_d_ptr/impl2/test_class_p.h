#pragma once

class TestClass;
class TestClassPrivate
{
public:
    TestClassPrivate(TestClass *q);

private:
    void testFunc();
 
private:
    TestClass * const q_ptr;
    Q_DECLARE_PUBLIC(TestClass);
    
    int m_val1;
    const char * m_ptr;
};
