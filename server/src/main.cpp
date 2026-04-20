#include <drogon/drogon.h>

int main(int argc, char* argv[]) {
  drogon::app().loadConfigFile("config/config.example.json");
  drogon::app().registerHandler(
      "/healthz",
      [](const drogon::HttpRequestPtr&,
         std::function<void(const drogon::HttpResponsePtr&)>&& callback) {
        Json::Value body(Json::objectValue);
        body["status"] = "ok";
        body["transport"] = "http";
        body["httpsReady"] = true;
        callback(drogon::HttpResponse::newHttpJsonResponse(body));
      },
      {drogon::Get});
  drogon::app().run();
  return 0;
}
