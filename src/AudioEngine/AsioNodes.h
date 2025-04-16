#pragma once

#include "AudioNode.h"
#include "AsioManager.h"

#include <vector>
#include <mutex>
#include <optional>

// Forward declare FFmpeg types
struct SwrContext;

namespace AudioEngine
{

	/**
	 * @brief Node that reads data from ASIO inputs
	 *
	 * Takes data from ASIO hardware inputs and makes it available
	 * as a source in the processing graph.
	 */
	class AsioSourceNode : public AudioNode
	{
	public:
		/**
		 * @brief Construct a new AsioSourceNode
		 *
		 * @param name Unique node name
		 * @param engine Pointer to parent engine
		 * @param asioMgr Pointer to ASIO manager
		 */
		AsioSourceNode(std::string name, AudioEngine *engine, AsioManager *asioMgr);

		/**
		 * @brief Destroy the AsioSourceNode
		 */
		~AsioSourceNode();

		/**
		 * @brief Configure the node with parameters
		 *
		 * Expected params:
		 * - "channels": Comma-separated list of ASIO input channels to use
		 *
		 * @param params Configuration parameters
		 * @param sampleRate System sample rate
		 * @param bufferSize System buffer size
		 * @param internalFormat Internal sample format
		 * @param internalLayout Internal channel layout
		 * @return true if configuration succeeded
		 * @return false if configuration failed
		 */
		bool configure(
			const std::map<std::string, std::string> &params,
			double sampleRate,
			long bufferSize,
			AVSampleFormat internalFormat,
			const AVChannelLayout &internalLayout) override;

		/**
		 * @brief Start the node (no-op for ASIO nodes)
		 *
		 * @return true Always succeeds
		 */
		bool start() override { return true; }

		/**
		 * @brief Process the node (no-op for ASIO source)
		 *
		 * Data is pushed via receiveAsioData instead.
		 */
		void process() override {}

		/**
		 * @brief Stop the node (no-op for ASIO nodes)
		 */
		void stop() override {}

		/**
		 * @brief Clean up resources
		 */
		void cleanup() override;

		/**
		 * @brief Get the output buffer
		 *
		 * @param padIndex Output pad index
		 * @return std::optional<AudioBuffer> Buffer if available, empty optional if not
		 */
		std::optional<AudioBuffer> getOutputBuffer(int padIndex = 0) override;

		/**
		 * @brief Receive data from ASIO callback
		 *
		 * Called by AudioEngine from ASIO callback.
		 *
		 * @param doubleBufferIndex Current buffer index
		 * @param asioBuffers Pointers to ASIO buffer data
		 */
		void receiveAsioData(long doubleBufferIndex, const std::vector<void *> &asioBuffers);

		/**
		 * @brief Get ASIO channel indices used by this node
		 *
		 * @return const std::vector<long>& Vector of channel indices
		 */
		const std::vector<long> &getAsioChannelIndices() const { return m_asioChannelIndices; }

	private:
		AsioManager *m_asioMgr;							  // ASIO manager
		std::vector<long> m_asioChannelIndices;			  // ASIO channel indices
		AudioBuffer m_outputBuffer;						  // Output buffer
		AVSampleFormat m_asioFormat = AV_SAMPLE_FMT_NONE; // ASIO buffer format
		SwrContext *m_swrCtx = nullptr;					  // Resampler context
		std::vector<uint8_t *> m_resampledBuf;			  // Temporary buffer for resampling
		int m_resampledLinesize = 0;					  // Line size for resampled buffer
		std::mutex m_bufferMutex;						  // Mutex for thread safety
		bool m_newDataAvailable = false;				  // Flag for new data
	};

	/**
	 * @brief Node that writes data to ASIO outputs
	 *
	 * Takes data from the processing graph and sends it to
	 * ASIO hardware outputs.
	 */
	class AsioSinkNode : public AudioNode
	{
	public:
		/**
		 * @brief Construct a new AsioSinkNode
		 *
		 * @param name Unique node name
		 * @param engine Pointer to parent engine
		 * @param asioMgr Pointer to ASIO manager
		 */
		AsioSinkNode(std::string name, AudioEngine *engine, AsioManager *asioMgr);

		/**
		 * @brief Destroy the AsioSinkNode
		 */
		~AsioSinkNode();

		/**
		 * @brief Configure the node with parameters
		 *
		 * Expected params:
		 * - "channels": Comma-separated list of ASIO output channels to use
		 *
		 * @param params Configuration parameters
		 * @param sampleRate System sample rate
		 * @param bufferSize System buffer size
		 * @param internalFormat Internal sample format
		 * @param internalLayout Internal channel layout
		 * @return true if configuration succeeded
		 * @return false if configuration failed
		 */
		bool configure(
			const std::map<std::string, std::string> &params,
			double sampleRate,
			long bufferSize,
			AVSampleFormat internalFormat,
			const AVChannelLayout &internalLayout) override;

		/**
		 * @brief Start the node (no-op for ASIO nodes)
		 *
		 * @return true Always succeeds
		 */
		bool start() override { return true; }

		/**
		 * @brief Process the node (no-op for ASIO sink)
		 *
		 * Data is pulled via provideAsioData instead.
		 */
		void process() override {}

		/**
		 * @brief Stop the node (no-op for ASIO nodes)
		 */
		void stop() override {}

		/**
		 * @brief Clean up resources
		 */
		void cleanup() override;

		/**
		 * @brief Set the input buffer
		 *
		 * @param buffer Buffer to send to ASIO
		 * @param padIndex Input pad index
		 * @return true if buffer was accepted
		 * @return false if buffer was rejected
		 */
		bool setInputBuffer(const AudioBuffer &buffer, int padIndex = 0) override;

		/**
		 * @brief Provide data to ASIO callback
		 *
		 * Called by AudioEngine from ASIO callback.
		 *
		 * @param doubleBufferIndex Current buffer index
		 * @param asioBuffers Pointers to ASIO buffer data
		 */
		void provideAsioData(long doubleBufferIndex, const std::vector<void *> &asioBuffers);

		/**
		 * @brief Get ASIO channel indices used by this node
		 *
		 * @return const std::vector<long>& Vector of channel indices
		 */
		const std::vector<long> &getAsioChannelIndices() const { return m_asioChannelIndices; }

	private:
		AsioManager *m_asioMgr;							  // ASIO manager
		std::vector<long> m_asioChannelIndices;			  // ASIO channel indices
		AudioBuffer m_inputBuffer;						  // Input buffer
		AVSampleFormat m_asioFormat = AV_SAMPLE_FMT_NONE; // ASIO buffer format
		SwrContext *m_swrCtx = nullptr;					  // Resampler context
		std::vector<uint8_t *> m_resampledBuf;			  // Temporary buffer for resampling
		int m_resampledLinesize = 0;					  // Line size for resampled buffer
		std::mutex m_bufferMutex;						  // Mutex for thread safety
		bool m_hasData = false;							  // Flag for data availability
	};

} // namespace AudioEngine
