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
#include <typeinfo>
#include <typeindex>

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
        using Key = std::pair<std::type_index, std::string>;

        struct Hasher
        {
            std::size_t operator()(const Key &key) const
            {
                std::size_t result = std::hash<std::type_index>{}(key.first);
                if (not key.second.empty())
                {
                    result ^= std::hash<std::string>{}(key.second)           // NOLINT
                              + 0x9e3779b9 + (result << 6) + (result >> 2);  // NOLINT
                }
                return (result);
            }
        };

        using SourceMap = std::unordered_map<Key, t_Value, Hasher>;
        using CollisionMap = std::unordered_map<std::string, std::size_t>;

    protected:
        SourceMap sources_;
        CollisionMap collision_counters_;

        std::mutex update_mutex_;
        std::mutex flush_mutex_;

    protected:
        /// @todo requires string copy
        static Key getKey(const std::string &id, const ariles2::DefaultBase &source)
        {
            // source.arilesDefaultID()
            // this is redundant and less reliable than type_index
            // since it is provided by the user

            // id
            // this is also provided by the user and is also unreliable,
            // but in general provides extra information than type_index
            // and should be added to hash
            return { std::type_index(typeid(source)), id };
        }

        std::string getUniqueId(const std::string &id)
        {
            const typename CollisionMap::iterator collision_counter_it = collision_counters_.find(id);
            if (collision_counters_.end() == collision_counter_it)
            {
                collision_counters_[id] = 0;
                return (id);
            }

            ++collision_counter_it->second;
            return (str_concat(id, "_intrometry", std::to_string(collision_counter_it->second)));
        }

    public:
        void tryVisit(const std::function<void(t_Value &)> visitor)
        {
            if (flush_mutex_.try_lock())
            {
                for (std::pair<const Key, t_Value> &source : sources_)
                {
                    visitor(source.second);
                }

                flush_mutex_.unlock();
            }
        }

        template <class... t_Args>
        void tryEmplace(const std::string &id, const ariles2::DefaultBase &source, t_Args &&...args)
        {
            const std::lock_guard<std::mutex> update_lock(update_mutex_);
            const std::lock_guard<std::mutex> flush_lock(flush_mutex_);

            sources_.try_emplace(
                    getKey(id, source),
                    source,
                    getUniqueId(id.empty() ? source.arilesDefaultID() : id),
                    std::forward<t_Args>(args)...);
        }

        void erase(const std::string &id, const ariles2::DefaultBase &source)
        {
            const std::lock_guard<std::mutex> update_lock(update_mutex_);
            const std::lock_guard<std::mutex> flush_lock(flush_mutex_);

            sources_.erase(getKey(id, source));
        }

        bool tryVisit(
                const std::string &id,
                const ariles2::DefaultBase &source,
                const std::function<void(t_Value &)> visitor)
        {
            if (update_mutex_.try_lock())
            {
                const typename SourceMap::iterator source_it = sources_.find(getKey(id, source));

                if (sources_.end() == source_it)
                {
                    update_mutex_.unlock();
                    return (false);
                }

                visitor(source_it->second);
                update_mutex_.unlock();
            }
            return (true);
        }
    };
}  // namespace intrometry::backend
