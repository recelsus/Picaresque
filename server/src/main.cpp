#include <drogon/drogon.h>

#include <filesystem>

namespace {

std::filesystem::path ResolveConfigPath() {
  if (std::filesystem::exists("config/config.local.json")) {
    return "config/config.local.json";
  }
  if (std::filesystem::exists("server/config/config.local.json")) {
    return "server/config/config.local.json";
  }
  if (std::filesystem::exists("config/config.example.json")) {
    return "config/config.example.json";
  }
  return "server/config/config.example.json";
}

}  // namespace

int main(int argc, char* argv[]) {
  drogon::app().loadConfigFile(ResolveConfigPath().string());
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
