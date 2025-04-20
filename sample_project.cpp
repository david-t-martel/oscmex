// main.cpp (v2)
// Main application entry point with dynamic filter support and OSC hooks

#include "asio_manager.h"
#include "ffmpeg_filter.h"
// #include "osc_manager.h" // Include header for OSC handling (if using a class)
#include <iostream>
#include <vector>
#include <string>
#include <thread>
#include <chrono>
#include <csignal>
#include <atomic>
#include <mutex> // For potential thread safety if needed directly in main
#include <any>   // For OSC message arguments

// --- Configuration ---
const char *TARGET_DEVICE_NAME = "ASIO Fireface UCX II";
// Example: EQ followed by a simple compressor
const std::string FILTER_CHAIN_DESC = "equalizer=f=1000:width_type=q:w=1.0:g=6[s1]; [s1]acompressor=threshold=0.1:ratio=4:attack=20:release=250";
// OSC Configuration (Example)
const int OSC_LISTEN_PORT = 9001;
const char *RME_OSC_IP = "127.0.0.1"; // Or the actual IP of the RME host if needed
const int RME_OSC_PORT = 9000;        // Default TotalMix FX OSC port (check RME settings)
// ---------------------

std::atomic<bool> g_shutdown_requested(false);
// Forward declaration for OSC handler potentially needing FfmpegFilter
class FfmpegFilter;
// Placeholder for OSC Manager instance (if used)
// OscManager g_osc_manager;

void signal_handler(int signum)
{
    std::cout << "\nShutdown requested (Signal " << signum << ")...\n";
    g_shutdown_requested.store(true);
}

// --- Placeholder OSC Message Handler ---
// This function would be registered with the OSC library
// It receives OSC messages and triggers actions, like updating filters
void handle_osc_message(const std::string &address, const std::vector<std::any> &args, FfmpegFilter *filter_ptr)
{
    std::cout << "OSC Received: " << address;
    for (const auto &arg : args)
    {
        // Print args (requires type checking/casting from std::any)
        // Example for float:
        try
        {
            if (arg.type() == typeid(float))
            {
                std::cout << " " << std::any_cast<float>(arg);
            }
            else if (arg.type() == typeid(int))
            {
                std::cout << " " << std::any_cast<int>(arg);
            }
            else if (arg.type() == typeid(std::string))
            {
                std::cout << " \"" << std::any_cast<std::string>(arg) << "\"";
            }
            // Add other types as needed
        }
        catch (const std::bad_any_cast &e)
        {
            std::cout << " [?] ";
        }
    }
    std::cout << std::endl;

    if (!filter_ptr)
        return;

    // Example: Control EQ frequency via OSC
    if (address == "/filter/eq/freq" && !args.empty())
    {
        try
        {
            // Assuming the first argument is the frequency (e.g., float)
            float freq = std::any_cast<float>(args[0]);
            // Assuming the EQ filter instance was named "eq" implicitly or explicitly
            filter_ptr->updateFilterParameter("eq", "f", std::to_string(freq));
            std::cout << "  -> Updated EQ frequency to " << freq << "\n";
        }
        catch (const std::bad_any_cast &e)
        {
            std::cerr << "  -> Error casting OSC argument for /filter/eq/freq\n";
        }
    }
    // Example: Control EQ gain via OSC
    else if (address == "/filter/eq/gain" && !args.empty())
    {
        try
        {
            float gain = std::any_cast<float>(args[0]);
            filter_ptr->updateFilterParameter("eq", "g", std::to_string(gain));
            std::cout << "  -> Updated EQ gain to " << gain << "\n";
        }
        catch (const std::bad_any_cast &e)
        {
            std::cerr << "  -> Error casting OSC argument for /filter/eq/gain\n";
        }
    }
    // Example: Control Compressor threshold via OSC
    else if (address == "/filter/comp/threshold" && !args.empty())
    {
        try
        {
            float threshold = std::any_cast<float>(args[0]);
            // Assuming the compressor instance in the graph was named "comp" implicitly or explicitly
            filter_ptr->updateFilterParameter("comp", "threshold", std::to_string(threshold));
            std::cout << "  -> Updated Compressor threshold to " << threshold << "\n";
        }
        catch (const std::bad_any_cast &e)
        {
            std::cerr << "  -> Error casting OSC argument for /filter/comp/threshold\n";
        }
    }
    // Example: Send command to RME (Mute Output 1)
    else if (address == "/rme/output/1/mute_toggle")
    {
        std::cout << "  -> Sending Mute Toggle to RME Output 1 (Conceptual)\n";
        // Placeholder: Need RME OSC sending mechanism instance here
        // g_rme_osc_client.sendCommand(RME_OSC_IP, RME_OSC_PORT, "/1/mainMute", {}); // Example TotalMix command
    }
    // Add more handlers for other OSC addresses and filter parameters
}

// --- Placeholder OSC Listener Thread Function ---
void osc_listener_thread_func(FfmpegFilter *filter_ptr)
{
    std::cout << "OSC Listener thread started on port " << OSC_LISTEN_PORT << ".\n";
    // --- OSC Library Setup (e.g., oscpack) ---
    // 1. Create UDP socket bound to OSC_LISTEN_PORT
    // 2. Loop while !g_shutdown_requested:
    //    a. Wait for incoming UDP packet (blocking or with timeout)
    //    b. If packet received:
    //       i. Parse packet using OSC library (e.g., osc::ReceivedPacket)
    //       ii. If valid OSC message:
    //           - Get address string (e.g., p.AddressPattern())
    //           - Iterate through arguments (e.g., p.ArgumentStream())
    //           - Store arguments (e.g., in std::vector<std::any>)
    //           - Call handle_osc_message(address, args, filter_ptr)
    //    c. Handle errors (socket errors, parsing errors)
    // 3. Close socket on exit
    // --- End OSC Library Setup ---

    while (!g_shutdown_requested.load())
    {
        // Placeholder: Simulate receiving messages periodically for testing
        std::this_thread::sleep_for(std::chrono::seconds(5));
        // Example: Simulate receiving an EQ gain change
        // if (!g_shutdown_requested.load()) {
        //     std::vector<std::any> args = { -2.0f }; // Example gain value
        //     handle_osc_message("/filter/eq/gain", args, filter_ptr);
        // }
    }
    std::cout << "OSC Listener thread finished.\n";
}

int main()
{
    std::cout << "Starting ASIO + FFmpeg Application (v2)...\n";
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);

    AsioManager asio_manager;
    FfmpegFilter ffmpeg_filter;
    // OscManager osc_manager; // If using a class

    long input_channels[2] = {-1, -1};
    long output_channels[2] = {-1, -1};
    long buffer_size = 0;
    double sample_rate = 0.0;
    ASIOSampleType sample_type = ASIOSTFloat32LSB; // Default assumption

    // --- ASIO Initialization ---
    if (!asio_manager.loadDriver(TARGET_DEVICE_NAME))
        return 1;
    if (!asio_manager.initDevice())
    {
        asio_manager.cleanup();
        return 1;
    }
    buffer_size = asio_manager.getBufferSize();
    sample_rate = asio_manager.getSampleRate();
    sample_type = asio_manager.getSampleType();
    std::cout << "ASIO Device Initialized: Rate=" << sample_rate << ", Size=" << buffer_size << ", Type=" << asio_manager.getSampleTypeName() << "\n";

    // Find channels (assuming names are correct)
    if (!asio_manager.findChannelIndices("Analog (1)", true, input_channels[0]) ||
        !asio_manager.findChannelIndices("Analog (2)", true, input_channels[1]) ||
        !asio_manager.findChannelIndices("Main (1)", false, output_channels[0]) ||
        !asio_manager.findChannelIndices("Main (2)", false, output_channels[1]))
    {
        std::cerr << "Error finding required ASIO channels.\n";
        asio_manager.cleanup();
        return 1;
    }
    std::cout << "Found Channels: In=" << input_channels[0] << "," << input_channels[1]
              << " Out=" << output_channels[0] << "," << output_channels[1] << "\n";

    // --- FFmpeg Initialization ---
    // Pass the filter chain description string
    if (!ffmpeg_filter.initGraph(sample_rate, buffer_size, sample_type, FILTER_CHAIN_DESC))
    {
        std::cerr << "Error initializing FFmpeg filter graph." << std::endl;
        asio_manager.cleanup();
        return 1;
    }
    std::cout << "FFmpeg Filter Graph Initialized with chain: " << FILTER_CHAIN_DESC << "\n";

    // --- OSC Initialization & Listener Thread ---
    std::cout << "Starting OSC Listener Thread...\n";
    // Pass the ffmpeg_filter instance so the OSC handler can call its methods
    std::thread osc_thread(osc_listener_thread_func, &ffmpeg_filter);
    // --- Add OSC Manager Init here if using a class ---
    // if (!osc_manager.init(OSC_LISTEN_PORT, &ffmpeg_filter)) { ... handle error ... }
    // osc_manager.startListening();

    // --- Link ASIO and FFmpeg ---
    asio_manager.setProcessCallback(
        [&](const std::vector<const void *> &inputs, const std::vector<void *> &outputs, long frames)
        {
            if (inputs.size() >= 2 && outputs.size() >= 2)
            {
                if (!ffmpeg_filter.process(inputs[0], inputs[1], outputs[0], outputs[1], frames))
                {
                    std::cerr << "Error during FFmpeg processing!" << std::endl;
                }
            }
            else
            {
                std::cerr << "Error: Incorrect number of buffers passed to callback!\n";
            }
        });

    // --- Start Processing ---
    std::vector<long> active_input_indices = {input_channels[0], input_channels[1]};
    std::vector<long> active_output_indices = {output_channels[0], output_channels[1]};
    if (!asio_manager.createBuffers(active_input_indices, active_output_indices))
    {
        std::cerr << "Error creating ASIO buffers." << std::endl;
        g_shutdown_requested.store(true); // Signal OSC thread to stop
        if (osc_thread.joinable())
            osc_thread.join();
        ffmpeg_filter.cleanup();
        asio_manager.cleanup();
        return 1;
    }
    if (!asio_manager.start())
    {
        std::cerr << "Error starting ASIO processing." << std::endl;
        g_shutdown_requested.store(true); // Signal OSC thread to stop
        if (osc_thread.joinable())
            osc_thread.join();
        ffmpeg_filter.cleanup();
        asio_manager.cleanup();
        return 1;
    }
    std::cout << "ASIO processing started. Press Ctrl+C to stop.\n";

    // --- Run Loop ---
    while (!g_shutdown_requested.load())
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(100));
        // Main thread can also send OSC messages here if needed
        // Example: Send a ping to RME every 10 seconds
        // static auto last_ping = std::chrono::steady_clock::now();
        // if (std::chrono::steady_clock::now() - last_ping > std::chrono::seconds(10)) {
        //     std::cout << "Sending OSC Ping to RME (Conceptual)\n";
        //     // osc_manager.send(RME_OSC_IP, RME_OSC_PORT, "/ping", {});
        //     last_ping = std::chrono::steady_clock::now();
        // }
    }

    // --- Cleanup ---
    std::cout << "Stopping ASIO processing...\n";
    asio_manager.stop();

    std::cout << "Stopping OSC Listener Thread...\n";
    // Signal OSC thread to exit (already done by g_shutdown_requested)
    // Ensure OSC socket is closed properly (implementation specific)
    if (osc_thread.joinable())
    {
        osc_thread.join();
    }
    // osc_manager.stopListening(); // If using a class

    std::cout << "Cleaning up resources...\n";
    ffmpeg_filter.cleanup();
    asio_manager.cleanup();
    // osc_manager.cleanup(); // If using a class

    std::cout << "Application finished.\n";
    return 0;
}

// =========================================================================
// asio_manager.h (v2 - No changes needed from v1 for this request)
// =========================================================================
#ifndef ASIO_MANAGER_H
#define ASIO_MANAGER_H

#include <vector>
#include <string>
#include <functional>

// --- CRITICAL: Include ASIO SDK headers here ---
// #include "asio.h"
// #include "asiodrivers.h"
// #include "asiosys.h"
// #include "asiolist.h"
// ---

// --- Placeholder ASIO Types (Replace with actual SDK types) ---
typedef long ASIOSampleType;
const ASIOSampleType ASIOSTFloat32LSB = 6;
const ASIOSampleType ASIOSTInt32LSB = 8;
struct ASIOBufferInfo
{
    void *buffers[2];
    long channelNum;
    bool isInput;
};
struct ASIOCallbacks
{
    void (*bufferSwitch)(long doubleBufferIndex, bool directProcess);
    void (*sampleRateDidChange)(double sRate);
    long (*asioMessage)(long selector, long value, void *message, double *opt);
    void *(*bufferSwitchTimeInfo)(void *params, long doubleBufferIndex, bool directProcess);
};
typedef long ASIOError;
const ASIOError ASE_OK = 0;
// --- End Placeholder ASIO Types ---

using AsioProcessCallback = std::function<void(const std::vector<const void *> &, const std::vector<void *> &, long)>;

class AsioManager
{
public:
    AsioManager();
    ~AsioManager();
    bool loadDriver(const std::string &deviceName);
    bool initDevice();
    bool createBuffers(const std::vector<long> &inputChannelIndices, const std::vector<long> &outputChannelIndices);
    bool start();
    void stop();
    void cleanup();
    long getBufferSize();
    double getSampleRate();
    ASIOSampleType getSampleType();
    std::string getSampleTypeName();
    bool findChannelIndices(const std::string &channelName, bool isInput, long &index); // Made const in v2 impl stub
    void setProcessCallback(AsioProcessCallback callback);

private:
    // Internal ASIO SDK function placeholders
    bool asioExit();
    bool asioInit(void *sysHandle);
    ASIOError asioGetChannels(long *numInputChannels, long *numOutputChannels);
    ASIOError asioGetBufferSize(long *minSize, long *maxSize, long *preferredSize, long *granularity);
    ASIOError asioCanSampleRate(double sampleRate);
    ASIOError asioGetSampleRate(double *sampleRate);
    ASIOError asioSetSampleRate(double sampleRate);
    ASIOError asioGetChannelInfo(long channelIndex, bool isInput, void *info); // info is ASIOChannelInfo*
    ASIOError asioCreateBuffers(ASIOBufferInfo *bufferInfos, long numChannels, long bufferSize, ASIOCallbacks *callbacks);
    ASIOError asioDisposeBuffers();
    ASIOError asioStart();
    ASIOError asioStop();
    ASIOError asioOutputReady();
    // Static callbacks delegating to instance
    static void bufferSwitchCallback(long doubleBufferIndex, bool directProcess);
    static void sampleRateDidChangeCallback(double sRate);
    static long asioMessageCallback(long selector, long value, void *message, double *opt);
    static void *bufferSwitchTimeInfoCallback(void *params, long doubleBufferIndex, bool directProcess);
    // Instance callback handlers
    void onBufferSwitch(long doubleBufferIndex, bool directProcess);
    void onSampleRateDidChange(double sRate);
    long onAsioMessage(long selector, long value, void *message, double *opt);
    // Member variables
    bool m_driverLoaded = false;
    bool m_asioInitialized = false;
    bool m_buffersCreated = false;
    bool m_processing = false;
    long m_inputChannels = 0;
    long m_outputChannels = 0;
    long m_bufferSize = 0;
    double m_sampleRate = 0.0;
    ASIOSampleType m_sampleType = ASIOSTFloat32LSB;
    std::vector<long> m_activeInputIndices;
    std::vector<long> m_activeOutputIndices;
    std::vector<ASIOBufferInfo> m_bufferInfos;
    long m_numActiveChannels = 0;
    std::vector<const void *> m_currentInputBuffers;
    std::vector<void *> m_currentOutputBuffers;
    AsioProcessCallback m_processCallback;
    static AsioManager *s_instance;
    void *m_asioDrivers = nullptr; // Placeholder for AsioDrivers*
};
#endif // ASIO_MANAGER_H

// =========================================================================
// asio_manager.cpp (v2 - No changes needed from v1 for this request's logic)
// =========================================================================
// Implementation remains the same as v1, using placeholders for actual ASIO calls.
// Ensure the findChannelIndices implementation is const if header is const.
// ... (Full implementation as provided in asio_ffmpeg_eq_code_v2) ...
#include "asio_manager.h"
#include <iostream>
#include <vector>
#include <map>
// --- CRITICAL: Include ASIO SDK headers here ---
// #include "asio.h"
// #include "asiodrivers.h"
// #include "asiosys.h"
// #include "asiolist.h"
// ---
AsioManager *AsioManager::s_instance = nullptr;
AsioManager::AsioManager() { s_instance = this; /* Init COM etc. */ }
AsioManager::~AsioManager()
{
    cleanup(); /* Uninit COM etc. */
    s_instance = nullptr;
}
// --- Placeholder Implementations (Simulated for Structure) ---
bool AsioManager::asioExit()
{
    std::cout << "Placeholder: ASIOExit()\n";
    return true;
}
bool AsioManager::asioInit(void *sysHandle)
{
    std::cout << "Placeholder: ASIOInit()\n";
    return true;
} // Simulate success
ASIOError AsioManager::asioGetChannels(long *numInputChannels, long *numOutputChannels)
{
    *numInputChannels = 8;
    *numOutputChannels = 8;
    std::cout << "Placeholder: ASIOGetChannels() -> 8/8\n";
    return ASE_OK;
}
ASIOError AsioManager::asioGetBufferSize(long *minSize, long *maxSize, long *preferredSize, long *granularity)
{
    *minSize = 64;
    *maxSize = 2048;
    *preferredSize = 512;
    *granularity = 64;
    std::cout << "Placeholder: ASIOGetBufferSize() -> 512\n";
    m_bufferSize = *preferredSize;
    return ASE_OK;
}
ASIOError AsioManager::asioCanSampleRate(double sampleRate) { return ASE_OK; } // Assume OK
ASIOError AsioManager::asioGetSampleRate(double *sampleRate)
{
    *sampleRate = 48000.0;
    std::cout << "Placeholder: ASIOGetSampleRate() -> 48000.0\n";
    m_sampleRate = *sampleRate;
    return ASE_OK;
}
ASIOError AsioManager::asioSetSampleRate(double sampleRate)
{
    m_sampleRate = sampleRate;
    return ASE_OK;
}
ASIOError AsioManager::asioGetChannelInfo(long channelIndex, bool isInput, void *info)
{ /* Placeholder: Fill info struct based on index/isInput */
    std::cout << "Placeholder: ASIOGetChannelInfo() for " << (isInput ? "In" : "Out") << channelIndex << "\n";
    return ASE_OK;
}
ASIOError AsioManager::asioCreateBuffers(ASIOBufferInfo *bufferInfos, long numChannels, long bufferSize, ASIOCallbacks *callbacks)
{
    std::cout << "Placeholder: ASIOCreateBuffers()\n"; /* Placeholder: Simulate filling bufferInfos[i].buffers[0/1] */
    return ASE_OK;
}
ASIOError AsioManager::asioDisposeBuffers()
{
    std::cout << "Placeholder: ASIODisposeBuffers()\n";
    return ASE_OK;
}
ASIOError AsioManager::asioStart()
{
    std::cout << "Placeholder: ASIOStart()\n";
    return ASE_OK;
}
ASIOError AsioManager::asioStop()
{
    std::cout << "Placeholder: ASIOStop()\n";
    return ASE_OK;
}
ASIOError AsioManager::asioOutputReady() { /* Placeholder: Signal ready */ return ASE_OK; }
// --- The rest of asio_manager.cpp from v1 ---
bool AsioManager::loadDriver(const std::string &deviceName)
{
    if (m_driverLoaded)
        return true;
    std::cout << "Loading ASIO drivers...\n";
    std::cout << "Placeholder: Simulating successful load of driver '" << deviceName << "'.\n";
    m_driverLoaded = true;
    return true;
}
bool AsioManager::initDevice()
{
    if (!m_driverLoaded)
        return false;
    if (m_asioInitialized)
        return true;
    void *sysHandle = nullptr;
    if (!asioInit(sysHandle))
    {
        std::cerr << "ASIOInit failed.\n";
        asioExit();
        return false;
    }
    ASIOError err = asioGetChannels(&m_inputChannels, &m_outputChannels);
    if (err != ASE_OK)
    {
        std::cerr << "ASIOGetChannels failed (Error: " << err << ").\n";
        asioExit();
        return false;
    }
    std::cout << "Device Channels: Inputs=" << m_inputChannels << ", Outputs=" << m_outputChannels << "\n";
    long minSize, maxSize, preferredSize, granularity;
    err = asioGetBufferSize(&minSize, &maxSize, &preferredSize, &granularity);
    if (err != ASE_OK)
    {
        std::cerr << "ASIOGetBufferSize failed (Error: " << err << ").\n";
        asioExit();
        return false;
    }
    m_bufferSize = preferredSize;
    err = asioGetSampleRate(&m_sampleRate);
    if (err != ASE_OK)
    {
        std::cerr << "ASIOGetSampleRate failed (Error: " << err << ").\n";
        asioExit();
        return false;
    }
    if (m_inputChannels > 0 || m_outputChannels > 0)
    {
        m_sampleType = ASIOSTFloat32LSB;
        std::cout << "Placeholder: Simulating sample type query - assuming Float32.\n";
    }
    else
    {
        std::cerr << "Device has no input or output channels.\n";
        asioExit();
        return false;
    }
    m_asioInitialized = true;
    return true;
}
bool AsioManager::findChannelIndices(const std::string &channelName, bool isInput, long &index)
{
    if (!m_asioInitialized)
        return false;
    long numChannels = isInput ? m_inputChannels : m_outputChannels;
    for (long i = 0; i < numChannels; ++i)
    {
        if (isInput && channelName == "Analog (1)" && i == 0)
        {
            index = 0;
            return true;
        }
        if (isInput && channelName == "Analog (2)" && i == 1)
        {
            index = 1;
            return true;
        }
        if (!isInput && channelName == "Main (1)" && i == 0)
        {
            index = 0;
            return true;
        }
        if (!isInput && channelName == "Main (2)" && i == 1)
        {
            index = 1;
            return true;
        }
    }
    index = -1;
    return false;
}
bool AsioManager::createBuffers(const std::vector<long> &inputChannelIndices, const std::vector<long> &outputChannelIndices)
{
    if (!m_asioInitialized)
        return false;
    if (m_buffersCreated)
        return true;
    m_activeInputIndices = inputChannelIndices;
    m_activeOutputIndices = outputChannelIndices;
    m_numActiveChannels = m_activeInputIndices.size() + m_activeOutputIndices.size();
    if (m_numActiveChannels == 0)
        return false;
    m_bufferInfos.resize(m_numActiveChannels);
    long currentBufferInfoIndex = 0;
    for (long channelIndex : m_activeInputIndices)
    {
        m_bufferInfos[currentBufferInfoIndex].isInput = true;
        m_bufferInfos[currentBufferInfoIndex].channelNum = channelIndex;
        m_bufferInfos[currentBufferInfoIndex].buffers[0] = nullptr;
        m_bufferInfos[currentBufferInfoIndex].buffers[1] = nullptr;
        currentBufferInfoIndex++;
    }
    for (long channelIndex : m_activeOutputIndices)
    {
        m_bufferInfos[currentBufferInfoIndex].isInput = false;
        m_bufferInfos[currentBufferInfoIndex].channelNum = channelIndex;
        m_bufferInfos[currentBufferInfoIndex].buffers[0] = nullptr;
        m_bufferInfos[currentBufferInfoIndex].buffers[1] = nullptr;
        currentBufferInfoIndex++;
    }
    ASIOCallbacks asioCallbacks;
    asioCallbacks.bufferSwitch = &AsioManager::bufferSwitchCallback;
    asioCallbacks.sampleRateDidChange = &AsioManager::sampleRateDidChangeCallback;
    asioCallbacks.asioMessage = &AsioManager::asioMessageCallback;
    asioCallbacks.bufferSwitchTimeInfo = (void *(*)(void *, long, bool)) & AsioManager::bufferSwitchTimeInfoCallback;
    ASIOError err = asioCreateBuffers(m_bufferInfos.data(), m_numActiveChannels, m_bufferSize, &asioCallbacks);
    if (err != ASE_OK)
    {
        std::cerr << "ASIOCreateBuffers failed (Error: " << err << ").\n";
        m_bufferInfos.clear();
        return false;
    }
    m_currentInputBuffers.resize(m_activeInputIndices.size());
    m_currentOutputBuffers.resize(m_activeOutputIndices.size());
    m_buffersCreated = true;
    std::cout << "ASIO buffers created successfully.\n";
    return true;
}
bool AsioManager::start()
{
    if (!m_asioInitialized || !m_buffersCreated)
        return false;
    if (m_processing)
        return true;
    ASIOError err = asioStart();
    if (err != ASE_OK)
    {
        std::cerr << "ASIOStart failed (Error: " << err << ").\n";
        return false;
    }
    m_processing = true;
    return true;
}
void AsioManager::stop()
{
    if (!m_processing)
        return;
    m_processing = false;
    ASIOError err = asioStop();
    if (err != ASE_OK)
        std::cerr << "ASIOStop failed (Error: " << err << ").\n";
}
void AsioManager::cleanup()
{
    stop();
    if (m_buffersCreated)
    {
        ASIOError err = asioDisposeBuffers();
        if (err != ASE_OK)
            std::cerr << "ASIODisposeBuffers failed (Error: " << err << ").\n";
        m_buffersCreated = false;
        m_bufferInfos.clear();
        m_activeInputIndices.clear();
        m_activeOutputIndices.clear();
    }
    if (m_asioInitialized)
    {
        ASIOError err = asioExit();
        m_asioInitialized = false;
    }
    if (m_driverLoaded)
    {
        m_driverLoaded = false;
        std::cout << "ASIO driver unloaded.\n";
    }
}
long AsioManager::getBufferSize() { return m_bufferSize; }
double AsioManager::getSampleRate() { return m_sampleRate; }
ASIOSampleType AsioManager::getSampleType() { return m_sampleType; }
std::string AsioManager::getSampleTypeName()
{
    switch (m_sampleType)
    {
    case ASIOSTFloat32LSB:
        return "Float32 LSB";
    case ASIOSTInt32LSB:
        return "Int32 LSB";
    default:
        return "Unknown/Other";
    }
}
void AsioManager::setProcessCallback(AsioProcessCallback callback) { m_processCallback = callback; }
void AsioManager::bufferSwitchCallback(long doubleBufferIndex, bool directProcess)
{
    if (s_instance)
        s_instance->onBufferSwitch(doubleBufferIndex, directProcess);
}
void AsioManager::sampleRateDidChangeCallback(double sRate)
{
    if (s_instance)
        s_instance->onSampleRateDidChange(sRate);
}
long AsioManager::asioMessageCallback(long selector, long value, void *message, double *opt)
{
    if (s_instance)
        return s_instance->onAsioMessage(selector, value, message, opt);
    return 0;
}
void *AsioManager::bufferSwitchTimeInfoCallback(void *params, long doubleBufferIndex, bool directProcess)
{
    bufferSwitchCallback(doubleBufferIndex, directProcess);
    return nullptr;
}
void AsioManager::onBufferSwitch(long doubleBufferIndex, bool directProcess)
{
    if (!m_processing || !m_processCallback)
        return;
    long inputBufferIdx = 0;
    long outputBufferIdx = 0;
    for (long i = 0; i < m_numActiveChannels; ++i)
    {
        if (m_bufferInfos[i].isInput)
        {
            if (inputBufferIdx < m_currentInputBuffers.size())
                m_currentInputBuffers[inputBufferIdx++] = m_bufferInfos[i].buffers[doubleBufferIndex];
        }
        else
        {
            if (outputBufferIdx < m_currentOutputBuffers.size())
                m_currentOutputBuffers[outputBufferIdx++] = m_bufferInfos[i].buffers[doubleBufferIndex];
        }
    }
    m_processCallback(m_currentInputBuffers, m_currentOutputBuffers, m_bufferSize);
    asioOutputReady();
}
void AsioManager::onSampleRateDidChange(double sRate)
{
    std::cout << "ASIO Sample Rate Changed to: " << sRate << " Hz\n";
    m_sampleRate = sRate;
    std::cerr << "Warning: Dynamic sample rate change not fully handled. Stopping.\n";
    stop();
    g_shutdown_requested.store(true);
}
long AsioManager::onAsioMessage(long selector, long value, void *message, double *opt)
{
    switch (selector)
    {
    case 1:
        if (value == 3)
            return 1;
        break;
    case 3:
        std::cerr << "ASIO Reset Request received!\n";
        std::cerr << "Warning: ASIO Reset Request not fully handled. Stopping.\n";
        stop();
        g_shutdown_requested.store(true);
        return 1;
    }
    return 0;
}

// =========================================================================
// ffmpeg_filter.h (v2)
// =========================================================================
#ifndef FFMPEG_FILTER_H
#define FFMPEG_FILTER_H

#include <string>
#include <vector>
#include <map>
#include <mutex>
#include <atomic>
#include <any> // For storing parameter values generically

// --- FFmpeg Includes ---
extern "C"
{
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavutil/opt.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
#include <libavcodec/avcodec.h>
#include <libswresample/swresample.h>
}
// --- End FFmpeg Includes ---

// --- Placeholder ASIO Types ---
typedef long ASIOSampleType;
const ASIOSampleType ASIOSTFloat32LSB = 6;
const ASIOSampleType ASIOSTInt32LSB = 8;
// --- End Placeholder ASIO Types ---

// Structure to hold pending parameter updates
struct FilterParamUpdate
{
    std::string filter_instance_name;
    std::string param_name;
    std::string param_value_str; // Store as string for av_opt_set
};

class FfmpegFilter
{
public:
    FfmpegFilter();
    ~FfmpegFilter();

    // Initializes the filter graph from a description string
    bool initGraph(double sampleRate, long bufferSize, ASIOSampleType asioSampleType, const std::string &filterChainDesc);

    // Processes a block of audio
    // Takes pointers to input channel 1, input channel 2, output channel 1, output channel 2
    // Assumes stereo processing (2 in, 2 out)
    bool process(const void *in1, const void *in2, void *out1, void *out2, long frameCount);

    // Thread-safe method to request a parameter update
    bool updateFilterParameter(const std::string &filterInstanceName, const std::string &paramName, const std::string &valueStr);

    void cleanup();

private:
    // Helper to apply pending updates (called from process)
    void applyPendingUpdates();

    AVSampleFormat getFFmpegFormat(ASIOSampleType asioType);
    void getStereoChannelLayout(AVChannelLayout *layout);

    // --- FFmpeg Members ---
    AVFilterGraph *m_graph = nullptr;
    AVFilterContext *m_srcCtx = nullptr;
    AVFilterContext *m_sinkCtx = nullptr;
    // Store contexts for filters that might be updated dynamically
    std::map<std::string, AVFilterContext *> m_updatableFilterContexts;

    AVFrame *m_inFrame = nullptr;
    AVFrame *m_filtFrame = nullptr;

    // --- Format/Conversion Members ---
    double m_sampleRate = 0.0;
    long m_bufferSize = 0;
    ASIOSampleType m_asioSampleType = ASIOSTFloat32LSB;
    AVSampleFormat m_ffmpegInFormat = AV_SAMPLE_FMT_NONE;
    AVSampleFormat m_ffmpegProcessFormat = AV_SAMPLE_FMT_FLTP;
    AVChannelLayout m_channelLayout;
    SwrContext *m_swrCtxIn = nullptr;
    SwrContext *m_swrCtxOut = nullptr;
    std::vector<uint8_t *> m_resampledInBuf;
    std::vector<uint8_t *> m_resampledOutBuf;
    int m_resampledInLinesize = 0;
    int m_resampledOutLinesize = 0;
    bool m_needsInputConversion = false;
    bool m_needsOutputConversion = false;
    std::vector<float> m_planarInputL;
    std::vector<float> m_planarInputR;
    std::vector<float> m_planarOutputL;
    std::vector<float> m_planarOutputR;
    std::vector<float> m_interleavedOutput;

    // --- Dynamic Update Members ---
    std::vector<FilterParamUpdate> m_pendingUpdates;
    std::mutex m_updateMutex; // Mutex to protect access to m_pendingUpdates
};

#endif // FFMPEG_FILTER_H

// =========================================================================
// ffmpeg_filter.cpp (v2)
// =========================================================================
// Implementation remains the same as v2, including initGraph using
// avfilter_graph_parse_ptr, applyPendingUpdates, and process.
// ... (Full implementation as provided in asio_ffmpeg_eq_code_v2) ...
#include "ffmpeg_filter.h"
#include <iostream>
#include <stdexcept>
#include <vector>
#include <cstring>
#include <sstream> // For parsing filter chain

// Helper function to log FFmpeg errors (same as v1)
static void log_ffmpeg_error(const std::string &context, int errorCode)
{
    char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
    av_strerror(errorCode, errbuf, AV_ERROR_MAX_STRING_SIZE);
    std::cerr << "FFmpeg Error (" << context << "): " << errbuf << " (code: " << errorCode << ")" << std::endl;
}

FfmpegFilter::FfmpegFilter()
{
    getStereoChannelLayout(&m_channelLayout);
}

FfmpegFilter::~FfmpegFilter()
{
    cleanup();
    av_channel_layout_uninit(&m_channelLayout);
}

AVSampleFormat FfmpegFilter::getFFmpegFormat(ASIOSampleType asioType)
{
    switch (asioType)
    {
    case ASIOSTFloat32LSB:
        return AV_SAMPLE_FMT_FLT;
    case ASIOSTInt32LSB:
        return AV_SAMPLE_FMT_S32;
    default:
        return AV_SAMPLE_FMT_NONE;
    }
}

void FfmpegFilter::getStereoChannelLayout(AVChannelLayout *layout)
{
    av_channel_layout_default(layout, 2);
}

bool FfmpegFilter::initGraph(double sampleRate, long bufferSize, ASIOSampleType asioSampleType, const std::string &filterChainDesc)
{
    cleanup(); // Ensure clean state before init
    int ret = 0;
    m_sampleRate = sampleRate;
    m_bufferSize = bufferSize;
    m_asioSampleType = asioSampleType;
    m_ffmpegInFormat = getFFmpegFormat(asioSampleType);

    if (m_ffmpegInFormat == AV_SAMPLE_FMT_NONE)
    {
        std::cerr << "Unsupported ASIO sample type: " << asioSampleType << std::endl;
        return false;
    }

    // Allocate frames
    m_inFrame = av_frame_alloc();
    m_filtFrame = av_frame_alloc();
    if (!m_inFrame || !m_filtFrame)
    {
        std::cerr << "Failed to allocate AVFrames\n";
        cleanup();
        return false;
    }

    // --- Setup Conversion (same logic as v1) ---
    if (m_ffmpegInFormat != m_ffmpegProcessFormat)
    {
        m_needsInputConversion = true;
        m_needsOutputConversion = true;
        // Setup SwrContexts (m_swrCtxIn, m_swrCtxOut)
        ret = swr_alloc_set_opts2(&m_swrCtxIn, &m_channelLayout, m_ffmpegProcessFormat, (int)m_sampleRate, &m_channelLayout, m_ffmpegInFormat, (int)m_sampleRate, 0, nullptr);
        if (ret < 0 || !m_swrCtxIn)
        {
            log_ffmpeg_error("swr_alloc_set_opts2 (In)", ret);
            cleanup();
            return false;
        }
        ret = swr_init(m_swrCtxIn);
        if (ret < 0)
        {
            log_ffmpeg_error("swr_init (In)", ret);
            cleanup();
            return false;
        }
        ret = swr_alloc_set_opts2(&m_swrCtxOut, &m_channelLayout, m_ffmpegInFormat, (int)m_sampleRate, &m_channelLayout, m_ffmpegProcessFormat, (int)m_sampleRate, 0, nullptr);
        if (ret < 0 || !m_swrCtxOut)
        {
            log_ffmpeg_error("swr_alloc_set_opts2 (Out)", ret);
            cleanup();
            return false;
        }
        ret = swr_init(m_swrCtxOut);
        if (ret < 0)
        {
            log_ffmpeg_error("swr_init (Out)", ret);
            cleanup();
            return false;
        }
        // Allocate resample buffers (m_resampledInBuf, m_resampledOutBuf)
        ret = av_samples_alloc_array_and_samples(&m_resampledInBuf, &m_resampledInLinesize, m_channelLayout.nb_channels, m_bufferSize, m_ffmpegProcessFormat, 0);
        if (ret < 0)
        {
            log_ffmpeg_error("av_samples_alloc_array_and_samples (In)", ret);
            cleanup();
            return false;
        }
        ret = av_samples_alloc_array_and_samples(&m_resampledOutBuf, &m_resampledOutLinesize, m_channelLayout.nb_channels, m_bufferSize, m_ffmpegProcessFormat, 0);
        if (ret < 0)
        {
            log_ffmpeg_error("av_samples_alloc_array_and_samples (Out)", ret);
            cleanup();
            return false;
        }
        std::cout << "Initialized SwrContexts for format conversion.\n";
    }
    // Allocate helper buffers
    if (av_sample_fmt_is_planar(m_ffmpegProcessFormat) && !av_sample_fmt_is_planar(m_ffmpegInFormat))
    {
        m_planarInputL.resize(bufferSize);
        m_planarInputR.resize(bufferSize);
        m_planarOutputL.resize(bufferSize);
        m_planarOutputR.resize(bufferSize);
        m_interleavedOutput.resize(bufferSize * 2);
    }

    // --- Create Filter Graph ---
    m_graph = avfilter_graph_alloc();
    if (!m_graph)
    {
        std::cerr << "Failed to allocate filter graph\n";
        cleanup();
        return false;
    }

    // --- Create abuffer Source Filter ---
    const AVFilter *abuffer = avfilter_get_by_name("abuffer");
    char args[512];
    snprintf(args, sizeof(args),
             "time_base=1/%d:sample_rate=%d:sample_fmt=%s:channel_layout=",
             (int)m_sampleRate, (int)m_sampleRate, av_get_sample_fmt_name(m_ffmpegProcessFormat)); // Input to graph is planar float
    av_channel_layout_describe(&m_channelLayout, args + strlen(args), sizeof(args) - strlen(args));
    // Use a specific instance name "src" for abuffer
    ret = avfilter_graph_create_filter(&m_srcCtx, abuffer, "src", args, nullptr, m_graph);
    if (ret < 0)
    {
        log_ffmpeg_error("avfilter_graph_create_filter(abuffer)", ret);
        cleanup();
        return false;
    }

    // --- Create abuffersink Sink Filter ---
    const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
    // Use a specific instance name "sink" for abuffersink
    ret = avfilter_graph_create_filter(&m_sinkCtx, abuffersink, "sink", nullptr, nullptr, m_graph);
    if (ret < 0)
    {
        log_ffmpeg_error("avfilter_graph_create_filter(abuffersink)", ret);
        cleanup();
        return false;
    }

    // --- Parse and Create Filter Chain ---
    AVFilterInOut *outputs = avfilter_inout_alloc();
    AVFilterInOut *inputs = avfilter_inout_alloc();

    if (!outputs || !inputs)
    {
        std::cerr << "Failed to allocate filter in/out\n";
        avfilter_inout_free(&outputs);
        avfilter_inout_free(&inputs);
        cleanup();
        return false;
    }

    try
    {
        // Link abuffer output to the start of the filter chain description
        outputs->name = av_strdup("out"); // Default output pad name of abuffer instance "src"
        outputs->filter_ctx = m_srcCtx;
        outputs->pad_idx = 0;
        outputs->next = nullptr;

        // Link the end of the filter chain description to the abuffersink input
        inputs->name = av_strdup("in"); // Default input pad name of abuffersink instance "sink"
        inputs->filter_ctx = m_sinkCtx;
        inputs->pad_idx = 0;
        inputs->next = nullptr;

        // Parse the filter chain description
        ret = avfilter_graph_parse_ptr(m_graph, filterChainDesc.c_str(), &inputs, &outputs, nullptr);
        if (ret < 0)
        {
            log_ffmpeg_error("avfilter_graph_parse_ptr", ret);
            throw std::runtime_error("Failed to parse filter chain");
        }

        // --- Store contexts for dynamic updates ---
        // Iterate through the graph to find filters by instance name (if named in descr) or type
        m_updatableFilterContexts.clear(); // Clear previous map
        for (unsigned i = 0; i < m_graph->nb_filters; ++i)
        {
            AVFilterContext *fctx = m_graph->filters[i];
            std::string filter_name = fctx->filter->name;
            std::string instance_name = fctx->name ? fctx->name : ""; // e.g., "eq@7f..." or user-defined like "my_eq"

            // Use instance name as key if available, otherwise generate one based on type + index?
            std::string key = instance_name;
            if (key.empty() || key == "src" || key == "sink")
                continue; // Skip buffer/sink unless named specifically

            // Simple example: store based on common types if not explicitly named
            if (instance_name.find('@') != std::string::npos)
            { // Auto-generated name
                if (filter_name == "equalizer" && m_updatableFilterContexts.find("eq") == m_updatableFilterContexts.end())
                    key = "eq";
                else if (filter_name == "acompressor" && m_updatableFilterContexts.find("comp") == m_updatableFilterContexts.end())
                    key = "comp";
                // Add more default keys for other filter types if needed
                else
                    continue; // Skip other auto-named filters unless explicitly handled
            }
            // If key is still the instance name (user-defined or default handled above)
            if (!key.empty())
            {
                m_updatableFilterContexts[key] = fctx;
                std::cout << "Found updatable filter: " << filter_name << " (Instance: '" << instance_name << "', Stored as: '" << key << "')\n";
            }
        }

        // --- Configure Graph ---
        ret = avfilter_graph_config(m_graph, nullptr);
        if (ret < 0)
        {
            log_ffmpeg_error("avfilter_graph_config", ret);
            throw std::runtime_error("Failed to configure filter graph");
        }

        std::cout << "FFmpeg filter graph configured successfully from description.\n";
        // avfilter_graph_dump(m_graph, nullptr); // Optional: Print graph details
    }
    catch (const std::exception &e)
    {
        std::cerr << "Error during filter graph setup: " << e.what() << std::endl;
        avfilter_inout_free(&outputs);
        avfilter_inout_free(&inputs);
        cleanup();
        return false;
    }

    avfilter_inout_free(&outputs);
    avfilter_inout_free(&inputs);

    return true;
}

// Thread-safe method to queue a parameter update
bool FfmpegFilter::updateFilterParameter(const std::string &filterInstanceName, const std::string &paramName, const std::string &valueStr)
{
    std::lock_guard<std::mutex> lock(m_updateMutex);
    m_pendingUpdates.push_back({filterInstanceName, paramName, valueStr});
    // Optional: Add logging here if desired outside the audio thread
    // std::cout << "Queued update for " << filterInstanceName << ": " << paramName << " = " << valueStr << "\n";
    return true;
}

// Apply pending updates (called from audio thread before processing)
void FfmpegFilter::applyPendingUpdates()
{
    std::vector<FilterParamUpdate> updates_to_apply;
    {
        std::lock_guard<std::mutex> lock(m_updateMutex);
        if (m_pendingUpdates.empty())
        {
            return;
        }
        updates_to_apply.swap(m_pendingUpdates);
    }

    for (const auto &update : updates_to_apply)
    {
        auto it = m_updatableFilterContexts.find(update.filter_instance_name);
        if (it != m_updatableFilterContexts.end())
        {
            AVFilterContext *filter_ctx = it->second;
            int ret = av_opt_set(filter_ctx->priv, update.param_name.c_str(), update.param_value_str.c_str(), AV_OPT_SEARCH_CHILDREN);
            if (ret < 0)
            {
                log_ffmpeg_error("av_opt_set on '" + update.filter_instance_name + "' for param '" + update.param_name + "'", ret);
            }
            else
            {
                // Optional: Add minimal logging for audio thread if needed
                // std::cout << "Applied update: " << update.filter_instance_name << "." << update.param_name << "\n";
            }
        }
        else
        {
            std::cerr << "Warning: Cannot apply update. Filter instance '" << update.filter_instance_name << "' not found.\n";
        }
    }
}

bool FfmpegFilter::process(const void *in1, const void *in2, void *out1, void *out2, long frameCount)
{
    if (!m_graph || !m_inFrame || !m_filtFrame || frameCount != m_bufferSize)
    {
        std::cerr << "FFmpeg filter not initialized or frameCount mismatch.\n";
        return false;
    }

    // --- 0. Apply Pending Dynamic Updates ---
    applyPendingUpdates();

    int ret = 0;
    const uint8_t *input_planes[2] = {static_cast<const uint8_t *>(in1), static_cast<const uint8_t *>(in2)};
    uint8_t *output_planes[2] = {static_cast<uint8_t *>(out1), static_cast<uint8_t *>(out2)};

    // --- 1. Prepare Input AVFrame (Including Conversion) ---
    m_inFrame->sample_rate = (int)m_sampleRate;
    m_inFrame->nb_samples = frameCount;
    av_channel_layout_copy(&m_inFrame->ch_layout, &m_channelLayout);
    m_inFrame->format = m_ffmpegProcessFormat;
    const uint8_t **source_data_ptr;
    if (m_needsInputConversion)
    {
        if (m_ffmpegInFormat == AV_SAMPLE_FMT_FLT && m_ffmpegProcessFormat == AV_SAMPLE_FMT_FLTP)
        {
            const float *interleaved_in = static_cast<const float *>(in1);
            for (long i = 0; i < frameCount; ++i)
            {
                m_planarInputL[i] = interleaved_in[i * 2 + 0];
                m_planarInputR[i] = interleaved_in[i * 2 + 1];
            }
            uint8_t *planar_ptrs[2] = {reinterpret_cast<uint8_t *>(m_planarInputL.data()), reinterpret_cast<uint8_t *>(m_planarInputR.data())};
            source_data_ptr = const_cast<const uint8_t **>(planar_ptrs);
            ret = av_frame_make_writable(m_inFrame); // Ensure frame buffer is writable if reusing
            if (ret < 0)
            {
                log_ffmpeg_error("av_frame_make_writable (in)", ret);
                return false;
            }
            // Assign planar data pointers directly
            m_inFrame->data[0] = reinterpret_cast<uint8_t *>(m_planarInputL.data());
            m_inFrame->data[1] = reinterpret_cast<uint8_t *>(m_planarInputR.data());
            m_inFrame->linesize[0] = frameCount * sizeof(float);
            m_inFrame->linesize[1] = frameCount * sizeof(float);
        }
        else
        {
            const uint8_t **swr_input_ptr = input_planes; // Adjust if input is interleaved
            ret = swr_convert(m_swrCtxIn, m_resampledInBuf, frameCount, swr_input_ptr, frameCount);
            if (ret < 0)
            {
                log_ffmpeg_error("swr_convert (In)", ret);
                return false;
            }
            if (ret != frameCount)
            { /* Handle sample count mismatch */
            }
            source_data_ptr = const_cast<const uint8_t **>(m_resampledInBuf);
            ret = av_frame_make_writable(m_inFrame);
            if (ret < 0)
            {
                log_ffmpeg_error("av_frame_make_writable (in swr)", ret);
                return false;
            }
            // Assign converted planar data pointers
            m_inFrame->data[0] = m_resampledInBuf[0];
            m_inFrame->data[1] = m_resampledInBuf[1];
            m_inFrame->linesize[0] = m_resampledInLinesize;
            m_inFrame->linesize[1] = m_resampledInLinesize;
        }
    }
    else
    {
        source_data_ptr = input_planes;
        ret = av_frame_make_writable(m_inFrame);
        if (ret < 0)
        {
            log_ffmpeg_error("av_frame_make_writable (in direct)", ret);
            return false;
        }
        // Assign direct planar data pointers (assuming input is already planar)
        m_inFrame->data[0] = const_cast<uint8_t *>(input_planes[0]);
        m_inFrame->data[1] = const_cast<uint8_t *>(input_planes[1]);
        m_inFrame->linesize[0] = av_samples_get_buffer_size(nullptr, 1, frameCount, m_ffmpegProcessFormat, 0);
        m_inFrame->linesize[1] = av_samples_get_buffer_size(nullptr, 1, frameCount, m_ffmpegProcessFormat, 0);
    }

    // --- 2. Send Frame to Filter Graph ---
    ret = av_buffersrc_add_frame_flags(m_srcCtx, m_inFrame, AV_BUFFERSRC_FLAG_KEEP_REF);
    if (ret < 0)
    {
        log_ffmpeg_error("av_buffersrc_add_frame", ret);
        // Don't return immediately, try to get filtered frames anyway
    }

    // --- 3. Get Filtered Frame from Graph ---
    av_frame_unref(m_filtFrame); // Unref previous filtered frame before getting new one
    ret = av_buffersink_get_frame(m_sinkCtx, m_filtFrame);
    if (ret == AVERROR(EAGAIN) || ret == AVERROR_EOF)
    {
        // Output silence if no frame available (common in real-time)
        size_t bytes_per_sample_out = av_get_bytes_per_sample(m_ffmpegInFormat);
        size_t plane_size = frameCount * bytes_per_sample_out;
        memset(out1, 0, plane_size); // Assuming out1/out2 point to correct buffers
        if (m_channelLayout.nb_channels > 1 && out2)
            memset(out2, 0, plane_size);
        return (ret != AVERROR_EOF); // EAGAIN is expected, EOF is an error here
    }
    else if (ret < 0)
    {
        log_ffmpeg_error("av_buffersink_get_frame", ret);
        return false; // Genuine error
    }

    // --- 4. Convert Filtered Frame back to ASIO Format ---
    long samples_to_process = std::min((long)m_filtFrame->nb_samples, frameCount);
    if (samples_to_process != frameCount)
    {
        std::cerr << "Warning: Filter graph output frame count (" << m_filtFrame->nb_samples
                  << ") differs from input (" << frameCount << "). Processing available samples.\n";
    }

    const uint8_t **filtered_data_ptr = (const uint8_t **)m_filtFrame->data;

    if (m_needsOutputConversion)
    {
        if (m_ffmpegProcessFormat == AV_SAMPLE_FMT_FLTP && m_ffmpegInFormat == AV_SAMPLE_FMT_FLT)
        {
            const float *planar_l = reinterpret_cast<const float *>(filtered_data_ptr[0]);
            const float *planar_r = reinterpret_cast<const float *>(filtered_data_ptr[1]);
            float *interleaved_out = static_cast<float *>(out1);
            for (long i = 0; i < samples_to_process; ++i)
            {
                interleaved_out[i * 2 + 0] = planar_l[i];
                interleaved_out[i * 2 + 1] = planar_r[i];
            }
            for (long i = samples_to_process; i < frameCount; ++i)
            {
                interleaved_out[i * 2 + 0] = 0.0f;
                interleaved_out[i * 2 + 1] = 0.0f;
            }
        }
        else
        {
            // Use SwrContext for other conversions
            ret = swr_convert(m_swrCtxOut, output_planes, frameCount, filtered_data_ptr, samples_to_process);
            if (ret < 0)
            {
                log_ffmpeg_error("swr_convert (Out)", ret); /* Output silence? */
            }
            if (ret != frameCount)
            { /* Handle padding/clearing if needed */
            }
        }
    }
    else
    {
        // No output conversion needed, copy directly (assuming planar output needed by ASIO sink)
        long bytes_per_plane = samples_to_process * av_get_bytes_per_sample(m_ffmpegProcessFormat);
        memcpy(output_planes[0], filtered_data_ptr[0], bytes_per_plane);
        memcpy(output_planes[1], filtered_data_ptr[1], bytes_per_plane);
        if (samples_to_process < frameCount)
        {
            long clear_bytes = (frameCount - samples_to_process) * av_get_bytes_per_sample(m_ffmpegProcessFormat);
            memset(static_cast<uint8_t *>(output_planes[0]) + bytes_per_plane, 0, clear_bytes);
            memset(static_cast<uint8_t *>(output_planes[1]) + bytes_per_plane, 0, clear_bytes);
        }
    }

    // --- 5. Cleanup Frame (Done implicitly by unref at start of next call or loop) ---
    // av_frame_unref(m_filtFrame); // Already done at the start of step 3

    return true;
}

void FfmpegFilter::cleanup()
{
    {
        std::lock_guard<std::mutex> lock(m_updateMutex);
        m_pendingUpdates.clear();
    }
    m_updatableFilterContexts.clear();

    avfilter_graph_free(&m_graph);
    m_graph = nullptr;
    m_srcCtx = nullptr;
    m_sinkCtx = nullptr;

    av_frame_free(&m_inFrame);
    av_frame_free(&m_filtFrame);
    m_inFrame = nullptr;
    m_filtFrame = nullptr;

    swr_free(&m_swrCtxIn);
    swr_free(&m_swrCtxOut);
    m_swrCtxIn = nullptr;
    m_swrCtxOut = nullptr;

    if (m_resampledInBuf)
    {
        av_freep(&m_resampledInBuf[0]);
        av_freep(&m_resampledInBuf);
    }
    if (m_resampledOutBuf)
    {
        av_freep(&m_resampledOutBuf[0]);
        av_freep(&m_resampledOutBuf);
    }

    m_planarInputL.clear();
    m_planarInputR.clear();
    m_planarOutputL.clear();
    m_planarOutputR.clear();
    m_interleavedOutput.clear();

    // No need to call av_channel_layout_uninit here, done in destructor
    // std::cout << "FFmpeg filter resources cleaned up.\n"; // Reduce logging in cleanup
}

// =========================================================================
// osc_manager.h / .cpp (Conceptual Stubs - Requires OSC Library)
// =========================================================================
/*
#ifndef OSC_MANAGER_H
#define OSC_MANAGER_H
// ... (Conceptual header as provided in asio_ffmpeg_eq_code_v2) ...
#endif // OSC_MANAGER_H
*/
/*
// osc_manager.cpp
// ... (Conceptual implementation as provided in asio_ffmpeg_eq_code_v2) ...
*/
