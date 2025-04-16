#pragma once

#include <string>
#include <vector>
#include <functional>

// Define ASIO types to avoid pulling in the full SDK in the header
typedef long ASIOSampleType;
typedef long ASIOError;

namespace AudioEngine
{

	/**
	 * @brief Manages ASIO driver setup and buffer handling
	 *
	 * Handles driver loading, ASIO callbacks, buffer creation, and
	 * provides an abstraction layer over the ASIO SDK.
	 */
	class AsioManager
	{
	public:
		/**
		 * @brief Callback type for ASIO buffer switch
		 *
		 * @param doubleBufferIndex The current buffer index (0 or 1)
		 */
		using AsioCallback = std::function<void(long /*doubleBufferIndex*/)>;

		/**
		 * @brief Construct a new AsioManager
		 */
		AsioManager();

		/**
		 * @brief Destroy the AsioManager and clean up resources
		 */
		~AsioManager();

		/**
		 * @brief Load an ASIO driver by name
		 *
		 * @param deviceName The ASIO device name (e.g., "ASIO Fireface UCX II")
		 * @return true if driver was loaded successfully
		 * @return false if driver failed to load
		 */
		bool loadDriver(const std::string &deviceName);

		/**
		 * @brief Initialize the loaded ASIO device
		 *
		 * @param preferredSampleRate Preferred sample rate (0 = use default)
		 * @param preferredBufferSize Preferred buffer size (0 = use default)
		 * @return true if initialization succeeded
		 * @return false if initialization failed
		 */
		bool initDevice(double preferredSampleRate = 0.0, long preferredBufferSize = 0);

		/**
		 * @brief Create ASIO buffers for the specified channels
		 *
		 * @param inputChannels Vector of input channel indices to activate
		 * @param outputChannels Vector of output channel indices to activate
		 * @return true if buffer creation succeeded
		 * @return false if buffer creation failed
		 */
		bool createBuffers(const std::vector<long> &inputChannels, const std::vector<long> &outputChannels);

		/**
		 * @brief Start ASIO processing
		 *
		 * @return true if start succeeded
		 * @return false if start failed
		 */
		bool start();

		/**
		 * @brief Stop ASIO processing
		 */
		void stop();

		/**
		 * @brief Clean up all ASIO resources
		 */
		void cleanup();

		/**
		 * @brief Get the current buffer size
		 *
		 * @return long Buffer size in samples
		 */
		long getBufferSize() const { return m_bufferSize; }

		/**
		 * @brief Get the current sample rate
		 *
		 * @return double Sample rate in Hz
		 */
		double getSampleRate() const { return m_sampleRate; }

		/**
		 * @brief Get the ASIO sample type
		 *
		 * @return ASIOSampleType The sample format of the ASIO buffers
		 */
		ASIOSampleType getSampleType() const { return m_sampleType; }

		/**
		 * @brief Get a human-readable name for the current sample type
		 *
		 * @return std::string Sample type name (e.g., "Float32LSB")
		 */
		std::string getSampleTypeName() const;

		/**
		 * @brief Get input latency in samples
		 *
		 * @return long Input latency
		 */
		long getInputLatency() const { return m_inputLatency; }

		/**
		 * @brief Get output latency in samples
		 *
		 * @return long Output latency
		 */
		long getOutputLatency() const { return m_outputLatency; }

		/**
		 * @brief Find a channel index by name
		 *
		 * @param channelName Channel name to search for
		 * @param isInput True for input channel, false for output
		 * @param index Output parameter to store found index
		 * @return true if channel was found
		 * @return false if channel was not found
		 */
		bool findChannelIndex(const std::string &channelName, bool isInput, long &index) const;

		/**
		 * @brief Set the callback function for buffer processing
		 *
		 * @param callback Function to call when new buffers are available
		 */
		void setCallback(AsioCallback callback);

		/**
		 * @brief Get buffer pointers for the current processing block
		 *
		 * @param doubleBufferIndex Current buffer index (0 or 1)
		 * @param activeIndices Channel indices to get buffers for
		 * @param bufferPtrs Output parameter to store buffer pointers
		 * @return true if buffers were retrieved
		 * @return false if buffer retrieval failed
		 */
		bool getBufferPointers(long doubleBufferIndex, const std::vector<long> &activeIndices, std::vector<void *> &bufferPtrs);

	private:
		// ASIO SDK wrapper functions
		bool asioExit();
		bool asioInit(void *sysHandle);
		ASIOError asioGetChannels(long *numInputs, long *numOutputs);
		ASIOError asioGetBufferSize(long *minSize, long *maxSize, long *preferredSize, long *granularity);
		ASIOError asioCanSampleRate(double sampleRate);
		ASIOError asioGetSampleRate(double *sampleRate);
		ASIOError asioSetSampleRate(double sampleRate);
		ASIOError asioGetChannelInfo(long channel, bool isInput, void *info);
		ASIOError asioCreateBuffers(void *bufferInfos, long numChannels, long bufferSize, void *callbacks);
		ASIOError asioDisposeBuffers();
		ASIOError asioStart();
		ASIOError asioStop();
		ASIOError asioOutputReady();
		ASIOError asioGetLatencies(long *inputLatency, long *outputLatency);

		// Static callbacks for ASIO SDK (forwarded to instance)
		static void bufferSwitchCallback(long doubleBufferIndex, bool directProcess);
		static void sampleRateDidChangeCallback(double sRate);
		static long asioMessageCallback(long selector, long value, void *message, double *opt);
		static void *bufferSwitchTimeInfoCallback(void *params, long doubleBufferIndex, bool directProcess);

		// Instance callback handlers
		void onBufferSwitch(long doubleBufferIndex, bool directProcess);
		void onSampleRateDidChange(double sRate);
		long onAsioMessage(long selector, long value, void *message, double *opt);

		// Member variables
		bool m_driverLoaded = false;	 // Is a driver loaded?
		bool m_asioInitialized = false;	 // Is ASIO initialized?
		bool m_buffersCreated = false;	 // Are buffers created?
		bool m_processing = false;		 // Is processing active?
		long m_inputChannels = 0;		 // Number of available input channels
		long m_outputChannels = 0;		 // Number of available output channels
		long m_bufferSize = 0;			 // Buffer size in samples
		double m_sampleRate = 0.0;		 // Sample rate in Hz
		ASIOSampleType m_sampleType = 0; // Sample format
		long m_inputLatency = 0;		 // Input latency in samples
		long m_outputLatency = 0;		 // Output latency in samples

		std::vector<void *> m_bufferInfos;		 // ASIO buffer info structs (opaque here)
		std::vector<long> m_activeInputIndices;	 // Active input channel indices
		std::vector<long> m_activeOutputIndices; // Active output channel indices
		long m_numActiveChannels = 0;			 // Total active channels

		AsioCallback m_callback;		// User callback function
		static AsioManager *s_instance; // Singleton for ASIO callbacks
		void *m_asioDrivers = nullptr;	// Opaque pointer to AsioDrivers
	};

} // namespace AudioEngine
