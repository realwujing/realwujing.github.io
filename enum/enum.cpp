// 写偏业务层代码
int query(const QString &package)
{
    int ret = exist(package);
    if (!ret) {
        qCritical() << "not exist";
        return -1;
    }
    return ret;
}

// 写偏底层的代码
int query(const QString &package)
{
    int ret = exist(package);
    if (!ret) {
        return 127; // 一般使用枚举类型
    }
    return ret;
}


int getErrorMessage(const int& errorCode, QString& errorMessage)
{
    int ret = errorCodetoErrorMessage(errorCode, errorMessage);
    if (!ret)
        qCritical() << "errorCode not exist";
        return -1；
    return ret;
}

// 0 代表成功
// 其它 代表失败
// 大于0数代表错误码

enum class RetCode {
    SUCCESS = 0, // 成功
    UAP_INSTALL_FILE_NOT_EXISTS = 401, // 文件不存在
    UAP_INSTALL_DIRECTORY_NOT_EXISTS = 402, // 目录不存在
};

template <typename T=quint32>
inline T RetCode(RetCode code){
    return static_cast<T>(code);
}