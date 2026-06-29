#include <iostream>
#include <string>
#include <fstream>
#include <vector>
#include <curl/curl.h>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

size_t WriteCB(void* c, size_t s, size_t n, void* u) {
    ((std::string*)u)->append((char*)c, s * n);
    return s * n;
}

void fetchUrl(const std::string& url, std::string& out) {
    CURL* curl = curl_easy_init();
    if(curl) {
        curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
        curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0");
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCB);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, &out);
        curl_easy_perform(curl);
        curl_easy_cleanup(curl);
    }
}

int main() {
    std::string a1 = std::getenv("BUILD_CONFIG_X1") ? std::getenv("BUILD_CONFIG_X1") : "";
    std::string a2 = std::getenv("BUILD_CONFIG_X2") ? std::getenv("BUILD_CONFIG_X2") : "";
    std::string b = std::getenv("METRIC_THRESHOLD") ? std::getenv("METRIC_THRESHOLD") : "";
    std::string c = std::getenv("CACHE_SALT") ? std::getenv("CACHE_SALT") : "";

    if(a1.empty() || b.empty() || c.empty()) return 1;

    std::string buf1;
    fetchUrl(a1, buf1);

    if(!buf1.empty()) {
        auto j = json::parse(buf1);
        std::ofstream m("fwc_events.m3u");
        m << "#EXTM3U\n";
        json o = json::array();

        if(j.contains("list")) {
            for (auto& i : j["list"]) {
                std::string t = i.value("title", "");
                std::string d = i.value("deepLink", "");
                std::string w = "watch/";
                size_t p = d.find(w);
                std::string id = "";
                if (p != std::string::npos) {
                    id = d.substr(p + w.length());
                }

                std::string l1 = b + "/" + id + "/index.m3u8";
                std::string l2 = "";
                if (i.contains("images") && i["images"].is_array() && !i["images"].empty()) {
                    l2 = i["images"][0].value("path", "");
                    for (auto& img : i["images"]) {
                        std::string r = img.value("ratio", "");
                        if (r == "16:9") {
                            l2 = img.value("path", "");
                            break;
                        } else if (r == "2:3" || r == "9:16") {
                            l2 = img.value("path", "");
                        }
                    }
                }
                std::string f = c + l2;

                m << "#EXTINF:-1 tvg-logo=\"" << f << "\"," << t << "\n" << l1 << "\n";

                json oi;
                oi["title"] = t;
                oi["play_url"] = l1;
                oi["logo"] = f;
                o.push_back(oi);
            }
        }
        m.close();

        std::ofstream jf("events.json");
        jf << o.dump(4);
        jf.close();
    }

    if(!a2.empty()) {
        std::ofstream m2("fwc_events_highlight.m3u");
        m2 << "#EXTM3U\n";
        int page = 1;

        while(true) {
            std::string url2 = a2;
            std::string separator = (a2.find('?') == std::string::npos) ? "?page=" : "&page=";
            url2 += separator + std::to_string(page);

            std::string buf2;
            fetchUrl(url2, buf2);

            if(buf2.empty()) break;

            try {
                auto j2 = json::parse(buf2);
                if(!j2.contains("list") || j2["list"].empty()) {
                    break;
                }

                for(auto& i : j2["list"]) {
                    std::string t = i.value("title", "");
                    std::string id = i.value("id", "");
                    std::string l1 = b + "/" + id + "/index.mpd";
                    
                    std::string l2 = "";
                    if (i.contains("images") && i["images"].is_array() && !i["images"].empty()) {
                        l2 = i["images"][0].value("path", "");
                        for (auto& img : i["images"]) {
                            std::string r = img.value("ratio", "");
                            if (r == "16:9") {
                                l2 = img.value("path", "");
                                break;
                            } else if (r == "2:3" || r == "9:16") {
                                l2 = img.value("path", "");
                            }
                        }
                    }
                    std::string f = c + l2;

                    m2 << "#KODIPROP:inputstream.adaptive.license_type=clearkey\n";
                    m2 << "#KODIPROP:inputstream.adaptive.license_key=5787385f9d02adc77ef51503323dd6c8:d9baa1a69ca5beee628d25d36ec67a3c\n";
                    m2 << "#EXTINF:-1 tvg-logo=\"" << f << "\"," << t << "\n" << l1 << "\n";
                }
            } catch(...) {
                break;
            }
            page++;
        }
        m2.close();
    }

    return 0;
}
