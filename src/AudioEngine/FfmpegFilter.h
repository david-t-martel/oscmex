#pragma once

#include <string>
#include <map>

extern "C"
{
#include <libavfilter/avfilter.h>
#include <libavutil/frame.h>
}

namespace AudioEngine
{

	/**
	 * @brief Wrapper for AVFilter graph handling
	 *
	 * Manages an FFmpeg filter graph for audio processing
	 */
	class FfmpegFilter
	{
	public:
		/**
		 * @brief Create a new FFmpeg filter
		 */
		FfmpegFilter();

		/**
		 * @brief Destroy the FFmpeg filter
		 */
		~FfmpegFilter();

		/**
		 * @brief Initialize the filter graph
		 *
		 * @param filterDescription FFmpeg filter description string
		 * @param sampleRate Sample rate in Hz
		 * @param format Sample format
		 * @param layout Channel layout
		 * @return true if initialization was successful
		 */
		bool initGraph(const std::string &filterDescription,
					   double sampleRate,
					   AVSampleFormat format,
					   const AVChannelLayout &layout);

		/**
		 * @brief Process audio through the filter graph
		 *
		 * @param inputFrame Input audio frame
		 * @param outputFrame Output audio frame (will be filled)
		 * @return true if processing was successful
		 */
		bool process(AVFrame *inputFrame, AVFrame *outputFrame);

		/**
		 * @brief Update a filter parameter
		 *
		 * @param filterName Name of the filter
		 * @param paramName Parameter name
		 * @param value New parameter value
		 * @return true if update was successful
		 */
		bool updateParameter(const std::string &filterName,
							 const std::string &paramName,
							 const std::string &value);

		/**
		 * @brief Clean up resources
		 */
		void cleanup();

	private:
		AVFilterGraph *m_graph;			// Filter graph
		AVFilterContext *m_srcContext;	// Source filter context
		AVFilterContext *m_sinkContext; // Sink filter context

		// Map of named filters in the graph for easier parameter updates
		std::map<std::string, AVFilterContext *> m_namedFilters;

		double m_sampleRate;			 // Current sample rate
		AVSampleFormat m_format;		 // Current sample format
		AVChannelLayout m_channelLayout; // Current channel layout
		bool m_initialized;				 // Initialization state
	};

} // namespace AudioEngine
