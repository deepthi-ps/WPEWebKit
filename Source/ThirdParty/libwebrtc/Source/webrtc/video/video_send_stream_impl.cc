/*
 *  Copyright 2018 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */
#include "video/video_send_stream_impl.h"

#include <algorithm>
#include <string>
#include <utility>

#include "call/rtp_transport_controller_send_interface.h"
#include "modules/pacing/packet_router.h"
#include "modules/rtp_rtcp/source/rtp_sender.h"
#include "rtc_base/checks.h"
#include "rtc_base/experiments/alr_experiment.h"
#include "rtc_base/file.h"
#include "rtc_base/location.h"
#include "rtc_base/logging.h"
#include "rtc_base/numerics/safe_conversions.h"
#include "rtc_base/trace_event.h"
#include "system_wrappers/include/field_trial.h"

namespace webrtc {
namespace internal {
namespace {

// Max positive size difference to treat allocations as "similar".
static constexpr int kMaxVbaSizeDifferencePercent = 10;
// Max time we will throttle similar video bitrate allocations.
static constexpr int64_t kMaxVbaThrottleTimeMs = 500;

bool TransportSeqNumExtensionConfigured(const VideoSendStream::Config& config) {
  const std::vector<RtpExtension>& extensions = config.rtp.extensions;
  return std::find_if(
             extensions.begin(), extensions.end(), [](const RtpExtension& ext) {
               return ext.uri == RtpExtension::kTransportSequenceNumberUri;
             }) != extensions.end();
}

const char kForcedFallbackFieldTrial[] =
    "WebRTC-VP8-Forced-Fallback-Encoder-v2";

absl::optional<int> GetFallbackMinBpsFromFieldTrial() {
  if (!webrtc::field_trial::IsEnabled(kForcedFallbackFieldTrial))
    return absl::nullopt;

  std::string group =
      webrtc::field_trial::FindFullName(kForcedFallbackFieldTrial);
  if (group.empty())
    return absl::nullopt;

  int min_pixels;
  int max_pixels;
  int min_bps;
  if (sscanf(group.c_str(), "Enabled-%d,%d,%d", &min_pixels, &max_pixels,
             &min_bps) != 3) {
    return absl::nullopt;
  }

  if (min_bps <= 0)
    return absl::nullopt;

  return min_bps;
}

int GetEncoderMinBitrateBps() {
  const int kDefaultEncoderMinBitrateBps = 30000;
  return GetFallbackMinBpsFromFieldTrial().value_or(
      kDefaultEncoderMinBitrateBps);
}

// Calculate max padding bitrate for a multi layer codec.
int CalculateMaxPadBitrateBps(const std::vector<VideoStream>& streams,
                              int min_transmit_bitrate_bps,
                              bool pad_to_min_bitrate,
                              bool alr_probing) {
  int pad_up_to_bitrate_bps = 0;

  // Filter out only the active streams;
  std::vector<VideoStream> active_streams;
  for (const VideoStream& stream : streams) {
    if (stream.active)
      active_streams.emplace_back(stream);
  }

  if (active_streams.size() > 1) {
    if (alr_probing) {
      // With alr probing, just pad to the min bitrate of the lowest stream,
      // probing will handle the rest of the rampup.
      pad_up_to_bitrate_bps = active_streams[0].min_bitrate_bps;
    } else {
      // Pad to min bitrate of the highest layer.
      pad_up_to_bitrate_bps =
          active_streams[active_streams.size() - 1].min_bitrate_bps;
      // Add target_bitrate_bps of the lower layers.
      for (size_t i = 0; i < active_streams.size() - 1; ++i)
        pad_up_to_bitrate_bps += active_streams[i].target_bitrate_bps;
    }
  } else if (!active_streams.empty() && pad_to_min_bitrate) {
    pad_up_to_bitrate_bps = active_streams[0].min_bitrate_bps;
  }

  pad_up_to_bitrate_bps =
      std::max(pad_up_to_bitrate_bps, min_transmit_bitrate_bps);

  return pad_up_to_bitrate_bps;
}

RtpSenderObservers CreateObservers(CallStats* call_stats,
                                   EncoderRtcpFeedback* encoder_feedback,
                                   SendStatisticsProxy* stats_proxy,
                                   SendDelayStats* send_delay_stats) {
  RtpSenderObservers observers;
  observers.rtcp_rtt_stats = call_stats;
  observers.intra_frame_callback = encoder_feedback;
  observers.rtcp_stats = stats_proxy;
  observers.rtp_stats = stats_proxy;
  observers.bitrate_observer = stats_proxy;
  observers.frame_count_observer = stats_proxy;
  observers.rtcp_type_observer = stats_proxy;
  observers.send_delay_observer = stats_proxy;
  observers.send_packet_observer = send_delay_stats;
  return observers;
}

absl::optional<AlrExperimentSettings> GetAlrSettings(
    VideoEncoderConfig::ContentType content_type) {
  if (content_type == VideoEncoderConfig::ContentType::kScreen) {
    return AlrExperimentSettings::CreateFromFieldTrial(
        AlrExperimentSettings::kScreenshareProbingBweExperimentName);
  }
  return AlrExperimentSettings::CreateFromFieldTrial(
      AlrExperimentSettings::kStrictPacingAndProbingExperimentName);
}

bool SameStreamsEnabled(const VideoBitrateAllocation& lhs,
                        const VideoBitrateAllocation& rhs) {
  for (size_t si = 0; si < kMaxSpatialLayers; ++si) {
    for (size_t ti = 0; ti < kMaxTemporalStreams; ++ti) {
      if (lhs.HasBitrate(si, ti) != rhs.HasBitrate(si, ti)) {
        return false;
      }
    }
  }
  return true;
}
}  // namespace

// CheckEncoderActivityTask is used for tracking when the encoder last produced
// and encoded video frame. If the encoder has not produced anything the last
// kEncoderTimeOutMs we also want to stop sending padding.
class VideoSendStreamImpl::CheckEncoderActivityTask : public rtc::QueuedTask {
 public:
  static const int kEncoderTimeOutMs = 2000;
  explicit CheckEncoderActivityTask(
      const rtc::WeakPtr<VideoSendStreamImpl>& send_stream)
      : activity_(0), send_stream_(std::move(send_stream)), timed_out_(false) {}

  void Stop() {
    RTC_CHECK(task_checker_.CalledSequentially());
    send_stream_.reset();
  }

  void UpdateEncoderActivity() {
    // UpdateEncoderActivity is called from VideoSendStreamImpl::Encoded on
    // whatever thread the real encoder implementation run on. In the case of
    // hardware encoders, there might be several encoders
    // running in parallel on different threads.
    rtc::AtomicOps::ReleaseStore(&activity_, 1);
  }

 private:
  bool Run() override {
    RTC_CHECK(task_checker_.CalledSequentially());
    if (!send_stream_)
      return true;
    if (!rtc::AtomicOps::AcquireLoad(&activity_)) {
      if (!timed_out_) {
        send_stream_->SignalEncoderTimedOut();
      }
      timed_out_ = true;
    } else if (timed_out_) {
      send_stream_->SignalEncoderActive();
      timed_out_ = false;
    }
    rtc::AtomicOps::ReleaseStore(&activity_, 0);

    rtc::TaskQueue::Current()->PostDelayedTask(
        std::unique_ptr<rtc::QueuedTask>(this), kEncoderTimeOutMs);
    // Return false to prevent this task from being deleted. Ownership has been
    // transferred to the task queue when PostDelayedTask was called.
    return false;
  }
  volatile int activity_;

  rtc::SequencedTaskChecker task_checker_;
  rtc::WeakPtr<VideoSendStreamImpl> send_stream_;
  bool timed_out_;
};

VideoSendStreamImpl::VideoSendStreamImpl(
    SendStatisticsProxy* stats_proxy,
    rtc::TaskQueue* worker_queue,
    CallStats* call_stats,
    RtpTransportControllerSendInterface* transport,
    BitrateAllocatorInterface* bitrate_allocator,
    SendDelayStats* send_delay_stats,
    VideoStreamEncoderInterface* video_stream_encoder,
    RtcEventLog* event_log,
    const VideoSendStream::Config* config,
    int initial_encoder_max_bitrate,
    double initial_encoder_bitrate_priority,
    std::map<uint32_t, RtpState> suspended_ssrcs,
    std::map<uint32_t, RtpPayloadState> suspended_payload_states,
    VideoEncoderConfig::ContentType content_type,
    std::unique_ptr<FecController> fec_controller)
    : has_alr_probing_(config->periodic_alr_bandwidth_probing ||
                       GetAlrSettings(content_type)),
      stats_proxy_(stats_proxy),
      config_(config),
      worker_queue_(worker_queue),
      check_encoder_activity_task_(nullptr),
      call_stats_(call_stats),
      transport_(transport),
      bitrate_allocator_(bitrate_allocator),
      max_padding_bitrate_(0),
      encoder_min_bitrate_bps_(0),
      encoder_target_rate_bps_(0),
      encoder_bitrate_priority_(initial_encoder_bitrate_priority),
      has_packet_feedback_(false),
      video_stream_encoder_(video_stream_encoder),
      encoder_feedback_(Clock::GetRealTimeClock(),
                        config_->rtp.ssrcs,
                        video_stream_encoder),
      bandwidth_observer_(transport->GetBandwidthObserver()),
      rtp_video_sender_(
          transport_->CreateRtpVideoSender(config_->rtp.ssrcs,
                                           suspended_ssrcs,
                                           suspended_payload_states,
                                           config_->rtp,
                                           config_->rtcp,
                                           config_->send_transport,
                                           CreateObservers(call_stats,
                                                           &encoder_feedback_,
                                                           stats_proxy_,
                                                           send_delay_stats),
                                           event_log,
                                           std::move(fec_controller))),
      weak_ptr_factory_(this) {
  RTC_DCHECK_RUN_ON(worker_queue_);
  RTC_LOG(LS_INFO) << "VideoSendStreamInternal: " << config_->ToString();
  weak_ptr_ = weak_ptr_factory_.GetWeakPtr();

  RTC_DCHECK(!config_->rtp.ssrcs.empty());
  RTC_DCHECK(call_stats_);
  RTC_DCHECK(transport_);
  RTC_DCHECK_NE(initial_encoder_max_bitrate, 0);

  if (initial_encoder_max_bitrate > 0) {
    encoder_max_bitrate_bps_ =
        rtc::dchecked_cast<uint32_t>(initial_encoder_max_bitrate);
  } else {
    // TODO(srte): Make sure max bitrate is not set to negative values. We don't
    // have any way to handle unset values in downstream code, such as the
    // bitrate allocator. Previously -1 was implicitly casted to UINT32_MAX, a
    // behaviour that is not safe. Converting to 10 Mbps should be safe for
    // reasonable use cases as it allows adding the max of multiple streams
    // without wrappping around.
    const int kFallbackMaxBitrateBps = 10000000;
    RTC_DLOG(LS_ERROR) << "ERROR: Initial encoder max bitrate = "
                       << initial_encoder_max_bitrate << " which is <= 0!";
    RTC_DLOG(LS_INFO) << "Using default encoder max bitrate = 10 Mbps";
    encoder_max_bitrate_bps_ = kFallbackMaxBitrateBps;
  }

  RTC_CHECK(AlrExperimentSettings::MaxOneFieldTrialEnabled());
  // If send-side BWE is enabled, check if we should apply updated probing and
  // pacing settings.
  if (TransportSeqNumExtensionConfigured(*config_)) {
    has_packet_feedback_ = true;

    absl::optional<AlrExperimentSettings> alr_settings =
        GetAlrSettings(content_type);
    if (alr_settings) {
      transport->EnablePeriodicAlrProbing(true);
      transport->SetPacingFactor(alr_settings->pacing_factor);
      configured_pacing_factor_ = alr_settings->pacing_factor;
      transport->SetQueueTimeLimit(alr_settings->max_paced_queue_time);
    } else {
      transport->EnablePeriodicAlrProbing(false);
      transport->SetPacingFactor(PacedSender::kDefaultPaceMultiplier);
      configured_pacing_factor_ = PacedSender::kDefaultPaceMultiplier;
      transport->SetQueueTimeLimit(PacedSender::kMaxQueueLengthMs);
    }
  }

  if (config_->periodic_alr_bandwidth_probing) {
    transport->EnablePeriodicAlrProbing(true);
  }

  RTC_DCHECK_GE(config_->rtp.payload_type, 0);
  RTC_DCHECK_LE(config_->rtp.payload_type, 127);

  video_stream_encoder_->SetStartBitrate(
      bitrate_allocator_->GetStartBitrate(this));

  // Only request rotation at the source when we positively know that the remote
  // side doesn't support the rotation extension. This allows us to prepare the
  // encoder in the expectation that rotation is supported - which is the common
  // case.
  bool rotation_applied =
      std::find_if(config_->rtp.extensions.begin(),
                   config_->rtp.extensions.end(),
                   [](const RtpExtension& extension) {
                     return extension.uri == RtpExtension::kVideoRotationUri;
                   }) == config_->rtp.extensions.end();

  video_stream_encoder_->SetSink(this, rotation_applied);
}

VideoSendStreamImpl::~VideoSendStreamImpl() {
  RTC_DCHECK_RUN_ON(worker_queue_);
  RTC_DCHECK(!rtp_video_sender_->IsActive())
      << "VideoSendStreamImpl::Stop not called";
  RTC_LOG(LS_INFO) << "~VideoSendStreamInternal: " << config_->ToString();
  transport_->DestroyRtpVideoSender(rtp_video_sender_);
}

void VideoSendStreamImpl::RegisterProcessThread(
    ProcessThread* module_process_thread) {
  rtp_video_sender_->RegisterProcessThread(module_process_thread);
}

void VideoSendStreamImpl::DeRegisterProcessThread() {
  rtp_video_sender_->DeRegisterProcessThread();
}

bool VideoSendStreamImpl::DeliverRtcp(const uint8_t* packet, size_t length) {
  // Runs on a network thread.
  RTC_DCHECK(!worker_queue_->IsCurrent());
  rtp_video_sender_->DeliverRtcp(packet, length);
  return true;
}

void VideoSendStreamImpl::UpdateActiveSimulcastLayers(
    const std::vector<bool> active_layers) {
  RTC_DCHECK_RUN_ON(worker_queue_);
  RTC_LOG(LS_INFO) << "VideoSendStream::UpdateActiveSimulcastLayers";
  bool previously_active = rtp_video_sender_->IsActive();
  rtp_video_sender_->SetActiveModules(active_layers);
  if (!rtp_video_sender_->IsActive() && previously_active) {
    // Payload router switched from active to inactive.
    StopVideoSendStream();
  } else if (rtp_video_sender_->IsActive() && !previously_active) {
    // Payload router switched from inactive to active.
    StartupVideoSendStream();
  }
}

void VideoSendStreamImpl::Start() {
  RTC_DCHECK_RUN_ON(worker_queue_);
  RTC_LOG(LS_INFO) << "VideoSendStream::Start";
  if (rtp_video_sender_->IsActive())
    return;
  TRACE_EVENT_INSTANT0("webrtc", "VideoSendStream::Start");
  rtp_video_sender_->SetActive(true);
  StartupVideoSendStream();
}

void VideoSendStreamImpl::StartupVideoSendStream() {
  RTC_DCHECK_RUN_ON(worker_queue_);
  bitrate_allocator_->AddObserver(
      this,
      MediaStreamAllocationConfig{
          static_cast<uint32_t>(encoder_min_bitrate_bps_),
          encoder_max_bitrate_bps_, static_cast<uint32_t>(max_padding_bitrate_),
          !config_->suspend_below_min_bitrate, config_->track_id,
          encoder_bitrate_priority_, has_packet_feedback_});
  // Start monitoring encoder activity.
  {
    rtc::CritScope lock(&encoder_activity_crit_sect_);
    RTC_DCHECK(!check_encoder_activity_task_);
    check_encoder_activity_task_ = new CheckEncoderActivityTask(weak_ptr_);
    worker_queue_->PostDelayedTask(
        std::unique_ptr<rtc::QueuedTask>(check_encoder_activity_task_),
        CheckEncoderActivityTask::kEncoderTimeOutMs);
  }

  video_stream_encoder_->SendKeyFrame();
}

void VideoSendStreamImpl::Stop() {
  RTC_DCHECK_RUN_ON(worker_queue_);
  RTC_LOG(LS_INFO) << "VideoSendStream::Stop";
  if (!rtp_video_sender_->IsActive())
    return;
  TRACE_EVENT_INSTANT0("webrtc", "VideoSendStream::Stop");
  rtp_video_sender_->SetActive(false);
  StopVideoSendStream();
}

void VideoSendStreamImpl::StopVideoSendStream() {
  bitrate_allocator_->RemoveObserver(this);
  {
    rtc::CritScope lock(&encoder_activity_crit_sect_);
    check_encoder_activity_task_->Stop();
    check_encoder_activity_task_ = nullptr;
  }
  video_stream_encoder_->OnBitrateUpdated(0, 0, 0);
  stats_proxy_->OnSetEncoderTargetRate(0);
}

void VideoSendStreamImpl::SignalEncoderTimedOut() {
  RTC_DCHECK_RUN_ON(worker_queue_);
  // If the encoder has not produced anything the last kEncoderTimeOutMs and it
  // is supposed to, deregister as BitrateAllocatorObserver. This can happen
  // if a camera stops producing frames.
  if (encoder_target_rate_bps_ > 0) {
    RTC_LOG(LS_INFO) << "SignalEncoderTimedOut, Encoder timed out.";
    bitrate_allocator_->RemoveObserver(this);
  }
}

void VideoSendStreamImpl::OnBitrateAllocationUpdated(
    const VideoBitrateAllocation& allocation) {
  if (!worker_queue_->IsCurrent()) {
    auto ptr = weak_ptr_;
    worker_queue_->PostTask([=] {
      if (!ptr.get())
        return;
      ptr->OnBitrateAllocationUpdated(allocation);
    });
    return;
  }

  RTC_DCHECK_RUN_ON(worker_queue_);

  int64_t now_ms = rtc::TimeMillis();
  if (encoder_target_rate_bps_ != 0) {
    if (video_bitrate_allocation_context_) {
      // If new allocation is within kMaxVbaSizeDifferencePercent larger than
      // the previously sent allocation and the same streams are still enabled,
      // it is considered "similar". We do not want send similar allocations
      // more once per kMaxVbaThrottleTimeMs.
      const VideoBitrateAllocation& last =
          video_bitrate_allocation_context_->last_sent_allocation;
      const bool is_similar =
          allocation.get_sum_bps() >= last.get_sum_bps() &&
          allocation.get_sum_bps() <
              (last.get_sum_bps() * (100 + kMaxVbaSizeDifferencePercent)) /
                  100 &&
          SameStreamsEnabled(allocation, last);
      if (is_similar &&
          (now_ms - video_bitrate_allocation_context_->last_send_time_ms) <
              kMaxVbaThrottleTimeMs) {
        // This allocation is too similar, cache it and return.
        video_bitrate_allocation_context_->throttled_allocation = allocation;
        return;
      }
    } else {
      video_bitrate_allocation_context_.emplace();
    }

    video_bitrate_allocation_context_->last_sent_allocation = allocation;
    video_bitrate_allocation_context_->throttled_allocation.reset();
    video_bitrate_allocation_context_->last_send_time_ms = now_ms;

    // Send bitrate allocation metadata only if encoder is not paused.
    rtp_video_sender_->OnBitrateAllocationUpdated(allocation);
  }
}

void VideoSendStreamImpl::SignalEncoderActive() {
  RTC_DCHECK_RUN_ON(worker_queue_);
  RTC_LOG(LS_INFO) << "SignalEncoderActive, Encoder is active.";
  bitrate_allocator_->AddObserver(
      this,
      MediaStreamAllocationConfig{
          static_cast<uint32_t>(encoder_min_bitrate_bps_),
          encoder_max_bitrate_bps_, static_cast<uint32_t>(max_padding_bitrate_),
          !config_->suspend_below_min_bitrate, config_->track_id,
          encoder_bitrate_priority_, has_packet_feedback_});
}

void VideoSendStreamImpl::OnEncoderConfigurationChanged(
    std::vector<VideoStream> streams,
    int min_transmit_bitrate_bps) {
  if (!worker_queue_->IsCurrent()) {
    rtc::WeakPtr<VideoSendStreamImpl> send_stream = weak_ptr_;
    worker_queue_->PostTask([send_stream, streams, min_transmit_bitrate_bps]() {
      if (send_stream)
        send_stream->OnEncoderConfigurationChanged(std::move(streams),
                                                   min_transmit_bitrate_bps);
    });
    return;
  }
  RTC_DCHECK_GE(config_->rtp.ssrcs.size(), streams.size());
  TRACE_EVENT0("webrtc", "VideoSendStream::OnEncoderConfigurationChanged");
  RTC_DCHECK_GE(config_->rtp.ssrcs.size(), streams.size());
  RTC_DCHECK_RUN_ON(worker_queue_);

  encoder_min_bitrate_bps_ =
      std::max(streams[0].min_bitrate_bps, GetEncoderMinBitrateBps());
  encoder_max_bitrate_bps_ = 0;
  double stream_bitrate_priority_sum = 0;
  for (const auto& stream : streams) {
    // We don't want to allocate more bitrate than needed to inactive streams.
    encoder_max_bitrate_bps_ += stream.active ? stream.max_bitrate_bps : 0;
    if (stream.bitrate_priority) {
      RTC_DCHECK_GT(*stream.bitrate_priority, 0);
      stream_bitrate_priority_sum += *stream.bitrate_priority;
    }
  }
  RTC_DCHECK_GT(stream_bitrate_priority_sum, 0);
  encoder_bitrate_priority_ = stream_bitrate_priority_sum;
  encoder_max_bitrate_bps_ =
      std::max(static_cast<uint32_t>(encoder_min_bitrate_bps_),
               encoder_max_bitrate_bps_);

  const VideoCodecType codec_type =
      PayloadStringToCodecType(config_->rtp.payload_name);
  if (codec_type == kVideoCodecVP9) {
    max_padding_bitrate_ = streams[0].target_bitrate_bps;
  } else {
    max_padding_bitrate_ = CalculateMaxPadBitrateBps(
        streams, min_transmit_bitrate_bps, config_->suspend_below_min_bitrate,
        has_alr_probing_);
  }

  // Clear stats for disabled layers.
  for (size_t i = streams.size(); i < config_->rtp.ssrcs.size(); ++i) {
    stats_proxy_->OnInactiveSsrc(config_->rtp.ssrcs[i]);
  }

  const size_t num_temporal_layers =
      streams.back().num_temporal_layers.value_or(1);

  rtp_video_sender_->SetEncodingData(streams[0].width, streams[0].height,
                                     num_temporal_layers);

  if (rtp_video_sender_->IsActive()) {
    // The send stream is started already. Update the allocator with new bitrate
    // limits.
    bitrate_allocator_->AddObserver(
        this, MediaStreamAllocationConfig{
                  static_cast<uint32_t>(encoder_min_bitrate_bps_),
                  encoder_max_bitrate_bps_,
                  static_cast<uint32_t>(max_padding_bitrate_),
                  !config_->suspend_below_min_bitrate, config_->track_id,
                  encoder_bitrate_priority_, has_packet_feedback_});
  }
}

EncodedImageCallback::Result VideoSendStreamImpl::OnEncodedImage(
    const EncodedImage& encoded_image,
    const CodecSpecificInfo* codec_specific_info,
    const RTPFragmentationHeader* fragmentation) {
  // Encoded is called on whatever thread the real encoder implementation run
  // on. In the case of hardware encoders, there might be several encoders
  // running in parallel on different threads.
  const size_t simulcast_idx =
      (codec_specific_info->codecType != kVideoCodecVP9)
          ? encoded_image.SpatialIndex().value_or(0)
          : 0;
  if (config_->post_encode_callback) {
    // TODO(nisse): Delete webrtc::EncodedFrame class, pass EncodedImage
    // instead.
    config_->post_encode_callback->EncodedFrameCallback(EncodedFrame(
        encoded_image._buffer, encoded_image._length, encoded_image._frameType,
        simulcast_idx, encoded_image.Timestamp()));
  }
  {
    rtc::CritScope lock(&encoder_activity_crit_sect_);
    if (check_encoder_activity_task_)
      check_encoder_activity_task_->UpdateEncoderActivity();
  }

  EncodedImageCallback::Result result = rtp_video_sender_->OnEncodedImage(
      encoded_image, codec_specific_info, fragmentation);

  // Check if there's a throttled VideoBitrateAllocation that we should try
  // sending.
  rtc::WeakPtr<VideoSendStreamImpl> send_stream = weak_ptr_;
  auto update_task = [send_stream]() {
    if (send_stream) {
      RTC_DCHECK_RUN_ON(send_stream->worker_queue_);
      auto& context = send_stream->video_bitrate_allocation_context_;
      if (context && context->throttled_allocation) {
        send_stream->OnBitrateAllocationUpdated(*context->throttled_allocation);
      }
    }
  };
  if (!worker_queue_->IsCurrent()) {
    worker_queue_->PostTask(update_task);
  } else {
    update_task();
  }

  return result;
}

std::map<uint32_t, RtpState> VideoSendStreamImpl::GetRtpStates() const {
  return rtp_video_sender_->GetRtpStates();
}

std::map<uint32_t, RtpPayloadState> VideoSendStreamImpl::GetRtpPayloadStates()
    const {
  return rtp_video_sender_->GetRtpPayloadStates();
}

uint32_t VideoSendStreamImpl::OnBitrateUpdated(uint32_t bitrate_bps,
                                               uint8_t fraction_loss,
                                               int64_t rtt,
                                               int64_t probing_interval_ms) {
  RTC_DCHECK_RUN_ON(worker_queue_);
  RTC_DCHECK(rtp_video_sender_->IsActive())
      << "VideoSendStream::Start has not been called.";

  rtp_video_sender_->OnBitrateUpdated(bitrate_bps, fraction_loss, rtt,
                                      stats_proxy_->GetSendFrameRate());
  encoder_target_rate_bps_ = rtp_video_sender_->GetPayloadBitrateBps();
  encoder_target_rate_bps_ =
      std::min(encoder_max_bitrate_bps_, encoder_target_rate_bps_);
  video_stream_encoder_->OnBitrateUpdated(encoder_target_rate_bps_,
                                          fraction_loss, rtt);
  stats_proxy_->OnSetEncoderTargetRate(encoder_target_rate_bps_);
  return rtp_video_sender_->GetProtectionBitrateBps();
}

}  // namespace internal
}  // namespace webrtc
