#ifndef SDS_DUMP_SENDER_H_SDS_AUDIO
#define SDS_DUMP_SENDER_H_SDS_AUDIO

/**
 * @file sds_dump_sender.h
 * @brief MIDI Sample Dump Standard (SDS) sender for sample download
 *
 * Streams a stored sample file back to the host in response to an SDS Dump
 * Request. The device acts as the SDS dump source: it sends the Dump Header,
 * waits for the host handshake, then sends data packets one at a time, each
 * gated on the host's ACK/NAK/WAIT/CANCEL response. If the host never
 * responds to the header, the transfer continues open-loop with a fixed
 * inter-packet interval, as permitted by the SDS spec. Once handshaking, a
 * missing packet response triggers retransmission of the same packet (the
 * packet or its ACK was lost in transit) rather than advancing.
 *
 * Sending is paced from the main loop via update(); no blocking occurs, so
 * audio and the sequencer keep running during a download.
 *
 * The device does not persist sample rates, so outgoing headers always claim
 * 44100 Hz (see SAMPLE_RATE_HZ).
 */

#include "drum/sysex/sds_protocol.h"
#include "etl/algorithm.h"
#include "etl/array.h"
#include "etl/optional.h"
#include "etl/span.h"

#include <cstdio>

namespace sds {

template <typename FileOperations> class DumpSender {
public:
  // If the host never responds to the header within two seconds (SDS spec),
  // it is assumed to be a non-handshaking receiver and the dump proceeds
  // open-loop.
  static constexpr uint64_t HEADER_RESPONSE_TIMEOUT_US = 2000000;
  // Once the host has handshaked, a missing packet response means the packet
  // (or its ACK) was lost in transit. Retransmit rather than advance: the
  // host is still waiting for exactly this packet, and skipping it would
  // lose data irrecoverably.
  static constexpr uint64_t RETRANSMIT_INTERVAL_US = 100000;
  // Open-loop pacing. SysEx goes out over USB only, where a 127-byte packet
  // is on the wire in a few milliseconds; 10 ms leaves headroom for the
  // receiver to store each packet, well under the ~20 ms SDS convention.
  static constexpr uint64_t OPEN_LOOP_INTERVAL_US = 10000;
  // A host that sent WAIT but never followed up is assumed gone.
  static constexpr uint64_t STALL_TIMEOUT_US = 30000000;
  // Sample rate reported in outgoing dump headers; the stored .pcm files
  // carry no rate metadata.
  static constexpr uint32_t SAMPLE_RATE_HZ = 44100;
  static constexpr size_t BYTES_PER_PACKET = 80; // 40 16-bit samples
  static constexpr size_t PACKET_SIZE = 123;     // type + num + 120 + checksum

  enum class State {
    Idle,
    AwaitingHeaderResponse,
    AwaitingPacketResponse,
    OpenLoop
  };

  constexpr DumpSender(FileOperations &file_ops, musin::Logger &logger)
      : file_ops_(file_ops), logger_(logger) {
  }

  // Handle a Dump Request message: [DUMP_REQUEST, sample# low, sample# high].
  // Sends the Dump Header on success, or CANCEL when the slot has no sample.
  template <typename SendMessage>
  Result handle_dump_request(const etl::span<const uint8_t> &message,
                             SendMessage send_message, absolute_time_t now) {
    if (message.size() < 3) {
      send_cancel(send_message);
      return Result::InvalidMessage;
    }
    if (state_ != State::Idle) {
      logger_.warn("SDS: Dump request while a dump is already active");
      send_cancel(send_message);
      return Result::StateError;
    }

    sample_number_ = ((static_cast<uint16_t>(message[1]) & 0x7F)) |
                     ((static_cast<uint16_t>(message[2]) & 0x7F) << 7);

    char filename[16];
    snprintf(filename, sizeof(filename), "/%02u.pcm", sample_number_);

    file_ = file_ops_.open_read(filename);
    if (!file_.has_value() || file_->size() < 2) {
      logger_.warn("SDS: Dump request for missing sample:",
                   static_cast<uint32_t>(sample_number_));
      file_.reset();
      send_cancel(send_message);
      return Result::FileError;
    }

    total_bytes_ = file_->size() & ~static_cast<size_t>(1); // whole words only
    bytes_sent_ = 0;
    packet_num_ = 0;
    wait_pending_ = false;

    send_dump_header(send_message);
    state_ = State::AwaitingHeaderResponse;
    last_activity_time_ = now;
    last_send_time_ = now;
    logger_.info("SDS: Dump started, bytes:",
                 static_cast<uint32_t>(total_bytes_));
    return Result::OK;
  }

  // Handle a host handshake response (ACK/NAK/WAIT/CANCEL) during a dump.
  template <typename SendMessage>
  Result handle_response(uint8_t type, SendMessage send_message,
                         absolute_time_t now) {
    if (state_ == State::Idle) {
      return Result::StateError;
    }
    last_activity_time_ = now;

    switch (type) {
    case ACK:
      wait_pending_ = false;
      // Complete only after the final packet went out (bytes_sent_ is zero
      // while the header awaits its response). Checked in every sending
      // state: a late-handshaking host may ACK the final open-loop packet,
      // and advancing would emit a spurious zero-filled packet past the end.
      if (transfer_complete()) {
        finish();
        logger_.info("SDS: Dump complete");
        return Result::SampleComplete;
      }
      send_next_packet(send_message, now);
      return Result::OK;
    case NAK:
      wait_pending_ = false;
      resend_last(send_message, now);
      return Result::OK;
    case WAIT:
      wait_pending_ = true;
      return Result::OK;
    case CANCEL:
      logger_.info("SDS: Dump cancelled by host");
      finish();
      return Result::Cancelled;
    default:
      return Result::InvalidMessage;
    }
  }

  // Drives handshake timeouts and open-loop pacing; call from the main loop.
  template <typename SendMessage>
  Result update(SendMessage send_message, absolute_time_t now) {
    if (state_ == State::Idle) {
      return Result::OK;
    }
    const int64_t elapsed = absolute_time_diff_us(last_activity_time_, now);
    if (elapsed > static_cast<int64_t>(STALL_TIMEOUT_US)) {
      logger_.warn("SDS: Dump stalled, aborting");
      finish();
      return Result::Cancelled;
    }
    if (wait_pending_) {
      return Result::OK; // Host asked us to wait; stall timeout still applies.
    }

    const int64_t since_send = absolute_time_diff_us(last_send_time_, now);
    switch (state_) {
    case State::AwaitingHeaderResponse:
      if (elapsed > static_cast<int64_t>(HEADER_RESPONSE_TIMEOUT_US)) {
        logger_.info("SDS: No header response, continuing open-loop");
        state_ = State::OpenLoop;
        send_next_packet(send_message, now);
      }
      return Result::OK;
    case State::AwaitingPacketResponse:
      // The host handshaked earlier but this packet's response is missing:
      // the packet or its ACK was lost. Retransmit until the host answers
      // or the stall timeout gives up on it.
      if (since_send > static_cast<int64_t>(RETRANSMIT_INTERVAL_US)) {
        resend_last(send_message, now);
      }
      return Result::OK;
    case State::OpenLoop:
      if (since_send > static_cast<int64_t>(OPEN_LOOP_INTERVAL_US)) {
        return advance_open_loop(send_message, now);
      }
      return Result::OK;
    default:
      return Result::OK;
    }
  }

  constexpr bool is_busy() const {
    return state_ != State::Idle;
  }

  constexpr etl::optional<uint16_t> current_sample_number_opt() const {
    if (state_ != State::Idle) {
      return sample_number_;
    }
    return etl::nullopt;
  }

private:
  FileOperations &file_ops_;
  musin::Logger &logger_;
  State state_ = State::Idle;
  etl::optional<typename FileOperations::ReadHandle> file_;
  uint16_t sample_number_ = 0;
  size_t total_bytes_ = 0;
  size_t bytes_sent_ = 0;
  uint8_t packet_num_ = 0;
  bool wait_pending_ = false;
  // Last host response (or transfer start); drives the stall timeout.
  absolute_time_t last_activity_time_{};
  // Last outgoing header/packet; drives retransmit and open-loop pacing.
  absolute_time_t last_send_time_{};
  etl::array<uint8_t, PACKET_SIZE> last_packet_{};
  bool header_is_last_sent_ = true;
  etl::array<uint8_t, 17> header_{};

  constexpr bool transfer_complete() const {
    return bytes_sent_ >= total_bytes_;
  }

  void finish() {
    file_.reset();
    state_ = State::Idle;
    wait_pending_ = false;
  }

  template <typename SendMessage> void send_cancel(SendMessage send_message) {
    const etl::array<uint8_t, 2> msg{CANCEL, 0};
    send_message(etl::span<const uint8_t>{msg});
  }

  // Encode a 21-bit value into three 7-bit bytes, LSB first (SDS format).
  static constexpr void encode_21bit(uint32_t value, uint8_t *out) {
    out[0] = value & 0x7F;
    out[1] = (value >> 7) & 0x7F;
    out[2] = (value >> 14) & 0x7F;
  }

  template <typename SendMessage>
  void send_dump_header(SendMessage send_message) {
    constexpr uint32_t period_ns = 1000000000U / SAMPLE_RATE_HZ;
    const uint32_t length_words = total_bytes_ / 2;

    header_[0] = DUMP_HEADER;
    header_[1] = sample_number_ & 0x7F;
    header_[2] = (sample_number_ >> 7) & 0x7F;
    header_[3] = 16; // bit depth
    encode_21bit(period_ns, &header_[4]);
    encode_21bit(length_words, &header_[7]);
    encode_21bit(length_words, &header_[10]); // loop start = length
    encode_21bit(length_words, &header_[13]); // loop end = length
    header_[16] = 0x7F;                       // no loop

    header_is_last_sent_ = true;
    send_message(etl::span<const uint8_t>{header_});
  }

  // Pack a signed 16-bit sample into the SDS left-justified 3-byte format
  // (inverse of Protocol::unpack_16bit_sample).
  static constexpr void pack_16bit_sample(int16_t sample, uint8_t *out) {
    const uint16_t unsigned_sample =
        static_cast<uint16_t>(static_cast<int32_t>(sample) + 0x8000);
    out[0] = (unsigned_sample >> 9) & 0x7F;
    out[1] = (unsigned_sample >> 2) & 0x7F;
    out[2] = (unsigned_sample << 5) & 0x7F;
  }

  template <typename SendMessage>
  void send_next_packet(SendMessage send_message, absolute_time_t now) {
    etl::array<uint8_t, BYTES_PER_PACKET> raw{}; // zero-padded final packet
    const size_t remaining = total_bytes_ - bytes_sent_;
    const size_t to_read = etl::min(remaining, BYTES_PER_PACKET);

    if (file_->read(etl::span<uint8_t>{raw.data(), to_read}) != to_read) {
      logger_.error("SDS: Sample file read failed, aborting dump");
      send_cancel(send_message);
      finish();
      return;
    }

    last_packet_[0] = DATA_PACKET;
    last_packet_[1] = packet_num_;
    for (size_t i = 0; i < BYTES_PER_PACKET / 2; ++i) {
      const int16_t sample =
          static_cast<int16_t>(static_cast<uint16_t>(raw[i * 2]) |
                               (static_cast<uint16_t>(raw[i * 2 + 1]) << 8));
      pack_16bit_sample(sample, &last_packet_[2 + i * 3]);
    }
    last_packet_[122] = calculate_data_checksum(
        packet_num_, etl::span<const uint8_t>{&last_packet_[2], 120});

    header_is_last_sent_ = false;
    send_message(etl::span<const uint8_t>{last_packet_});

    bytes_sent_ += to_read;
    packet_num_ = (packet_num_ + 1) & 0x7F;
    if (state_ != State::OpenLoop) {
      state_ = State::AwaitingPacketResponse;
    } else {
      // No host responses are expected open-loop; sending is the only
      // activity that can keep the stall timeout at bay.
      last_activity_time_ = now;
    }
    last_send_time_ = now;
  }

  template <typename SendMessage>
  void resend_last(SendMessage send_message, absolute_time_t now) {
    if (header_is_last_sent_) {
      send_message(etl::span<const uint8_t>{header_});
    } else {
      send_message(etl::span<const uint8_t>{last_packet_});
    }
    last_send_time_ = now;
  }

  template <typename SendMessage>
  Result advance_open_loop(SendMessage send_message, absolute_time_t now) {
    if (transfer_complete()) {
      finish();
      logger_.info("SDS: Dump complete (open-loop)");
      return Result::SampleComplete;
    }
    send_next_packet(send_message, now);
    return Result::OK;
  }
};

} // namespace sds

#endif // SDS_DUMP_SENDER_H_SDS_AUDIO
