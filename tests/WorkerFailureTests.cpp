#include "RealtimeGuard.h"
#include "TestSupport.h"
#include "WavFixtures.h"
#include "ir/IrLibrary.h"
#include "ir/IrLoader.h"
#include <chrono>
namespace test
{
int runWorkerAllocationProbe()
{
    allocationMainThread = std::this_thread::get_id();
    try
    {
        TempDir tmp;
        juce::AudioBuffer<float> ir(1, 1024);
        ir.clear();
        ir.setSample(0, 0, 0.25f);
        auto file = tmp.wav("known.wav", ir);
        {
            ch::IrLoader loader;
            loader.configure(48000, 256, 1);
            auto wait = [&]
            {
                auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                while (loader.status().loading && std::chrono::steady_clock::now() < end)
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                REQUIRE(!loader.status().loading);
            };
            loader.request({file});
            wait();
            REQUIRE(loader.status().ir);
            juce::AudioBuffer<float> audio(1, 256);
            audio.clear();
            audio.setSample(0, 0, 1);
            loader.processWet(audio);
            failWorkerAllocationSize = 4096;
            loader.request({file});
            wait();
            REQUIRE(failWorkerAllocationSize.load() == 0);
            REQUIRE(loader.status().error.isNotEmpty());
            audio.clear();
            audio.setSample(0, 0, 1);
            loader.processWet(audio);
            near(audio.getSample(0, 0), 0.25f);
            loader.request({file});
            wait();
            REQUIRE(loader.status().error.isEmpty());
        }
        {
            ch::IrLibrary library;
            failWorkerAllocationSize = sizeof(ch::IrLibrary::Root);
            library.addRoot(tmp.dir, true);
            auto wait = [&]
            {
                auto end = std::chrono::steady_clock::now() + std::chrono::seconds(5);
                while (library.snapshot().scanning && std::chrono::steady_clock::now() < end)
                    std::this_thread::sleep_for(std::chrono::milliseconds(2));
                REQUIRE(!library.snapshot().scanning);
            };
            wait();
            REQUIRE(failWorkerAllocationSize.load() == 0);
            REQUIRE(library.snapshot().error.isNotEmpty());
            library.scanAsync();
            wait();
            REQUIRE(library.snapshot().error.isEmpty());
            REQUIRE(library.snapshot().entries.size() == 1);
        }
        std::cout << "2/2 workers survived allocation failures and recovered\n";
        return 0;
    }
    catch (const std::exception &e)
    {
        std::cerr << "Worker recovery probe failed: " << e.what() << '\n';
        return 1;
    }
}
} // namespace test
TEST(worker_allocation_failures_preserve_host_and_allow_retry)
{
    juce::ChildProcess child;
    REQUIRE(child.start(
        juce::StringArray{juce::File::getSpecialLocation(juce::File::currentExecutableFile).getFullPathName(),
                          "--worker-allocation-probe"}));
    REQUIRE(child.waitForProcessToFinish(15000));
    auto output = child.readAllProcessOutput();
    if (child.getExitCode() != 0)
        std::cerr << output;
    REQUIRE(child.getExitCode() == 0);
    REQUIRE(output.contains("2/2 workers survived"));
}
