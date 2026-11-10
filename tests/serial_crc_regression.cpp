#include <algorithm>
#include <array>
#include <cerrno>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <iostream>
#include <memory>
#include <poll.h>
#include <stdexcept>
#include <string>
#include <sys/ioctl.h>
#include <thread>
#include <unistd.h>
#include <vector>

#include "global/Config.hpp"
#include "serialport/CRC_Check.h"
#include "serialport/SerialPort.h"

namespace {

// Every open() made by the linked project code is intercepted.  The only real
// open performed by this test is the /dev/pts/ slave created by posix_openpt.
int redirected_slave = -1;
const char *expected_serial_path = nullptr;
int redirected_open_count = 0;
int unexpected_open_count = 0;
int passed_cases = 0;

using Bytes = std::vector<uint8_t>;
const Bytes tx_lost{0xA5, 0xA5, 0, 0, 0, 0, 0, 0xA3, 0x65};
const Bytes tx_found{0xA5, 0xA5, 0, 0, 0, 0, 1, 0x2A, 0x74};
const Bytes rx_mode1{0xA5, 1, 0xCE, 0, 1, 0, 0xA9, 0x19};
const Bytes rx_old_crc16{0xA5, 1, 0xCE, 0, 1, 0, 0x32, 0x1B};

void require(bool condition, const std::string &message) {
    if (!condition) throw std::runtime_error(message);
}

template <typename Test>
void run_case(const char *name, Test test) {
    test();
    ++passed_cases;
    std::cout << "\nPASS: " << name << '\n';
}

using Clock = std::chrono::steady_clock;

void wait_readable(int fd, int milliseconds) {
    pollfd item{fd, POLLIN, 0};
    int result;
    do { result = poll(&item, 1, milliseconds); } while (result < 0 && errno == EINTR);
    require(result > 0 && (item.revents & POLLIN), "PTY receive timed out");
}

Bytes read_exact(int fd, size_t length) {
    Bytes output;
    const auto deadline = Clock::now() + std::chrono::seconds(2);
    while (output.size() < length) {
        require(Clock::now() < deadline, "PTY packet read timed out");
        wait_readable(fd, 1000);
        std::array<uint8_t, 64> buffer{};
        const auto count = read(fd, buffer.data(), std::min(buffer.size(), length - output.size()));
        if (count < 0 && (errno == EAGAIN || errno == EINTR)) continue;
        require(count > 0, "PTY packet read failed");
        output.insert(output.end(), buffer.begin(), buffer.begin() + count);
    }
    return output;
}

}  // namespace

extern "C" int __real_open(const char *path, int flags, ...);

extern "C" int __wrap_open(const char *path, int /*flags*/, ...) {
    if (redirected_slave < 0 || expected_serial_path == nullptr ||
        std::strcmp(path, expected_serial_path) != 0) {
        ++unexpected_open_count;
        errno = EPERM;
        return -1;
    }
    ++redirected_open_count;
    return dup(redirected_slave);
}

namespace {

class SerialFixture {
public:
    SerialFixture() {
        master_ = posix_openpt(O_RDWR | O_NOCTTY | O_NONBLOCK);
        require(master_ >= 0, "PTY unavailable: posix_openpt failed; no hardware fallback is allowed");
        try {
            require(grantpt(master_) == 0 && unlockpt(master_) == 0,
                    "PTY unavailable: grantpt/unlockpt failed");
            const char *name = ptsname(master_);
            require(name != nullptr && std::strncmp(name, "/dev/pts/", 9) == 0,
                    "Refusing to open a non-PTY device");
            slave_ = __real_open(name, O_RDWR | O_NOCTTY | O_NONBLOCK);
            require(slave_ >= 0, "PTY unavailable: opening the private slave failed");
            configured_path_ = static_cast<std::string>(J_SENSORS.config_["serialport"]["port"]);
            expected_serial_path = configured_path_.c_str();
            redirected_slave = slave_;
            const int calls_before = redirected_open_count;
            port_ = std::make_unique<SerialPort>();
            require(redirected_open_count == calls_before + 1, "SerialPort open was not redirected to PTY");
        } catch (...) {
            cleanup();
            throw;
        }
    }

    ~SerialFixture() { cleanup(); }
    SerialFixture(const SerialFixture &) = delete;
    SerialFixture &operator=(const SerialFixture &) = delete;

    SerialPort &port() { return *port_; }

    void queue(const Bytes &bytes) {
        size_t offset = 0;
        while (offset < bytes.size()) {
            const auto count = write(master_, bytes.data() + offset, bytes.size() - offset);
            if (count < 0 && errno == EINTR) continue;
            require(count > 0, "Writing the private PTY failed");
            offset += static_cast<size_t>(count);
        }
        const auto deadline = Clock::now() + std::chrono::seconds(1);
        int available = 0;
        while (available < static_cast<int>(bytes.size())) {
            require(ioctl(slave_, FIONREAD, &available) == 0, "PTY FIONREAD failed");
            require(Clock::now() < deadline, "PTY input was not delivered completely");
            if (available < static_cast<int>(bytes.size())) std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
    }

    Bytes sent_packet() { return read_exact(master_, sizeof(VisionData)); }

private:
    void cleanup() {
        port_.reset();
        redirected_slave = -1;
        expected_serial_path = nullptr;
        if (slave_ >= 0) close(slave_);
        if (master_ >= 0) close(master_);
        slave_ = master_ = -1;
    }

    int master_ = -1;
    int slave_ = -1;
    std::string configured_path_;
    std::unique_ptr<SerialPort> port_;
};

void require_rejected(SerialFixture &fixture, Bytes bytes, const char *message) {
    CarData output{};
    output.mode = 0x7F;
    output.number = 0x7E;
    fixture.queue(bytes);
    require(!fixture.port().receive(output), message);
    require(output.mode == 0x7F && output.number == 0x7E,
            "A rejected packet changed output data");
}

void crc_vectors() {
    static_assert(sizeof(CarData) == 8);
    static_assert(sizeof(VisionData) == 9);

    std::array<uint8_t, 2> header{0xA5, 0};
    Append_CRC8_Check_Sum(header.data(), header.size());
    require(header[1] == 0xA5 && Verify_CRC8_Check_Sum(header.data(), header.size()),
            "Two-byte TX header CRC8 is wrong");

    std::array<uint8_t, 3> incoming_header{0xA5, 1, 0};
    Append_CRC8_Check_Sum(incoming_header.data(), incoming_header.size());
    require(incoming_header[2] == 0xCE &&
            Verify_CRC8_Check_Sum(incoming_header.data(), incoming_header.size()),
            "Three-byte RX header CRC8 is wrong");

    Bytes text{'1','2','3','4','5','6','7','8','9'};
    require(Get_CRC16_Check_Sum(text.data(), text.size(), 0xFFFF) == 0xE7A1,
            "CRC16 differs from the supplied firmware's 123456789 vector");

    for (const auto *golden : {&tx_lost, &tx_found, &rx_mode1}) {
        Bytes packet = *golden;
        require(Verify_CRC16_Check_Sum(packet.data(), packet.size()), "Firmware golden CRC16 was rejected");
        packet[packet.size() - 2] = packet.back() = 0;
        Append_CRC16_Check_Sum(packet.data(), packet.size());
        require(packet == *golden, "Append CRC16 differs from firmware golden bytes");
    }
    Bytes legacy = rx_old_crc16;
    require(!Verify_CRC16_Check_Sum(legacy.data(), legacy.size()), "Old CRC16 unexpectedly accepted");
}

void invalid_crc_lengths() {
    for (unsigned length = 0; length <= 1; ++length) {
        std::array<uint8_t, 3> buffer{0xA5, 0x55, 0xAA};
        const auto before = buffer;
        Append_CRC8_Check_Sum(buffer.data(), length);
        require(buffer == before && !Verify_CRC8_Check_Sum(buffer.data(), length),
                "CRC8 accepted or changed an invalid short message");
    }
    for (unsigned length = 0; length <= 2; ++length) {
        std::array<uint8_t, 3> buffer{0xA5, 0x55, 0xAA};
        const auto before = buffer;
        Append_CRC16_Check_Sum(buffer.data(), length);
        require(buffer == before && !Verify_CRC16_Check_Sum(buffer.data(), length),
                "CRC16 accepted or changed an invalid short message");
    }
    Append_CRC8_Check_Sum(nullptr, 3);
    Append_CRC16_Check_Sum(nullptr, 8);
    require(!Verify_CRC8_Check_Sum(nullptr, 3) && !Verify_CRC16_Check_Sum(nullptr, 8),
            "CRC verification accepted a null message");
}

void actual_send() {
    SerialFixture fixture;
    VisionData output{};
    output.yaw_error = 0.0F;
    output.target_status = 0;
    fixture.port().send(output);
    require(fixture.sent_packet() == tx_lost, "Actual SerialPort::send lost packet differs from firmware bytes");
    output.target_status = 1;
    fixture.port().send(output);
    require(fixture.sent_packet() == tx_found, "Actual SerialPort::send found packet differs from firmware bytes");
}

void actual_receive() {
    SerialFixture fixture;
    CarData output{};
    fixture.queue(rx_mode1);
    require(fixture.port().receive(output), "Actual receive rejected valid firmware packet");
    require(output.header == 0xA5 && output.mode == 1 && output.CRC8 == 0xCE &&
            output.status == 0 && output.number == 1 && output.dune == 0 && output.CRC16 == 0x19A9,
            "Actual receive decoded wrong fields or CRC16 byte order");

    Bytes corrupt_body = rx_mode1;
    corrupt_body[4] ^= 1;
    require_rejected(fixture, corrupt_body, "Body corruption passed receive CRC16 checking");

    Bytes corrupt_crc8 = rx_mode1;
    corrupt_crc8[2] ^= 1;
    // Recompute the tail so rejection must be caused by the header CRC8 check.
    Append_CRC16_Check_Sum(corrupt_crc8.data(), corrupt_crc8.size());
    require_rejected(fixture, corrupt_crc8, "Invalid CRC8 with valid CRC16 was accepted");

    Bytes corrupt_tail = rx_mode1;
    corrupt_tail.back() ^= 1;
    require_rejected(fixture, corrupt_tail, "Invalid CRC16 tail was accepted");
    require_rejected(fixture, rx_old_crc16, "Old firmware CRC16 was accepted by actual receive");
}

void receive_short_frames() {
    SerialFixture fixture;
    for (size_t length = 1; length < sizeof(CarData); ++length) {
        Bytes fragment(rx_mode1.begin(), rx_mode1.begin() + length);
        require_rejected(fixture, fragment, "A short frame was accepted");
        // Cross-read reassembly is intentionally outside this regression's scope.
    }
    CarData output{};
    require(!fixture.port().receive(output), "An empty PTY should not produce a frame");
}

void receive_latest_complete_frame() {
    SerialFixture fixture;
    Bytes newer = rx_mode1;
    newer[3] = 2;
    newer[4] = 4;
    Append_CRC16_Check_Sum(newer.data(), newer.size());
    Bytes combined = rx_mode1;
    combined.insert(combined.end(), newer.begin(), newer.end());
    fixture.queue(combined);
    CarData output{};
    require(fixture.port().receive(output) && output.status == 2 && output.number == 4,
            "Receive did not choose the newest complete frame");

    Bytes complete_then_partial = rx_mode1;
    complete_then_partial.insert(complete_then_partial.end(), newer.begin(), newer.begin() + 4);
    fixture.queue(complete_then_partial);
    require(fixture.port().receive(output) && output.status == 0 && output.number == 1,
            "Trailing partial frame prevented receiving a complete frame");
}

}  // namespace

int main() {
    try {
        run_case("fixed firmware CRC vectors and two-/three-byte CRC8 headers", crc_vectors);
        run_case("CRC invalid lengths and null messages", invalid_crc_lengths);
        run_case("actual SerialPort::send over private PTY", actual_send);
        run_case("actual receive accepts valid frame and rejects corrupt/old CRCs", actual_receive);
        run_case("one- to seven-byte fragments stay in bounds", receive_short_frames);
        run_case("newest complete frame and complete frame followed by a partial frame", receive_latest_complete_frame);
        require(unexpected_open_count == 0, "Project attempted an unexpected open call");
        std::cout << "\nPASS: " << passed_cases << " regression groups; " << redirected_open_count
                  << " project open calls redirected to private PTYs; no real serial port opened.\n";
        return 0;
    } catch (const std::exception &error) {
        std::cerr << "\nFAIL: " << error.what() << '\n';
        return 1;
    }
}
