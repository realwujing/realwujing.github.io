#include <iostream>

using namespace std;

int main ()
{
   int  *ptr = NULL;

   if (ptr == nullptr)
   {
       cout << true << endl;
   }
   

   cout << "ptr 的值是 " << ptr ;
 
   return 0;
}