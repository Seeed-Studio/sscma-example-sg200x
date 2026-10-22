#include "onvif_soap.h"

#include <cstdio>
#include <cstring>
#include <ctime>
#include <string>
#include <vector>

#include "core/ma_debug.h"

#include "onvif_service.h"
#include "onvif_utils.h"

namespace ma::node {

static constexpr char TAG[] = "ma::node::onvif";

namespace onvif {

// ---------------------------------------------------------------- xml glue

static const char ENVELOPE_OPEN[] =
    "<?xml version=\"1.0\" encoding=\"UTF-8\"?>"
    "<s:Envelope xmlns:s=\"http://www.w3.org/2003/05/soap-envelope\" "
    "xmlns:tds=\"http://www.onvif.org/ver10/device/wsdl\" "
    "xmlns:tt=\"http://www.onvif.org/ver10/schema\" "
    "xmlns:trt=\"http://www.onvif.org/ver10/media/wsdl\">";

static std::string envelope(const std::string& body) {
    std::string out = ENVELOPE_OPEN;
    out += "<s:Body>";
    out += body;
    out += "</s:Body></s:Envelope>";
    return out;
}

static std::string fault(const char* subcode, const char* text) {
    char buf[512];
    snprintf(buf,
             sizeof(buf),
             "<s:Fault><s:Code><s:Value>s:Sender</s:Value>"
             "<s:Subcode><s:Value xmlns:ter=\"http://www.onvif.org/ver10/error\">ter:%s</s:Value></s:Subcode></s:Code>"
             "<s:Reason><s:Text xml:lang=\"en\">%s</s:Text></s:Reason></s:Fault>",
             subcode,
             text);
    return envelope(buf);
}

// ------------------------------------------------------------- device info

static std::string getServices() {
    const std::string xaddr = Service::instance().deviceServiceUrl();
    std::string out         = "<tds:GetServicesResponse>";
    const char* namespaces[] = {
        "http://www.onvif.org/ver10/device/wsdl",
        "http://www.onvif.org/ver10/media/wsdl",
    };
    for (const char* ns : namespaces) {
        out += "<tds:Service><tds:Namespace>";
        out += ns;
        out += "</tds:Namespace><tds:XAddr>";
        out += xaddr;
        out += "</tds:XAddr><tds:Version><tds:Major>2</tds:Major><tds:Minor>60</tds:Minor></tds:Version></tds:Service>";
    }
    out += "</tds:GetServicesResponse>";
    return envelope(out);
}

static std::string getCapabilities() {
    const std::string xaddr = Service::instance().deviceServiceUrl();
    // Built by concatenation: the full capabilities document exceeds 1KB and
    // snprintf into a fixed buffer silently truncates it into invalid XML,
    // which strict ONVIF clients (e.g. Hikvision NVRs) reject at device add.
    std::string out = "<tds:GetCapabilitiesResponse><tds:Capabilities>";
    out += "<tt:Media><tt:XAddr>" + xaddr + "</tt:XAddr><tt:StreamingUri></tt:StreamingUri>";
    out += "<tt:RTP_Multicast>false</tt:RTP_Multicast><tt:TCP>true</tt:TCP><tt:RTP_RTSP_TCP>true</tt:RTP_RTSP_TCP>";
    out += "<tt:Extension><tt:Profile>" + std::to_string(Service::instance().profiles().size()) + "</tt:Profile></tt:Extension></tt:Media>";
    out += "<tt:Device><tt:XAddr>" + xaddr + "</tt:XAddr>";
    out += "<tt:Network><tt:IPFilter>false</tt:IPFilter><tt:ZeroConfiguration>false</tt:ZeroConfiguration>";
    out += "<tt:IPVersion6>false</tt:IPVersion6><tt:DynDNS>false</tt:DynDNS></tt:Network>";
    out += "<tt:System><tt:DiscoveryResolve>false</tt:DiscoveryResolve><tt:DiscoveryBye>false</tt:DiscoveryBye>";
    out += "<tt:RemoteDiscovery>false</tt:RemoteDiscovery><tt:SystemBackup>false</tt:SystemBackup>";
    out += "<tt:SystemLogging>false</tt:SystemLogging><tt:Upgrade>false</tt:Upgrade>";
    out += "<tt:SupportedVersions><tt:Version><tt:Major>2</tt:Major><tt:Minor>60</tt:Minor></tt:Version></tt:SupportedVersions></tt:System>";
    out += "<tt:Security><tt:TLS1_1>false</tt:TLS1_1><tt:TLS1_2>false</tt:TLS1_2><tt:OnboardKeyGeneration>false</tt:OnboardKeyGeneration>";
    out += "<tt:AccessPolicyConfig>false</tt:AccessPolicyConfig><tt:DefaultAccessPolicy>true</tt:DefaultAccessPolicy>";
    out += "<tt:Dot1X>false</tt:Dot1X><tt:RemoteUserHandling>false</tt:RemoteUserHandling></tt:Security>";
    out += "</tt:Device></tds:Capabilities></tds:GetCapabilitiesResponse>";
    return envelope(out);
}

static std::string getSystemDateAndTime() {
    time_t now = time(nullptr);
    struct tm utcTm, locTm;
    gmtime_r(&now, &utcTm);
    localtime_r(&now, &locTm);

    char tz[32];
    long hours   = locTm.tm_gmtoff / 3600;
    long minutes = (locTm.tm_gmtoff % 3600) / 60;
    if (minutes < 0) {
        minutes = -minutes;
    }
    snprintf(tz, sizeof(tz), "UTC%+ld:%02ld", hours, minutes);

    char buf[1024];
    snprintf(buf,
             sizeof(buf),
             "<tds:GetSystemDateAndTimeResponse><tt:SystemDateTime>"
             "<tt:DateTimeType>Manual</tt:DateTimeType><tt:DaylightSavings>false</tt:DaylightSavings>"
             "<tt:TimeZone><tt:TZ>%s</tt:TZ></tt:TimeZone>"
             "<tt:UTCDateTime><tt:Time><tt:Hour>%d</tt:Hour><tt:Minute>%d</tt:Minute><tt:Second>%d</tt:Second></tt:Time>"
             "<tt:Date><tt:Year>%d</tt:Year><tt:Month>%d</tt:Month><tt:Day>%d</tt:Day></tt:Date></tt:UTCDateTime>"
             "<tt:LocalDateTime><tt:Time><tt:Hour>%d</tt:Hour><tt:Minute>%d</tt:Minute><tt:Second>%d</tt:Second></tt:Time>"
             "<tt:Date><tt:Year>%d</tt:Year><tt:Month>%d</tt:Month><tt:Day>%d</tt:Day></tt:Date></tt:LocalDateTime>"
             "</tt:SystemDateTime></tds:GetSystemDateAndTimeResponse>",
             tz,
             utcTm.tm_hour,
             utcTm.tm_min,
             utcTm.tm_sec,
             utcTm.tm_year + 1900,
             utcTm.tm_mon + 1,
             utcTm.tm_mday,
             locTm.tm_hour,
             locTm.tm_min,
             locTm.tm_sec,
             locTm.tm_year + 1900,
             locTm.tm_mon + 1,
             locTm.tm_mday);
    return envelope(buf);
}

static std::string getDeviceInformation() {
    char buf[768];
    snprintf(buf,
             sizeof(buf),
             "<tds:GetDeviceInformationResponse>"
             "<tds:Manufacturer>Seeed</tds:Manufacturer>"
             "<tds:Model>%s</tds:Model>"
             "<tds:FirmwareVersion>%s</tds:FirmwareVersion>"
             "<tds:SerialNumber>%s</tds:SerialNumber>"
             "<tds:HardwareId>SG2002</tds:HardwareId>"
             "</tds:GetDeviceInformationResponse>",
             model().c_str(),
             firmwareVersion().c_str(),
             serialNumber().c_str());
    return envelope(buf);
}

static std::string getHostname() {
    char buf[512];
    snprintf(buf,
             sizeof(buf),
             "<tds:GetHostnameResponse><tt:HostnameInformation>"
             "<tt:FromDHCP>true</tt:FromDHCP><tt:Name>%s</tt:Name>"
             "</tt:HostnameInformation></tds:GetHostnameResponse>",
             hostname().c_str());
    return envelope(buf);
}

static std::string getEndpointReference() {
    char buf[512];
    snprintf(buf,
             sizeof(buf),
             "<tds:GetEndpointReferenceResponse>"
             "<wsa:EndpointReference xmlns:wsa=\"http://schemas.xmlsoap.org/ws/2004/08/addressing\">"
             "<wsa:Address>%s</wsa:Address></wsa:EndpointReference>"
             "</tds:GetEndpointReferenceResponse>",
             endpointUUID().c_str());
    return envelope(buf);
}

static std::string getUsers() {
    std::string out = "<tds:GetUsersResponse>";
    for (const Profile& p : Service::instance().profiles()) {
        out += "<tt:User><tt:Username>";
        out += p.username;
        out += "</tt:Username><tt:UserLevel>Administrator</tt:UserLevel></tt:User>";
    }
    out += "</tds:GetUsersResponse>";
    return envelope(out);
}

// ------------------------------------------------------------ media service

static std::string videoEncoderConfig(const Profile& p) {
    char buf[1024];
    snprintf(buf,
             sizeof(buf),
             "<tt:VideoEncoderConfiguration token=\"%s_vec\">"
             "<tt:Name>%s</tt:Name><tt:UseCount>1</tt:UseCount>"
             "<tt:Encoding>H264</tt:Encoding>"
             "<tt:Resolution><tt:Width>%d</tt:Width><tt:Height>%d</tt:Height></tt:Resolution>"
             "<tt:Quality>5</tt:Quality>"
             "<tt:RateControl><tt:FrameRateLimit>%d</tt:FrameRateLimit><tt:EncodingInterval>1</tt:EncodingInterval><tt:BitrateLimit>4096</tt:BitrateLimit></tt:RateControl>"
             "<tt:H264><tt:GovLength>30</tt:GovLength><tt:H264Profile>Main</tt:H264Profile></tt:H264>"
             "<tt:SessionTimeout>PT60S</tt:SessionTimeout>"
             "</tt:VideoEncoderConfiguration>",
             p.token.c_str(),
             p.token.c_str(),
             p.width,
             p.height,
             p.fps);
    return buf;
}

static std::string getProfiles() {
    std::string out = "<trt:GetProfilesResponse>";
    for (const Profile& p : Service::instance().profiles()) {
        char head[512];
        snprintf(head,
                 sizeof(head),
                 "<trt:Profiles fixed=\"true\" token=\"%s\">"
                 "<tt:Name>%s</tt:Name>"
                 "<tt:VideoSourceConfiguration token=\"%s_vsc\">"
                 "<tt:Name>%s</tt:Name><tt:UseCount>1</tt:UseCount>"
                 "<tt:SourceToken>video_src_0</tt:SourceToken>"
                 "<tt:Bounds x=\"0\" y=\"0\" width=\"%d\" height=\"%d\"/>"
                 "</tt:VideoSourceConfiguration>",
                 p.token.c_str(),
                 p.token.c_str(),
                 p.token.c_str(),
                 p.token.c_str(),
                 p.width,
                 p.height);
        out += head;
        out += videoEncoderConfig(p);
        out += "</trt:Profiles>";
    }
    out += "</trt:GetProfilesResponse>";
    return envelope(out);
}

static std::string getStreamUri(const std::string& body) {
    std::string token = xmlText(body, "ProfileToken");
    const std::string ip = localIP();
    for (const Profile& p : Service::instance().profiles()) {
        if (p.token == token) {
            char buf[512];
            snprintf(buf,
                     sizeof(buf),
                     "<trt:GetStreamUriResponse><trt:MediaUri>"
                     "<tt:Uri>rtsp://%s:%d%s</tt:Uri>"
                     "<tt:InvalidAfterConnect>false</tt:InvalidAfterConnect>"
                     "<tt:InvalidAfterReboot>false</tt:InvalidAfterReboot>"
                     "<tt:Timeout>PT60S</tt:Timeout>"
                     "</trt:MediaUri></trt:GetStreamUriResponse>",
                     ip.c_str(),
                     p.rtspPort,
                     p.rtspPath.c_str());
            return envelope(buf);
        }
    }
    return fault("InvalidArgVal", "No such profile");
}

static std::string getVideoEncoderConfiguration(const std::string& body) {
    std::string token = xmlText(body, "ConfigurationToken");
    if (token.empty()) {
        token = xmlText(body, "ProfileToken");
    }
    // accept both the raw session token and the suffixed config token
    size_t cut = token.rfind("_vec");
    if (cut != std::string::npos) {
        token = token.substr(0, cut);
    }
    for (const Profile& p : Service::instance().profiles()) {
        if (p.token == token) {
            std::string out = "<trt:GetVideoEncoderConfigurationResponse>";
            out += videoEncoderConfig(p);
            out += "</trt:GetVideoEncoderConfigurationResponse>";
            return envelope(out);
        }
    }
    return fault("InvalidArgVal", "No such configuration");
}

static std::string getGuaranteedNumberOfVideoEncoderInstances() {
    return envelope("<trt:GetGuaranteedNumberOfVideoEncoderInstancesResponse>"
                    "<trt:TotalNumber>1</trt:TotalNumber>"
                    "</trt:GetGuaranteedNumberOfVideoEncoderInstancesResponse>");
}

// ------------------------------------------------------------------ auth

// WS-UsernameToken: Password is either plaintext or
// Base64(SHA1(Nonce_raw + Created + password)). Returns true when the
// credentials in `body` match any registered profile.
static bool checkAuth(const std::string& body) {
    std::string username = xmlText(body, "Username");
    std::string nonceB64 = xmlText(body, "Nonce");
    std::string created  = xmlText(body, "Created");
    std::string password = xmlText(body, "Password");

    if (username.empty()) {
        return false;
    }

    for (const Profile& p : Service::instance().profiles()) {
        if (p.username != username) {
            continue;
        }
        if (password == p.password) {
            return true;  // PasswordText
        }
        // PasswordDigest
        std::string nonce(base64Decode(nonceB64));
        unsigned char digest[20];
        sha1(nonce + created + p.password, digest);
        if (base64Encode(digest, 20) == password) {
            return true;
        }
    }
    return false;
}

// --------------------------------------------------------------- dispatch

static const char* const ACTIONS[] = {
    // longest first so body scanning never matches a prefix of another
    "GetGuaranteedNumberOfVideoEncoderInstances",
    "GetVideoEncoderConfiguration",
    "GetSystemDateAndTime",
    "GetDeviceInformation",
    "GetEndpointReference",
    "GetStreamUri",
    "GetCapabilities",
    "GetProfiles",
    "GetHostname",
    "GetServices",
    "GetUsers",
};

static const char* const FREE_ACTIONS[] = {
    "GetSystemDateAndTime",
    "GetCapabilities",
    "GetServices",
    "GetHostname",
    "GetEndpointReference",
};

static bool inList(const char* const* list, size_t n, const std::string& action) {
    for (size_t i = 0; i < n; ++i) {
        if (action == list[i]) {
            return true;
        }
    }
    return false;
}

// Extracts the operation name from an "http://.../wsdl/GetProfiles"-style
// action URI (quotes, whitespace and Content-Type parameters tolerated).
static std::string actionFromHeader(std::string header) {
    size_t pos = header.find("action=");
    if (pos != std::string::npos) {
        header = header.substr(pos + 7);
    }
    if (!header.empty() && (header.front() == '"' || header.front() == '\'')) {
        header = header.substr(1);
    }
    pos = header.find_last_of("/: ");
    if (pos != std::string::npos && pos + 1 < header.size()) {
        header = header.substr(pos + 1);
    }
    while (!header.empty() && (header.back() == '"' || header.back() == '\'' || header.back() == ';' || header.back() == ' ')) {
        header.pop_back();
    }
    return header;
}

static std::string dispatch(const std::string& action, const std::string& body) {
    if (action == "GetServices") {
        return getServices();
    }
    if (action == "GetCapabilities") {
        return getCapabilities();
    }
    if (action == "GetSystemDateAndTime") {
        return getSystemDateAndTime();
    }
    if (action == "GetDeviceInformation") {
        return getDeviceInformation();
    }
    if (action == "GetHostname") {
        return getHostname();
    }
    if (action == "GetEndpointReference") {
        return getEndpointReference();
    }
    if (action == "GetUsers") {
        return getUsers();
    }
    if (action == "GetProfiles") {
        return getProfiles();
    }
    if (action == "GetStreamUri") {
        return getStreamUri(body);
    }
    if (action == "GetVideoEncoderConfiguration") {
        return getVideoEncoderConfiguration(body);
    }
    if (action == "GetGuaranteedNumberOfVideoEncoderInstances") {
        return getGuaranteedNumberOfVideoEncoderInstances();
    }
    return fault("ActionNotSupported", "Action not supported");
}

std::string handleSoap(const std::string& body, const std::string& soapAction, const std::string& contentType) {
    std::string action = actionFromHeader(soapAction);
    if (action.empty() || !inList(ACTIONS, sizeof(ACTIONS) / sizeof(ACTIONS[0]), action)) {
        action.clear();
    }

    if (action.empty()) {
        // fall back to the operation element inside the body
        for (const char* candidate : ACTIONS) {
            std::string needle = ":";
            needle += candidate;
            size_t pos = body.find(needle);
            while (pos != std::string::npos) {
                char next = pos + needle.size() < body.size() ? body[pos + needle.size()] : '\0';
                if (next == '>' || next == ' ' || next == '/' || next == '\n' || next == '\r' || next == '\t') {
                    action = candidate;
                    break;
                }
                pos = body.find(needle, pos + needle.size());
            }
            if (!action.empty()) {
                break;
            }
        }
    }

    if (action.empty()) {
        MA_LOGW(TAG, "soap request with unknown action");
        return fault("ActionNotSupported", "Action not supported");
    }

    if (!inList(FREE_ACTIONS, sizeof(FREE_ACTIONS) / sizeof(FREE_ACTIONS[0]), action)) {
        if (!checkAuth(body)) {
            MA_LOGW(TAG, "not authorized: %s", action.c_str());
            return fault("NotAuthorized", "Sender not authorized");
        }
    }

    MA_LOGD(TAG, "soap action: %s", action.c_str());
    return dispatch(action, body);
}

}  // namespace onvif

}  // namespace ma::node
