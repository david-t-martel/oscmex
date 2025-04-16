#include "FfmpegFilter.h"
#include <iostream>
#include <sstream>

extern "C"
{
#include <libavfilter/avfilter.h>
#include <libavfilter/buffersink.h>
#include <libavfilter/buffersrc.h>
#include <libavutil/opt.h>
#include <libavutil/channel_layout.h>
}

namespace AudioEngine
{

	FfmpegFilter::FfmpegFilter()
		: m_graph(nullptr),
		  m_srcContext(nullptr),
		  m_sinkContext(nullptr),
		  m_sampleRate(0),
		  m_format(AV_SAMPLE_FMT_NONE),
		  m_bufferSize(0),
		  m_valid(false)
	{
		// Initialize channel layout to empty
		av_channel_layout_default(&m_channelLayout, 0);
	}

	FfmpegFilter::~FfmpegFilter()
	{
		cleanup();
	}

	void FfmpegFilter::cleanup()
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		m_namedFilters.clear();

		// Free the filter graph
		if (m_graph)
		{
			avfilter_graph_free(&m_graph);
			m_graph = nullptr;
		}

		m_srcContext = nullptr;
		m_sinkContext = nullptr;
		m_valid = false;
	}

	bool FfmpegFilter::initGraph(const std::string &filterDescription,
								 double sampleRate,
								 AVSampleFormat format,
								 const AVChannelLayout &layout,
								 long bufferSize)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		// Cleanup any existing graph
		if (m_graph)
		{
			avfilter_graph_free(&m_graph);
			m_graph = nullptr;
		}

		m_namedFilters.clear();
		m_valid = false;

		// Store the configuration
		m_filterDescription = filterDescription;
		m_sampleRate = sampleRate;
		m_format = format;
		if (av_channel_layout_copy(&m_channelLayout, &layout) < 0)
		{
			std::cerr << "Failed to copy channel layout" << std::endl;
			return false;
		}
		m_bufferSize = bufferSize;

		// Create filter graph
		m_graph = avfilter_graph_alloc();
		if (!m_graph)
		{
			std::cerr << "Failed to allocate filter graph" << std::endl;
			return false;
		}

		// Get the abuffer and abuffersink filters
		const AVFilter *abuffer = avfilter_get_by_name("abuffer");
		const AVFilter *abuffersink = avfilter_get_by_name("abuffersink");
		if (!abuffer || !abuffersink)
		{
			std::cerr << "Failed to get filters" << std::endl;
			cleanup();
			return false;
		}

		// Create source filter (abuffer)
		char args[512];
		char ch_layout[128];
		av_channel_layout_describe(&m_channelLayout, ch_layout, sizeof(ch_layout));

		snprintf(args, sizeof(args),
				 "sample_rate=%d:sample_fmt=%s:channel_layout=%s:time_base=1/%d",
				 static_cast<int>(m_sampleRate),
				 av_get_sample_fmt_name(m_format),
				 ch_layout,
				 static_cast<int>(m_sampleRate));

		int ret = avfilter_graph_create_filter(&m_srcContext, abuffer, "src",
											   args, nullptr, m_graph);
		if (ret < 0)
		{
			std::cerr << "Failed to create abuffer filter: " << ret << std::endl;
			cleanup();
			return false;
		}

		// Create sink filter (abuffersink)
		ret = avfilter_graph_create_filter(&m_sinkContext, abuffersink, "sink",
										   nullptr, nullptr, m_graph);
		if (ret < 0)
		{
			std::cerr << "Failed to create abuffersink filter: " << ret << std::endl;
			cleanup();
			return false;
		}

		// Set output properties on the sink
		ret = av_opt_set_bin(m_sinkContext, "sample_fmts",
							 (uint8_t *)&m_format, sizeof(m_format),
							 AV_OPT_SEARCH_CHILDREN);
		if (ret < 0)
		{
			std::cerr << "Failed to set output sample format" << std::endl;
			cleanup();
			return false;
		}

		ret = av_opt_set_bin(m_sinkContext, "ch_layouts",
							 (uint8_t *)&m_channelLayout, sizeof(m_channelLayout),
							 AV_OPT_SEARCH_CHILDREN);
		if (ret < 0)
		{
			std::cerr << "Failed to set output channel layout" << std::endl;
			cleanup();
			return false;
		}

		ret = av_opt_set_bin(m_sinkContext, "sample_rates",
							 (uint8_t *)&m_sampleRate, sizeof(m_sampleRate),
							 AV_OPT_SEARCH_CHILDREN);
		if (ret < 0)
		{
			std::cerr << "Failed to set output sample rate" << std::endl;
			cleanup();
			return false;
		}

		// Parse the filter graph description
		AVFilterInOut *inputs = nullptr, *outputs = nullptr;

		ret = avfilter_graph_parse2(m_graph, m_filterDescription.c_str(), &inputs, &outputs);
		if (ret < 0)
		{
			std::cerr << "Failed to parse filter graph: " << ret << std::endl;
			cleanup();
			avfilter_inout_free(&inputs);
			avfilter_inout_free(&outputs);
			return false;
		}

		// Link the inputs/outputs to our source/sink
		if (inputs)
		{
			inputs->name = av_strdup("in");
			inputs->filter_ctx = m_srcContext;
			inputs->pad_idx = 0;
			inputs->next = nullptr;
		}

		if (outputs)
		{
			outputs->name = av_strdup("out");
			outputs->filter_ctx = m_sinkContext;
			outputs->pad_idx = 0;
			outputs->next = nullptr;
		}

		// Configure the filter graph
		ret = avfilter_graph_config(m_graph, nullptr);
		if (ret < 0)
		{
			std::cerr << "Failed to configure filter graph: " << ret << std::endl;
			cleanup();
			avfilter_inout_free(&inputs);
			avfilter_inout_free(&outputs);
			return false;
		}

		avfilter_inout_free(&inputs);
		avfilter_inout_free(&outputs);

		// Build a map of named filters
		for (unsigned int i = 0; i < m_graph->nb_filters; i++)
		{
			AVFilterContext *filter = m_graph->filters[i];

			// Skip source/sink filters
			if (filter == m_srcContext || filter == m_sinkContext)
				continue;

			// Store filter by name for parameter access
			m_namedFilters[filter->name] = filter;
			std::cout << "Filter registered: " << filter->name << " (" << filter->filter->name << ")" << std::endl;
		}

		m_valid = true;
		return true;
	}

	bool FfmpegFilter::process(AVFrame *inputFrame, AVFrame *outputFrame)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		if (!m_valid || !m_graph || !m_srcContext || !m_sinkContext)
		{
			std::cerr << "Filter graph not initialized" << std::endl;
			return false;
		}

		// Push the input frame into the graph
		int ret = av_buffersrc_add_frame_flags(m_srcContext, inputFrame, AV_BUFFERSRC_FLAG_KEEP_REF);
		if (ret < 0)
		{
			std::cerr << "Error feeding the filter graph: " << ret << std::endl;
			return false;
		}

		// Pull the output frame from the graph
		ret = av_buffersink_get_frame(m_sinkContext, outputFrame);
		if (ret < 0)
		{
			if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF)
			{
				std::cerr << "Error getting frame from the filter graph: " << ret << std::endl;
			}
			return false;
		}

		return true;
	}

	bool FfmpegFilter::updateParameter(const std::string &filterName,
									   const std::string &paramName,
									   const std::string &value)
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		if (!m_valid)
		{
			std::cerr << "Filter graph not initialized" << std::endl;
			return false;
		}

		// Find the filter context
		AVFilterContext *filterCtx = findFilterByName(filterName);
		if (!filterCtx)
		{
			std::cerr << "Filter not found: " << filterName << std::endl;
			return false;
		}

		// Set the parameter
		int ret = avfilter_graph_send_command(m_graph, filterName.c_str(),
											  paramName.c_str(), value.c_str(),
											  nullptr, 0, 0);

		if (ret < 0)
		{
			char errbuf[AV_ERROR_MAX_STRING_SIZE];
			av_strerror(ret, errbuf, sizeof(errbuf));
			std::cerr << "Failed to set parameter '" << paramName << "' on filter '"
					  << filterName << "': " << errbuf << std::endl;
			return false;
		}

		return true;
	}

	bool FfmpegFilter::reset()
	{
		std::lock_guard<std::mutex> lock(m_mutex);

		if (!m_valid)
		{
			std::cerr << "Filter graph not initialized" << std::endl;
			return false;
		}

		// Reinitialize with the same parameters
		return initGraph(m_filterDescription, m_sampleRate, m_format, m_channelLayout, m_bufferSize);
	}

	AVFilterContext *FfmpegFilter::findFilterByName(const std::string &name) const
	{
		// Check if it's in our named filters map
		auto it = m_namedFilters.find(name);
		if (it != m_namedFilters.end())
		{
			return it->second;
		}

		// Otherwise search through all filters
		if (m_graph)
		{
			for (unsigned int i = 0; i < m_graph->nb_filters; i++)
			{
				if (m_graph->filters[i]->name && name == m_graph->filters[i]->name)
				{
					return m_graph->filters[i];
				}
			}
		}

		return nullptr;
	}

} // namespace AudioEngine
