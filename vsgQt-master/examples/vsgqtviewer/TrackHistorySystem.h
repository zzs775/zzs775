#pragma once
#include <chrono>
#include <deque>
#include <limits>
#include <rocky/ecs/Transform.h>
#include <rocky/vsg/VSGContext.h>
#include <rocky/vsg/ecs/LineSystem.h>
#include <rocky/vsg/ecs/System.h>
#include <rocky/ecs/Visibility.h>

using namespace ROCKY_NAMESPACE;

struct TrackHistory
{
    struct Chunk
    {
        entt::entity attach_point = entt::null;
        std::size_t numPoints = 0;
    };

    entt::entity style = entt::null;
    unsigned maxPoints = 2000;

    std::deque<Chunk> chunks;
};

class TrackHistorySystem : public System
{
public:
    float update_hertz = 20.0f;
    bool tracks_visible = true;
    std::vector<TrackHistory::Chunk> freelist;

    static auto create(Registry& r)
    {
        return std::make_shared<TrackHistorySystem>(r);
    }

    TrackHistorySystem(Registry& r) : System(r)
    {
        r.write([&](entt::registry& reg) {
            reg.on_destroy<TrackHistory>().connect<&TrackHistorySystem::on_destroy_TrackHistory>(this);
        });
    }

    void update(VSGContext& vsgcontext) override
    {
        auto now = std::chrono::steady_clock::now();
        auto freq = std::chrono::nanoseconds((long long)(1e9 / update_hertz));
        auto elapsed = (now - last_update);

        if (elapsed >= freq)
        {
            auto write_lock = _registry.write();
            auto& reg = write_lock.registry;

            auto view = reg.view<TrackHistory, Transform>();
            for (auto&& [entity, track, transform] : view.each())
            {
                if (transform.position.valid())
                {
                    const int track_chunk_size = 8;
                    if (track.chunks.empty() || track.chunks.back().numPoints >= track_chunk_size)
                    {
                        addChunk(reg, entity, track, transform);
                        updateChunk(reg, entity, track, transform, track.chunks.back());
                    }
                    else
                    {
                        updateChunk(reg, entity, track, transform, track.chunks.back());

                        auto maxChunks = track.maxPoints / track_chunk_size;
                        if (track.chunks.size() > 1u && track.chunks.size() > maxChunks)
                        {
                            releaseChunk(reg, std::move(track.chunks.front()));
                            track.chunks.pop_front();
                        }
                    }
                }
            }

            last_update = now;
        }

        _registry.read([&](entt::registry& registry) {
            updateVisibility(registry);
        });
    }

    void reset(entt::registry& reg, entt::entity entity, TrackHistory& track)
    {
        for (auto& chunk : track.chunks)
        {
            releaseChunk(reg, std::move(chunk));
        }
        track.chunks.clear();
    }

    void addChunk(entt::registry& registry, entt::entity host_entity, TrackHistory& track, Transform& transform)
    {
        auto chunk = createChunk(registry, track.style);
        track.chunks.emplace_back(chunk);

        updateVisibility(registry, host_entity, chunk);

        auto& geom = registry.get<LineGeometry>(chunk.attach_point);
        geom.srs = transform.position.srs;

        if (track.chunks.size() > 1)
        {
            auto prev_chunk = std::prev(std::prev(track.chunks.end()));
            auto& prev_geom = registry.get<LineGeometry>(prev_chunk->attach_point);

            geom.points.emplace_back(prev_geom.points.back());
            geom.dirty(registry);
            chunk.numPoints++;
        }

        (void)registry.get_or_emplace<ActiveState>(chunk.attach_point);
    }

    void updateChunk(entt::registry& registry, entt::entity host_entity, TrackHistory& track, Transform& transform, TrackHistory::Chunk& chunk)
    {
        auto& geom = registry.get<LineGeometry>(chunk.attach_point);

        if (geom.points.size() > 0 && geom.points.back() == (glm::dvec3)(transform.position))
            return;

        geom.points.emplace_back((glm::dvec3)transform.position);
        geom.dirty(registry);
        chunk.numPoints++;
    }

    void updateVisibility(entt::registry& registry, entt::entity host_entity, TrackHistory::Chunk& chunk)
    {
        // 确保 chunk 上有 Visibility 组件
        if (!registry.all_of<Visibility>(chunk.attach_point))
            registry.emplace<Visibility>(chunk.attach_point);

        bool shouldBeVisible = tracks_visible && registry.all_of<ActiveState>(host_entity);
        if (shouldBeVisible && registry.all_of<Visibility>(host_entity))
        {
            // 从实体的 Visibility 继承可见性（取 view 0）
            shouldBeVisible = registry.get<Visibility>(host_entity).visible[0];
        }

        // 使用 rocky 的 setVisible 辅助函数（内部用 fill()，准确设置所有视图）
        setVisible(registry, chunk.attach_point, shouldBeVisible);
    }

    void updateVisibility(entt::registry& registry)
    {
        registry.view<TrackHistory>().each([&](auto entity, auto& track) {
            for (auto& chunk : track.chunks)
            {
                updateVisibility(registry, entity, chunk);
            }
        });
    }

protected:
    std::chrono::steady_clock::time_point last_update = std::chrono::steady_clock::now();

    void on_destroy_TrackHistory(entt::registry& registry, entt::entity entity)
    {
        auto& track = registry.get<TrackHistory>(entity);
        for (auto& chunk : track.chunks)
        {
            releaseChunk(registry, std::move(chunk));
        }
        track.chunks.clear();
    }

    void releaseChunk(entt::registry& reg, TrackHistory::Chunk&& chunk)
    {
        reg.remove<ActiveState>(chunk.attach_point);
        auto& geom = reg.get<LineGeometry>(chunk.attach_point);
        geom.recycle(reg);
        freelist.emplace_back(std::move(chunk));
    }

    TrackHistory::Chunk createChunk(entt::registry& reg, entt::entity styleEntity)
    {
        if (freelist.empty())
        {
            TrackHistory::Chunk chunk;
            chunk.attach_point = reg.create();
            auto& geom = reg.emplace<LineGeometry>(chunk.attach_point);
            if (reg.valid(styleEntity))
            {
                auto& style = reg.get<LineStyle>(styleEntity);
                reg.emplace<Line>(chunk.attach_point, geom, style);
            }
            else
            {
                reg.emplace<Line>(chunk.attach_point, geom); // Fallback
            }
            return chunk;
        }
        else
        {
            auto chunk = std::move(freelist.back());
            freelist.pop_back();

            auto& geom = reg.get<LineGeometry>(chunk.attach_point);
            geom.points.clear();
            chunk.numPoints = 0;
            geom.dirty(reg);

            auto& line = reg.get<Line>(chunk.attach_point);
            if (line.style != styleEntity && reg.valid(styleEntity))
            {
                line.style = styleEntity;
                line.dirty(reg);
            }

            return chunk;
        }
    }
};
