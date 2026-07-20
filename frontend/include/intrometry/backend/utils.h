/**
    @file
    @author  Alexander Sherikov
    @copyright 2025 Alexander Sherikov. Licensed under the Apache License,
    Version 2.0. (see LICENSE or http://www.apache.org/licenses/LICENSE-2.0)

    @brief Source parameters and necessary ariles inclusions.
*/

#pragma once

#include <memory>
#include <shared_mutex>
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
        (result += ... += std::forward<t_String>(strings));
        return result;
    }

    uint64_t now();
    uint32_t getRandomUInt32();

    std::string getRandomId(const std::size_t length);
    std::string normalizeId(const std::string &input_id);


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


    class INTROMETRY_HIDDEN SourceContainerBase
    {
    protected:
        using Key = std::pair<std::type_index, std::string>;

        struct Hasher
        {
            std::size_t operator()(const Key &key) const;
        };

        using CollisionMap = std::unordered_map<std::string, std::size_t>;

    protected:
        CollisionMap collision_counters_;

        // additions and removals of sources are exclusive
        // visits are shared since there are extra locks for each source
        std::shared_mutex sources_mutex_;

    protected:
        static Key getKey(const std::string &id, const ariles2::DefaultBase &source);
        std::string getUniqueId(const std::string &id);
    };


    template <class t_Value>
    class INTROMETRY_HIDDEN SourceContainer : public SourceContainerBase
    {
    protected:
        using SourceMap = std::unordered_map<Key, t_Value, Hasher>;

    protected:
        SourceMap sources_;

    public:
        void tryFlush(const std::function<void(t_Value &)> visitor)
        {
            if (sources_mutex_.try_lock_shared())
            {
                for (std::pair<const Key, t_Value> &source : sources_)
                {
                    visitor(source.second);
                }

                sources_mutex_.unlock();
            }
        }

        template <class... t_Args>
        void tryEmplace(const std::string &id, const ariles2::DefaultBase &source, t_Args &&...args)
        {
            const std::lock_guard lock(sources_mutex_);

            sources_.try_emplace(
                    getKey(id, source),
                    source,
                    getUniqueId(id.empty() ? source.arilesDefaultID() : id),
                    std::forward<t_Args>(args)...);
        }

        void erase(const std::string &id, const ariles2::DefaultBase &source)
        {
            const std::lock_guard lock(sources_mutex_);

            sources_.erase(getKey(id, source));
        }

        bool tryWrite(
                const std::string &id,
                const ariles2::DefaultBase &source,
                const std::function<void(t_Value &)> visitor)
        {
            if (sources_mutex_.try_lock_shared())
            {
                const typename SourceMap::iterator source_it = sources_.find(getKey(id, source));

                if (sources_.end() == source_it)
                {
                    sources_mutex_.unlock();
                    return (false);
                }

                visitor(source_it->second);
                sources_mutex_.unlock();
            }
            return (true);
        }
    };
}  // namespace intrometry::backend
