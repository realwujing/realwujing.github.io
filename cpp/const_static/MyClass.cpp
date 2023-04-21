#include <iostream>
using namespace std;

class Myclass{  
public:  
    Myclass(int a, int b, int c);  
    void GetNumber();  
    void GetSum();  
private:  
    int A, B, C;  
    static int Sum;  
};  
  
int Myclass::Sum = 0;  
  
Myclass::Myclass(int a, int b, int c){  
    A = a;  
    B = b;  
    C = c;  
    Sum += A+B+C;  
}  
  
void Myclass::GetNumber(){  
    cout<<"Number=" << A << "," << B << "," << C <<endl;  
 }  
  
void Myclass::GetSum(){  
    cout<<"Sum="<< Sum <<endl;  
 }  
  
int main(){  
    Myclass M(3, 7, 10),N(14, 9, 11);  
    M.GetNumber();  
    N.GetNumber();  
    M.GetSum();  
    N.GetSum();  

    return 0;
}  