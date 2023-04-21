#include <iostream>
namespace TestSpace {
enum Color {
  red = 0,
  green,
  blue,
};
}; // namespace TestSpace

int main() {
  using namespace TestSpace;
  for (int i = 0; i < static_cast<int>(Color::blue); i++) {
    std::cout << "__LINE__:" << __LINE__ << "|" << i << std::endl;
  }
  // or
  Color color = Color::green;
  if (1 <= color ) {
    std::cout << "__LINE__:" << __LINE__ << "|" << color << std::endl;
  }
  return 0;
}
