#include <iostream>
#include <map>
#include <string>

enum Status {
    SUCCESS = 0,   // 成功
    FAIL = 1,      // 两数之和小于10
    NOT_fOUND = 2  // statusMessage中没有这个key
};

std::map<size_t, std::string> statusMessage{{SUCCESS, "两数之和大于10"},
                                            {FAIL, "两数之和小于10"},
                                            {NOT_fOUND, "状态码不存在"}};

int getStatusMessage(const int& status, std::string& message)
{
  auto iter = statusMessage.find(status);
  if (iter == statusMessage.end()) {
    message = statusMessage[NOT_fOUND];
    return NOT_fOUND;
  }

  message = iter->second;
  return SUCCESS;
}

// 判断两数之和大于10
int add(const int& a, const int& b)
{
  if (a + b <= 10) {
    return FAIL;
  }
  return SUCCESS;
}

int main() 
{
  int ret = add(3, 5);
  if(SUCCESS != ret) {
    std::string message;
    getStatusMessage(ret, message);
    std::cout << message << std::endl;
    return FAIL;
  }
  return SUCCESS;
}
