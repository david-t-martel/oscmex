#include "AudioNode.h"

namespace AudioEngine
{

	AudioNode::AudioNode(std::string name, NodeType type, AudioEngine *engine)
		: m_name(std::move(name)), m_type(type), m_engine(engine)
	{
		// Initialize default channel layout (mono)
		av_channel_layout_default(&m_internalLayout, 1);
	}

} // namespace AudioEngine
