#include<iostream>
using namespace std;
enum GameResult{WIN,LOSE,TIE,CANCEL};

int main(){
	GameResult result;
	// enum GameResult omit=CANCEL;
    GameResult omit=CANCEL;
	// 上述两种方式都是可以的。带或者不带上enum关键字都可以。
	for(int count=WIN;count<=CANCEL;count++){
		// 这里是不限定作用域的枚举类型。所以可知直接比较（隐式类型转换），
		// 不需要显式类型转换
		result = static_cast<GameResult>(count);
		//不能直接用一个整数给枚举值赋值，需要进行强制类型转化。
		if (result==omit){
			cout<<"The game was cancelled"<<endl;
		}
		else{
			cout<<"The game was played ";
			if(result==WIN){
				cout<<"and we won!";
			}
			if(result==LOSE){
				cout<<"and we lost...";
			}
			cout<<endl;
		}
	}
	return 0;
}
