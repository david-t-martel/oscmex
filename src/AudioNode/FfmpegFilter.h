#pragma once

#include <string>
#include <map>
#include <memory>
#include <mutex>

// Use local FFmpeg source headers with all required components
extern "C"
{
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/samplefmt.h>
#include <libavutil/channel_layout.h>
#include <libavutil/error.h>
#include <libavutil/frame.h>
}

namespace AudioEngine
{

	/**
	 * @brief Wrapper class for managing FFmpeg filter graphs
	 *
	 * This class handles creating, connecting, and executing
	 * FFmpeg audio filter graphs for real-time processing.
	 */
	class FfmpegFilter
	{
	public:
		/**
		 * @brief Create a new FFmpeg filter
		 */
		FfmpegFilter();

		/**
		 * @brief Destroy the filter graph
		 */
		~FfmpegFilter();

		/**
		 * @brief Initialize the filter graph
		 *
		 * @param filterDescription FFmpeg filter graph description string
		 * @param sampleRate Sample rate in Hz
		 * @param format Sample format
		 * @param layout Channel layout
		 * @param bufferSize Buffer size in frames
		 * @return true if initialization was successful
		 */
		bool initGraph(const std::string &filterDescription,
					   double sampleRate,
					   AVSampleFormat format,
					   const AVChannelLayout &layout,
					   long bufferSize);

		/**
		 * @brief Process audio through the filter graph
		 *
		 * @param inputFrame Input AVFrame
		 * @param outputFrame Output AVFrame
		 * @return true if processing was successful
		 */
		bool process(AVFrame *inputFrame, AVFrame *outputFrame);

		/**
		 * @brief Update a filter parameter
		 *
		 * @param filterName Name of the filter to update
		 * @param paramName Name of the parameter to update
		 * @param value New value for the parameter
		 * @return true if update was successful
		 */
		bool updateParameter(const std::string &filterName,
							 const std::string &paramName,
							 const std::string &value);

		/**
		 * @brief Check if filter graph is valid
		 *
		 * @return true if valid
		 */
		bool isValid() const { return m_valid; }

		/**
		 * @brief Reset filter graph state
		 *
		 * @return true if reset was successful
		 */
		bool reset();

	private:
		// FFmpeg filter graph
		AVFilterGraph *m_graph;

		// Filter contexts
		AVFilterContext *m_srcContext;	// Source (abuffer)
		AVFilterContext *m_sinkContext; // Sink (abuffersink)

		// Named filters for parameter control
		std::map<std::string, AVFilterContext *> m_namedFilters;

		// Filter configuration
		std::string m_filterDescription;
		double m_sampleRate;
		AVSampleFormat m_format;
		AVChannelLayout m_channelLayout;
		long m_bufferSize;

		// State
		bool m_valid;
		std::mutex m_mutex;

		// Helper methods
		void cleanup();
		AVFilterContext *findFilterByName(const std::string &name) const;
	};

} // namespace AudioEngine
