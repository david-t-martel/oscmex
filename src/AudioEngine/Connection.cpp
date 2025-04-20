#include "Connection.h"
#include "AudioNode.h"
#include <sstream>

namespace AudioEngine
{

    Connection::Connection(AudioNode *sourceNode, int sourcePad,
                           AudioNode *sinkNode, int sinkPad)
        : m_sourceNode(sourceNode), m_sourcePad(sourcePad),
          m_sinkNode(sinkNode), m_sinkPad(sinkPad)
    {
    }

    bool Connection::transfer()
    {
        // Get output buffer from source node's output pad
        std::shared_ptr<AudioBuffer> buffer = m_sourceNode->getOutputBuffer(m_sourcePad);

        // If buffer is valid, pass it to sink node's input pad
        if (buffer)
        {
            return m_sinkNode->setInputBuffer(buffer, m_sinkPad);
        }

        return false;
    }

    std::string Connection::toString() const
    {
        std::stringstream ss;
        ss << m_sourceNode->getName() << " (pad " << m_sourcePad << ") -> "
           << m_sinkNode->getName() << " (pad " << m_sinkPad << ")";
        return ss.str();
    }

} // namespace AudioEngine