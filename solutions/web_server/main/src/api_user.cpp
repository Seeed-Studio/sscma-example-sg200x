#include <iomanip>
#include <random>
#include <sstream>

#include "api_user.h"

api_status_t api_user::addSShkey(request_t req, response_t res)
{
    string result;
    json body;
    int code = 0;
    if (!get_body(req, body)) {
        result = "Format error.";
        goto exit;
    }

    result = script(__func__, body["name"], body["time"], body["value"]);
    if (result != STR_OK) {
        code = -1;
    }

exit:
    response(res, code, result);
    return API_STATUS_OK;
}

api_status_t api_user::deleteSShkey(request_t req, response_t res)
{
    response(res, 0);
    return API_STATUS_OK;
}

api_status_t api_user::login(request_t req, response_t res)
{
    string username, password;
    json body;

    int code = 0;
    string msg = STR_OK;

    if (!get_body(req, body)) {
        code = -1;
        msg = "Format error.";
        goto exit;
    }
    MA_LOGV(body);

    username = body["userName"];
    password = body["password"];
    MA_LOGV("username=", username);
    MA_LOGV("password=", password);
    if (username.empty() || password.empty()) {
        code = -1;
        msg = "Username or password is empty.";
        goto exit;
    }

    if (username == "recamera") {
        response(res, 0, STR_OK, json({
            { "token", genToken() },
            { "expire", 0 },
        }));

        return API_STATUS_AUTHORIZED;
    }

    msg = "Incorrect password";

exit:
    response(res, code, msg, json({
        { "retryCount", 1 },
    }));

    return API_STATUS_OK;
}

api_status_t api_user::queryUserInfo(request_t req, response_t res)
{
    json data;

    data["userName"] = "recamera";
    data["firstLogin"] = false;
    data["sshEnabled"] = true;

    vector<json> ssh_key_list;
    data["sshKeyList"] = ssh_key_list;

    response(res, 0, STR_OK, data);

    return API_STATUS_OK;
}

api_status_t api_user::setSShStatus(request_t req, response_t res)
{
    response(res, 0);

    return API_STATUS_OK;
}

api_status_t api_user::updatePassword(request_t req, response_t res)
{
    response(res, 0);

    return API_STATUS_OK;
}
