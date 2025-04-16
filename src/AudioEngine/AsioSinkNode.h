#pragma once

#include "AudioNode.h"
#include <vector>
#include <mutex>

namespace AudioEngine
{

	// Forward declaration
	class AsioManager;

	/**
	 * @brief Node for sending audio to ASIO outputs
	 */
	class AsioSinkNode : public AudioNode
	{
	public:
		/**
		 * @brief Create a new ASIO sink node
		 *
		 * @param name Node name
		 * @param engine Reference to the audio engine
		 * @param asioManager Reference to the ASIO manager
		 */
		AsioSinkNode(const std::string &name, AudioEngine *engine, AsioManager *asioManager);

		/**
		 * @brief Destroy the ASIO sink node
		 */
		~AsioSinkNode();

		// AudioNode interface implementations
		bool configure(const std::map<std::string, std::string> &params,
					   double sampleRate,
					   long bufferSize,
					   AVSampleFormat format,
					   const AVChannelLayout &layout) override;

		bool start() override;
		bool process() override;
		void stop() override;

		std::shared_ptr<AudioBuffer> getOutputBuffer(int padIndex) override;
		bool setInputBuffer(std::shared_ptr<AudioBuffer> buffer, int padIndex) override;

		int getInputPadCount() const override { return 1; }	 // One input
		int getOutputPadCount() const override { return 0; } // No outputs

		/**
		 * @brief Provide data for ASIO callbacks
		 *
		 * @param doubleBufferIndex ASIO double buffer index
		 * @param asioBuffers ASIO buffer pointers
		 * @return true if data was provided successfully
		 */
		bool provideAsioData(long doubleBufferIndex, void **asioBuffers);

		/**
		 * @brief Get the ASIO channel indices used by this node
		 *
		 * @return Vector of ASIO channel indices
		 */
		const std::vector<long> &getAsioChannelIndices() const { return m_asioChannelIndices; }

	private:
		// ASIO manager
		AsioManager *m_asioManager;

		// ASIO channel configuration
		std::vector<long> m_asioChannelIndices;

		// Input buffer
		std::shared_ptr<AudioBuffer> m_inputBuffer;
		std::mutex m_bufferMutex;

		// Double buffering support
		bool m_doubleBufferSwitch;
		std::shared_ptr<AudioBuffer> m_inputBufferA;
		std::shared_ptr<AudioBuffer> m_inputBufferB;

		// Buffer for silence when no input is available
		std::shared_ptr<AudioBuffer> m_silenceBuffer;
	};

} // namespace AudioEngine
