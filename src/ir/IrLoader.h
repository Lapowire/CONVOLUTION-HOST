#pragma once
#include "IrDecoder.h"
#include "audio/ConvolutionEngine.h"
#include <array>
#include <atomic>
#include <condition_variable>
#include <mutex>
#include <optional>
#include <thread>
namespace ch
{
class IrLoader
{
  public:
    struct Status
    {
        bool loading = false;
        juce::String error;
        std::shared_ptr<const PreparedIr> ir;
        std::uint64_t requestId = 0;
    };
    IrLoader();
    ~IrLoader();
    // configure/reset require audio processing to be suspended by the caller.
    void configure(double sampleRate, int blockSize, int channels);
    std::uint64_t request(IrRequest, RoutingModel = {}, std::optional<BusLayout> = {});
    std::uint64_t clear();
    void processWet(juce::AudioBuffer<float> &) noexcept;
    void reset() noexcept;
    bool hasActive() const noexcept { return activeFlag.load(std::memory_order_acquire); }
    double durationSeconds() const noexcept { return duration.load(std::memory_order_relaxed); }
    std::uint64_t activeRequest() const noexcept { return accepted.load(std::memory_order_acquire); }
    Status status() const;

  private:
    struct Packet
    {
        std::unique_ptr<ConvolutionEngine> engine;
        std::uint64_t id = 0, generation = 0;
    };
    struct Job
    {
        IrRequest request;
        RoutingModel routing;
        std::optional<BusLayout> layout;
        double rate = 48000;
        int block = 512, channels = 2;
        std::uint64_t id = 0, generation = 0;
    };
    void run();
    void destroy(Packet *);
    bool retire(Packet *) noexcept;
    void collect();
    mutable std::mutex mutex;
    std::condition_variable wake;
    std::optional<Job> pending;
    Status current;
    double rate = 48000;
    int blockSize = 512, channelCount = 2;
    bool stopping = false;
    std::atomic<std::uint64_t> latest{0}, generation{1}, accepted{0}, readyId{0}, clearRequest{0};
    std::atomic<Packet *> ready{nullptr};
    Packet *active = nullptr;
    Packet *fading = nullptr;
    std::array<Packet *, 16> retired{};
    std::atomic<unsigned> readIndex{0}, writeIndex{0};
    std::atomic<int> live{0};
    std::atomic<bool> activeFlag{false};
    std::atomic<double> duration{0};
    juce::AudioBuffer<float> oldBuffer;
    int fadePosition = 0, fadeLength = 2400;
    std::thread worker;
};
} // namespace ch
