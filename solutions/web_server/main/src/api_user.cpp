#include <iomanip>
#include <random>
#include <sstream>

#include "api_user.h"

#define JSON_BODY()                         \
    json body;                              \
    if (!get_body(req, body)) {             \
        response(res, -1, "Format error."); \
        return API_STATUS_OK;               \
    }

api_status_t api_user::addSShkey(request_t req, response_t res)
{
    JSON_BODY();

    int code = 0;
    string result = script(__func__,
        body.value("name", ""), body.value("time", ""), body.value("value", ""));
    if (result != STR_OK) {
        code = -1;
    }

    response(res, code, result);
    return API_STATUS_OK;
}

api_status_t api_user::deleteSShkey(request_t req, response_t res)
{
    JSON_BODY();

    string result = script(__func__, body.value("id", ""));
    if (result != STR_OK) {
        response(res, -1, result);
        return API_STATUS_OK;
    }

    response(res, 0, result);
    return API_STATUS_OK;
}

api_status_t api_user::login(request_t req, response_t res)
{
    static int retryCount = 5;
    static struct timespec tsLastFailed;

    JSON_BODY();

    string username = body.value("userName", "");
    string password = body.value("password", "");

    if (username.empty() || password.empty()) {
        response(res, -1, "Username or password is empty.");
        return API_STATUS_OK;
    }

    if (!verify_pwd(username, aes_decrypt(password))) {
        if (retryCount != 5) {
            struct timespec ts;
            timespec_get(&ts, TIME_UTC);
            if (ts.tv_sec - tsLastFailed.tv_sec > 60) {
                retryCount = 5;
            }
        }
        if (retryCount > 0) {
            retryCount--;
        }
        timespec_get(&tsLastFailed, TIME_UTC);

        MA_LOGE("Authentication failed for user: ", username);
        response(res, -1,
            "Invalid username or password",
            json({
                { "retryCount", retryCount },
            }));
        return API_STATUS_OK;
    }
    retryCount = 5;

    script(__func__);

    response(res, 0, STR_OK,
        json({
            { "token", gen_token() },
            { "expire", 0 },
        }));

    return API_STATUS_AUTHORIZED;
}

api_status_t api_user::queryUserInfo(request_t req, response_t res)
{
    string result = script(__func__);
    json data = json::parse(result);

    string fname = data.value("sshKeyFile", "");
    if (fname.empty()) {
        response(res, -1, "SSH key file not found.");
        return API_STATUS_OK;
    }

    ifstream file(fname);
    if (!file.is_open()) {
        response(res, -1, "SSH key file open failed.");
        return API_STATUS_OK;
    }

    // parse file
    vector<json> ssh_key_list;
    string line;
    int cnt = 0;

    while (getline(file, line)) {
        if (line.empty())
            continue;

        vector<string> parts;
        string_split(line, ' ', parts);

        json sshkey;
        sshkey["id"] = (parts.size() < 1) ? "-" : parts[0];
        sshkey["value"] = (parts.size() < 3) ? "-" : parts[2];
        sshkey["name"] = (parts.size() < 6) ? "-" : parts[5];
        sshkey["addTime"] = (parts.size() < 7) ? "-" : parts[6];
        ssh_key_list.push_back(sshkey);
    }
    file.close();
    data["sshkeyList"] = ssh_key_list;

    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_user::setSShStatus(request_t req, response_t res)
{
    JSON_BODY();

    string result = script(__func__, body.value("enabled", true));
    if (result != STR_OK) {
        response(res, -1, result);
        return API_STATUS_OK;
    }

    response(res, 0, result);
    return API_STATUS_OK;
}

api_status_t api_user::updatePassword(request_t req, response_t res)
{
    JSON_BODY();

    string old_pwd = body.value("oldPassword", "");
    string new_pwd = body.value("newPassword", "");

    if (old_pwd.empty() || new_pwd.empty()) {
        response(res, -1, "Old password or new password is empty.");
        return API_STATUS_OK;
    }
    if (old_pwd == new_pwd) {
        response(res, -1, "New password is same with old password.");
        return API_STATUS_OK;
    }

    old_pwd = aes_decrypt(old_pwd);
    if (!verify_pwd(get_username(), old_pwd)) {
        response(res, -1, "Invalid old password.");
        return API_STATUS_OK;
    }

    new_pwd = aes_decrypt(new_pwd);
    response(res, 0, script(__func__, old_pwd, new_pwd));
    return API_STATUS_OK;
}
