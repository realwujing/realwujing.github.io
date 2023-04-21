#include <iostream>
#include <map>
#include <string>

enum class Status {
    SUCCESS = 0,   // 成功
    FAIL = 1,      // 两数之和小于10
    NOT_fOUND = 2  // statusMessage中没有这个key
};


std::map<size_t, std::string> statusMessage{{static_cast<size_t>(Status::SUCCESS), "两数之和大于10"},
                                            {static_cast<size_t>(Status::FAIL), "两数之和小于10"},
                                            {static_cast<size_t>(Status::NOT_fOUND), "statusMessage中没有这个key"}};

int getStatusMessage(const int& status, std::string& message)
{
  auto iter = statusMessage.find(status);
  if (iter == statusMessage.end()) {
    return static_cast<int>(Status::NOT_fOUND);
  }

  message = iter->second;
  return static_cast<int>(Status::SUCCESS);
}

// 判断两数之和大于10
int add(const int& a, const int& b)
{
  if (a + b <= 10) {
    return static_cast<int>(Status::FAIL);
  }
  return static_cast<int>(Status::SUCCESS);
}

int main() {
  int ret = add(3, 5);
  if(static_cast<int>(Status::SUCCESS) != ret) {
    std::string message;
    getStatusMessage(ret, message);
    std::cout << message << std::endl;
    return static_cast<int>(Status::FAIL);
  }
  return static_cast<int>(Status::SUCCESS);
}
