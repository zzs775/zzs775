#pragma once

#include <entt/entt.hpp>
#include <iostream>
#include <memory>
#include <rocky/ecs/Registry.h>
#include <rocky/vsg/MapNode.h>
#include <string>
#include <unordered_map>
#include <vsg/all.h>

// --- Core ECS Components ---

// Identifier component
struct InfoComponent
{
    std::string id;
    std::string name;
};

// Transform component bridging to Rocky/VSG
struct TransformComponent
{
    vsg::ref_ptr<vsg::MatrixTransform> node;
};

// Model component holding the 3D model
struct ModelComponent
{
    vsg::ref_ptr<vsg::Node> node;
};

// --- Instance Manager ---

class InstanceManager
{
public:
    InstanceManager(vsg::ref_ptr<vsg::Group> dynamicGroup, vsg::ref_ptr<rocky::MapNode> mapNode, rocky::Registry reg);
    ~InstanceManager();

    entt::entity CreateEntity(const std::string& id, const std::string& name, vsg::ref_ptr<vsg::Node> modelNode);
    bool RemoveEntity(const std::string& id);
    void ClearAllEntities();

    std::string GetEntityIdByNodePath(const vsg::Intersector::NodePath& path) const;

    // Apply changes using a lambda
    template<typename T, typename Func>
    void PatchEntity(const std::string& id, Func func)
    {
        auto entity = FindEntity(id);
        if (entity != entt::null)
        {
            auto write_lock = _rockyReg.write();
            auto& _registry = write_lock.registry;
            if (auto* comp = _registry.try_get<T>(entity))
            {
                func(*comp);
            }
            else
            {
                func(_registry.emplace<T>(entity));
            }
        }
    }

    entt::entity FindEntity(const std::string& id) const;
    bool HasEntity(const std::string& id) const { return FindEntity(id) != entt::null; }
    rocky::Registry& GetRegistry() { return _rockyReg; }

private:
    rocky::Registry _rockyReg;
    std::unordered_map<std::string, entt::entity> _idMap;
    std::unordered_map<const vsg::Node*, std::string> _nodeToEntityId;

    vsg::ref_ptr<vsg::Group> _dynamicGroup;
    vsg::ref_ptr<rocky::MapNode> _mapNode;
};

// --- Global Context Namespace ---
namespace _G
{
    extern std::shared_ptr<InstanceManager> entityManager;
}
