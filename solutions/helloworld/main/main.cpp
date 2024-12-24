#include <iostream>
#include <crypt.h>
#include <cstring>

int main() {
    const char *password = "recamera";  // 需要加密的密码
    const char *salt = "$2y$08$abcdefghijklmnopqrstuvwx";  // bcrypt 盐值（通常包含工作因子和盐）

    // 生成 bcrypt 哈希
    char *hashed_password = crypt(password, salt);

    if (hashed_password == nullptr) {
        std::cerr << "Error hashing password." << std::endl;
        return 1;
    }

    std::cout << "Hashed password: " << hashed_password << std::endl;

    // 验证密码
    const char *input_password = "my_secure_password";  // 输入的密码进行验证
    //char *check_hash = crypt(input_password, salt);

    // if (strcmp(hashed_password, check_hash) == 0) {
    //     std::cout << "Password is correct." << std::endl;
    // } else {
    //     std::cout << "Password is incorrect." << std::endl;
    // }

    return 0;
}
