#include <iomanip>
#include <random>
#include <sstream>

#include "api_user.h"
#include "mongoose.h"

void gen_token(json& response)
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

    printf("%s,%d: token: %s\n", __func__, __LINE__, token);

    response["code"] = 0;
    response["msg"] = "";
    response["data"]["token"] = token;
    response["data"]["expires"] = TOKEN_EXPIRATION_TIME;
}

// api_status_t api_user::login(json &json)//(mg_connection* c, mg_http_message* hm)
// {
//     printf("%s,%d\n", __func__, __LINE__);

//     if (hm->body.len == 0) {
//         mg_http_reply(c, 400, "Content-Type: application/json\r\n",
//             "{\"code\":-1,\"msg\":\"Empty request body\"}");
//         return API_STATUS_OK;
//     }

//     string name = mg_json_get_str(hm->body, "$.userName");
//     string pwd = mg_json_get_str(hm->body, "$.password");

//     if (name.empty() || pwd.empty()) {
//         mg_http_reply(c, 400, "Content-Type: application/json\r\n",
//             "{\"code\":-1,\"msg\":\"Missing required fields\"}");
//         return API_STATUS_OK;
//     }

//     printf("userName: %s, password: %s\n", name.c_str(), pwd.c_str());

//     if (name != "recamera") {
//         return API_STATUS_UNAUTHORIZED;
//     }

//     return API_STATUS_AUTHORIZED;
// }

// api_status_t api_user::queryUserInfo(json &json);//(mg_connection* c, mg_http_message* hm)
// {
//     json data;

//     data["userName"] = "recamera";
//     data["firstLogin"] = false;
//     data["sshEnabled"] = true;

//     vector<json> ssh_key_list;
//     data["sshKeyList"] = ssh_key_list;

//     json resp;
//     resp["code"] = 0;
//     resp["msg"]  = "";
//     resp["data"] = data;

//     // response(c, resp);
//     return API_STATUS_OK;
// }

//     // response(c, resp);
//     return API_STATUS_OK;
// }