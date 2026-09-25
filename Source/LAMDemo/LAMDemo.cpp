#include "Modules/ModuleManager.h"
#include "LAMBlueprintDemoTest.h"
class FLAMDemoModule : public FDefaultGameModuleImpl
{
    FTSTicker::FDelegateHandle TestTicker;

  public:
    void StartupModule() override
    {
        TestTicker = StartLAMBlueprintDemoTests();
    }
    void ShutdownModule() override
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TestTicker);
    }
};
IMPLEMENT_PRIMARY_GAME_MODULE(FLAMDemoModule, LAMDemo, "LAMDemo");
