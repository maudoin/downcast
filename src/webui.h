#include "hv/WebSocketServer.h"
#include "applogic.h"
#include <memory>

class WebUi
{
public:
    WebUi(std::shared_ptr<AppLogic> const& applogic, int port=9999);
    ~WebUi();
    void start();
    void stop();
private:

    HttpService _http;
    WebSocketService _ws;
    websocket_server_t _server;
};
