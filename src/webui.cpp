#include "webui.h"

#include "hv/WebSocketServer.h"
#include "hv/EventLoop.h"
#include "hv/htime.h"
#include "hv/hssl.h"

#define TEST_WSS 0

using namespace hv;


class MyContext {
public:
    MyContext() {
        printf("MyContext::MyContext()\n");
        timerID = INVALID_TIMER_ID;
    }
    ~MyContext() {
        printf("MyContext::~MyContext()\n");
    }

    int handleMessage(const WebSocketChannelPtr& channel,  const std::string& msg, enum ws_opcode opcode) {
        printf("onmessage(type=%s len=%d): %.*s\n", opcode == WS_OPCODE_TEXT ? "text" : "binary",
            (int)msg.size(), (int)msg.size(), msg.data());
        channel->send("echo:"+msg);
        return msg.size();
    }

    TimerID timerID;
};
const char* page = R"foo(
<!DOCTYPE html>
<html lang = "en">
   <head>
      <meta charset = utf-8>
      <title>Downcast</title>

      <body>

         <div>

            <style>

               #status {
                  padding: 5px;
                  color: #fff;
                  background: #ccc;
               }

               #status.fail {
                  background: #c00;
               }

               #status.success {
                  background: #0c0;
               }

               #status.offline {
                  background: #c00;
               }

               #status.online {
                  background: #0c0;
               }

            </style>

            <div>
               <form onsubmit = "addMessage(); return false;">
                  <input type = "text" id = "sendInput" placeholder = "Type and press enter to send" />
               </form>
               <p id = "status">Not connected</p>
               <ul id = "log"></ul>
            </div>

            <script>
               log = document.getElementById("log");
               sendInput = document.getElementById("sendInput");
               form = sendInput.form;
               state = document.getElementById("status");

               if (window.WebSocket === undefined) {
                  state.innerHTML = "sockets not supported";
                  state.className = "fail";
               }else {
                  if (typeof String.prototype.startsWith != "function") {
                     String.prototype.startsWith = function (str) {
                        return this.indexOf(str) == 0;
                     };
                  }

                  window.addEventListener("load", onLoad, false);
               }

               function onLoad() {
                  var wsUri = "ws://127.0.0.1:9999";
                  websocket = new WebSocket(wsUri);
                  websocket.onopen = function(evt) { onOpen(evt) };
                  websocket.onclose = function(evt) { onClose(evt) };
                  websocket.onmessage = function(evt) { onMessage(evt) };
                  websocket.onerror = function(evt) { onError(evt) };
               }

               function onOpen(evt) {
                  state.className = "success";
                  state.innerHTML = "Connected to server";
               }

               function onClose(evt) {
                  state.className = "fail";
                  state.innerHTML = "Not connected";
               }

               function onMessage(evt) {
                  var message = evt.data;
                  log.innerHTML = '<li class = "message">' +
                        message + "</li>" + log.innerHTML;
               }

               function onError(evt) {
                  state.className = "fail";
                  state.innerHTML = "Communication error";
               }

               function addMessage() {
                  var message = sendInput.value;
                  sendInput.value = "";
                  websocket.send(message);
               }

            </script>

         </div>

      </body>

   </head>

</html>
)foo";

WebUi::WebUi(std::shared_ptr<AppLogic> const& applogic, int port)
{

    _http.GET("/", [](const HttpContextPtr& ctx) {
        return ctx->send(page, TEXT_HTML);
    });

    _ws.onopen = [](const WebSocketChannelPtr& channel, const HttpRequestPtr& req) {
        printf("onopen: GET %s\n", req->Path().c_str());
        auto ctx = channel->newContextPtr<MyContext>();
        // send(time) every 1s
        ctx->timerID = setInterval(1000, [channel](TimerID id) {
            if (channel->isConnected() && channel->isWriteComplete()) {
                char str[DATETIME_FMT_BUFLEN] = {0};
                datetime_t dt = datetime_now();
                datetime_fmt(&dt, str);
                channel->send(str);
            }
        });
    };
    _ws.onmessage = [](const WebSocketChannelPtr& channel, const std::string& msg) {
        auto ctx = channel->getContextPtr<MyContext>();
        ctx->handleMessage(channel, msg, channel->opcode);
    };
    _ws.onclose = [](const WebSocketChannelPtr& channel) {
        printf("onclose\n");
        auto ctx = channel->getContextPtr<MyContext>();
        if (ctx->timerID != INVALID_TIMER_ID) {
            killTimer(ctx->timerID);
            ctx->timerID = INVALID_TIMER_ID;
        }
        // channel->deleteContextPtr();
    };

    _server.port = port;
#if TEST_WSS
    server.https_port = port + 1;
    hssl_ctx_init_param_t param;
    memset(&param, 0, sizeof(param));
    param.crt_file = "cert/server.crt";
    param.key_file = "cert/server.key";
    param.endpoint = HSSL_SERVER;
    if (hssl_ctx_init(&param) == NULL) {
        fprintf(stderr, "hssl_ctx_init failed!\n");
        return -20;
    }
#endif
    _server.service = &_http;
    _server.ws = &_ws;
}

void WebUi::start()
{
 websocket_server_run(&_server, 1);
}
void WebUi::stop()
{
 websocket_server_stop(&_server);
}

WebUi::~WebUi()
{
    websocket_server_stop(&_server);
}
