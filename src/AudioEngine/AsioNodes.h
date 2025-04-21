#pragma once

#include "AudioNode.h"
#include "AsioManager.h"
#include "AudioBuffer.h"
#include <vector>
#include <string>
#include <mutex>
#include <memory>

namespace AudioEngine
{

	/**
	 * @brief Node that receives audio from ASIO inputs
	 */
	class AsioSourceNode : public AudioNode
	{
	public:
		/**
		 * @brief Construct a new AsioSourceNode
		 *
		 * @param name Node name
		 * @param engine Pointer to AudioEngine
		 * @param asioManager Pointer to AsioManager
		 */
		AsioSourceNode(const std::string &name, class AudioEngine *engine, AsioManager *asioManager);

		/**
		 * @brief Destroy the AsioSourceNode
		 */
		virtual ~AsioSourceNode();

		/**
		 * @brief Configure the node with parameters
		 *
		 * @param params Node parameters (JSON object)
		 * @param sampleRate Sample rate
		 * @param bufferSize Buffer size
		 * @param format Sample format
		 * @param channelLayout Channel layout
		 * @return true if configuration succeeded
		 */
		virtual bool configure(const std::string &params, double sampleRate, long bufferSize,
							   AVSampleFormat format, AVChannelLayout channelLayout) override;

		/**
		 * @brief Start the node
		 *
		 * @return true if start succeeded
		 */
		virtual bool start() override;

		/**
		 * @brief Process audio (called from AudioEngine)
		 *
		 * @return true if processing succeeded
		 */
		virtual bool process() override;

		/**
		 * @brief Stop the node
		 *
		 * @return true if stop succeeded
		 */
		virtual bool stop() override;

		/**
		 * @brief Get the node's type
		 *
		 * @return NodeType
		 */
		virtual NodeType getType() const override { return NodeType::ASIO_SOURCE; }

		/**
		 * @brief Get the number of output pads
		 *
		 * @return int
		 */
		virtual int getOutputPadCount() const override { return 1; }

		/**
		 * @brief Get the number of input pads
		 *
		 * @return int
		 */
		virtual int getInputPadCount() const override { return 0; }

		/**
		 * @brief Get the output buffer for the specified pad
		 *
		 * @param padIndex Pad index
		 * @return shared_ptr<AudioBuffer>
		 */
		virtual std::shared_ptr<AudioBuffer> getOutputBuffer(int padIndex) override;

		/**
		 * @brief Set the input buffer for the specified pad (not used for source node)
		 *
		 * @param buffer Input buffer
		 * @param padIndex Pad index
		 * @return true if set succeeded
		 */
		virtual bool setInputBuffer(std::shared_ptr<AudioBuffer> buffer, int padIndex) override { return false; }

		/**
		 * @brief Receive ASIO data (called from AsioManager callback)
		 *
		 * @param doubleBufferIndex ASIO buffer index
		 * @param asioBuffers ASIO buffer pointers
		 * @return true if receive succeeded
		 */
		bool receiveAsioData(long doubleBufferIndex, const std::vector<void *> &asioBuffers);

	private:
		/**
		 * @brief Parse channel parameters from JSON
		 *
		 * @param params JSON parameters string
		 * @return true if parsing succeeded
		 */
		bool parseChannelParams(const std::string &params);

		/**
		 * @brief Find channel indices by name
		 *
		 * @param channelNames Channel names to find
		 * @return true if all channels were found
		 */
		bool findChannelIndices(const std::vector<std::string> &channelNames);

		AsioManager *m_asioManager;					 // Pointer to ASIO manager
		std::vector<long> m_asioChannelIndices;		 // ASIO channel indices
		std::vector<std::string> m_asioChannelNames; // ASIO channel names
		std::shared_ptr<AudioBuffer> m_outputBuffer; // Output buffer
		std::mutex m_bufferMutex;					 // Buffer mutex
		SwrContext *m_swrContext;					 // Sample format conversion context
	};

	/**
	 * @brief Node that sends audio to ASIO outputs
	 */
	class AsioSinkNode : public AudioNode
	{
	public:
		/**
		 * @brief Construct a new AsioSinkNode
		 *
		 * @param name Node name
		 * @param engine Pointer to AudioEngine
		 * @param asioManager Pointer to AsioManager
		 */
		AsioSinkNode(const std::string &name, class AudioEngine *engine, AsioManager *asioManager);

		/**
		 * @brief Destroy the AsioSinkNode
		 */
		virtual ~AsioSinkNode();

		/**
		 * @brief Configure the node with parameters
		 *
		 * @param params Node parameters (JSON object)
		 * @param sampleRate Sample rate
		 * @param bufferSize Buffer size
		 * @param format Sample format
		 * @param channelLayout Channel layout
		 * @return true if configuration succeeded
		 */
		virtual bool configure(const std::string &params, double sampleRate, long bufferSize,
							   AVSampleFormat format, AVChannelLayout channelLayout) override;

		/**
		 * @brief Start the node
		 *
		 * @return true if start succeeded
		 */
		virtual bool start() override;

		/**
		 * @brief Process audio (called from AudioEngine)
		 *
		 * @return true if processing succeeded
		 */
		virtual bool process() override;

		/**
		 * @brief Stop the node
		 *
		 * @return true if stop succeeded
		 */
		virtual bool stop() override;

		/**
		 * @brief Get the node's type
		 *
		 * @return NodeType
		 */
		virtual NodeType getType() const override { return NodeType::ASIO_SINK; }

		/**
		 * @brief Get the number of output pads
		 *
		 * @return int
		 */
		virtual int getOutputPadCount() const override { return 0; }

		/**
		 * @brief Get the number of input pads
		 *
		 * @return int
		 */
		virtual int getInputPadCount() const override { return 1; }

		/**
		 * @brief Get the output buffer for the specified pad (not used for sink node)
		 *
		 * @param padIndex Pad index
		 * @return shared_ptr<AudioBuffer>
		 */
		virtual std::shared_ptr<AudioBuffer> getOutputBuffer(int padIndex) override { return nullptr; }

		/**
		 * @brief Set the input buffer for the specified pad
		 *
		 * @param buffer Input buffer
		 * @param padIndex Pad index
		 * @return true if set succeeded
		 */
		virtual bool setInputBuffer(std::shared_ptr<AudioBuffer> buffer, int padIndex) override;

		/**
		 * @brief Provide ASIO data (called from AsioManager callback)
		 *
		 * @param doubleBufferIndex ASIO buffer index
		 * @param asioBuffers ASIO buffer pointers
		 * @return true if provide succeeded
		 */
		bool provideAsioData(long doubleBufferIndex, std::vector<void *> &asioBuffers);

	private:
		/**
		 * @brief Parse channel parameters from JSON
		 *
		 * @param params JSON parameters string
		 * @return true if parsing succeeded
		 */
		bool parseChannelParams(const std::string &params);

		/**
		 * @brief Find channel indices by name
		 *
		 * @param channelNames Channel names to find
		 * @return true if all channels were found
		 */
		bool findChannelIndices(const std::vector<std::string> &channelNames);

		AsioManager *m_asioManager;					 // Pointer to ASIO manager
		std::vector<long> m_asioChannelIndices;		 // ASIO channel indices
		std::vector<std::string> m_asioChannelNames; // ASIO channel names
		std::shared_ptr<AudioBuffer> m_inputBuffer;	 // Input buffer
		std::mutex m_bufferMutex;					 // Buffer mutex
		SwrContext *m_swrContext;					 // Sample format conversion context
	};

} // namespace AudioEngine
