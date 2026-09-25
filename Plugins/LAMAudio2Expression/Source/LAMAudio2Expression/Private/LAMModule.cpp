#include "Modules/ModuleManager.h"
#include "Misc/QueuedThreadPool.h"
#include "Async/Async.h"
#include "LAMCore.h"
static FQueuedThreadPool *Pool = nullptr;
class FLAMModule : public IModuleInterface
{
    void StartupModule() override
    {
        Pool = FQueuedThreadPool::Allocate();
        Pool->Create(1, 512 * 1024, TPri_BelowNormal, TEXT("LAMInference"));
    }
    void ShutdownModule() override
    {
        if (Pool)
        {
            Pool->Destroy();
            delete Pool;
            Pool = nullptr;
        }
    }
};
void LAM::Queue(TFunction<void()> Task)
{
    AsyncPool(*Pool, MoveTemp(Task));
}
IMPLEMENT_MODULE(FLAMModule, LAMAudio2Expression)
