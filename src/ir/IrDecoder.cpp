#include "IrDecoder.h"
#include <cmath>
#include <juce_audio_formats/juce_audio_formats.h>
#include <juce_cryptography/juce_cryptography.h>
namespace ch
{
namespace
{
DecodeResult fail(const char *message)
{
    return {nullptr, message};
}
DecodeResult finish(std::shared_ptr<PreparedIr> ir, const IrRequest &req)
{
    const int fullN = ir->samples.getNumSamples(), channels = ir->samples.getNumChannels();
    const auto start = std::int64_t(std::llround(req.startMs * req.sampleRate / 1000));
    const auto end = req.endMs > 0 ? std::int64_t(std::llround(req.endMs * req.sampleRate / 1000)) : fullN;
    if (start < 0 || end > fullN || end <= start)
        return fail("IR trim must select a non-empty region inside the file");
    ir->sourceDurationSeconds = fullN / req.sampleRate;
    ir->sourceStartMs = double(start) * 1000 / req.sampleRate;
    const int n = int(end - start);
    if (start != 0 || n != fullN)
    {
        juce::AudioBuffer<float> trimmed(channels, n);
        for (int c = 0; c < channels; ++c)
            trimmed.copyFrom(c, 0, ir->samples, c, int(start), n);
        ir->samples = std::move(trimmed);
    }
    const int inN = int(std::min(req.fadeInMs * req.sampleRate / 1000, double(n))),
              outN = int(std::min(req.fadeOutMs * req.sampleRate / 1000, double(n)));
    for (int i = 0; i < n; ++i)
    {
        float gain = 1;
        if (inN > 0 && i < inN)
            gain *= float(i) / float(std::max(1, inN - 1));
        if (outN > 0 && i >= n - outN)
            gain *= float(n - 1 - i) / float(std::max(1, outN - 1));
        for (int c = 0; c < channels; ++c)
            ir->samples.setSample(c, i, ir->samples.getSample(c, i) * gain);
    }
    float peak = 0;
    for (int c = 0; c < channels; ++c)
        peak = std::max(peak, ir->samples.getMagnitude(c, 0, n));
    if (req.normalise && peak > 0)
        ir->samples.applyGain(1 / peak);
    const int bins = std::min(1024, n);
    ir->waveform.assign(std::size_t(bins), 0);
    ir->decayDb.assign(std::size_t(bins), -120);
    ir->channelWaveforms.assign(std::size_t(channels), std::vector<float>(std::size_t(bins), 0));
    std::vector<double> energy(std::size_t(bins), 0);
    for (int i = 0; i < n; ++i)
    {
        const auto bin = std::size_t(std::min(bins - 1, int(std::int64_t(i) * bins / n)));
        for (int c = 0; c < channels; ++c)
        {
            const float s = ir->samples.getSample(c, i);
            if (!std::isfinite(s))
                return fail("IR processing produced invalid samples");
            ir->waveform[bin] = std::max(ir->waveform[bin], std::abs(s));
            ir->channelWaveforms[std::size_t(c)][bin] =
                std::max(ir->channelWaveforms[std::size_t(c)][bin], std::abs(s));
            energy[bin] += double(s) * s;
        }
    }
    for (int i = bins - 2; i >= 0; --i)
        energy[std::size_t(i)] += energy[std::size_t(i + 1)];
    const double total = energy[0];
    for (int i = 0; i < bins; ++i)
        ir->decayDb[std::size_t(i)] =
            total > 0 ? float(10 * std::log10(std::max(1.e-12, energy[std::size_t(i)] / total))) : -120.f;
    ir->estimatedBytes = IrDecoder::estimateBytes(n, channels, 1024);
    return {std::move(ir), {}};
}
} // namespace
std::size_t IrDecoder::estimateBytes(int samples, int routes, int blockSize)
{
    if (samples < 1 || routes < 1 || blockSize < 1)
        return budgetBytes + 1;
    return std::size_t(samples) * std::size_t(routes) * 64ull +
           std::size_t(routes) * std::size_t(blockSize) * 256ull + 4ull * 1024 * 1024;
}
DecodeResult IrDecoder::decode(const IrRequest &req) const
{
    if (!std::isfinite(req.sampleRate) || req.sampleRate < 8000 || req.sampleRate > 384000)
        return fail("Unsupported sample rate");
    for (double v : {req.startMs, req.endMs, req.fadeInMs, req.fadeOutMs})
        if (!std::isfinite(v) || v < 0 || v > 30000)
            return fail("Invalid IR selection");
    if (!req.additionalFiles.empty())
    {
        if (req.additionalFiles.size() != 3)
            return fail("True stereo requires four mono WAV files");
        std::vector<juce::File> files{req.file};
        files.insert(files.end(), req.additionalFiles.begin(), req.additionalFiles.end());
        std::vector<std::shared_ptr<const PreparedIr>> decoded;
        int n = 0;
        juce::String hashes;
        for (const auto &file : files)
        {
            IrRequest child;
            child.file = file;
            child.sampleRate = req.sampleRate;
            auto result = decode(child);
            if (!result)
                return result;
            if (result.ir->samples.getNumChannels() != 1)
                return fail("Each true-stereo source file must be mono");
            n = std::max(n, result.ir->samples.getNumSamples());
            hashes += result.ir->hash;
            decoded.push_back(result.ir);
        }
        if (estimateBytes(n, 4, 1024) > budgetBytes)
            return fail("IR exceeds the 512 MiB engine budget");
        auto ir = std::make_shared<PreparedIr>();
        ir->file = req.file;
        ir->sampleRate = req.sampleRate;
        ir->originalSampleRate = decoded[0]->originalSampleRate;
        ir->samples.setSize(4, n);
        ir->samples.clear();
        for (int c = 0; c < 4; ++c)
        {
            const auto &source = *decoded[std::size_t(c)];
            ir->samples.copyFrom(c, 0, source.samples, 0, 0, source.samples.getNumSamples());
            ir->sourceHashes.add(source.hash);
        }
        ir->hash = juce::SHA256(hashes.toRawUTF8(), std::size_t(hashes.getNumBytesAsUTF8())).toHexString();
        return finish(std::move(ir), req);
    }
    if (!req.file.hasFileExtension("wav"))
        return fail("Select a WAV impulse response");
    juce::WavAudioFormat fmt;
    auto stream = req.file.createInputStream();
    if (!stream)
        return fail("Impulse file is missing or unreadable; locate it again");
    std::unique_ptr<juce::AudioFormatReader> reader(fmt.createReaderFor(stream.release(), true));
    if (!reader || reader->lengthInSamples < 1 || reader->numChannels < 1 || reader->numChannels > 16 ||
        !std::isfinite(reader->sampleRate) || reader->sampleRate < 8000 || reader->sampleRate > 384000)
        return fail("Invalid or unsupported WAV impulse response");
    if (reader->lengthInSamples / reader->sampleRate > 30.000001)
        return fail("IR exceeds the 30 second limit");
    const int sourceN = int(reader->lengthInSamples), channels = int(reader->numChannels),
              destN = int(std::ceil(sourceN * req.sampleRate / reader->sampleRate));
    if (destN < 1 || estimateBytes(destN, channels, 1024) > budgetBytes)
        return fail("IR exceeds the 512 MiB engine budget");
    juce::AudioBuffer<float> source(channels, sourceN + 8);
    source.clear();
    if (!reader->read(&source, 0, sourceN, 0, true, true))
        return fail("Could not decode WAV samples");
    for (int c = 0; c < channels; ++c)
        for (int i = 0; i < sourceN; ++i)
            if (!std::isfinite(source.getSample(c, i)))
                return fail("IR contains non-finite samples");
    auto ir = std::make_shared<PreparedIr>();
    ir->file = req.file;
    ir->sampleRate = req.sampleRate;
    ir->originalSampleRate = reader->sampleRate;
    ir->samples.setSize(channels, destN);
    if (req.sampleRate == reader->sampleRate)
    {
        for (int c = 0; c < channels; ++c)
            ir->samples.copyFrom(c, 0, source, c, 0, destN);
    }
    else
    {
        // All resampling and anti-alias filtering run on the loader worker.
        juce::MemoryAudioSource memory(source, false, false);
        juce::ResamplingAudioSource resampler(&memory, false, channels);
        resampler.setResamplingRatio(reader->sampleRate / req.sampleRate);
        resampler.prepareToPlay(4096, req.sampleRate);
        for (int offset = 0; offset < destN; offset += 4096)
            resampler.getNextAudioBlock({&ir->samples, offset, std::min(4096, destN - offset)});
        resampler.releaseResources();
        // An IR is a filter kernel: preserve its sum (DC gain) across sample rates.
        ir->samples.applyGain(float(reader->sampleRate / req.sampleRate));
    }
    auto hashStream = req.file.createInputStream();
    if (!hashStream)
        return fail("IR became unreadable while loading");
    ir->hash = juce::SHA256(*hashStream).toHexString();
    ir->sourceHashes.add(ir->hash);
    return finish(std::move(ir), req);
}
} // namespace ch
