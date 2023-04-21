// 0 代表成功
// 其它 代表失败
// 大于0数代表错误码

enum class RetCode {
    SUCCESS = 0, // 成功
    UAP_INSTALL_FILE_NOT_EXISTS = 401, // 文件不存在
    UAP_INSTALL_DIRECTORY_NOT_EXISTS = 402, // 目录不存在
};

template <typename T=int>
inline T RetCode(RetCode code){
    return static_cast<T>(code);
}