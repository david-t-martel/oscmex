#pragma once

#include "AudioNode.h"
#include <vector>
#include <mutex>

namespace AudioEngine
{

	// Forward declaration
	class AsioManager;

	/**
	 * @brief Node for receiving audio from ASIO inputs
	 */
	class AsioSourceNode : public AudioNode
	{
	public:
		/**
		 * @brief Create a new ASIO source node
		 *
		 * @param name Node name
		 * @param engine Reference to the audio engine
		 * @param asioManager Reference to the ASIO manager
		 */
		AsioSourceNode(const std::string &name, AudioEngine *engine, AsioManager *asioManager);

		/**
		 * @brief Destroy the ASIO source node
		 */
		~AsioSourceNode();

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

		int getInputPadCount() const override { return 0; }	 // No inputs
		int getOutputPadCount() const override { return 1; } // One output

		/**
		 * @brief Receive data from ASIO callbacks
		 *
		 * Called by AsioManager during bufferSwitch callback
		 *
		 * @param doubleBufferIndex ASIO double buffer index
		 * @param asioBuffers ASIO buffer pointers
		 * @return true if data was received successfully
		 */
		bool receiveAsioData(long doubleBufferIndex, void **asioBuffers);

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

		// Output buffer
		std::shared_ptr<AudioBuffer> m_outputBuffer;
		std::mutex m_bufferMutex;

		// Double buffering support
		bool m_doubleBufferSwitch;
		std::shared_ptr<AudioBuffer> m_outputBufferA;
		std::shared_ptr<AudioBuffer> m_outputBufferB;

		// Helper methods
		bool createBuffers();
		void convertAsioToAudioBuffer(void *asioBuffer, uint8_t *destBuffer, long frames,
									  int asioType, AVSampleFormat format);
	};

} // namespace AudioEngine
