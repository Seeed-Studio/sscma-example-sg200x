#include <iomanip>
#include <random>
#include <sstream>

#include "api_user.h"

api_status_t api_user::addSShkey(request_t req, response_t res)
{
    auto&& body = parse_body(req);
    auto&& result = script(__func__,
        body.value("name", ""), body.value("time", ""), body.value("value", ""));
    response(res, (result == STR_OK) ? 0 : -1, result);
    return API_STATUS_OK;
}

api_status_t api_user::deleteSShkey(request_t req, response_t res)
{
    auto&& body = parse_body(req);
    response(res, 0, script(__func__, body.value("id", "")));
    return API_STATUS_OK;
}

api_status_t api_user::setSShStatus(request_t req, response_t res)
{
    auto&& body = parse_body(req);
    response(res, 0, script(__func__, body.value("enabled", true)));
    return API_STATUS_OK;
}

api_status_t api_user::login(request_t req, response_t res)
{
    static const int max_retry_count = 5;
    static int retryCount = max_retry_count;
    static struct timespec tsLastFailed;

    auto&& body = parse_body(req);
    const auto& username = body.value("userName", "");
    const auto& password = body.value("password", "");

    if (username.empty() || password.empty()) {
        response(res, -1, "Username or password is empty.");
        return API_STATUS_OK;
    }

    if (!verify_pwd(username, aes_decrypt(password))) {
        if (retryCount != max_retry_count) {
            struct timespec ts;
            timespec_get(&ts, TIME_UTC);
            if (ts.tv_sec - tsLastFailed.tv_sec > 60) {
                retryCount = max_retry_count;
            }
        }
        if (retryCount > 0) {
            retryCount--;
        }
        timespec_get(&tsLastFailed, TIME_UTC);

        LOGW("Authentication failed for user: %s, retryCount: %d",
            username.c_str(), retryCount);
        response(res, -1,
            "Invalid username or password",
            json({
                { "retryCount", retryCount },
            }));
        return API_STATUS_OK;
    }
    retryCount = max_retry_count;

    script(__func__); // remove first login record

    std::string token = gen_token();
    save_token(token);
    response(res, 0, STR_OK,
        json({
            { "token", token },
            { "expire", TOKEN_EXPIRATION_TIME },
        }));

    return API_STATUS_OK;
}

api_status_t api_user::queryUserInfo(request_t req, response_t res)
{
    std::vector<json> ssh_key_list;
    auto&& data = parse_result(script(__func__));
    auto&& list = parse_result(data.value("sshKeyFile", ""), ' ');
    for (auto&& l : list) {
        if (l.size() < 4)
            continue;
        json sshkey;
        sshkey["id"] = l[0];
        sshkey["value"] = l[2];
        sshkey["name"] = (l.size() < 6) ? "-" : l[5];
        sshkey["addTime"] = (l.size() < 7) ? "-" : l[6];
        ssh_key_list.push_back(sshkey);
    }
    data["sshkeyList"] = ssh_key_list;
    response(res, 0, STR_OK, data);
    return API_STATUS_OK;
}

api_status_t api_user::updatePassword(request_t req, response_t res)
{
    auto&& body = parse_body(req);
    std::string old_pwd = body.value("oldPassword", "");
    std::string new_pwd = body.value("newPassword", "");
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
    auto&& result = script(__func__, new_pwd);
    response(res, (result == STR_OK) ? 0 : -1, result);
    return API_STATUS_OK;
}
