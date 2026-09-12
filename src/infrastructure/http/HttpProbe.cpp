// ~/AgenAI/frans/src/infrastructure/http/HttpProbe.cpp
#include "infrastructure/http/HttpProbe.h"

#include "httplib.h"

namespace frans::infrastructure {

HttpProbeResult httpGet(const std::string& host,
                        int port,
                        const std::string& path,
                        int timeout_sec) {
    HttpProbeResult r;

    httplib::Client cli(host, port);

    // Jangan follow redirect, jangan retry — kita cuma mau snapshot.
    cli.set_follow_location(false);
    cli.set_connection_timeout(0, 500000);            // 500 ms
    cli.set_read_timeout(timeout_sec > 0 ? timeout_sec : 3, 0);
    cli.set_write_timeout(1, 0);
    cli.set_keep_alive(false);

    const auto res = cli.Get(path.c_str());
    if (!res) {
        r.error = httplib::to_string(res.error());
        return r;
    }

    r.reachable   = true;
    r.status_code = res->status;
    r.body        = res->body;
    return r;
}

} // namespace frans::infrastructure
