/**
 * @file main.cpp / audio_engine.h / etc.
 * @brief Modular Audio Engine Implementation (ASIO, Files, FFmpeg, OSC)
 *
 * This code outlines the structure for a configurable audio engine capable of
 * routing audio between ASIO devices, files, and FFmpeg processing chains,
 * with OSC control for an RME TotalMix FX matrix.
 *
 * Dependencies:
 * - C++17 Compiler
 * - Steinberg ASIO SDK (Headers required)
 * - FFmpeg Development Libraries (libavformat, libavcodec, libavfilter, libswresample, libavutil)
 * - C++ OSC Library (e.g., oscpack - conceptual integration shown)
 * - C++ Configuration Parsing Library (e.g., nlohmann/json, yaml-cpp - conceptual integration shown)
 *
 * Build System (like CMake) is highly recommended.
 */

#include <vector>
#include <string>
#include <map>
#include <memory>
#include <functional>
#include <thread>
#include <mutex>
#include <atomic>
#include <any>
#include <stdexcept>
#include <iostream>
#include <chrono>
#include <csignal>
#include <optional> // For optional return values

// --- FFmpeg Includes ---
extern "C"
{
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersrc.h>
#include <libavfilter/buffersink.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/opt.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
#include <libswresample/swresample.h>
}
// --- End FFmpeg Includes ---

// --- ASIO SDK Placeholders (CRITICAL: Replace with actual SDK includes/types) ---
// #include "asio.h"
// #include "asiodrivers.h"
// #include "asiosys.h"
// #include "asiolist.h"
typedef long ASIOSampleType;
const ASIOSampleType ASIOSTFloat32LSB = 6; // Example value, check asio.h
const ASIOSampleType ASIOSTInt32LSB = 8;   // Example value, check asio.h
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
// --- End ASIO SDK Placeholders ---

// --- OSC Library Placeholders (Conceptual) ---
namespace osc
{
	class OutboundPacketStream; /* ... other types ... */
}
namespace Ip
{
	class UdpSocket; /* ... other types ... */
}
// --- End OSC Library Placeholders ---

// =========================================================================
// Utility Functions / Types
// =========================================================================

// Simple helper for FFmpeg error logging
static void log_ffmpeg_error(const std::string &context, int errorCode)
{
	char errbuf[AV_ERROR_MAX_STRING_SIZE] = {0};
	av_strerror(errorCode, errbuf, AV_ERROR_MAX_STRING_SIZE);
	std::cerr << "FFmpeg Error (" << context << "): " << errbuf << " (code: " << errorCode << ")" << std::endl;
}

// Basic Audio Buffer Representation (Needs more detail for practical use)
// Consider using AVFrame directly or a more robust buffer class with pooling.
struct AudioBuffer
{
	std::vector<uint8_t *> data; // Pointers per channel
	std::vector<int> linesize;	 // Optional: linesize per plane
	long frames = 0;
	double sampleRate = 0.0;
	AVSampleFormat format = AV_SAMPLE_FMT_NONE;
	AVChannelLayout channelLayout; // Use AVChannelLayout directly

	AudioBuffer() { av_channel_layout_default(&channelLayout, 0); } // Init empty layout
	~AudioBuffer() { av_channel_layout_uninit(&channelLayout); }
	// Disable copy, enable move
	AudioBuffer(const AudioBuffer &) = delete;
	AudioBuffer &operator=(const AudioBuffer &) = delete;
	AudioBuffer(AudioBuffer &&other) noexcept : data(std::move(other.data)), linesize(std::move(other.linesize)), frames(other.frames), sampleRate(other.sampleRate), format(other.format)
	{
		av_channel_layout_copy(&channelLayout, &other.channelLayout);
		av_channel_layout_uninit(&other.channelLayout); // Clear moved-from layout
		other.frames = 0;
		other.sampleRate = 0.0;
		other.format = AV_SAMPLE_FMT_NONE;
	}
	AudioBuffer &operator=(AudioBuffer &&other) noexcept
	{
		if (this != &other)
		{
			data = std::move(other.data);
			linesize = std::move(other.linesize);
			frames = other.frames;
			sampleRate = other.sampleRate;
			format = other.format;
			av_channel_layout_uninit(&channelLayout);
			av_channel_layout_copy(&channelLayout, &other.channelLayout);
			av_channel_layout_uninit(&other.channelLayout);
			other.frames = 0;
			other.sampleRate = 0.0;
			other.format = AV_SAMPLE_FMT_NONE;
		}
		return *this;
	}

	// TODO: Add methods for allocation, deallocation, copying, reference counting?
};

// Structure to hold parsed configuration (Example - needs detail)
struct NodeConfig
{
	std::string name;
	std::string type;						   // "asio_source", "file_sink", "ffmpeg_processor", etc.
	std::map<std::string, std::string> params; // Type-specific params
};
struct ConnectionConfig
{
	std::string sourceNode;
	int sourcePad = 0;
	std::string sinkNode;
	int sinkPad = 0;
};
struct RmeOscCommandConfig
{
	std::string address;
	std::vector<std::any> args; // Use std::any or specific types
};
struct Configuration
{
	std::string asioDeviceName;
	std::string rmeOscIp;
	int rmeOscPort = 0;
	double sampleRate = 48000.0; // Default or detected
	long bufferSize = 512;		 // Default or detected
	std::vector<NodeConfig> nodes;
	std::vector<ConnectionConfig> connections;
	std::vector<RmeOscCommandConfig> rmeCommands;
};

// Forward Declarations for Engine/Nodes
class AudioEngine;
class AudioNode;

// =========================================================================
// AsioManager Class (Refined Interface)
// =========================================================================
class AsioManager
{
public:
	using AsioCallback = std::function<void(long /*doubleBufferIndex*/)>;

	AsioManager();
	~AsioManager();

	bool loadDriver(const std::string &deviceName);
	bool initDevice(double preferredSampleRate = 0.0, long preferredBufferSize = 0); // Allow requesting rate/size
	bool createBuffers(const std::vector<long> &inputChannels, const std::vector<long> &outputChannels);
	bool start();
	void stop();
	void cleanup();

	// Getters
	long getBufferSize() const;
	double getSampleRate() const;
	ASIOSampleType getSampleType() const;
	std::string getSampleTypeName() const;
	long getInputLatency() const;
	long getOutputLatency() const;
	bool findChannelIndex(const std::string &channelName, bool isInput, long &index) const;

	// Set the callback function (called by AudioEngine)
	void setCallback(AsioCallback callback);

	// Get buffer pointers for the current processing block (called by AudioEngine/Nodes)
	bool getBufferPointers(long doubleBufferIndex, const std::vector<long> &activeIndices, std::vector<void *> &bufferPtrs);

private:
	// Internal ASIO SDK function placeholders (same as before)
	bool asioExit();
	bool asioInit(void *sysHandle);
	ASIOError asioGetChannels(long *, long *);
	ASIOError asioGetBufferSize(long *, long *, long *, long *);
	ASIOError asioCanSampleRate(double);
	ASIOError asioGetSampleRate(double *);
	ASIOError asioSetSampleRate(double);
	ASIOError asioGetChannelInfo(long, bool, void *);
	ASIOError asioCreateBuffers(ASIOBufferInfo *, long, long, ASIOCallbacks *);
	ASIOError asioDisposeBuffers();
	ASIOError asioStart();
	ASIOError asioStop();
	ASIOError asioOutputReady();
	ASIOError asioGetLatencies(long *inputLatency, long *outputLatency); // Placeholder

	// Static callbacks delegating to instance
	static void bufferSwitchCallback(long doubleBufferIndex, bool directProcess);
	static void sampleRateDidChangeCallback(double sRate);
	static long asioMessageCallback(long selector, long value, void *message, double *opt);
	static void *bufferSwitchTimeInfoCallback(void *params, long doubleBufferIndex, bool directProcess);

	// Instance callback handlers
	void onBufferSwitch(long doubleBufferIndex, bool directProcess);
	void onSampleRateDidChange(double sRate);
	long onAsioMessage(long selector, long value, void *message, double *opt);

	// Member variables (similar to before, but track requested channels)
	bool m_driverLoaded = false;
	bool m_asioInitialized = false;
	bool m_buffersCreated = false;
	bool m_processing = false;
	long m_inputChannels = 0;
	long m_outputChannels = 0;
	long m_bufferSize = 0;
	double m_sampleRate = 0.0;
	ASIOSampleType m_sampleType = ASIOSTFloat32LSB;
	long m_inputLatency = 0;
	long m_outputLatency = 0;
	std::vector<ASIOBufferInfo> m_bufferInfos; // Holds info for *active* channels only
	std::vector<long> m_activeInputIndices;	   // Indices passed to createBuffers
	std::vector<long> m_activeOutputIndices;
	long m_numActiveChannels = 0;
	AsioCallback m_callback; // Callback function provided by AudioEngine
	static AsioManager *s_instance;
	void *m_asioDrivers = nullptr; // Placeholder for AsioDrivers*
};

// =========================================================================
// FfmpegFilter Class (Wrapper for FFmpeg processing chain)
// =========================================================================
// Reuse the FfmpegFilter class from asio_ffmpeg_eq_code_v2.hpp / .cpp
// Ensure it's included or defined here. It handles graph creation from
// a description string, processing, and dynamic parameter updates via
// updateFilterParameter().
#include "ffmpeg_filter.h" // Assuming it's in a separate file now

// =========================================================================
// OscController Class
// =========================================================================
#include "OscController.h" // Include the renamed header

// =========================================================================
// AudioNode Base & Derived Classes (More detailed stubs)
// =========================================================================
class AudioNode
{
public:
	enum class NodeType
	{
		SOURCE,
		SINK,
		PROCESSOR
	};

	AudioNode(std::string name, NodeType type, AudioEngine *engine)
		: m_name(std::move(name)), m_type(type), m_engine(engine) {}
	virtual ~AudioNode() = default;

	// Configure node based on parsed parameters
	virtual bool configure(const std::map<std::string, std::string> &params, double sampleRate, long bufferSize, AVSampleFormat internalFormat, const AVChannelLayout &internalLayout) = 0;
	// Prepare node for processing (e.g., open files/devices)
	virtual bool start() = 0;
	// Core processing function (pull/push data)
	virtual void process() = 0;
	// Stop processing (e.g., close files, stop threads)
	virtual void stop() = 0;
	// Cleanup resources
	virtual void cleanup() = 0;

	// Interface for connecting nodes (needs refinement)
	virtual std::optional<AudioBuffer> getOutputBuffer(int padIndex = 0) { return std::nullopt; }
	virtual bool setInputBuffer(const AudioBuffer &buffer, int padIndex = 0) { return false; }

	const std::string &getName() const { return m_name; }
	NodeType getType() const { return m_type; }

protected:
	std::string m_name;
	NodeType m_type;
	AudioEngine *m_engine; // Pointer back to the engine
	double m_sampleRate = 0.0;
	long m_bufferSize = 0;
	AVSampleFormat m_internalFormat = AV_SAMPLE_FMT_NONE; // Target format for connections
	AVChannelLayout m_internalLayout;					  // Target layout for connections

	AudioNode(const AudioNode &) = delete; // Non-copyable
	AudioNode &operator=(const AudioNode &) = delete;
};

// --- AsioSourceNode ---
class AsioSourceNode : public AudioNode
{
public:
	AsioSourceNode(std::string name, AudioEngine *engine, AsioManager *asioMgr)
		: AudioNode(std::move(name), NodeType::SOURCE, engine), m_asioMgr(asioMgr) {}

	virtual bool configure(const std::map<std::string, std::string> &params, double sampleRate, long bufferSize, AVSampleFormat internalFormat, const AVChannelLayout &internalLayout) override;
	virtual bool start() override { return true; } // Started by AsioManager
	virtual void process() override { /* Data pushed via receiveAsioData */ }
	virtual void stop() override { /* Stopped by AsioManager */ }
	virtual void cleanup() override { /* Buffers freed by AsioManager */ }
	virtual std::optional<AudioBuffer> getOutputBuffer(int padIndex = 0) override;

	// Called by AudioEngine from ASIO callback
	void receiveAsioData(long doubleBufferIndex, const std::vector<void *> &asioBuffers);
	const std::vector<long> &getAsioChannelIndices() const { return m_asioChannelIndices; }

private:
	AsioManager *m_asioMgr;
	std::vector<long> m_asioChannelIndices;			  // Which ASIO channels this node uses
	AudioBuffer m_outputBuffer;						  // Holds converted data ready for output pad
	AVSampleFormat m_asioFormat = AV_SAMPLE_FMT_NONE; // Actual ASIO format
	// SwrContext for converting ASIO -> internal format if needed
	SwrContext *m_swrCtx = nullptr;
	std::vector<uint8_t *> m_resampledBuf; // Temp buffer for swr output
	int m_resampledLinesize = 0;
	std::mutex m_bufferMutex; // Protect access to m_outputBuffer
};

// --- AsioSinkNode ---
class AsioSinkNode : public AudioNode
{
public:
	AsioSinkNode(std::string name, AudioEngine *engine, AsioManager *asioMgr)
		: AudioNode(std::move(name), NodeType::SINK, engine), m_asioMgr(asioMgr) {}
	~AsioSinkNode();

	virtual bool configure(const std::map<std::string, std::string> &params, double sampleRate, long bufferSize, AVSampleFormat internalFormat, const AVChannelLayout &internalLayout) override;
	virtual bool start() override { return true; }
	virtual void process() override { /* Data pulled via provideAsioData */ }
	virtual void stop() override {}
	virtual void cleanup() override;
	virtual bool setInputBuffer(const AudioBuffer &buffer, int padIndex = 0) override;

	// Called by AudioEngine from ASIO callback
	void provideAsioData(long doubleBufferIndex, const std::vector<void *> &asioBuffers);
	const std::vector<long> &getAsioChannelIndices() const { return m_asioChannelIndices; }

private:
	AsioManager *m_asioMgr;
	std::vector<long> m_asioChannelIndices;
	AudioBuffer m_inputBuffer; // Holds data received via setInputBuffer
	AVSampleFormat m_asioFormat = AV_SAMPLE_FMT_NONE;
	// SwrContext for converting internal -> ASIO format if needed
	SwrContext *m_swrCtx = nullptr;
	std::vector<uint8_t *> m_resampledBuf; // Temp buffer for swr input
	int m_resampledLinesize = 0;
	std::mutex m_bufferMutex;
};

// --- FfmpegProcessorNode ---
class FfmpegProcessorNode : public AudioNode
{
public:
	FfmpegProcessorNode(std::string name, AudioEngine *engine)
		: AudioNode(std::move(name), NodeType::PROCESSOR, engine)
	{
		m_ffmpegFilter = std::make_unique<FfmpegFilter>();
	}
	~FfmpegProcessorNode() { cleanup(); }

	virtual bool configure(const std::map<std::string, std::string> &params, double sampleRate, long bufferSize, AVSampleFormat internalFormat, const AVChannelLayout &internalLayout) override;
	virtual bool start() override { return true; }
	virtual void process() override; // Process m_inputBuffer -> m_outputBuffer
	virtual void stop() override {}
	virtual void cleanup() override
	{
		if (m_ffmpegFilter)
			m_ffmpegFilter->cleanup();
	}
	virtual std::optional<AudioBuffer> getOutputBuffer(int padIndex = 0) override;
	virtual bool setInputBuffer(const AudioBuffer &buffer, int padIndex = 0) override;

	// Method to handle dynamic updates
	bool updateParameter(const std::string &filterName, const std::string &paramName, const std::string &valueStr);

private:
	std::unique_ptr<FfmpegFilter> m_ffmpegFilter;
	std::string m_filterDesc;
	AudioBuffer m_inputBuffer;	// Stores the latest input
	AudioBuffer m_outputBuffer; // Stores the latest output
	std::mutex m_bufferMutex;	// Protect buffer access if process called async?
};

// --- FileSourceNode / FileSinkNode (Conceptual Stubs) ---
class FileSourceNode : public AudioNode
{
	// ... (Members: path, thread, atomic flag, FFmpeg contexts, output queue) ...
public:
	FileSourceNode(std::string name, AudioEngine *engine) : AudioNode(std::move(name), NodeType::SOURCE, engine) {}
	~FileSourceNode() { cleanup(); }
	virtual bool configure(const std::map<std::string, std::string> &params, double sampleRate, long bufferSize, AVSampleFormat internalFormat, const AVChannelLayout &internalLayout) override { /* Get path, target format */ return true; }
	virtual bool start() override
	{ /* Launch reader thread */
		m_running = true;
		return true;
	}
	virtual void process() override { /* N/A - Driven by thread */ }
	virtual void stop() override { /* Signal thread, join */ m_running = false; }
	virtual void cleanup() override { stop(); /* Free FFmpeg contexts */ }
	virtual std::optional<AudioBuffer> getOutputBuffer(int padIndex = 0) override { /* Pop from thread-safe queue */ return std::nullopt; }

private:
	void readerThreadFunc() { /* while(m_running) { Read->Decode->Resample->PushToQueue } */ }
	std::string m_filePath;
	std::thread m_readerThread;
	std::atomic<bool> m_running{false};
	// AVFormatContext, AVCodecContext, SwrContext, OutputQueue...
};

class FileSinkNode : public AudioNode
{
	// ... (Members: path, thread, atomic flag, FFmpeg contexts, input queue) ...
public:
	FileSinkNode(std::string name, AudioEngine *engine) : AudioNode(std::move(name), NodeType::SINK, engine) {}
	~FileSinkNode() { cleanup(); }
	virtual bool configure(const std::map<std::string, std::string> &params, double sampleRate, long bufferSize, AVSampleFormat internalFormat, const AVChannelLayout &internalLayout) override { /* Get path, encoder params */ return true; }
	virtual bool start() override
	{ /* Open file, write header, launch thread */
		m_running = true;
		return true;
	}
	virtual void process() override { /* N/A - Driven by thread */ }
	virtual void stop() override { /* Signal thread (flush encoder), join, write trailer */ m_running = false; }
	virtual void cleanup() override { stop(); /* Free FFmpeg contexts, close file */ }
	virtual bool setInputBuffer(const AudioBuffer &buffer, int padIndex = 0) override { /* Push to thread-safe queue */ return true; }

private:
	void writerThreadFunc() { /* while(m_running or !queue.empty()) { PopFromQueue->Resample->Encode->WritePacket } */ }
	std::string m_filePath;
	std::thread m_writerThread;
	std::atomic<bool> m_running{false};
	// AVFormatContext, AVCodecContext, SwrContext, InputQueue...
};

// =========================================================================
// AudioEngine Class (More Detail)
// =========================================================================
class AudioEngine
{
public:
	AudioEngine();
	~AudioEngine();

	bool initialize(Configuration config); // Take config by value or const ref
	void run();
	void stop();
	void cleanup();

	// Called by AsioManager
	void processAsioBlock(long doubleBufferIndex);

	// Getters
	AsioManager *getAsioManager() { return m_asioManager.get(); }
	OscController *getRmeController() { return m_rmeController.get(); } // Update return type
	double getSampleRate() const { return m_sampleRate; }
	long getBufferSize() const { return m_bufferSize; }
	const AVChannelLayout &getInternalLayout() const { return m_internalLayout; }
	AVSampleFormat getInternalFormat() const { return m_internalFormat; }

private:
	bool setupAsio();
	bool createAndConfigureNodes();
	bool setupConnections();
	bool sendRmeCommands();
	void runFileProcessingLoop(); // For non-ASIO mode

	Configuration m_config;
	std::unique_ptr<AsioManager> m_asioManager;
	std::unique_ptr<OscController> m_rmeController; // Update type

	std::vector<std::shared_ptr<AudioNode>> m_nodes;
	std::map<std::string, std::shared_ptr<AudioNode>> m_nodeMap;

	// Define connections more concretely
	struct Connection
	{
		std::shared_ptr<AudioNode> sourceNode;
		int sourcePad;
		std::shared_ptr<AudioNode> sinkNode;
		int sinkPad;
	};
	std::vector<Connection> m_connections;
	// Store processing order if needed (e.g., topological sort of nodes)
	std::vector<std::shared_ptr<AudioNode>> m_processingOrder;

	std::atomic<bool> m_isRunning{false};
	bool m_useAsio = false;
	double m_sampleRate = 0.0;
	long m_bufferSize = 0;
	AVSampleFormat m_internalFormat = AV_SAMPLE_FMT_FLTP; // Internal processing format
	AVChannelLayout m_internalLayout;					  // Internal processing layout (e.g., stereo)

	// TODO: Add AudioBuffer pool management
};

// =========================================================================
// Configuration Parser (Conceptual Stub)
// =========================================================================
class ConfigurationParser
{
public:
	// Parses command line or a config file (e.g., JSON)
	bool parse(int argc, char *argv[], Configuration &outConfig)
	{
		std::cout << "Parsing configuration (Conceptual)...\n";
		// --- Placeholder Logic ---
		// Example: Hardcode a simple ASIO In -> EQ -> ASIO Out config
		outConfig.asioDeviceName = "ASIO Fireface UCX II"; // Or get from args
		outConfig.rmeOscIp = "127.0.0.1";
		outConfig.rmeOscPort = 9000; // Check RME default

		// Node Definitions
		outConfig.nodes.push_back({"asio_in", "asio_source", {{"channels", "0,1"}}}); // Use ASIO channels 0, 1
		outConfig.nodes.push_back({"eq_proc", "ffmpeg_processor", {{"chain", "equalizer=f=1000:width_type=q:w=1.0:g=3"}}});
		outConfig.nodes.push_back({"asio_out", "asio_sink", {{"channels", "0,1"}}}); // Use ASIO channels 0, 1

		// Connection Definitions
		outConfig.connections.push_back({"asio_in", 0, "eq_proc", 0});
		outConfig.connections.push_back({"eq_proc", 0, "asio_out", 0});

		// RME OSC Command Definitions (Example: Route HW In 1->ASIO Out 1, ASIO In 1->HW Out 1)
		// NOTE: These addresses/formats depend heavily on RME TotalMix OSC docs!
		// outConfig.rmeCommands.push_back({"/1/matrix/1/1/gain", {std::any(1.0f)}}); // HW In 1 -> ASIO Out 1 Gain (Conceptual)
		// outConfig.rmeCommands.push_back({"/1/matrixchmix/1/1/gain", {std::any(1.0f)}}); // ASIO In 1 -> HW Out 1 Gain (Conceptual)

		std::cout << "Using hardcoded default configuration.\n";
		return true;
		// --- End Placeholder ---

		// Real implementation would use args or a JSON/YAML library
	}
};

// =========================================================================
// Main Application Entry Point (Conceptual)
// =========================================================================
std::atomic<bool> g_shutdown_requested(false); // Shared shutdown flag

void signal_handler(int signum)
{
	std::cout << "\nShutdown requested (Signal " << signum << ")...\n";
	g_shutdown_requested.store(true);
}

int main(int argc, char *argv[])
{
	std::cout << "Starting Modular Audio Engine v1.0...\n";
	signal(SIGINT, signal_handler);
	signal(SIGTERM, signal_handler);

	Configuration config;
	ConfigurationParser parser;
	AudioEngine engine;

	try
	{
		if (!parser.parse(argc, argv, config))
		{
			std::cerr << "Failed to parse configuration.\n";
			return 1;
		}
		if (!engine.initialize(std::move(config)))
		{ // Move config into engine
			std::cerr << "Failed to initialize audio engine.\n";
			engine.cleanup();
			return 1;
		}

		engine.run(); // Start processing loop (blocking or non-blocking)

		std::cout << "Engine running. Press Ctrl+C to stop.\n";
		while (!g_shutdown_requested.load())
		{
			std::this_thread::sleep_for(std::chrono::milliseconds(200));
			// Main thread can potentially interact with engine here if needed
		}
	}
	catch (const std::exception &e)
	{
		std::cerr << "Unhandled exception: " << e.what() << std::endl;
	}
	catch (...)
	{
		std::cerr << "Unknown exception occurred." << std::endl;
	}

	std::cout << "Shutting down...\n";
	engine.stop();
	engine.cleanup();

	std::cout << "Application finished.\n";
	return 0;
}

// =========================================================================
// Implementation Stubs (Need full implementation)
// =========================================================================

// --- AsioManager Implementation ---
// (Similar to asio_ffmpeg_eq_code_v2.cpp, but needs adaptation for
// dynamic channel activation in createBuffers and callback routing to AudioEngine)
// ... Full AsioManager implementation needed ...

// --- OscController Implementation ---
// (Implementation is now in OscController.cpp)

// --- AudioNode Derived Class Implementations (Stubs) ---
// ... Need full implementations for configure, start, process, stop, cleanup, get/set buffer ...
// ... AsioSourceNode::configure needs to parse "channels" param, store indices ...
// ... AsioSinkNode::configure needs to parse "channels" param, store indices ...
// ... FileSourceNode::configure needs path, thread start/stop, FFmpeg setup ...
// ... FileSinkNode::configure needs path, encoder params, thread start/stop, FFmpeg setup ...
// ... FfmpegProcessorNode::configure needs filter desc, calls m_ffmpegFilter->initGraph ...

// --- AudioEngine Implementation ---
AudioEngine::AudioEngine() { av_channel_layout_default(&m_internalLayout, 2); } // Default to stereo layout
AudioEngine::~AudioEngine() { cleanup(); }
bool AudioEngine::initialize(Configuration config)
{
	m_config = std::move(config);		   // Take ownership
	m_internalFormat = AV_SAMPLE_FMT_FLTP; // Default internal format
	// TODO: Get internal layout/format from config?

	m_rmeController = std::make_unique<OscController>(); // Update type
	if (!m_rmeController->configure(m_config.rmeOscIp, m_config.rmeOscPort))
	{
		std::cerr << "Failed to configure RME OSC Controller.\n";
		return false;
	}

	if (!createAndConfigureNodes())
		return false;
	if (!setupConnections())
		return false; // Determine processing order here too

	// Determine if ASIO is needed based on configured nodes
	m_useAsio = false;
	for (const auto &node : m_nodes)
	{
		if (dynamic_cast<AsioSourceNode *>(node.get()) || dynamic_cast<AsioSinkNode *>(node.get()))
		{
			m_useAsio = true;
			break;
		}
	}

	if (m_useAsio)
	{
		if (!setupAsio())
			return false;
	}

	if (!sendRmeCommands())
		return false; // Send after nodes/ASIO setup potentially

	std::cout << "Audio Engine Initialized.\n";
	return true;
}
bool AudioEngine::setupAsio()
{
	m_asioManager = std::make_unique<AsioManager>();
	if (!m_asioManager->loadDriver(m_config.asioDeviceName))
		return false;
	// Pass preferred rate/size from config if available
	if (!m_asioManager->initDevice(m_config.sampleRate, m_config.bufferSize))
		return false;

	// Update engine state with actual ASIO parameters
	m_sampleRate = m_asioManager->getSampleRate();
	m_bufferSize = m_asioManager->getBufferSize();
	m_config.sampleRate = m_sampleRate; // Ensure config reflects reality
	m_config.bufferSize = m_bufferSize;

	// Set the callback BEFORE creating buffers
	m_asioManager->setCallback([this](long dbIdx)
							   { this->processAsioBlock(dbIdx); });

	// Collect ASIO channels needed by nodes
	std::vector<long> asio_inputs, asio_outputs;
	for (const auto &node : m_nodes)
	{
		if (auto *src = dynamic_cast<AsioSourceNode *>(node.get()))
		{
			asio_inputs.insert(asio_inputs.end(), src->getAsioChannelIndices().begin(), src->getAsioChannelIndices().end());
		}
		else if (auto *sink = dynamic_cast<AsioSinkNode *>(node.get()))
		{
			asio_outputs.insert(asio_outputs.end(), sink->getAsioChannelIndices().begin(), sink->getAsioChannelIndices().end());
		}
	}
	// Remove duplicates if necessary before passing to createBuffers
	// std::sort(asio_inputs.begin(), ...); asio_inputs.erase(std::unique(...), ...);
	// std::sort(asio_outputs.begin(), ...); asio_outputs.erase(std::unique(...), ...);

	if (!m_asioManager->createBuffers(asio_inputs, asio_outputs))
		return false;

	return true;
}
bool AudioEngine::createAndConfigureNodes()
{
	std::cout << "Creating and configuring nodes...\n";
	AVSampleFormat format = getInternalFormat();
	const AVChannelLayout &layout = getInternalLayout();

	for (const auto &nodeCfg : m_config.nodes)
	{
		std::shared_ptr<AudioNode> newNode = nullptr;
		if (nodeCfg.type == "asio_source")
		{
			if (!m_asioManager)
			{
				std::cerr << "ASIO manager not available for asio_source\n";
				return false;
			}
			newNode = std::make_shared<AsioSourceNode>(nodeCfg.name, this, m_asioManager.get());
		}
		else if (nodeCfg.type == "asio_sink")
		{
			if (!m_asioManager)
			{
				std::cerr << "ASIO manager not available for asio_sink\n";
				return false;
			}
			newNode = std::make_shared<AsioSinkNode>(nodeCfg.name, this, m_asioManager.get());
		}
		else if (nodeCfg.type == "file_source")
		{
			newNode = std::make_shared<FileSourceNode>(nodeCfg.name, this);
		}
		else if (nodeCfg.type == "file_sink")
		{
			newNode = std::make_shared<FileSinkNode>(nodeCfg.name, this);
		}
		else if (nodeCfg.type == "ffmpeg_processor")
		{
			newNode = std::make_shared<FfmpegProcessorNode>(nodeCfg.name, this);
		}
		else
		{
			std::cerr << "Unknown node type: " << nodeCfg.type << "\n";
			return false;
		}

		if (!newNode->configure(nodeCfg.params, m_config.sampleRate, m_config.bufferSize, format, layout))
		{
			std::cerr << "Failed to configure node: " << nodeCfg.name << "\n";
			return false;
		}
		m_nodes.push_back(newNode);
		m_nodeMap[nodeCfg.name] = newNode;
		std::cout << "  Created node: " << nodeCfg.name << " (" << nodeCfg.type << ")\n";
	}
	return true;
}
bool AudioEngine::setupConnections()
{
	std::cout << "Setting up node connections...\n";
	m_connections.clear();
	for (const auto &connCfg : m_config.connections)
	{
		auto srcIt = m_nodeMap.find(connCfg.sourceNode);
		auto sinkIt = m_nodeMap.find(connCfg.sinkNode);
		if (srcIt == m_nodeMap.end() || sinkIt == m_nodeMap.end())
		{
			std::cerr << "Cannot create connection: Node not found (" << connCfg.sourceNode << " or " << connCfg.sinkNode << ")\n";
			return false;
		}
		m_connections.push_back({srcIt->second, connCfg.sourcePad, sinkIt->second, connCfg.sinkPad});
		std::cout << "  Connected: " << connCfg.sourceNode << ":" << connCfg.sourcePad
				  << " -> " << connCfg.sinkNode << ":" << connCfg.sinkPad << "\n";
	}
	// TODO: Determine processing order (e.g., topological sort) and store in m_processingOrder
	m_processingOrder = m_nodes; // Simple sequential order for now
	return true;
}
bool AudioEngine::sendRmeCommands()
{
	std::cout << "Sending initial RME OSC commands...\n";
	for (const auto &cmdCfg : m_config.rmeCommands)
	{
		if (!m_rmeController->sendCommand(cmdCfg.address, cmdCfg.args))
		{
			std::cerr << "Failed to send RME OSC command: " << cmdCfg.address << "\n";
			// Decide if this is fatal or just a warning
			// return false;
		}
	}
	return true;
}
void AudioEngine::run()
{
	if (m_isRunning.load())
		return;
	m_isRunning.store(true);
	std::cout << "AudioEngine starting...\n";
	// Start file node threads
	for (auto &node : m_nodes)
	{
		node->start();
	}

	if (m_useAsio)
	{
		if (!m_asioManager->start())
		{
			m_isRunning = false; /* error */
		}
		// Loop runs via ASIO callback
	}
	else
	{
		// Start file processing loop thread or run blocking here
		runFileProcessingLoop();
	}
}
void AudioEngine::stop()
{
	if (!m_isRunning.load())
		return;
	m_isRunning.store(false);
	std::cout << "AudioEngine stopping...\n";
	if (m_useAsio)
	{
		m_asioManager->stop();
	}
	// Stop file node threads (signal + join)
	for (auto &node : m_nodes)
	{
		node->stop();
	}
}
void AudioEngine::cleanup()
{
	stop();
	std::cout << "AudioEngine cleaning up...\n";
	for (auto &node : m_nodes)
	{
		node->cleanup();
	}
	m_nodes.clear();
	m_nodeMap.clear();
	m_connections.clear();
	m_processingOrder.clear();
	if (m_asioManager)
		m_asioManager->cleanup();
	// Cleanup RME controller, etc.
}
void AudioEngine::processAsioBlock(long doubleBufferIndex)
{
	if (!m_isRunning.load())
		return;

	// --- 1. Get Data from ASIO Sources ---
	std::vector<void *> currentAsioInputBuffers; // Temp storage for this block
	std::vector<void *> currentAsioOutputBuffers;
	// Populate these based on doubleBufferIndex and active channels using AsioManager

	for (auto &node : m_processingOrder)
	{
		if (auto *src = dynamic_cast<AsioSourceNode *>(node.get()))
		{
			// Get relevant buffer pointers for this node
			// m_asioManager->getBufferPointers(doubleBufferIndex, src->getAsioChannelIndices(), node_asio_buffers);
			// src->receiveAsioData(doubleBufferIndex, node_asio_buffers);
		}
	}

	// --- 2. Process Node Graph ---
	// Iterate through m_processingOrder or m_connections
	// Example: Iterate connections
	for (const auto &conn : m_connections)
	{
		std::optional<AudioBuffer> bufferOpt = conn.sourceNode->getOutputBuffer(conn.sourcePad);
		if (bufferOpt)
		{
			conn.sinkNode->setInputBuffer(*bufferOpt, conn.sinkPad);
			// Need buffer ownership/copying strategy here!
		}
		else
		{
			// Handle buffer not ready (e.g., from file source)
		}
	}
	// Call process on processor nodes after inputs are set
	for (auto &node : m_processingOrder)
	{
		if (node->getType() == AudioNode::NodeType::PROCESSOR)
		{
			node->process();
		}
	}

	// --- 3. Provide Data to ASIO Sinks ---
	for (auto &node : m_processingOrder)
	{
		if (auto *sink = dynamic_cast<AsioSinkNode *>(node.get()))
		{
			// Get relevant buffer pointers for this node
			// m_asioManager->getBufferPointers(doubleBufferIndex, sink->getAsioChannelIndices(), node_asio_buffers);
			// sink->provideAsioData(doubleBufferIndex, node_asio_buffers);
		}
	}

	// --- 4. Signal ASIO Ready ---
	// m_asioManager->asioOutputReady(); // Call placeholder
}
void AudioEngine::runFileProcessingLoop()
{
	while (m_isRunning.load())
	{
		// Check if file sources have data
		// Process graph similar to processAsioBlock
		// Check if file sinks are ready for data
		// Handle timing / sleep if running too fast
		std::this_thread::sleep_for(std::chrono::milliseconds(5)); // Prevent busy-wait
	}
}

// --- TODO: Implement remaining class methods and helper functions ---
// - ConfigurationParser::parse
// - AsioManager full implementation (using SDK)
// - OscController full implementation (using OSC lib)
// - AudioNode derived classes full implementation (configure, start, process, stop, cleanup, buffer handling)
// - AudioEngine::processAsioBlock / runFileProcessingLoop connection/buffer logic
// - Robust error handling
// - Buffer management strategy
