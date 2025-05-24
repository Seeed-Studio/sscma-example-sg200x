#ifndef API_USER_H
#define API_USER_H

#include <crypt.h>
#include <openssl/aes.h>
#include <shadow.h>

#include "api_base.h"

class api_user : public api_base {
private:
    static api_status_t addSShkey(request_t req, response_t res);
    static api_status_t deleteSShkey(request_t req, response_t res);
    static api_status_t login(request_t req, response_t res);
    static api_status_t queryUserInfo(request_t req, response_t res);
    static api_status_t setSShStatus(request_t req, response_t res);
    static api_status_t updatePassword(request_t req, response_t res);

public:
    api_user()
        : api_base("userMgr")
    {
        MA_LOGV("");

        REG_API(addSShkey);
        REG_API(deleteSShkey);
        REG_API_NO_AUTH(login);
        REG_API_NO_AUTH(queryUserInfo); // fixed: no auth
        REG_API(setSShStatus); // fixed: no auth
        REG_API(updatePassword); // fixed: no auth
    }

    ~api_user()
    {
        MA_LOGV("");
    }

private:
    static inline const string KEY_AES_128 = "zqCwT7H7!rNdP3wL";

    static string get_username(void)
    {
        return script(__func__);
    }

    static string gen_token(void)
    {
        return script(__func__);
    }

    static string toHex(const unsigned char* data, size_t len)
    {
        stringstream ss;
        ss << hex << setfill('0');
        for (size_t i = 0; i < len; ++i)
            ss << setw(2) << static_cast<int>(data[i]);
        return ss.str();
    }

    static bool fromHex(const string& hexStr, unsigned char* data, size_t& len)
    {
        if (hexStr.length() % 2 != 0)
            return false;

        for (auto& c : hexStr)
            if (!isxdigit(c))
                return false;

        len = hexStr.length() / 2;
        for (size_t i = 0; i < len; ++i)
            sscanf(hexStr.c_str() + 2 * i, "%2hhx", &data[i]);

        return true;
    }

    static string aes_encrypt(const string& plaintext)
    {
        AES_KEY encryptKey;
        AES_set_encrypt_key((const unsigned char*)KEY_AES_128.c_str(), 128, &encryptKey);

        size_t len = plaintext.size();
        size_t paddedLen = (len / AES_BLOCK_SIZE + 1) * AES_BLOCK_SIZE;
        vector<unsigned char> input(paddedLen, 0);
        memcpy(input.data(), plaintext.c_str(), len);

        vector<unsigned char> output(paddedLen);
        for (size_t i = 0; i < paddedLen; i += AES_BLOCK_SIZE) {
            AES_encrypt(input.data() + i, output.data() + i, &encryptKey);
        }

        return toHex(output.data(), paddedLen);
    }

    static string aes_decrypt(const string& ciphertextHex)
    {
        size_t len = ciphertextHex.length() / 2;
        vector<unsigned char> ciphertext(len);
        if (!fromHex(ciphertextHex, ciphertext.data(), len)) {
            return "";
        }

        AES_KEY decryptKey;
        AES_set_decrypt_key((const unsigned char*)KEY_AES_128.c_str(), 128, &decryptKey);

        vector<unsigned char> decryptedText(len);
        for (size_t i = 0; i < len; i += AES_BLOCK_SIZE) {
            if (i + AES_BLOCK_SIZE > len)
                break;
            AES_decrypt(ciphertext.data() + i, decryptedText.data() + i, &decryptKey);
        }

        string decryptedStr(reinterpret_cast<char*>(decryptedText.data()), len);

        size_t pos = decryptedStr.find_last_not_of('\0');
        if (pos != string::npos) {
            decryptedStr = decryptedStr.substr(0, pos + 1);
        }

        return decryptedStr;
    }

    static bool verify_pwd(const string& user, const string& password)
    {
        if (geteuid() != 0) {
            MA_LOGE("Require root privileges for password verification");
            return false;
        }

        struct spwd* sp = getspnam(user.c_str());
        if (!sp) {
            MA_LOGE("User not found: ", user);
            return false;
        }

        struct crypt_data data;
        data.initialized = 0;
        char* encrypted = crypt_r(password.c_str(), sp->sp_pwdp, &data);
        if (!encrypted || strcmp(encrypted, sp->sp_pwdp) != 0) {
            MA_LOGE("Password mismatch for user: ", user);
            return false;
        }

        return true;
    }
};

#endif // API_USER_H
