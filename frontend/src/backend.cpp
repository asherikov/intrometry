/**
    @file
    @author  Alexander Sherikov
    @copyright 2025 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)
    @brief
*/

#include <chrono>
#include <random>
#include <algorithm>
#include <ratio>
#include <thread>
#include <sstream>
#include <iomanip>

#include <intrometry/backend/utils.h>


namespace
{
    namespace intrometry_private::backend
    {
        const std::string valid_chars = "0123456789abcdefghijklmnopqrstuvwxyz";
    }  // namespace intrometry_private::backend
}  // namespace


namespace intrometry::backend
{
    uint64_t now()
    {
        return (std::chrono::duration_cast<std::chrono::nanoseconds>(
                        std::chrono::system_clock::now().time_since_epoch())
                        .count());
    }


    uint32_t getRandomUInt32()
    {
        std::mt19937 gen((std::random_device())());

        std::uniform_int_distribution<uint32_t> distrib(
                std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::max());

        return (distrib(gen));
    }


    std::string getRandomId(const std::size_t length)
    {
        std::mt19937 gen((std::random_device())());

        const std::uniform_int_distribution<uint32_t> distrib(
                std::numeric_limits<uint32_t>::min(), std::numeric_limits<uint32_t>::max());

        std::string result = intrometry_private::backend::valid_chars;
        std::shuffle(result.begin(), result.end(), gen);

        return (result.substr(0, length));
    }


    std::string normalizeId(const std::string &input_id)
    {
        std::string result;
        result.resize(input_id.size());
        std::transform(
                input_id.cbegin(),
                input_id.cend(),
                result.begin(),
                [](unsigned char c) -> unsigned char
                {
                    c = std::tolower(c);
                    if (intrometry_private::backend::valid_chars.find(static_cast<char>(c)) == std::string::npos)
                    {
                        return ('_');
                    }
                    return (c);
                });

        const std::size_t first_non_underscore = result.find_first_not_of('_');
        if (first_non_underscore == std::string::npos)
        {
            result = "";
        }
        else
        {
            if (first_non_underscore > 0)
            {
                result = result.substr(first_non_underscore);
            }
        }

        return (result);
    }


    std::string getDateString()
    {
        const std::time_t date_now = std::chrono::system_clock::to_time_t(std::chrono::system_clock::now());
        std::stringstream date_stream;
        // thread-unsafe
        date_stream << std::put_time(std::gmtime(&date_now), "%Y%m%d_%H%M%S");  // NOLINT

        return (date_stream.str());
    }
}  // namespace intrometry::backend


namespace intrometry::backend
{
    class RateTimer::Implementation
    {
    public:
        const std::chrono::nanoseconds step_;
        std::chrono::time_point<std::chrono::steady_clock> time_threshold_;

    public:
        explicit Implementation(const std::size_t rate) : step_(std::nano::den / rate)
        {
        }
    };

    RateTimer::RateTimer(const std::size_t rate) : pimpl_(std::make_unique<RateTimer::Implementation>(rate))
    {
    }

    RateTimer::~RateTimer() = default;

    bool RateTimer::valid() const
    {
        return (pimpl_->step_.count() > 0);
    }

    void RateTimer::start()
    {
        pimpl_->time_threshold_ = std::chrono::steady_clock::now();
    }

    void RateTimer::step()
    {
        // the clock is monotonic, so this is always >= 0
        const std::chrono::nanoseconds time_diff = std::chrono::steady_clock::now() - pimpl_->time_threshold_;
        pimpl_->time_threshold_ += (time_diff / pimpl_->step_ + 1) * pimpl_->step_;
        std::this_thread::sleep_until(pimpl_->time_threshold_);
    }
}  // namespace intrometry::backend
