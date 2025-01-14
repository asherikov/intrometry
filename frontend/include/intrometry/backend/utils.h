/**
    @file
    @author  Alexander Sherikov
    @copyright 2025 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)

    @brief Source parameters and necessary ariles inclusions.
*/

#pragma once

#include <memory>
#include <mutex>
#include <unordered_map>
#include <functional>

#include <ariles2/ariles.h>

#define INTROMETRY_PUBLIC __attribute__((visibility("default")))
#define INTROMETRY_HIDDEN __attribute__((visibility("hidden")))


namespace intrometry::backend
{
    template <typename... t_String>
    std::string str_concat(t_String &&...strings)
    {
        std::string result;
        (result += ... += strings);
        return result;
    }

    uint64_t now();
    uint32_t getRandomUInt32();

    std::string getRandomId(const std::size_t length);
    std::string normalizeId(const std::string &input_id);
    std::string getDateString();


    class INTROMETRY_HIDDEN RateTimer
    {
    protected:
        class Implementation;

    protected:
        const std::unique_ptr<Implementation> pimpl_;

    public:
        explicit RateTimer(const std::size_t rate);
        ~RateTimer();
        [[nodiscard]] bool valid() const;
        void start();
        void step();
    };


    template <class t_Value>
    class INTROMETRY_HIDDEN SourceContainer
    {
    protected:
        using SourceSet = std::unordered_map<std::string, t_Value>;

        SourceSet sources_;

        std::mutex update_mutex_;
        std::mutex drain_mutex_;

    public:
        void tryVisit(const std::function<void(t_Value &)> visitor)
        {
            if (drain_mutex_.try_lock())
            {
                for (std::pair<const std::string, t_Value> &source : sources_)
                {
                    visitor(source.second);
                }

                drain_mutex_.unlock();
            }
        }

        template <class... t_Args>
        void tryEmplace(const ariles2::DefaultBase &source, t_Args &&...args)
        {
            const std::lock_guard<std::mutex> update_lock(update_mutex_);
            const std::lock_guard<std::mutex> drain_lock(drain_mutex_);

            sources_.try_emplace(source.arilesDefaultID(), source, std::forward<t_Args>(args)...);
        }

        void erase(const ariles2::DefaultBase &source)
        {
            const std::lock_guard<std::mutex> update_lock(update_mutex_);
            const std::lock_guard<std::mutex> drain_lock(drain_mutex_);

            sources_.erase(source.arilesDefaultID());
        }

        bool tryVisit(const ariles2::DefaultBase &source, const std::function<void(t_Value &)> visitor)
        {
            if (update_mutex_.try_lock())
            {
                const typename SourceSet::iterator source_it = sources_.find(source.arilesDefaultID());

                if (sources_.end() == source_it)
                {
                    update_mutex_.unlock();
                    return (false);
                }
                else
                {
                    visitor(source_it->second);
                }

                update_mutex_.unlock();
            }
            return (true);
        }
    };
}  // namespace intrometry::backend
