#include <iomanip>
#include <random>
#include <sstream>

#include "api_user.h"
#include "mongoose.h"

void api_user::gen_token(json& response)
{
    std::time_t now = std::time(nullptr);
    char token[128] = { 0 };
    constexpr size_t RAND_SIZE = 32;
    constexpr size_t MIN_LENGTH = 48;
    char random_bytes[RAND_SIZE];

    std::generate_n(random_bytes, RAND_SIZE, [] {
        static std::mt19937 gen(std::random_device {}());
        return static_cast<char>(gen() & 0xFF);
    });

    std::stringstream ss;
    ss.write(random_bytes, RAND_SIZE);
    ss << now << "recamera";

    if (ss.str().size() < MIN_LENGTH) {
        ss.seekp(0, std::ios_base::end);
        const size_t remaining = MIN_LENGTH - ss.str().size();
        ss.write(random_bytes, std::min(remaining, RAND_SIZE));
    }

    mg_base64_encode(reinterpret_cast<const unsigned char*>(ss.str().c_str()),
        ss.str().size(), token, sizeof(token));

    MA_LOGV("%s", token);

    response["code"] = 0;
    response["msg"] = "";
    response["data"]["token"] = token;
    response["data"]["expire"] = TOKEN_EXPIRATION_TIME;
}

api_status_t api_user::login(const json& request, json& response)
{
    string username;
    string password;

    json body = request["body"];
    MA_LOGV("%s", body.dump().c_str());
    if (body.empty()) {
        goto pwd_error;
    }

    username = body["userName"];
    password = body["password"];
    MA_LOGV("username=%s, password=%s", username.c_str(), password.c_str());
    if (username.empty() || password.empty()) {
        goto pwd_error;
    }

    if (username == "recamera") {
        gen_token(response);
        MA_LOGV("%s", response.dump().c_str());
        return API_STATUS_AUTHORIZED;
    }

pwd_error:
    response["code"] = -1;
    response["msg"] = "Incorrect password";
    response["data"] = json({
        { "retryCount", 1 },
    });
    MA_LOGV("%s", response.dump().c_str());

    return API_STATUS_OK;
}

api_status_t api_user::queryUserInfo(const json& request, json& response)
{
    json data;

    data["userName"] = "recamera";
    data["firstLogin"] = false;
    data["sshEnabled"] = true;

    vector<json> ssh_key_list;
    data["sshKeyList"] = ssh_key_list;

    response["code"] = 0;
    response["msg"] = "";
    response["data"] = data;

    return API_STATUS_OK;
}
