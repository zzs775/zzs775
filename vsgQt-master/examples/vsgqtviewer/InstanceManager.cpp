#include "InstanceManager.h"
#include <algorithm>
#include <iostream>

namespace _G
{
    std::shared_ptr<InstanceManager> entityManager = nullptr;
}

InstanceManager::InstanceManager(vsg::ref_ptr<vsg::Group> dynamicGroup, vsg::ref_ptr<rocky::MapNode> mapNode, rocky::Registry reg) : _rockyReg(reg), _dynamicGroup(dynamicGroup), _mapNode(mapNode)
{
}

InstanceManager::~InstanceManager()
{
    auto write_lock = _rockyReg.write();
    auto& _registry = write_lock.registry;
    _registry.clear();
}

entt::entity InstanceManager::CreateEntity(const std::string& id, const std::string& name, vsg::ref_ptr<vsg::Node> modelNode)
{
    if (HasEntity(id)) return FindEntity(id);

    auto write_lock = _rockyReg.write();
    auto& _registry = write_lock.registry;

    auto entity = _registry.create();
    _idMap[id] = entity;

    _registry.emplace<InfoComponent>(entity, InfoComponent{id, name});

    // 创建 vsg::MatrixTransform 包装节点
    auto rt = vsg::MatrixTransform::create();
    rt->setValue("DataVariance", vsg::DYNAMIC_DATA);

    if (modelNode)
    {
        rt->addChild(modelNode);
        _registry.emplace<ModelComponent>(entity, ModelComponent{modelNode});
    }

    // 关键：将位置作为 DataVariance 变更，防止静态优化剔除掉动态模型
    _registry.emplace<TransformComponent>(entity, TransformComponent{rt});

    _nodeToEntityId[rt.get()] = id;
    if (modelNode)
    {
        _nodeToEntityId[modelNode.get()] = id;
    }

    if (_dynamicGroup)
    {
        _dynamicGroup->addChild(rt);
    }

    return entity;
}

bool InstanceManager::RemoveEntity(const std::string& id)
{
    auto entity = FindEntity(id);
    if (entity == entt::null) return false;

    auto write_lock = _rockyReg.write();
    auto& _registry = write_lock.registry;

    if (auto* tc = _registry.try_get<TransformComponent>(entity))
    {
        if (_dynamicGroup && tc->node)
        {
            auto& children = _dynamicGroup->children;
            children.erase(
                std::remove_if(children.begin(), children.end(),
                               [&](const vsg::ref_ptr<vsg::Node>& child) { return child == tc->node; }),
                children.end());
            _nodeToEntityId.erase(tc->node.get());
        }
    }

    if (auto* mc = _registry.try_get<ModelComponent>(entity))
    {
        if (mc->node)
        {
            _nodeToEntityId.erase(mc->node.get());
        }
    }

    _registry.destroy(entity);
    _idMap.erase(id);
    return true;
}

void InstanceManager::ClearAllEntities()
{
    if (_dynamicGroup)
    {
        _dynamicGroup->children.clear();
    }
    auto write_lock = _rockyReg.write();
    auto& _registry = write_lock.registry;
    _registry.clear();
    _idMap.clear();
    _nodeToEntityId.clear();
}

entt::entity InstanceManager::FindEntity(const std::string& id) const
{
    auto it = _idMap.find(id);
    if (it != _idMap.end())
    {
        return it->second; // 查表并返回 Entity IDs
    }
    return entt::null;
}

std::string InstanceManager::GetEntityIdByNodePath(const vsg::Intersector::NodePath& path) const
{
    for (auto it = path.rbegin(); it != path.rend(); ++it)
    {
        auto mapIt = _nodeToEntityId.find(*it); // NodePath stores Node* directly since VSG 1.0 generally
        if (mapIt != _nodeToEntityId.end())
        {
            return mapIt->second;
        }
    }
    return "";
}
