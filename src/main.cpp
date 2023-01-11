#include "ui.h"
#include "webui.h"
#include "applogic.h"
#include <memory>

int main(int, char **)
{
  std::shared_ptr<AppLogic> app = std::make_shared<AppLogic>("castapod.db3");

  WebUi webui(app);
  std::thread t{[&]{
      try
      {
        webui.start();
      }
      catch (...)
      {

      };
  }};

  //regular ui
  runUI(app);

  webui.stop();
  return 0;
}
